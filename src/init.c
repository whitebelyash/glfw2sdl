/*
 * GLFW support layer on top of SDL3 -- initialization and platform info.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "internal.h"

_GLFWglobal _glfw;

/* Resolve the "GLFW platform" token from the SDL video driver name. */
static int platformFromSdlDriver(const char *driver)
{
    if (!driver || !driver[0])
        return GLFW_PLATFORM_NULL;
    if (SDL_strcmp(driver, "x11") == 0)
        return GLFW_PLATFORM_X11;
    if (SDL_strcmp(driver, "wayland") == 0)
        return GLFW_PLATFORM_WAYLAND;
    if (SDL_strcmp(driver, "cocoa") == 0)
        return GLFW_PLATFORM_COCOA;
    return GLFW_PLATFORM_NULL;
}

/* Best-effort pre-init platform detection (used by glfwGetPlatform). */
static int detectPlatformPreInit(void)
{
#if defined(__APPLE__)
    return GLFW_PLATFORM_COCOA;
#elif defined(_WIN32)
    return GLFW_PLATFORM_NULL; /* shim targets POSIX only */
#else
    const char *session = SDL_getenv("XDG_SESSION_TYPE");
    if (session && SDL_strcasecmp(session, "wayland") == 0 &&
        SDL_getenv("WAYLAND_DISPLAY"))
        return GLFW_PLATFORM_WAYLAND;
    if (SDL_getenv("WAYLAND_DISPLAY"))
        return GLFW_PLATFORM_WAYLAND;
    return GLFW_PLATFORM_X11;
#endif
}

GLFWAPI int glfwInit(void)
{
    Uint32 sdlSubsystems = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;

    if (_glfw.initialized)
    {
        _glfw.refcount++;
        return GLFW_TRUE;
    }

    memset(&_glfw, 0, sizeof(_glfw));
    _glfw.requestedPlatform = GLFW_ANY_PLATFORM;
    _glfw.resolvedPlatform  = GLFW_PLATFORM_NULL;
    _glfw.joystickHatButtons = GLFW_TRUE;   /* GLFW default */

    _glfwInitKeyTables();

    /* Redirect SDL_Log to stdout so SDL's own boot-time messages land in
     * our log stream (restored to the previous sink at glfwTerminate). */
    _glfwInstallLogOutput();

    /* Translate the GLFW_PLATFORM init hint into an SDL video driver hint. */
    switch (_glfw.requestedPlatform)
    {
        case GLFW_PLATFORM_X11:
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
            break;
        case GLFW_PLATFORM_WAYLAND:
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
            break;
        case GLFW_ANY_PLATFORM:
        default:
            break;
    }

    if (!SDL_Init(sdlSubsystems))
    {
        const char *reason = SDL_GetError();
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
                       "SDL3 initialization failed: %s", reason);
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL3 initialization failed: %s", reason);
        _glfwRestoreLogOutput();
        return GLFW_FALSE;
    }

    _glfw.resolvedPlatform = platformFromSdlDriver(SDL_GetCurrentVideoDriver());

    _glfwLogBootInfo();

    _glfwRefreshMonitors();
    _glfwRefreshJoysticks();

    _glfw.initialized = GLFW_TRUE;
    _glfw.refcount = 1;

    /* Establish default window hints (games may override via glfwWindowHint). */
    glfwDefaultWindowHints();

    return GLFW_TRUE;
}

GLFWAPI void glfwTerminate(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    /* Destroy all windows (this releases contexts as well). */
    while (_glfw.windowList)
        _glfwDestroyWindow(_glfw.windowList);

    /* Destroy all cursors. */
    while (_glfw.cursorList)
    {
        _GLFWcursor *cursor = _glfw.cursorList;
        _glfw.cursorList = cursor->next;
        if (cursor->sdlCursor)
            SDL_DestroyCursor(cursor->sdlCursor);
        free(cursor);
    }

    /* Free monitor data. */
    _glfwDestroyMonitors();

    /* Free clipboards, joystick handles. */
    if (_glfw.clipboard)
        free(_glfw.clipboard);
    _glfw.clipboard = NULL;

    for (int i = 0; i <= GLFW_JOYSTICK_LAST; i++)
    {
        _GLFWjoystick *js = &_glfw.joysticks[i];
        if (js->sdlGamepad)
            SDL_CloseGamepad(js->sdlGamepad);
        else if (js->sdlJoystick)
            SDL_CloseJoystick(js->sdlJoystick);
        memset(js, 0, sizeof(*js));
    }

    SDL_Quit();

    /* Give the host application its previous/log-default SDL log sink back
     * and reset our own (a later glfwInit re-installs the redirect). */
    _glfwRestoreLogOutput();

    _glfw.initialized = GLFW_FALSE;
    _glfw.refcount = 0;
    _glfw.resolvedPlatform = GLFW_PLATFORM_NULL;
}

GLFWAPI void glfwInitHint(int hint, int value)
{
    switch (hint)
    {
        case GLFW_PLATFORM:
            switch (value)
            {
                case GLFW_ANY_PLATFORM:
                case GLFW_PLATFORM_X11:
                case GLFW_PLATFORM_WAYLAND:
                case GLFW_PLATFORM_COCOA:
                    _glfw.requestedPlatform = value;
                    break;
                default:
                    _glfwInputError(GLFW_INVALID_ENUM,
                                    "Invalid GLFW_PLATFORM init hint value 0x%08X", value);
                    return;
            }
            break;

        case GLFW_JOYSTICK_HAT_BUTTONS:
            _glfw.joystickHatButtons = value ? GLFW_TRUE : GLFW_FALSE;
            break;

        case GLFW_ANGLE_PLATFORM_TYPE:
        case GLFW_COCOA_CHDIR_RESOURCES:
        case GLFW_COCOA_MENUBAR:
            /* Not applicable to the SDL backend; accepted and ignored. */
            break;

        default:
            _glfwInputError(GLFW_INVALID_ENUM, "Unknown init hint 0x%08X", hint);
            return;
    }
}

GLFWAPI void glfwInitAllocator(const GLFWallocator *allocator)
{
    if (allocator)
    {
        /* GLFW 3.5 requires every member to be a valid function pointer. */
        if (!allocator->allocate || !allocator->reallocate || !allocator->deallocate)
        {
            _glfwInputError(GLFW_INVALID_VALUE,
                            "Custom allocator functions must all be non-NULL");
            return;
        }
        _glfw.allocator = *allocator;
        _glfw.allocatorSet = true;
    }
    else
    {
        memset(&_glfw.allocator, 0, sizeof(_glfw.allocator));
        _glfw.allocatorSet = false;
    }
    /* Note: the shim always allocates through libc malloc/realloc/free;
     * the allocator is stored for API compatibility (see README). */
}

GLFWAPI int glfwGetPlatform(void)
{
    if (!_glfw.initialized)
        return detectPlatformPreInit();
    return _glfw.resolvedPlatform;
}

GLFWAPI int glfwPlatformSupported(int platform)
{
    switch (platform)
    {
        case GLFW_ANY_PLATFORM:
        case GLFW_PLATFORM_NULL:
            return GLFW_TRUE;

        case GLFW_PLATFORM_X11:
        case GLFW_PLATFORM_WAYLAND:
#if defined(__APPLE__) || defined(_WIN32)
            return GLFW_FALSE;
#else
            return GLFW_TRUE;
#endif

        case GLFW_PLATFORM_COCOA:
#if defined(__APPLE__)
            return GLFW_TRUE;
#else
            return GLFW_FALSE;
#endif

        default:
            _glfwInputError(GLFW_INVALID_ENUM, "Invalid platform 0x%08X", platform);
            return GLFW_FALSE;
    }
}

GLFWAPI void glfwGetVersion(int *major, int *minor, int *rev)
{
    if (major) *major = GLFW_VERSION_MAJOR;
    if (minor) *minor = GLFW_VERSION_MINOR;
    if (rev)   *rev   = GLFW_VERSION_REVISION;
}

GLFWAPI const char *glfwGetVersionString(void)
{
    return "3.5.0 SDL3 GLFW-compat shim";
}