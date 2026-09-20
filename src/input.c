/*
 * GLFW support layer on top of SDL3 -- input: keyboard, mouse, cursors,
 * clipboard, timers, joysticks and gamepads.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "internal.h"

/* ------------------------------------------------------------------ */
/* Cursor application (used when the mouse enters a window, when the   */
/* window regains focus and after every cursor-mode change)            */
/* ------------------------------------------------------------------ */

void _glfwApplyCursor(_GLFWwindow *window)
{
    if (!window)
        return;

    switch (window->cursorMode)
    {
        case GLFW_CURSOR_NORMAL:
            SDL_SetWindowRelativeMouseMode(window->sdlWindow, false);
            SDL_SetWindowMouseGrab(window->sdlWindow, false);
            window->relativeMode = false;
            SDL_ShowCursor();
            SDL_SetCursor(window->sdlCursor ? window->sdlCursor
                                            : SDL_GetDefaultCursor());
            break;

        case GLFW_CURSOR_HIDDEN:
            SDL_SetWindowRelativeMouseMode(window->sdlWindow, false);
            SDL_SetWindowMouseGrab(window->sdlWindow, false);
            window->relativeMode = false;
            SDL_HideCursor();
            break;

        case GLFW_CURSOR_DISABLED:
            SDL_HideCursor();
            SDL_SetWindowMouseGrab(window->sdlWindow, true);
            SDL_SetWindowRelativeMouseMode(window->sdlWindow, true);
            window->relativeMode = true;
            break;

        case GLFW_CURSOR_CAPTURED:
            SDL_SetWindowMouseGrab(window->sdlWindow, true);
            SDL_ShowCursor();
            SDL_SetCursor(window->sdlCursor ? window->sdlCursor
                                            : SDL_GetDefaultCursor());
            break;

        default:
            break;
    }
}

/* Per-frame cursor reconciliation.
 *
 * SDL's cursor visibility is global and a mouse grab can be released by
 * the window system (focus loss, a grab-release hotkey, ...) without the
 * application asking for it.  A cursor hidden for GLFW_CURSOR_DISABLED
 * would then stay invisible over the window until the pointer leaves it
 * or something re-sets the cursor shape.  Keep the cursor's visibility in
 * sync with whether the grab we hide it for is actually in effect.
 */
void _glfwReconcileCursor(_GLFWwindow *window)
{
    if (!window)
        return;

    switch (window->cursorMode)
    {
        case GLFW_CURSOR_DISABLED:
            /* The cursor is hidden only while the grab it relies on is
             * active; once the grab is gone the pointer must be visible. */
            if (!SDL_GetWindowMouseGrab(window->sdlWindow))
                SDL_ShowCursor();
            else if (SDL_CursorVisible())
                SDL_HideCursor();
            break;

        case GLFW_CURSOR_HIDDEN:
            if (SDL_CursorVisible())
                SDL_HideCursor();
            break;

        case GLFW_CURSOR_NORMAL:
        case GLFW_CURSOR_CAPTURED:
            if (!SDL_CursorVisible())
                SDL_ShowCursor();
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Input modes                                                         */
/* ------------------------------------------------------------------ */

GLFWAPI void glfwSetInputMode(GLFWwindow *handle, int mode, int value)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    switch (mode)
    {
        case GLFW_CURSOR:
        {
            switch (value)
            {
                case GLFW_CURSOR_NORMAL:
                case GLFW_CURSOR_HIDDEN:
                case GLFW_CURSOR_DISABLED:
                case GLFW_CURSOR_CAPTURED:
                    window->cursorMode = value;
                    _glfwApplyCursor(window);
                    break;
                default:
                    _glfwInputError(GLFW_INVALID_ENUM,
                                    "Invalid GLFW_CURSOR value 0x%08X", value);
                    return;
            }
            break;
        }

        case GLFW_STICKY_KEYS:
            window->stickyKeys = value ? true : false;
            break;
        case GLFW_STICKY_MOUSE_BUTTONS:
            window->stickyMouseButtons = value ? true : false;
            break;
        case GLFW_LOCK_KEY_MODS:
            window->lockKeyMods = value ? true : false;
            break;
        case GLFW_RAW_MOUSE_MOTION:
            window->rawMouseMotion = value ? true : false;
            /* SDL's relative mode already provides raw-style motion deltas;
             * the flag only has an effect with a disabled cursor. */
            break;

        case GLFW_UNLIMITED_MOUSE_BUTTONS:
            /* SDL reports at most a handful of buttons, so this mostly
             * serves API parity; when set, the mouse button callback is
             * not limited to GLFW_MOUSE_BUTTON_LAST (see events.c). */
            window->unlimitedMouseButtons = value ? true : false;
            break;

        case GLFW_IME:
            window->imeEnabled = value ? true : false;
            if (value)
                SDL_StartTextInput(window->sdlWindow);
            else
                SDL_StopTextInput(window->sdlWindow);
            break;

        default:
            _glfwInputError(GLFW_INVALID_ENUM, "Invalid input mode 0x%08X", mode);
    }
}

GLFWAPI int glfwGetInputMode(GLFWwindow *handle, int mode)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return 0;
    }

    switch (mode)
    {
        case GLFW_CURSOR:
            return window->cursorMode;
        case GLFW_STICKY_KEYS:
            return window->stickyKeys ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_STICKY_MOUSE_BUTTONS:
            return window->stickyMouseButtons ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_LOCK_KEY_MODS:
            return window->lockKeyMods ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_RAW_MOUSE_MOTION:
            return window->rawMouseMotion ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_UNLIMITED_MOUSE_BUTTONS:
            return window->unlimitedMouseButtons ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_IME:
            return window->imeEnabled ? GLFW_TRUE : GLFW_FALSE;
        default:
            _glfwInputError(GLFW_INVALID_ENUM, "Invalid input mode 0x%08X", mode);
    }
    return 0;
}

GLFWAPI int glfwRawMouseMotionSupported(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_FALSE;
    }

    /* SDL implements relative mouse mode natively on every video driver
     * that can express raw motion (X11, Wayland, Cocoa); where it cannot,
     * glfwSetInputMode(GLFW_CURSOR, GLFW_CURSOR_DISABLED) still gives games
     * functional raw-style deltas.  Report it as supported. */
    return GLFW_TRUE;
}

/* ------------------------------------------------------------------ */
/* IME (stored state only)                                             */
/*                                                                     */
/* SDL3 reports text input but no preedit/candidate events, so these   */
/* functions accept and store the IME configuration without ever       */
/* delivering preedit callbacks.  Games that use IME for CJK text will  */
/* still get final text via glfwSetCharCallback.                       */
/* ------------------------------------------------------------------ */

static void imeCheckInit(void)
{
    if (!_glfw.initialized)
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
}

GLFWAPI GLFWpreeditfun glfwSetPreeditCallback(GLFWwindow *handle, GLFWpreeditfun callback)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    const GLFWpreeditfun previous = window->preeditCb;
    window->preeditCb = callback;
    return previous;
}

GLFWAPI GLFWimestatusfun glfwSetIMEStatusCallback(GLFWwindow *handle, GLFWimestatusfun callback)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    const GLFWimestatusfun previous = window->imeStatusCb;
    window->imeStatusCb = callback;
    return previous;
}

GLFWAPI GLFWpreeditcandidatefun glfwSetPreeditCandidateCallback(GLFWwindow *handle,
                                                               GLFWpreeditcandidatefun callback)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    const GLFWpreeditcandidatefun previous = window->preeditCandidateCb;
    window->preeditCandidateCb = callback;
    return previous;
}

GLFWAPI void glfwSetPreeditCursorRectangle(GLFWwindow *handle,
                                          int x, int y, int w, int h)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    imeCheckInit();
    if (!_glfw.initialized)
        return;
    window->preeditX = x;
    window->preeditY = y;
    window->preeditW = w;
    window->preeditH = h;

    /* Tell SDL where the text cursor is so the IME candidate window follows
     * it (no-op on drivers without an IME). */
    const SDL_Rect rect = { x, y, w, h };
    SDL_SetTextInputArea(window->sdlWindow, &rect, 0);
}

GLFWAPI void glfwGetPreeditCursorRectangle(GLFWwindow *handle,
                                          int *x, int *y, int *w, int *h)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (x) *x = 0;
    if (y) *y = 0;
    if (w) *w = 0;
    if (h) *h = 0;
    imeCheckInit();
    if (!_glfw.initialized)
        return;
    if (x) *x = window->preeditX;
    if (y) *y = window->preeditY;
    if (w) *w = window->preeditW;
    if (h) *h = window->preeditH;
}

GLFWAPI void glfwResetPreeditText(GLFWwindow *handle)
{
    /* No live IME session to reset through SDL; accepted for API parity. */
    imeCheckInit();
}

GLFWAPI unsigned int *glfwGetPreeditCandidate(GLFWwindow *handle,
                                              int index, int *textCount)
{
    if (textCount) *textCount = 0;
    imeCheckInit();
    /* Preedit candidates are only produced on Windows in real GLFW; the
     * SDL backend without IME support has no candidates to return. */
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Key / mouse button state                                            */
/* ------------------------------------------------------------------ */

GLFWAPI int glfwGetKey(GLFWwindow *handle, int key)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_RELEASE;
    }

    if (key < 0 || key > GLFW_KEY_LAST)
    {
        /* GLFW_KEY_UNKNOWN is never a real key; other out-of-range keys
         * are rejected. */
        if (key == GLFW_KEY_UNKNOWN)
            return GLFW_RELEASE;
        _glfwInputError(GLFW_INVALID_ENUM, "Invalid key %i", key);
        return GLFW_RELEASE;
    }

    if (window->stickyKeys && window->keys[key] == _GLFW_STICK_PRESS)
    {
        window->keys[key] = GLFW_RELEASE;
        return GLFW_PRESS;
    }

    return window->keys[key];
}

GLFWAPI int glfwGetKeyScancode(int key)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return -1;
    }
    if (key < 0 || key > GLFW_KEY_LAST)
    {
        _glfwInputError(GLFW_INVALID_ENUM, "Invalid key %i", key);
        return -1;
    }
    return (int)_glfwKeyToScan(key);
}

GLFWAPI const char *glfwGetKeyName(int key, int scancode)
{
    SDL_Scancode scan;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }

    if (scancode != -1)
        scan = (SDL_Scancode)scancode;
    else if (key != GLFW_KEY_UNKNOWN)
        scan = _glfwKeyToScan(key);
    else
        return NULL;

    if (scan == SDL_SCANCODE_UNKNOWN)
        return NULL;

    /* SDL_GetKeyFromScancode applies the current keyboard layout, which is
     * what GLFW's key names do. */
    const SDL_Keycode keyCode = SDL_GetKeyFromScancode(scan, SDL_GetModState(), true);
    if (keyCode == 0)
        return NULL;

    const char *name = SDL_GetKeyName(keyCode);
    if (!name || !name[0])
        return NULL;
    return name;
}

GLFWAPI int glfwGetMouseButton(GLFWwindow *handle, int button)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_RELEASE;
    }

    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST)
    {
        _glfwInputError(GLFW_INVALID_ENUM, "Invalid mouse button %i", button);
        return GLFW_RELEASE;
    }

    if (window->stickyMouseButtons && window->mouseButtons[button] == _GLFW_STICK_PRESS)
    {
        window->mouseButtons[button] = GLFW_RELEASE;
        return GLFW_PRESS;
    }

    return window->mouseButtons[button];
}

/* ------------------------------------------------------------------ */
/* Cursor position                                                     */
/* ------------------------------------------------------------------ */

GLFWAPI void glfwGetCursorPos(GLFWwindow *handle, double *xpos, double *ypos)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;

    if (xpos) *xpos = 0.0;
    if (ypos) *ypos = 0.0;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    if (xpos) *xpos = window->virtualX;
    if (ypos) *ypos = window->virtualY;
}

GLFWAPI void glfwSetCursorPos(GLFWwindow *handle, double xpos, double ypos)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    window->virtualX = xpos;
    window->virtualY = ypos;

    if (!window->relativeMode)
        SDL_WarpMouseInWindow(window->sdlWindow, (float)xpos, (float)ypos);
}

/* ------------------------------------------------------------------ */
/* Cursor objects                                                      */
/* ------------------------------------------------------------------ */

GLFWAPI GLFWcursor *glfwCreateCursor(const GLFWimage *image, int xhot, int yhot)
{
    _GLFWcursor *cursor;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }

    if (!image || !image->pixels || image->width <= 0 || image->height <= 0)
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid cursor image");
        return NULL;
    }

    SDL_Surface *surface =
        SDL_CreateSurfaceFrom(image->width, image->height,
                              SDL_PIXELFORMAT_RGBA8888, image->pixels,
                              image->width * 4);
    if (!surface)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_CreateSurfaceFrom failed: %s", SDL_GetError());
        return NULL;
    }

    SDL_Cursor *sdlCursor = SDL_CreateColorCursor(surface, xhot, yhot);
    SDL_DestroySurface(surface);
    if (!sdlCursor)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_CreateColorCursor failed: %s", SDL_GetError());
        return NULL;
    }

    cursor = calloc(1, sizeof(_GLFWcursor));
    if (!cursor)
    {
        SDL_DestroyCursor(sdlCursor);
        _glfwInputError(GLFW_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    cursor->sdlCursor = sdlCursor;
    cursor->next = _glfw.cursorList;
    _glfw.cursorList = cursor;
    return (GLFWcursor *)cursor;
}

GLFWAPI GLFWcursor *glfwCreateStandardCursor(int shape)
{
    SDL_SystemCursor sdlShape;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }

    switch (shape)
    {
        case GLFW_ARROW_CURSOR:     sdlShape = SDL_SYSTEM_CURSOR_DEFAULT;  break;
        case GLFW_IBEAM_CURSOR:     sdlShape = SDL_SYSTEM_CURSOR_TEXT;     break;
        case GLFW_CROSSHAIR_CURSOR: sdlShape = SDL_SYSTEM_CURSOR_CROSSHAIR; break;
        case GLFW_HAND_CURSOR:      sdlShape = SDL_SYSTEM_CURSOR_POINTER;  break;
        case GLFW_HRESIZE_CURSOR:   sdlShape = SDL_SYSTEM_CURSOR_EW_RESIZE; break;
        case GLFW_VRESIZE_CURSOR:   sdlShape = SDL_SYSTEM_CURSOR_NS_RESIZE; break;
        default:
            _glfwInputError(GLFW_INVALID_ENUM, "Invalid standard cursor shape 0x%08X",
                            shape);
            return NULL;
    }

    SDL_Cursor *sdlCursor = SDL_CreateSystemCursor(sdlShape);
    if (!sdlCursor)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_CreateSystemCursor failed: %s", SDL_GetError());
        return NULL;
    }

    _GLFWcursor *cursor = calloc(1, sizeof(_GLFWcursor));
    if (!cursor)
    {
        SDL_DestroyCursor(sdlCursor);
        _glfwInputError(GLFW_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    cursor->sdlCursor = sdlCursor;
    cursor->standard = true;
    cursor->next = _glfw.cursorList;
    _glfw.cursorList = cursor;
    return (GLFWcursor *)cursor;
}

GLFWAPI void glfwDestroyCursor(GLFWcursor *handle)
{
    _GLFWcursor *cursor = (_GLFWcursor *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    _GLFWcursor **link = &_glfw.cursorList;
    while (*link && *link != cursor)
        link = &(*link)->next;
    if (!*link)
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid cursor handle");
        return;
    }
    *link = cursor->next;

    /* Reset any window that was using this cursor to the default. */
    for (_GLFWwindow *w = _glfw.windowList; w; w = w->next)
    {
        if (w->sdlCursor == cursor->sdlCursor)
        {
            w->sdlCursor = NULL;
            _glfwApplyCursor(w);
        }
    }

    SDL_DestroyCursor(cursor->sdlCursor);
    free(cursor);
}

GLFWAPI void glfwSetCursor(GLFWwindow *handle, GLFWcursor *cursorHandle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    _GLFWcursor *cursor = (_GLFWcursor *)cursorHandle;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    if (cursor)
    {
        /* Validate the handle. */
        bool found = false;
        for (_GLFWcursor *c = _glfw.cursorList; c; c = c->next)
        {
            if (c == cursor) { found = true; break; }
        }
        if (!found)
        {
            _glfwInputError(GLFW_INVALID_VALUE, "Invalid cursor handle");
            return;
        }
        window->sdlCursor = cursor->sdlCursor;
        _glfwApplyCursor(window);
    }
    else
    {
        window->sdlCursor = NULL;
        _glfwApplyCursor(window);
    }
}

/* ------------------------------------------------------------------ */
/* Clipboard                                                           */
/* ------------------------------------------------------------------ */

GLFWAPI void glfwSetClipboardString(GLFWwindow *handle, const char *string)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (!SDL_SetClipboardText(string ? string : ""))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_SetClipboardText failed: %s", SDL_GetError());
    }
}

GLFWAPI const char *glfwGetClipboardString(GLFWwindow *handle)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }

    char *sdlText = SDL_GetClipboardText();
    if (!sdlText)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_GetClipboardText failed: %s", SDL_GetError());
        return NULL;
    }

    const size_t len = SDL_strlen(sdlText) + 1;
    if (len > _glfw.clipboardCap)
    {
        char *grown = realloc(_glfw.clipboard, len);
        if (!grown)
        {
            SDL_free(sdlText);
            _glfwInputError(GLFW_OUT_OF_MEMORY, "Out of memory");
            return NULL;
        }
        _glfw.clipboard = grown;
        _glfw.clipboardCap = len;
    }
    memcpy(_glfw.clipboard, sdlText, len);
    _glfw.clipboardLen = len;
    SDL_free(sdlText);
    return _glfw.clipboard;
}

/* ------------------------------------------------------------------ */
/* Time                                                                */
/* ------------------------------------------------------------------ */

static double nowSeconds(void)
{
    return (double)SDL_GetPerformanceCounter() /
           (double)SDL_GetPerformanceFrequency();
}

GLFWAPI double glfwGetTime(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return 0.0;
    }
    return nowSeconds() + _glfw.timeOffset;
}

GLFWAPI void glfwSetTime(double time)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (time != time || time < 0.0)
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid time value %f", time);
        return;
    }
    _glfw.timeOffset = time - nowSeconds();
}

GLFWAPI uint64_t glfwGetTimerValue(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return 0;
    }
    return (uint64_t)SDL_GetPerformanceCounter();
}

GLFWAPI uint64_t glfwGetTimerFrequency(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return 0;
    }
    return (uint64_t)SDL_GetPerformanceFrequency();
}

/* ------------------------------------------------------------------ */
/* Joysticks and gamepads                                              */
/* ------------------------------------------------------------------ */

_GLFWjoystick *_glfwSlot(int jid)
{
    if (jid < GLFW_JOYSTICK_1 || jid > GLFW_JOYSTICK_LAST)
        return NULL;
    return &_glfw.joysticks[jid];
}

SDL_JoystickID _glfwJoystickSlotToSDL(int jid)
{
    _GLFWjoystick *js = _glfwSlot(jid);
    return js ? js->instanceID : 0;
}

/* (Re)map GLFW joystick slots 1..16 onto the currently connected SDL
 * joysticks (in SDL's enumeration order), keeping device identity. */
void _glfwRefreshJoysticks(void)
{
    SDL_JoystickID previous[GLFW_JOYSTICK_LAST + 1];
    int count = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&count);

    for (int i = 0; i <= GLFW_JOYSTICK_LAST; i++)
    {
        previous[i] = _glfw.joysticks[i].instanceID;
        _glfw.joysticks[i].instanceID = 0;
        _glfw.joysticks[i].isGamepad = false;
    }

    if (ids)
    {
        for (int i = 0; i < count && i <= GLFW_JOYSTICK_LAST; i++)
        {
            _GLFWjoystick *js = &_glfw.joysticks[i];
            js->instanceID = ids[i];
            js->isGamepad = SDL_IsGamepad(ids[i]) ? true : false;
            SDL_GUIDToString(SDL_GetJoystickGUIDForID(ids[i]),
                             js->guidString, sizeof(js->guidString));

            /* Keep handles open if the same device is still connected. */
            if (previous[i] != ids[i])
            {
                if (js->sdlGamepad)
                { SDL_CloseGamepad(js->sdlGamepad); js->sdlGamepad = NULL; }
                if (js->sdlJoystick)
                { SDL_CloseJoystick(js->sdlJoystick); js->sdlJoystick = NULL; }
            }
        }
        SDL_free(ids);
    }

    /* For slots that are no longer mapped, close and notify. */
    for (int i = 0; i <= GLFW_JOYSTICK_LAST; i++)
    {
        _GLFWjoystick *js = &_glfw.joysticks[i];
        if (js->instanceID == 0 && previous[i] != 0)
        {
            if (js->sdlGamepad)
            { SDL_CloseGamepad(js->sdlGamepad); js->sdlGamepad = NULL; }
            if (js->sdlJoystick)
            { SDL_CloseJoystick(js->sdlJoystick); js->sdlJoystick = NULL; }
            if (_glfw.initialized && _glfw.joystickCallback)
                _glfw.joystickCallback(i, GLFW_DISCONNECTED);
        }
        else if (js->instanceID != 0 && previous[i] == 0)
        {
            if (_glfw.initialized && _glfw.joystickCallback)
                _glfw.joystickCallback(i, GLFW_CONNECTED);
        }
    }
}

static SDL_Joystick *slotJoystick(_GLFWjoystick *js)
{
    if (!js->instanceID)
        return NULL;

    if (!js->sdlJoystick)
    {
        if (js->isGamepad)
        {
            js->sdlGamepad = SDL_OpenGamepad(js->instanceID);
            js->sdlJoystick = js->sdlGamepad ? SDL_GetGamepadJoystick(js->sdlGamepad) : NULL;
        }
        else
        {
            js->sdlJoystick = SDL_OpenJoystick(js->instanceID);
        }
    }
    return js->sdlJoystick;
}

GLFWAPI int glfwJoystickPresent(int jid)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_FALSE;
    }
    return _glfwJoystickSlotToSDL(jid) ? GLFW_TRUE : GLFW_FALSE;
}

GLFWAPI const float *glfwGetJoystickAxes(int jid, int *count)
{
    *count = 0;
    _GLFWjoystick *js = _glfwSlot(jid);
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    if (!js || !js->instanceID)
        return NULL;

    SDL_Joystick *joy = slotJoystick(js);
    if (!joy)
        return NULL;

    const int n = SDL_GetNumJoystickAxes(joy);
    if (n > js->axesSize)
    {
        float *grown = realloc(js->axes, (size_t)n * sizeof(float));
        if (!grown)
        {
            _glfwInputError(GLFW_OUT_OF_MEMORY, "Out of memory");
            return NULL;
        }
        js->axes = grown;
        js->axesSize = n;
    }

    for (int i = 0; i < n; i++)
    {
        /* SDL axes are -32768..32767; GLFW normalizes to -1..1. */
        const Sint16 raw = SDL_GetJoystickAxis(joy, i);
        js->axes[i] = (float)raw / 32767.0f;
        if (js->axes[i] < -1.0f) js->axes[i] = -1.0f;
        if (js->axes[i] > 1.0f)  js->axes[i] = 1.0f;
    }

    *count = n;
    return js->axes;
}

GLFWAPI const unsigned char *glfwGetJoystickButtons(int jid, int *count)
{
    *count = 0;
    _GLFWjoystick *js = _glfwSlot(jid);
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    if (!js || !js->instanceID)
        return NULL;

    SDL_Joystick *joy = slotJoystick(js);
    if (!joy)
        return NULL;

    const int n = SDL_GetNumJoystickButtons(joy);
    const int total = n + (_glfw.joystickHatButtons ? 4 : 0);

    if (total > js->buttonsSize)
    {
        unsigned char *grown = realloc(js->buttons, (size_t)total);
        if (!grown)
        {
            _glfwInputError(GLFW_OUT_OF_MEMORY, "Out of memory");
            return NULL;
        }
        js->buttons = grown;
        js->buttonsSize = total;
    }

    for (int i = 0; i < n; i++)
        js->buttons[i] = SDL_GetJoystickButton(joy, i) ? GLFW_PRESS : GLFW_RELEASE;

    if (_glfw.joystickHatButtons)
    {
        const Uint8 hat = SDL_GetJoystickHat(joy, 0);
        /* GLFW exposes the four hat directions as buttons appended after
         * the regular buttons. */
        js->buttons[n + 0] = (hat & SDL_HAT_UP)    ? GLFW_PRESS : GLFW_RELEASE;
        js->buttons[n + 1] = (hat & SDL_HAT_RIGHT) ? GLFW_PRESS : GLFW_RELEASE;
        js->buttons[n + 2] = (hat & SDL_HAT_DOWN)  ? GLFW_PRESS : GLFW_RELEASE;
        js->buttons[n + 3] = (hat & SDL_HAT_LEFT)  ? GLFW_PRESS : GLFW_RELEASE;
    }

    *count = total;
    return js->buttons;
}

GLFWAPI const unsigned char *glfwGetJoystickHats(int jid, int *count)
{
    *count = 0;
    _GLFWjoystick *js = _glfwSlot(jid);
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    if (!js || !js->instanceID)
        return NULL;

    SDL_Joystick *joy = slotJoystick(js);
    if (!joy)
        return NULL;

    const int n = SDL_GetNumJoystickHats(joy);
    if (n > js->hatsSize)
    {
        unsigned char *grown = realloc(js->hats, (size_t)n);
        if (!grown)
        {
            _glfwInputError(GLFW_OUT_OF_MEMORY, "Out of memory");
            return NULL;
        }
        js->hats = grown;
        js->hatsSize = n;
    }

    for (int i = 0; i < n; i++)
    {
        /* SDL_HAT_* and GLFW_HAT_* use the same bit layout. */
        js->hats[i] = (unsigned char)SDL_GetJoystickHat(joy, i);
    }

    *count = n;
    return js->hats;
}

GLFWAPI const char *glfwGetJoystickName(int jid)
{
    _GLFWjoystick *js = _glfwSlot(jid);
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    if (!js || !js->instanceID)
        return NULL;

    SDL_Joystick *joy = slotJoystick(js);
    return joy ? SDL_GetJoystickName(joy) : NULL;
}

GLFWAPI const char *glfwGetJoystickGUID(int jid)
{
    _GLFWjoystick *js = _glfwSlot(jid);
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    if (!js || !js->instanceID)
        return NULL;
    return js->guidString;
}

GLFWAPI void glfwSetJoystickUserPointer(int jid, void *pointer)
{
    _GLFWjoystick *js = _glfwSlot(jid);
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (!js || !js->instanceID)
    {
        _glfwInputError(GLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return;
    }
    js->userPointer = pointer;
}

GLFWAPI void *glfwGetJoystickUserPointer(int jid)
{
    _GLFWjoystick *js = _glfwSlot(jid);
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    if (!js || !js->instanceID)
    {
        _glfwInputError(GLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }
    return js->userPointer;
}

GLFWAPI int glfwJoystickIsGamepad(int jid)
{
    _GLFWjoystick *js = _glfwSlot(jid);
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_FALSE;
    }
    if (!js || !js->instanceID)
        return GLFW_FALSE;
    return js->isGamepad ? GLFW_TRUE : GLFW_FALSE;
}

GLFWAPI GLFWjoystickfun glfwSetJoystickCallback(GLFWjoystickfun callback)
{
    const GLFWjoystickfun previous = _glfw.joystickCallback;
    _glfw.joystickCallback = callback;
    return previous;
}

/* ------------------------------------------------------------------ */
/* Gamepad mappings (SDL2 "gamecontrollerdb" style, one per line)      */
/* ------------------------------------------------------------------ */

GLFWAPI int glfwUpdateGamepadMappings(const char *string)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_FALSE;
    }
    if (!string)
        return GLFW_FALSE;

    bool allOk = true;
    const char *line = string;
    while (line && *line)
    {
        const char *end = strchr(line, '\n');
        const size_t len = end ? (size_t)(end - line) : strlen(line);

        /* Split on the line (also handle the final line without \n). */
        char *buf = malloc(len + 1);
        if (buf)
        {
            memcpy(buf, line, len);
            buf[len] = '\0';

            const char *trimmed = buf;
            while (*trimmed == ' ' || *trimmed == '\t')
                trimmed++;
            if (*trimmed && *trimmed != '#')
            {
                if (!SDL_AddGamepadMapping(trimmed))
                    allOk = false;
            }
            free(buf);
        }

        line = end ? end + 1 : NULL;
    }

    return allOk ? GLFW_TRUE : GLFW_FALSE;
}

GLFWAPI const char *glfwGetGamepadName(int jid)
{
    _GLFWjoystick *js = _glfwSlot(jid);
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    if (!js || !js->instanceID || !js->isGamepad)
        return NULL;

    if (!js->sdlGamepad)
        slotJoystick(js);
    return js->sdlGamepad ? SDL_GetGamepadName(js->sdlGamepad) : NULL;
}

static SDL_GamepadButton glfwToSdlGamepadButton(int b)
{
    switch (b)
    {
        case GLFW_GAMEPAD_BUTTON_A:             return SDL_GAMEPAD_BUTTON_SOUTH;
        case GLFW_GAMEPAD_BUTTON_B:             return SDL_GAMEPAD_BUTTON_EAST;
        case GLFW_GAMEPAD_BUTTON_X:             return SDL_GAMEPAD_BUTTON_WEST;
        case GLFW_GAMEPAD_BUTTON_Y:             return SDL_GAMEPAD_BUTTON_NORTH;
        case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER:   return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
        case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER:  return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
        case GLFW_GAMEPAD_BUTTON_BACK:          return SDL_GAMEPAD_BUTTON_BACK;
        case GLFW_GAMEPAD_BUTTON_START:         return SDL_GAMEPAD_BUTTON_START;
        case GLFW_GAMEPAD_BUTTON_GUIDE:         return SDL_GAMEPAD_BUTTON_GUIDE;
        case GLFW_GAMEPAD_BUTTON_LEFT_THUMB:    return SDL_GAMEPAD_BUTTON_LEFT_STICK;
        case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB:   return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
        case GLFW_GAMEPAD_BUTTON_DPAD_UP:       return SDL_GAMEPAD_BUTTON_DPAD_UP;
        case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT:    return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
        case GLFW_GAMEPAD_BUTTON_DPAD_DOWN:     return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
        case GLFW_GAMEPAD_BUTTON_DPAD_LEFT:     return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
        default:                                return SDL_GAMEPAD_BUTTON_INVALID;
    }
}

static SDL_GamepadAxis glfwToSdlGamepadAxis(int a)
{
    switch (a)
    {
        case GLFW_GAMEPAD_AXIS_LEFT_X:       return SDL_GAMEPAD_AXIS_LEFTX;
        case GLFW_GAMEPAD_AXIS_LEFT_Y:       return SDL_GAMEPAD_AXIS_LEFTY;
        case GLFW_GAMEPAD_AXIS_RIGHT_X:      return SDL_GAMEPAD_AXIS_RIGHTX;
        case GLFW_GAMEPAD_AXIS_RIGHT_Y:      return SDL_GAMEPAD_AXIS_RIGHTY;
        case GLFW_GAMEPAD_AXIS_LEFT_TRIGGER: return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        case GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER:return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        default:                             return SDL_GAMEPAD_AXIS_INVALID;
    }
}

GLFWAPI int glfwGetGamepadState(int jid, GLFWgamepadstate *state)
{
    memset(state, 0, sizeof(*state));

    _GLFWjoystick *js = _glfwSlot(jid);
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_FALSE;
    }
    if (!js || !js->instanceID || !js->isGamepad)
        return GLFW_FALSE;

    if (!js->sdlGamepad)
        slotJoystick(js);
    if (!js->sdlGamepad)
        return GLFW_FALSE;

    for (int b = GLFW_GAMEPAD_BUTTON_A; b <= GLFW_GAMEPAD_BUTTON_DPAD_LEFT; b++)
    {
        const SDL_GamepadButton sdlButton = glfwToSdlGamepadButton(b);
        if (SDL_GetGamepadButton(js->sdlGamepad, sdlButton))
            state->buttons[b] = GLFW_PRESS;
    }

    for (int a = GLFW_GAMEPAD_AXIS_LEFT_X; a <= GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER; a++)
    {
        const Sint16 raw = SDL_GetGamepadAxis(js->sdlGamepad, glfwToSdlGamepadAxis(a));
        float v = (float)raw / 32767.0f;
        if (v < -1.0f) v = -1.0f;
        if (v > 1.0f)  v = 1.0f;
        state->axes[a] = v;
    }

    return GLFW_TRUE;
}