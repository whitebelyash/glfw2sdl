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
/* Event cooperation (coexistence with other SDL3 consumers)           */
/* ------------------------------------------------------------------ */
/*                                                                    */
/* In Minecraft, the Controlify mod is a second SDL3 consumer: it     */
/* initializes its own SDL3 and installs a global event filter that   */
/* drops everything except joystick add/remove.  When its SDL calls   */
/* land on the same SDL3 instance as ours (library soname merging),   */
/* the game's keyboard/mouse events are dropped at push time and      */
/* never reach our pump -- input goes completely silent.              */
/*                                                                    */
/* We defend by keeping a pass-through event filter installed:       */
/* events are never dropped by a foreign filter for more than one     */
/* frame.  Joystick/gamepad events are still translated for GLFW's    */
/* joystick API, but left on the shared queue so the other consumer   */
/* can poll them too.                                                 */

/* Marker makes our own filter identifiable via SDL_GetEventFilter. */
static char _glfwEventFilterMarker;

static bool SDLCALL _glfwEventFilterPassThrough(void *userdata, SDL_Event *event)
{
    (void) userdata;
    (void) event;
    return true; /* never drop */
}

/* Restore the pass-through filter if another consumer replaced it. */
static void _glfwRestoreEventFilter(void)
{
    SDL_EventFilter filter = NULL;
    void *userdata = NULL;

    if (SDL_GetEventFilter(&filter, &userdata) &&
        filter == _glfwEventFilterPassThrough &&
        userdata == &_glfwEventFilterMarker)
    {
        return; /* our filter is already in place */
    }

    SDL_SetEventFilter(_glfwEventFilterPassThrough, &_glfwEventFilterMarker);

    if (!_glfw.eventFilterWarned)
    {
        _glfw.eventFilterWarned = true;
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN,
                       "Another SDL3 consumer replaced the event filter; "
                       "restored pass-through (input coexistence)");
    }
}

void _glfwInstallEventCooperation(void)
{
    SDL_EventFilter filter = NULL;
    void *userdata = NULL;
    const bool foreignFilter =
        SDL_GetEventFilter(&filter, &userdata) &&
        (filter != _glfwEventFilterPassThrough ||
         userdata != &_glfwEventFilterMarker);

    SDL_SetEventFilter(_glfwEventFilterPassThrough, &_glfwEventFilterMarker);

    if (foreignFilter)
    {
        /* A host consumer had its own (possibly dropping) filter; ours
         * takes over so the game's input is never filtered out. */
        _glfw.eventFilterWarned = true;
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN,
                       "Replaced a foreign SDL event filter with "
                       "pass-through (input coexistence)");
    }
    else
    {
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO,
                       "Installed pass-through SDL event filter "
                       "(safe with other SDL3 consumers)");
    }
}

void _glfwRemoveEventCooperation(void)
{
    SDL_EventFilter filter = NULL;
    void *userdata = NULL;

    /* Only clear the filter if it is still ours, so a host consumer's
     * own filter is left alone. */
    if (SDL_GetEventFilter(&filter, &userdata) &&
        filter == _glfwEventFilterPassThrough &&
        userdata == &_glfwEventFilterMarker)
    {
        SDL_SetEventFilter(NULL, NULL);
    }
    _glfw.eventFilterWarned = false;
}

/* Events that belong to any other SDL consumer polling the shared queue.
 * They are translated for GLFW's joystick API, but a copy is left on the
 * queue so the other consumer still receives them. */
static bool _glfwEventBelongsToOtherConsumers(SDL_EventType type)
{
    switch (type)
    {
        case SDL_EVENT_JOYSTICK_ADDED:
        case SDL_EVENT_JOYSTICK_REMOVED:
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            return true;
        default:
            return false;
    }
}

/* Translate a single event for GLFW, leaving joystick/gamepad events on
 * the shared queue for other SDL consumers (safe outside a poll loop:
 * SDL_PushEvent only re-queues, it does not re-modify SDL's state). */
static void _glfwProcessEvent(const SDL_Event *event)
{
    if (_glfwEventBelongsToOtherConsumers(event->type))
    {
        handleEvent(event);
        SDL_PushEvent((SDL_Event *) event);
    }
    else
        handleEvent(event);
}

/* ------------------------------------------------------------------ */
/* Public event API                                                    */
/* ------------------------------------------------------------------ */

/* Drain the shared SDL event queue.  Returns the number of events that
 * were dequeued (translated for GLFW).  Joystick/gamepad events are
 * re-posted so other SDL consumers in the process still see them. */
int _glfwPumpEvents(void)
{
    _glfwRestoreEventFilter();
    SDL_PumpEvents();

    SDL_Event event;
    SDL_Event shared[8];
    int sharedCount = 0;
    int processed = 0;

    while (SDL_PollEvent(&event))
    {
        processed++;
        if (_glfwEventBelongsToOtherConsumers(event.type))
        {
            /* Collect first and re-post after the drain: re-posting inside
             * the poll loop would make the loop pick up the event again. */
            if (sharedCount < (int)(sizeof(shared) / sizeof(shared[0])))
                shared[sharedCount++] = event;
        }
        handleEvent(&event);
    }

    for (int i = 0; i < sharedCount; i++)
        SDL_PushEvent(&shared[i]);

    return processed;
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

    /* Wake as soon as at least one event was processed.  SDL_WaitEventTimeout
     * blocks (no busy loop) and returns the moment an event arrives, so a
     * wake is not delayed by the 100 ms poll interval. */
    for (;;)
    {
        if (_glfwPumpEvents() > 0)
            return;

        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 100))
        {
            _glfwProcessEvent(&event);
            _glfwPumpEvents();
            return;
        }
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

    /* Cap the timeout so seconds * 1000 fits in the SDL Sint32. */
    const double maxSeconds = 2147483.0;
    double remaining = timeout > maxSeconds ? maxSeconds : timeout;

    for (;;)
    {
        /* Handle anything that is already queued; this also means the
         * function returns immediately when events are pending. */
        if (_glfwPumpEvents() > 0)
            return;

        if (remaining <= 0.0)
            return; /* nothing arrived within the timeout */

        Sint32 ms = (Sint32)(remaining * 1000.0);
        if (ms < 1)
            ms = 1;

        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, ms))
        {
            _glfwProcessEvent(&event);
            _glfwPumpEvents();
            return;
        }

        remaining = 0.0; /* the timeout elapsed */
    }
}