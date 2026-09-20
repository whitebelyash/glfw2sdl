/*
 * GLFW support layer on top of SDL3 -- event pump and dispatch.
 *
 * Every SDL event that matters is translated here into the corresponding
 * GLFW callback or internal state update.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "internal.h"

/* ------------------------------------------------------------------ */
/* UTF-8 helper for the character callback                             */
/* ------------------------------------------------------------------ */

static const char *utf8Next(const char *p, Uint32 *codepoint)
{
    const unsigned char *s = (const unsigned char *)p;
    Uint32 cp;

    if (s[0] < 0x80)
    {
        cp = s[0];
        p += 1;
    }
    else if ((s[0] & 0xE0) == 0xC0)
    {
        cp = (Uint32)(s[0] & 0x1F) << 6 | (Uint32)(s[1] & 0x3F);
        p += 2;
    }
    else if ((s[0] & 0xF0) == 0xE0)
    {
        cp = (Uint32)(s[0] & 0x0F) << 12 |
             (Uint32)(s[1] & 0x3F) << 6 |
             (Uint32)(s[2] & 0x3F);
        p += 3;
    }
    else if ((s[0] & 0xF8) == 0xF0)
    {
        cp = (Uint32)(s[0] & 0x07) << 18 |
             (Uint32)(s[1] & 0x3F) << 12 |
             (Uint32)(s[2] & 0x3F) << 6 |
             (Uint32)(s[3] & 0x3F);
        p += 4;
    }
    else
    {
        cp = 0xFFFD; /* replacement character */
        p += 1;
    }

    *codepoint = cp;
    return p;
}

/* ------------------------------------------------------------------ */
/* Individual event handlers                                           */
/* ------------------------------------------------------------------ */

static void handleKeyEvent(_GLFWwindow *window, const SDL_KeyboardEvent *kev)
{
    int key = _glfwScanToKey(kev->scancode);
    if (key == GLFW_KEY_UNKNOWN)
    {
        /* SDL_GetScancodeFromKey also reports the modifier state. */
        SDL_Keymod keyMods = 0;
        key = _glfwScanToKey(SDL_GetScancodeFromKey(kev->key, &keyMods));
    }

    int action;
    if (kev->repeat)
        action = GLFW_REPEAT;
    else
        action = kev->down ? GLFW_PRESS : GLFW_RELEASE;

    if (key >= 0 && key <= GLFW_KEY_LAST)
    {
        if (action == GLFW_RELEASE && window->stickyKeys)
            window->keys[key] = _GLFW_STICK_PRESS;
        else
            window->keys[key] = (char)action;
    }

    const int mods = _glfwModsToGLFW(kev->mod, window->lockKeyMods);
    if (window->keyCb)
        window->keyCb((GLFWwindow *)window, key, (int)kev->scancode, action, mods);
}

static void handleCharEvent(_GLFWwindow *window, const SDL_TextInputEvent *tev)
{
    const int mods = _glfwModsToGLFW(SDL_GetModState(), window->lockKeyMods);
    const char *p = tev->text;

    while (*p)
    {
        Uint32 codepoint;
        p = utf8Next(p, &codepoint);

        if (window->charCb)
            window->charCb((GLFWwindow *)window, codepoint);
        if (window->charModsCb)
            window->charModsCb((GLFWwindow *)window, codepoint, mods);
    }
}

static void handleMouseMotion(_GLFWwindow *window, const SDL_MouseMotionEvent *mev)
{
    if (window->relativeMode)
    {
        /* Cursor disabled: GLFW reports the accumulated virtual position. */
        window->virtualX += mev->xrel;
        window->virtualY += mev->yrel;
    }
    else
    {
        window->virtualX = mev->x;
        window->virtualY = mev->y;
    }

    if (window->cursorPosCb)
        window->cursorPosCb((GLFWwindow *)window, window->virtualX, window->virtualY);
}

static void handleMouseButton(_GLFWwindow *window, const SDL_MouseButtonEvent *bev)
{
    int button = _glfwSdlMouseButtonToGLFW(bev->button);
    if (button < 0 && window->unlimitedMouseButtons && bev->button >= 1)
        button = (int)bev->button - 1; /* no GLFW_MOUSE_BUTTON_LAST limit */
    if (button < 0)
        return;

    window->virtualX = bev->x;
    window->virtualY = bev->y;

    const int action = bev->down ? GLFW_PRESS : GLFW_RELEASE;

    if (button <= GLFW_MOUSE_BUTTON_LAST)
    {
        if (action == GLFW_RELEASE && window->stickyMouseButtons)
            window->mouseButtons[button] = _GLFW_STICK_PRESS;
        else
            window->mouseButtons[button] = action;
    }

    const int mods = _glfwModsToGLFW(SDL_GetModState(), window->lockKeyMods);
    if (window->mouseButtonCb)
        window->mouseButtonCb((GLFWwindow *)window, button, action, mods);
}

static void handleScroll(_GLFWwindow *window, const SDL_MouseWheelEvent *wev)
{
    float x = wev->x;
    float y = wev->y;
    if (wev->direction == SDL_MOUSEWHEEL_FLIPPED)
    {
        x = -x;
        y = -y;
    }

    if (window->scrollCb)
        window->scrollCb((GLFWwindow *)window, x, y);
}

static void resetWindowInput(_GLFWwindow *window)
{
    for (int i = 0; i <= GLFW_KEY_LAST; i++)
    {
        if (window->keys[i] != GLFW_RELEASE)
            window->keys[i] = GLFW_RELEASE;
    }
    for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++)
    {
        if (window->mouseButtons[i] != GLFW_RELEASE)
            window->mouseButtons[i] = GLFW_RELEASE;
    }
}

static void clearDropPaths(_GLFWwindow *window)
{
    if (!window->dropPaths)
        return;
    for (int i = 0; i < window->dropPathCount; i++)
        free(window->dropPaths[i]);
    free(window->dropPaths);
    window->dropPaths = NULL;
    window->dropPathCount = 0;
    window->dropPathCapacity = 0;
}

static void addDropPath(_GLFWwindow *window, const char *path)
{
    if (window->dropPathCount >= window->dropPathCapacity)
    {
        const int cap = window->dropPathCapacity ? window->dropPathCapacity * 2 : 8;
        char **grown = realloc(window->dropPaths, (size_t)cap * sizeof(char *));
        if (!grown)
        {
            _glfwInputError(GLFW_OUT_OF_MEMORY, "Out of memory");
            return;
        }
        window->dropPaths = grown;
        window->dropPathCapacity = cap;
    }
    window->dropPaths[window->dropPathCount++] = strdup(path);
}

static void fireDrop(_GLFWwindow *window)
{
    if (!window->dropCb)
    {
        clearDropPaths(window);
        return;
    }
    window->dropCb((GLFWwindow *)window, window->dropPathCount,
                   (const char **)window->dropPaths);
    clearDropPaths(window);
}

static void handleWindowEvent(const SDL_WindowEvent *wev)
{
    _GLFWwindow *window = _glfwFindWindowByID(wev->windowID);
    if (!window)
        return;

    switch (wev->type)
    {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            window->shouldClose = GLFW_TRUE;
            if (window->closeCb)
                window->closeCb((GLFWwindow *)window);
            break;

        case SDL_EVENT_WINDOW_MOVED:
            if (window->posCb)
                window->posCb((GLFWwindow *)window, wev->data1, wev->data2);
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            if (window->sizeCb)
                window->sizeCb((GLFWwindow *)window, wev->data1, wev->data2);
            break;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            if (window->fbSizeCb)
                window->fbSizeCb((GLFWwindow *)window, wev->data1, wev->data2);
            break;

        case SDL_EVENT_WINDOW_EXPOSED:
            if (window->refreshCb)
                window->refreshCb((GLFWwindow *)window);
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if (window->focusCb)
                window->focusCb((GLFWwindow *)window, GLFW_TRUE);
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (window->focusCb)
                window->focusCb((GLFWwindow *)window, GLFW_FALSE);
            resetWindowInput(window);
            break;

        case SDL_EVENT_WINDOW_MINIMIZED:
            window->iconified = true;
            if (window->iconifyCb)
                window->iconifyCb((GLFWwindow *)window, GLFW_TRUE);
            break;

        case SDL_EVENT_WINDOW_MAXIMIZED:
            window->maximized = true;
            if (window->maximizeCb)
                window->maximizeCb((GLFWwindow *)window, GLFW_TRUE);
            break;

        case SDL_EVENT_WINDOW_RESTORED:
            if (window->maximized && window->maximizeCb)
                window->maximizeCb((GLFWwindow *)window, GLFW_FALSE);
            if (window->iconified && window->iconifyCb)
                window->iconifyCb((GLFWwindow *)window, GLFW_FALSE);
            window->maximized = false;
            window->iconified = false;
            break;

        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            window->cursorEntered = true;
            if (window->cursorEnterCb)
                window->cursorEnterCb((GLFWwindow *)window, GLFW_TRUE);
            _glfwApplyCursor(window);
            break;

        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            window->cursorEntered = false;
            if (window->cursorEnterCb)
                window->cursorEnterCb((GLFWwindow *)window, GLFW_FALSE);
            break;

        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            if (window->scaleCb)
            {
                const float scale = SDL_GetWindowDisplayScale(window->sdlWindow);
                window->scaleCb((GLFWwindow *)window, scale, scale);
            }
            break;

        default:
            break;
    }
}

static void handleEvent(const SDL_Event *event)
{
    switch (event->type)
    {
        case SDL_EVENT_QUIT:
        {
            /* GLFW: a quit request marks every window for closing. */
            for (_GLFWwindow *w = _glfw.windowList; w; w = w->next)
                w->shouldClose = GLFW_TRUE;
            break;
        }

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        {
            _GLFWwindow *window = _glfwFindWindowByID(event->key.windowID);
            if (window)
                handleKeyEvent(window, &event->key);
            break;
        }

        case SDL_EVENT_TEXT_INPUT:
        {
            _GLFWwindow *window = _glfwFindWindowByID(event->text.windowID);
            if (window)
                handleCharEvent(window, &event->text);
            break;
        }

        case SDL_EVENT_MOUSE_MOTION:
        {
            _GLFWwindow *window = _glfwFindWindowByID(event->motion.windowID);
            if (window)
                handleMouseMotion(window, &event->motion);
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            _GLFWwindow *window = _glfwFindWindowByID(event->button.windowID);
            if (window)
                handleMouseButton(window, &event->button);
            break;
        }

        case SDL_EVENT_MOUSE_WHEEL:
        {
            _GLFWwindow *window = _glfwFindWindowByID(event->wheel.windowID);
            if (window)
                handleScroll(window, &event->wheel);
            break;
        }

        case SDL_EVENT_DROP_BEGIN:
        {
            _GLFWwindow *window = _glfwFindWindowByID(event->drop.windowID);
            if (window)
                clearDropPaths(window);
            break;
        }

        case SDL_EVENT_DROP_FILE:
        {
            _GLFWwindow *window = _glfwFindWindowByID(event->drop.windowID);
            if (window && event->drop.data)
                addDropPath(window, event->drop.data);
            break;
        }

        case SDL_EVENT_DROP_COMPLETE:
        {
            _GLFWwindow *window = _glfwFindWindowByID(event->drop.windowID);
            if (window)
                fireDrop(window);
            break;
        }

        case SDL_EVENT_JOYSTICK_ADDED:
        case SDL_EVENT_JOYSTICK_REMOVED:
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            _glfwRefreshJoysticks();
            break;

        case SDL_EVENT_DISPLAY_ADDED:
        case SDL_EVENT_DISPLAY_REMOVED:
        case SDL_EVENT_DISPLAY_ORIENTATION:
        case SDL_EVENT_DISPLAY_MOVED:
            _glfwRefreshMonitors();
            break;

        default:
            if (event->type >= SDL_EVENT_WINDOW_FIRST &&
                event->type <= SDL_EVENT_WINDOW_LAST)
            {
                handleWindowEvent(&event->window);
            }
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Public event API                                                    */
/* ------------------------------------------------------------------ */

void _glfwPumpEvents(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        handleEvent(&event);
}

GLFWAPI void glfwPollEvents(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    _glfwPumpEvents();
}

GLFWAPI void glfwPostEmptyEvent(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    /* Wake glfwWaitEvents, which SDL_PollEvent/WaitEvent will then consume.
     * A user event is not translated to any GLFW callback. */
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_USER;
    if (!SDL_PushEvent(&event))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_PushEvent failed: %s", SDL_GetError());
    }
}

GLFWAPI void glfwWaitEvents(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    while (true)
    {
        SDL_Event event;
        SDL_WaitEvent(&event);
        handleEvent(&event);
        /* Process anything that arrived while we were blocked. */
        _glfwPumpEvents();
        return;
    }
}

GLFWAPI void glfwWaitEventsTimeout(double timeout)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (timeout != timeout || timeout < 0.0)
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid wait timeout %f", timeout);
        return;
    }

    SDL_Event event;
    if (SDL_WaitEventTimeout(&event, (Sint32)(timeout * 1000.0)))
        handleEvent(&event);

    _glfwPumpEvents();
}