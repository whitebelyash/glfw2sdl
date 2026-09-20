/*
 * GLFW support layer on top of SDL3 -- windows.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "internal.h"

/* ------------------------------------------------------------------ */
/* Window hint template                                                */
/* ------------------------------------------------------------------ */

/*
 * GLFW keeps a set of creation hints that are copied into each new
 * window.  We mirror that with a static template window structure.
 */
static _GLFWwindow _hints;
static char *_hintStringFrameName;
static char *_hintStringX11Class;
static char *_hintStringX11Instance;
static char *_hintStringWaylandAppId;

static void setHintString(char **slot, const char *value)
{
    free(*slot);
    *slot = value ? strdup(value) : NULL;
}

#define HINT_BOOL(hintmac, field) \
    case hintmac: _hints.field = value ? GLFW_TRUE : GLFW_FALSE; return;

GLFWAPI void glfwDefaultWindowHints(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    memset(&_hints, 0, sizeof(_hints));
    _hints.resizable            = GLFW_TRUE;
    _hints.visible              = GLFW_TRUE;
    _hints.decorated            = GLFW_TRUE;
    _hints.focused              = GLFW_TRUE;
    _hints.autoIconify          = GLFW_TRUE;
    _hints.centerCursor         = GLFW_TRUE;
    _hints.focusOnShow          = GLFW_TRUE;
    _hints.clientAPI            = GLFW_OPENGL_API;
    _hints.contextCreationAPI   = GLFW_NATIVE_CONTEXT_API;
    _hints.contextVersionMajor  = 1;
    _hints.contextVersionMinor  = 0;
    _hints.contextRobustness    = GLFW_NO_ROBUSTNESS;
    _hints.contextReleaseBehavior = GLFW_ANY_RELEASE_BEHAVIOR;
    _hints.openGLProfile        = GLFW_OPENGL_ANY_PROFILE;
    _hints.doublebuffer         = GLFW_TRUE;
    _hints.redBits              = 8;
    _hints.greenBits            = 8;
    _hints.blueBits             = 8;
    _hints.alphaBits            = 8;
    _hints.depthBits            = 24;
    _hints.stencilBits          = 8;
    _hints.refreshRate          = GLFW_DONT_CARE;

    setHintString(&_hintStringFrameName, NULL);
    setHintString(&_hintStringX11Class, NULL);
    setHintString(&_hintStringX11Instance, NULL);
    setHintString(&_hintStringWaylandAppId, NULL);
}

GLFWAPI void glfwWindowHint(int hint, int value)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    switch (hint)
    {
        HINT_BOOL(GLFW_RESIZABLE,            resizable)
        HINT_BOOL(GLFW_VISIBLE,              visible)
        HINT_BOOL(GLFW_DECORATED,            decorated)
        HINT_BOOL(GLFW_FOCUSED,              focused)
        HINT_BOOL(GLFW_AUTO_ICONIFY,         autoIconify)
        HINT_BOOL(GLFW_FLOATING,             floating)
        HINT_BOOL(GLFW_MAXIMIZED,            maximized)
        HINT_BOOL(GLFW_CENTER_CURSOR,        centerCursor)
        HINT_BOOL(GLFW_TRANSPARENT_FRAMEBUFFER, transparentFramebuffer)
        HINT_BOOL(GLFW_FOCUS_ON_SHOW,        focusOnShow)
        HINT_BOOL(GLFW_SCALE_TO_MONITOR,     scaleToMonitor)
        HINT_BOOL(GLFW_MOUSE_PASSTHROUGH,    mousePassthrough)

        case GLFW_CLIENT_API:
            if (value != GLFW_OPENGL_API && value != GLFW_OPENGL_ES_API &&
                value != GLFW_NO_API)
            {
                _glfwInputError(GLFW_INVALID_ENUM,
                                "Invalid GLFW_CLIENT_API hint value 0x%08X", value);
                return;
            }
            if (_hints.clientAPI != value)
            {
                if (_hints.clientAPI == GLFW_NO_API || value == GLFW_NO_API)
                {
                    /* Switching between GL and no GL resets context hints */
                    _hints.contextVersionMajor = 1;
                    _hints.contextVersionMinor = 0;
                    _hints.contextRevision = 0;
                    _hints.openGLForwardCompat = GLFW_FALSE;
                    _hints.openGLDebugContext = GLFW_FALSE;
                    _hints.openGLProfile = GLFW_OPENGL_ANY_PROFILE;
                    _hints.contextRobustness = GLFW_NO_ROBUSTNESS;
                    _hints.contextReleaseBehavior = GLFW_ANY_RELEASE_BEHAVIOR;
                    _hints.contextNoError = GLFW_FALSE;
                }
            }
            _hints.clientAPI = value;
            return;

        case GLFW_CONTEXT_VERSION_MAJOR:
            if (value < 0)
            {
                _glfwInputError(GLFW_INVALID_VALUE,
                                "Invalid GLFW_CONTEXT_VERSION_MAJOR value %d", value);
                return;
            }
            _hints.contextVersionMajor = value;
            return;

        case GLFW_CONTEXT_VERSION_MINOR:
            if (value < 0)
            {
                _glfwInputError(GLFW_INVALID_VALUE,
                                "Invalid GLFW_CONTEXT_VERSION_MINOR value %d", value);
                return;
            }
            _hints.contextVersionMinor = value;
            return;

        case GLFW_CONTEXT_REVISION:
            if (value < 0)
            {
                _glfwInputError(GLFW_INVALID_VALUE,
                                "Invalid GLFW_CONTEXT_REVISION value %d", value);
                return;
            }
            _hints.contextRevision = value;
            return;

        case GLFW_CONTEXT_ROBUSTNESS:
            if (value != GLFW_NO_ROBUSTNESS &&
                value != GLFW_NO_RESET_NOTIFICATION &&
                value != GLFW_LOSE_CONTEXT_ON_RESET)
            {
                _glfwInputError(GLFW_INVALID_ENUM,
                                "Invalid GLFW_CONTEXT_ROBUSTNESS value 0x%08X", value);
                return;
            }
            _hints.contextRobustness = value;
            return;

        case GLFW_CONTEXT_RELEASE_BEHAVIOR:
            if (value != GLFW_ANY_RELEASE_BEHAVIOR &&
                value != GLFW_RELEASE_BEHAVIOR_FLUSH &&
                value != GLFW_RELEASE_BEHAVIOR_NONE)
            {
                _glfwInputError(GLFW_INVALID_ENUM,
                                "Invalid GLFW_CONTEXT_RELEASE_BEHAVIOR value 0x%08X", value);
                return;
            }
            _hints.contextReleaseBehavior = value;
            return;

        case GLFW_CONTEXT_NO_ERROR:
            _hints.contextNoError = value ? GLFW_TRUE : GLFW_FALSE;
            return;

        case GLFW_CONTEXT_CREATION_API:
            if (value == GLFW_NATIVE_CONTEXT_API || value == GLFW_EGL_CONTEXT_API ||
                value == GLFW_OSMESA_CONTEXT_API)
            {
                _hints.contextCreationAPI = value;
                return;
            }
            _glfwInputError(GLFW_INVALID_ENUM,
                            "Invalid GLFW_CONTEXT_CREATION_API value 0x%08X", value);
            return;

        case GLFW_OPENGL_FORWARD_COMPAT:
            _hints.openGLForwardCompat = value ? GLFW_TRUE : GLFW_FALSE;
            return;

        case GLFW_OPENGL_DEBUG_CONTEXT:
            _hints.openGLDebugContext = value ? GLFW_TRUE : GLFW_FALSE;
            return;

        case GLFW_OPENGL_PROFILE:
            if (value != GLFW_OPENGL_ANY_PROFILE &&
                value != GLFW_OPENGL_CORE_PROFILE &&
                value != GLFW_OPENGL_COMPAT_PROFILE)
            {
                _glfwInputError(GLFW_INVALID_ENUM,
                                "Invalid GLFW_OPENGL_PROFILE value 0x%08X", value);
                return;
            }
            _hints.openGLProfile = value;
            return;

        case GLFW_SAMPLES:
            if (value < 0)
            {
                _glfwInputError(GLFW_INVALID_VALUE,
                                "Invalid GLFW_SAMPLES value %d", value);
                return;
            }
            _hints.samples = value;
            return;

        case GLFW_STEREO:                        _hints.stereo = value ? GLFW_TRUE : GLFW_FALSE; return;
        case GLFW_SRGB_CAPABLE:                  _hints.srgbCapable = value ? GLFW_TRUE : GLFW_FALSE; return;
        case GLFW_DOUBLEBUFFER:                  _hints.doublebuffer = value ? GLFW_TRUE : GLFW_FALSE; return;

        case GLFW_REFRESH_RATE:
            _hints.refreshRate = value;
            return;

        case GLFW_RED_BITS:                      _hints.redBits = value; return;
        case GLFW_GREEN_BITS:                    _hints.greenBits = value; return;
        case GLFW_BLUE_BITS:                     _hints.blueBits = value; return;
        case GLFW_ALPHA_BITS:                    _hints.alphaBits = value; return;
        case GLFW_DEPTH_BITS:                    _hints.depthBits = value; return;
        case GLFW_STENCIL_BITS:                  _hints.stencilBits = value; return;

        case GLFW_ACCUM_RED_BITS:                _hints.accumRedBits = value; return;
        case GLFW_ACCUM_GREEN_BITS:              _hints.accumGreenBits = value; return;
        case GLFW_ACCUM_BLUE_BITS:               _hints.accumBlueBits = value; return;
        case GLFW_ACCUM_ALPHA_BITS:              _hints.accumAlphaBits = value; return;
        case GLFW_AUX_BUFFERS:                   _hints.auxBuffers = value; return;

        case GLFW_WIN32_KEYBOARD_MENU:
            /* Windows-only hint; accepted and ignored (POSIX backend). */
            return;

        default:
            _glfwInputError(GLFW_INVALID_ENUM, "Unknown window hint 0x%08X", hint);
    }
}

GLFWAPI void glfwWindowHintString(int hint, const char *value)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    switch (hint)
    {
        case GLFW_COCOA_FRAME_NAME:
            setHintString(&_hintStringFrameName, value);
            return;
        case GLFW_X11_CLASS_NAME:
            setHintString(&_hintStringX11Class, value);
            return;
        case GLFW_X11_INSTANCE_NAME:
            setHintString(&_hintStringX11Instance, value);
            return;
        case GLFW_WAYLAND_APP_ID:
            setHintString(&_hintStringWaylandAppId, value);
            return;
        case GLFW_ANGLE_PLATFORM_TYPE:
        case GLFW_WIN32_KEYBOARD_MENU:
            return; /* accepted and ignored */
        default:
            _glfwInputError(GLFW_INVALID_ENUM,
                            "Unknown window string hint 0x%08X", hint);
    }
}

/* ------------------------------------------------------------------ */
/* Find helpers                                                        */
/* ------------------------------------------------------------------ */

_GLFWwindow *_glfwFindWindow(SDL_Window *sdlWindow)
{
    for (_GLFWwindow *w = _glfw.windowList; w; w = w->next)
    {
        if (w->sdlWindow == sdlWindow)
            return w;
    }
    return NULL;
}

_GLFWwindow *_glfwFindWindowByID(SDL_WindowID id)
{
    return _glfwFindWindow(SDL_GetWindowFromID(id));
}

/* ------------------------------------------------------------------ */
/* Context attribute setup (used by _glfwCreateContext in context.c)   */
/* ------------------------------------------------------------------ */

void _glfwApplyContextAttributes(_GLFWwindow *window)
{
    /* Clients provide "minimum" sizes; SDL satisfies with closest match. */
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   window->redBits);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, window->greenBits);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  window->blueBits);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, window->alphaBits);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, window->depthBits);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, window->stencilBits);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, window->doublebuffer ? 1 : 0);

    if (window->samples)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, window->samples);
    }
    else
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    }

    if (window->srgbCapable)
        SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
    if (window->stereo)
        SDL_GL_SetAttribute(SDL_GL_STEREO, 1);

    if (window->accumRedBits || window->accumGreenBits ||
        window->accumBlueBits || window->accumAlphaBits)
    {
        SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE,   window->accumRedBits);
        SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE, window->accumGreenBits);
        SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE,  window->accumBlueBits);
        SDL_GL_SetAttribute(SDL_GL_ACCUM_ALPHA_SIZE, window->accumAlphaBits);
    }

    /* Context version / profile / flags */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, window->contextVersionMajor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, window->contextVersionMinor);

    int flags = 0;
    if (window->openGLForwardCompat)
        flags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
    if (window->openGLDebugContext)
        flags |= SDL_GL_CONTEXT_DEBUG_FLAG;

    if (window->clientAPI == GLFW_OPENGL_ES_API)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    }
    else if (window->openGLProfile == GLFW_OPENGL_CORE_PROFILE)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        flags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
    }
    else if (window->openGLProfile == GLFW_OPENGL_COMPAT_PROFILE)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    }

    switch (window->contextRobustness)
    {
        case GLFW_NO_RESET_NOTIFICATION:
            flags |= SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_RESET_NOTIFICATION,
                                SDL_GL_CONTEXT_RESET_NO_NOTIFICATION);
            break;
        case GLFW_LOSE_CONTEXT_ON_RESET:
            flags |= SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_RESET_NOTIFICATION,
                                SDL_GL_CONTEXT_RESET_LOSE_CONTEXT);
            break;
        default:
            break;
    }

    if (window->contextReleaseBehavior == GLFW_RELEASE_BEHAVIOR_FLUSH)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_RELEASE_BEHAVIOR,
                            SDL_GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH);
    }
    else if (window->contextReleaseBehavior == GLFW_RELEASE_BEHAVIOR_NONE)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_RELEASE_BEHAVIOR,
                            SDL_GL_CONTEXT_RELEASE_BEHAVIOR_NONE);
    }

    if (window->contextNoError)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_NO_ERROR, 1);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, flags);
}

/* ------------------------------------------------------------------ */
/* Window creation / destruction                                       */
/* ------------------------------------------------------------------ */

GLFWAPI GLFWwindow *glfwCreateWindow(int width, int height, const char *title,
                                     GLFWmonitor *monitor, GLFWwindow *share)
{
    _GLFWmonitor *sdlMonitor = (_GLFWmonitor *)monitor;
    _GLFWwindow *shareWindow = (_GLFWwindow *)share;
    Uint32 flags = 0;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }

    if (width <= 0 || height <= 0)
    {
        _glfwInputError(GLFW_INVALID_VALUE,
                        "Invalid window size %ix%i", width, height);
        return NULL;
    }

    if (sdlMonitor && !_glfwFindMonitor(sdlMonitor->displayID))
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid monitor handle");
        return NULL;
    }

    if (shareWindow && shareWindow->clientAPI == GLFW_NO_API)
    {
        _glfwInputError(GLFW_NO_WINDOW_CONTEXT,
                        "Context-sharing with a window with no client API is invalid");
        return NULL;
    }

    if (shareWindow && _hints.clientAPI == GLFW_NO_API)
    {
        _glfwInputError(GLFW_INVALID_VALUE,
                        "Cannot share a context with a window with no client API");
        return NULL;
    }

    _GLFWwindow *window = calloc(1, sizeof(_GLFWwindow));
    if (!window)
    {
        _glfwInputError(GLFW_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    *window = _hints; /* copy the hint template */

    /* Sanitize client-API-specific values */
    if (window->clientAPI == GLFW_OPENGL_ES_API)
    {
        if (window->contextVersionMajor > 3 ||
            (window->contextVersionMajor == 3 && window->contextVersionMinor > 2) ||
            window->contextVersionMajor == 4)
        {
            _glfwInputError(GLFW_INVALID_VALUE,
                            "OpenGL ES context version mismatch");
            free(window);
            return NULL;
        }
        if (window->openGLForwardCompat || window->openGLProfile == GLFW_OPENGL_CORE_PROFILE)
        {
            _glfwInputError(GLFW_INVALID_VALUE,
                            "Forward-compatibility and core profile are not supported for OpenGL ES");
            free(window);
            return NULL;
        }
    }

    /* ----- build SDL window flags ----- */
    if (window->clientAPI != GLFW_NO_API)
        flags |= SDL_WINDOW_OPENGL;
    else
        flags |= SDL_WINDOW_VULKAN;   /* allows glfwCreateWindowSurface later */

    if (!window->visible)
        flags |= SDL_WINDOW_HIDDEN;
    else if (!window->focused)
        flags |= SDL_WINDOW_NOT_FOCUSABLE;
    if (!window->decorated)
        flags |= SDL_WINDOW_BORDERLESS;
    if (window->resizable)
        flags |= SDL_WINDOW_RESIZABLE;
    if (window->floating)
        flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    if (window->maximized)
        flags |= SDL_WINDOW_MAXIMIZED;
    if (window->scaleToMonitor)
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (window->transparentFramebuffer)
        flags |= SDL_WINDOW_TRANSPARENT;
    /* GLFW_MOUSE_PASSTHROUGH has no SDL3 equivalent; it is stored in
     * window->mousePassthrough (see glfwGetWindowAttrib) but cannot be
     * applied to the native window through SDL. */

    /* ----- configure the EGL-vs-native context backend ----- */
    if (window->contextCreationAPI == GLFW_EGL_CONTEXT_API)
        SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
    else if (window->contextCreationAPI == GLFW_OSMESA_CONTEXT_API)
    {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "OSMesa context creation is not supported by the SDL backend");
        free(window);
        return NULL;
    }

    /* ----- apply GL attributes ----- */
    if (window->clientAPI != GLFW_NO_API)
        _glfwApplyContextAttributes(window);

    /* ----- create the SDL window ----- */
    if (sdlMonitor)
        window->sdlWindow = SDL_CreateWindow(title, 1, 1, SDL_WINDOW_HIDDEN);
    else
        window->sdlWindow = SDL_CreateWindow(title, width, height, flags);

    if (!window->sdlWindow && window->clientAPI == GLFW_NO_API)
    {
        /* The Vulkan-capable flag makes SDL require a working Vulkan driver;
         * GLFW_NO_API windows are also used purely for input.  Retry without
         * the flag so such windows work on every driver (Vulkan surfaces are
         * only requested later via glfwCreateWindowSurface, which needs a
         * Vulkan-capable driver anyway). */
        window->sdlWindow = SDL_CreateWindow(title, width, height,
                                             flags & ~SDL_WINDOW_VULKAN);
    }

    if (!window->sdlWindow)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_CreateWindow failed: %s", SDL_GetError());
        free(window);
        return NULL;
    }

    if (sdlMonitor)
    {
        /* Set the requested size and go fullscreen on the target monitor. */
        glfwSetWindowMonitor((GLFWwindow *)window, monitor, 0, 0,
                             width, height, window->refreshRate);
        SDL_ShowWindow(window->sdlWindow);
    }

    /* ----- create the GL context ----- */
    if (window->clientAPI != GLFW_NO_API)
    {
        if (!_glfwCreateContext(window, shareWindow))
        {
            SDL_DestroyWindow(window->sdlWindow);
            free(window);
            return NULL;
        }
    }

    /* Restore attribute defaults so later windows start clean. */
    SDL_GL_ResetAttributes();

    /* ----- finish setup ----- */
    window->cursorMode = GLFW_CURSOR_NORMAL;
    window->virtualX = window->virtualY = 0.0;

    SDL_strlcpy(window->title, title ? title : "", sizeof(window->title));

    if (window->centerCursor && monitor == NULL)
    {
        int cx = 0, cy = 0;
        SDL_GetWindowSize(window->sdlWindow, &cx, &cy);
        SDL_WarpMouseInWindow(window->sdlWindow, cx / 2.0f, cy / 2.0f);
        window->virtualX = cx / 2.0;
        window->virtualY = cy / 2.0;
    }

    /* Save initial geometry for fullscreen restore. */
    SDL_GetWindowPosition(window->sdlWindow, &window->prevX, &window->prevY);
    SDL_GetWindowSize(window->sdlWindow, &window->prevW, &window->prevH);

    SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &window->samples);

    /* Register */
    window->next = _glfw.windowList;
    _glfw.windowList = window;

    return (GLFWwindow *)window;
}

void _glfwDestroyWindow(_GLFWwindow *window)
{
    if (!window)
        return;

    if (window->context)
    {
        /* Release if current */
        if (SDL_GL_GetCurrentContext() == window->context)
            SDL_GL_MakeCurrent(window->sdlWindow, NULL);
        SDL_GL_DestroyContext(window->context);
        window->context = NULL;
    }

    if (window->dropPaths)
    {
        for (int i = 0; i < window->dropPathCount; i++)
            free(window->dropPaths[i]);
        free(window->dropPaths);
        window->dropPaths = NULL;
    }

    if (window->sdlWindow)
        SDL_DestroyWindow(window->sdlWindow);
    window->sdlWindow = NULL;

    /* Unlink */
    _GLFWwindow **link = &_glfw.windowList;
    while (*link && *link != window)
        link = &(*link)->next;
    if (*link)
        *link = window->next;

    free(window);
}

GLFWAPI void glfwDestroyWindow(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    /* GLFW checks that the handle is one of ours. */
    bool found = false;
    for (_GLFWwindow *w = _glfw.windowList; w; w = w->next)
    {
        if (w == window) { found = true; break; }
    }
    if (!found)
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid window handle");
        return;
    }

    _glfwDestroyWindow(window);
}

GLFWAPI int glfwWindowShouldClose(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_FALSE;
    }
    return window->shouldClose ? GLFW_TRUE : GLFW_FALSE;
}

GLFWAPI void glfwSetWindowShouldClose(GLFWwindow *handle, int value)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    window->shouldClose = value ? GLFW_TRUE : GLFW_FALSE;
}

/* ------------------------------------------------------------------ */
/* Window operations                                                   */
/* ------------------------------------------------------------------ */

GLFWAPI void glfwSetWindowTitle(GLFWwindow *handle, const char *title)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_SetWindowTitle(window->sdlWindow, title);
    SDL_strlcpy(window->title, title ? title : "", sizeof(window->title));
}

GLFWAPI const char *glfwGetWindowTitle(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    return window->title;
}

static SDL_Surface *imageToSurface(const GLFWimage *image)
{
    if (!image || !image->pixels || image->width <= 0 || image->height <= 0)
        return NULL;

    /* GLFWimage is top-left origin RGBA8; SDL wants a surface we can
     * immediately hand to SDL_CreateColorCursor / SDL_SetWindowIcon. */
    return SDL_CreateSurfaceFrom(image->width, image->height,
                                 SDL_PIXELFORMAT_RGBA8888, image->pixels,
                                 image->width * 4);
}

GLFWAPI void glfwSetWindowIcon(GLFWwindow *handle, int count, const GLFWimage *images)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (!images || count <= 0)
    {
        /* Reset to default icon. */
        SDL_SetWindowIcon(window->sdlWindow, NULL);
        return;
    }

    /* Prefer the largest image, like GLFW/WM conventions do (Windows WM
     * actually picks by size; SDL takes a single surface). */
    const GLFWimage *best = &images[0];
    for (int i = 1; i < count; i++)
    {
        if (images[i].width > best->width && images[i].height > best->height)
            best = &images[i];
    }

    SDL_Surface *surface = imageToSurface(best);
    if (!surface)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR, "Failed to create icon surface");
        return;
    }
    SDL_SetWindowIcon(window->sdlWindow, surface);
    SDL_DestroySurface(surface);
}

GLFWAPI void glfwGetWindowPos(GLFWwindow *handle, int *xpos, int *ypos)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (xpos) *xpos = 0;
    if (ypos) *ypos = 0;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_GetWindowPosition(window->sdlWindow, xpos, ypos);
}

GLFWAPI void glfwSetWindowPos(GLFWwindow *handle, int xpos, int ypos)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_SetWindowPosition(window->sdlWindow, xpos, ypos);
}

GLFWAPI void glfwGetWindowSize(GLFWwindow *handle, int *width, int *height)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (width) *width = 0;
    if (height) *height = 0;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_GetWindowSize(window->sdlWindow, width, height);
}

GLFWAPI void glfwSetWindowSize(GLFWwindow *handle, int width, int height)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (window->fullscreen)
    {
        /* Change the fullscreen video mode rather than the window size. */
        glfwSetWindowMonitor(handle, glfwGetWindowMonitor(handle),
                             0, 0, width, height, GLFW_DONT_CARE);
        return;
    }
    SDL_SetWindowSize(window->sdlWindow, width, height);
}

GLFWAPI void glfwSetWindowSizeLimits(GLFWwindow *handle,
                                     int minwidth, int minheight,
                                     int maxwidth, int maxheight)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_SetWindowMinimumSize(window->sdlWindow,
                             minwidth  == GLFW_DONT_CARE ? 0 : minwidth,
                             minheight == GLFW_DONT_CARE ? 0 : minheight);
    SDL_SetWindowMaximumSize(window->sdlWindow,
                             maxwidth  == GLFW_DONT_CARE ? 0 : maxwidth,
                             maxheight == GLFW_DONT_CARE ? 0 : maxheight);
}

GLFWAPI void glfwSetWindowAspectRatio(GLFWwindow *handle, int numer, int denom)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (numer == GLFW_DONT_CARE || denom == GLFW_DONT_CARE)
    {
        SDL_SetWindowAspectRatio(window->sdlWindow, 0, 0);
        return;
    }
    SDL_SetWindowAspectRatio(window->sdlWindow, numer, denom);
}

GLFWAPI void glfwGetFramebufferSize(GLFWwindow *handle, int *width, int *height)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (width) *width = 0;
    if (height) *height = 0;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_GetWindowSizeInPixels(window->sdlWindow, width, height);
}

GLFWAPI void glfwGetWindowFrameSize(GLFWwindow *handle,
                                    int *left, int *top, int *right, int *bottom)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    int l = 0, t = 0, r = 0, b = 0;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (SDL_GetWindowBordersSize(window->sdlWindow, &t, &l, &b, &r))
    {
        if (left)   *left   = l;
        if (top)    *top    = t;
        if (right)  *right  = r;
        if (bottom) *bottom = b;
    }
}

GLFWAPI void glfwGetWindowContentScale(GLFWwindow *handle,
                                       float *xscale, float *yscale)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (xscale) *xscale = 1.0f;
    if (yscale) *yscale = 1.0f;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    const float scale = SDL_GetWindowDisplayScale(window->sdlWindow);
    if (xscale) *xscale = scale;
    if (yscale) *yscale = scale;
}

GLFWAPI GLFWmonitor *glfwGetWindowMonitor(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    if (!window->fullscreen)
        return NULL;

    const SDL_DisplayID id = SDL_GetDisplayForWindow(window->sdlWindow);
    return (GLFWmonitor *)_glfwFindMonitor(id);
}

GLFWAPI void glfwSetWindowMonitor(GLFWwindow *handle, GLFWmonitor *monitor,
                                  int xpos, int ypos,
                                  int width, int height, int refreshRate)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    _GLFWmonitor *sdlMonitor = (_GLFWmonitor *)monitor;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    if (sdlMonitor && !_glfwFindMonitor(sdlMonitor->displayID))
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid monitor handle");
        return;
    }

    if (width == 0 || height == 0)
    {
        /* GLFW treats a zero size as invalid. */
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid window size");
        return;
    }

    if (width == GLFW_DONT_CARE)
        width = window->prevW;
    if (height == GLFW_DONT_CARE)
        height = window->prevH;

    if (!sdlMonitor)
    {
        /* Windowed mode */
        if (window->fullscreen)
        {
            SDL_SetWindowFullscreenMode(window->sdlWindow, NULL);
            SDL_SetWindowFullscreen(window->sdlWindow, false);
            window->fullscreen = false;
            /* Restore the geometry the window had before fullscreen. */
            if (xpos != GLFW_DONT_CARE && ypos != GLFW_DONT_CARE)
                SDL_SetWindowPosition(window->sdlWindow, xpos, ypos);
            else
                SDL_SetWindowPosition(window->sdlWindow, window->prevX, window->prevY);
            SDL_SetWindowSize(window->sdlWindow, width, height);
        }
        else
        {
            if (xpos != GLFW_DONT_CARE && ypos != GLFW_DONT_CARE)
                SDL_SetWindowPosition(window->sdlWindow, xpos, ypos);
            SDL_SetWindowSize(window->sdlWindow, width, height);
        }
        return;
    }

    /* Fullscreen mode */
    if (!window->fullscreen)
    {
        SDL_GetWindowPosition(window->sdlWindow, &window->prevX, &window->prevY);
        SDL_GetWindowSize(window->sdlWindow, &window->prevW, &window->prevH);
        window->fullscreen = true;
    }

    SDL_DisplayMode mode;
    const float rate = (refreshRate > 0) ? (float)refreshRate : 0.0f;
    bool haveMode = false;

    if (width != GLFW_DONT_CARE || height != GLFW_DONT_CARE)
    {
        if (SDL_GetClosestFullscreenDisplayMode(sdlMonitor->displayID,
                                                width, height, rate,
                                                false, &mode))
            haveMode = true;
    }

#if defined(__APPLE__)
    /* macOS: always use the native fullscreen Space (the "green button"
     * transition) instead of an exclusive display-mode switch.  SDL's cocoa
     * driver animates into a Space only for fullscreen-desktop, i.e. when
     * no exclusive mode is set; entering exclusive and exiting via a Space
     * (or vice versa) is what made leaving fullscreen first pop to the
     * fullscreen Space and then immediately fall back to the old workspace.
     * By never setting an exclusive mode on macOS the window enters and
     * leaves the same Space. */
    haveMode = false;
#endif

    SDL_SetWindowFullscreenMode(window->sdlWindow, haveMode ? &mode : NULL);
    SDL_SetWindowFullscreen(window->sdlWindow, true);
}

GLFWAPI float glfwGetWindowOpacity(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return 1.0f;
    }
    return SDL_GetWindowOpacity(window->sdlWindow);
}

GLFWAPI void glfwSetWindowOpacity(GLFWwindow *handle, float opacity)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (opacity != opacity || opacity < 0.0f || opacity > 1.0f)
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid window opacity %f", opacity);
        return;
    }
    if (!SDL_SetWindowOpacity(window->sdlWindow, opacity))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_SetWindowOpacity failed: %s", SDL_GetError());
    }
}

GLFWAPI void glfwIconifyWindow(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_MinimizeWindow(window->sdlWindow);
}

GLFWAPI void glfwRestoreWindow(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_RestoreWindow(window->sdlWindow);
}

GLFWAPI void glfwMaximizeWindow(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_MaximizeWindow(window->sdlWindow);
}

GLFWAPI void glfwShowWindow(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_ShowWindow(window->sdlWindow);
}

GLFWAPI void glfwHideWindow(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_HideWindow(window->sdlWindow);
}

GLFWAPI void glfwFocusWindow(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_RaiseWindow(window->sdlWindow);
}

GLFWAPI void glfwRequestWindowAttention(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_FlashWindow(window->sdlWindow, SDL_FLASH_UNTIL_FOCUSED);
}

/* ------------------------------------------------------------------ */
/* Window attributes                                                   */
/* ------------------------------------------------------------------ */

GLFWAPI int glfwGetWindowAttrib(GLFWwindow *handle, int attrib)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return 0;
    }

    const Uint64 sdlFlags = SDL_GetWindowFlags(window->sdlWindow);

    switch (attrib)
    {
        case GLFW_FOCUSED:
            return (sdlFlags & SDL_WINDOW_INPUT_FOCUS) ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_ICONIFIED:
            return (sdlFlags & SDL_WINDOW_MINIMIZED) ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_VISIBLE:
            return (sdlFlags & SDL_WINDOW_HIDDEN) ? GLFW_FALSE : GLFW_TRUE;
        case GLFW_HOVERED:
            return (sdlFlags & SDL_WINDOW_MOUSE_FOCUS) ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_MAXIMIZED:
            return (sdlFlags & SDL_WINDOW_MAXIMIZED) ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_RESIZABLE:
            return (sdlFlags & SDL_WINDOW_RESIZABLE) ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_DECORATED:
            return (sdlFlags & SDL_WINDOW_BORDERLESS) ? GLFW_FALSE : GLFW_TRUE;
        case GLFW_FLOATING:
            return (sdlFlags & SDL_WINDOW_ALWAYS_ON_TOP) ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_MOUSE_PASSTHROUGH:
            /* SDL3 has no mouse-passthrough window flag; the value mirrors
             * what was requested through the hint/attribute. */
            return window->mousePassthrough ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_TRANSPARENT_FRAMEBUFFER:
            return window->transparentFramebuffer ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_AUTO_ICONIFY:
            return window->autoIconify ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_FOCUS_ON_SHOW:
            return window->focusOnShow ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_SCALE_TO_MONITOR:
            return window->scaleToMonitor ? GLFW_TRUE : GLFW_FALSE;

        case GLFW_CLIENT_API:
            return window->clientAPI;
        case GLFW_CONTEXT_VERSION_MAJOR:
            return window->contextVersionMajor;
        case GLFW_CONTEXT_VERSION_MINOR:
            return window->contextVersionMinor;
        case GLFW_CONTEXT_REVISION:
            return window->contextRevision;
        case GLFW_CONTEXT_CREATION_API:
            return window->contextCreationAPI;
        case GLFW_CONTEXT_ROBUSTNESS:
            return window->contextRobustness;
        case GLFW_CONTEXT_RELEASE_BEHAVIOR:
            return window->contextReleaseBehavior;
        case GLFW_CONTEXT_NO_ERROR:
            return window->contextNoError ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_OPENGL_FORWARD_COMPAT:
            return window->openGLForwardCompat ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_OPENGL_DEBUG_CONTEXT:
            return window->openGLDebugContext ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_OPENGL_PROFILE:
            return window->openGLProfile;

        case GLFW_STEREO:
            return window->stereo ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_SRGB_CAPABLE:
            return window->srgbCapable ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_DOUBLEBUFFER:
            return window->doublebuffer ? GLFW_TRUE : GLFW_FALSE;
        case GLFW_SAMPLES:
            return window->samples;

        default:
            _glfwInputError(GLFW_INVALID_ENUM, "Invalid window attribute 0x%08X", attrib);
    }
    return 0;
}

GLFWAPI void glfwSetWindowAttrib(GLFWwindow *handle, int attrib, int value)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    switch (attrib)
    {
        case GLFW_RESIZABLE:
            SDL_SetWindowResizable(window->sdlWindow, value ? true : false);
            return;
        case GLFW_DECORATED:
            SDL_SetWindowBordered(window->sdlWindow, value ? true : false);
            if (_hints.decorated != value) _hints.decorated = value;
            return;
        case GLFW_FLOATING:
            SDL_SetWindowAlwaysOnTop(window->sdlWindow, value ? true : false);
            return;
        case GLFW_AUTO_ICONIFY:
            window->autoIconify = value ? GLFW_TRUE : GLFW_FALSE;
            return;
        case GLFW_FOCUS_ON_SHOW:
            window->focusOnShow = value ? GLFW_TRUE : GLFW_FALSE;
            return;
        case GLFW_MOUSE_PASSTHROUGH:
            /* SDL3 cannot make a window click-through; mirror GLFW's
             * bookkeeping so the attribute reads back what was set. */
            window->mousePassthrough = value ? GLFW_TRUE : GLFW_FALSE;
            return;
        default:
            _glfwInputError(GLFW_INVALID_ENUM,
                            "Invalid window attribute 0x%08X (settable attributes are "
                            "GLFW_RESIZABLE, GLFW_DECORATED, GLFW_FLOATING, "
                            "GLFW_AUTO_ICONIFY, GLFW_FOCUS_ON_SHOW and "
                            "GLFW_MOUSE_PASSTHROUGH)", attrib);
    }
}

/* ------------------------------------------------------------------ */
/* User pointer + callback registration                                */
/* ------------------------------------------------------------------ */

GLFWAPI void glfwSetWindowUserPointer(GLFWwindow *handle, void *pointer)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    window->userPointer = pointer;
}

GLFWAPI void *glfwGetWindowUserPointer(GLFWwindow *handle)
{
    return ((_GLFWwindow *)handle)->userPointer;
}

#define CALLBACK_SETTER(name, field, type) \
    GLFWAPI type glfwSet##name(GLFWwindow *handle, type callback) \
    { \
        _GLFWwindow *window = (_GLFWwindow *)handle; \
        const type previous = window->field; \
        window->field = callback; \
        return previous; \
    }

CALLBACK_SETTER(WindowPosCallback,            posCb,          GLFWwindowposfun)
CALLBACK_SETTER(WindowSizeCallback,           sizeCb,         GLFWwindowsizefun)
CALLBACK_SETTER(WindowCloseCallback,          closeCb,        GLFWwindowclosefun)
CALLBACK_SETTER(WindowRefreshCallback,        refreshCb,      GLFWwindowrefreshfun)
CALLBACK_SETTER(WindowFocusCallback,          focusCb,        GLFWwindowfocusfun)
CALLBACK_SETTER(WindowIconifyCallback,        iconifyCb,      GLFWwindowiconifyfun)
CALLBACK_SETTER(WindowMaximizeCallback,       maximizeCb,     GLFWwindowmaximizefun)
CALLBACK_SETTER(FramebufferSizeCallback,      fbSizeCb,       GLFWframebuffersizefun)
CALLBACK_SETTER(WindowContentScaleCallback,   scaleCb,        GLFWwindowcontentscalefun)
CALLBACK_SETTER(MouseButtonCallback,          mouseButtonCb,  GLFWmousebuttonfun)
CALLBACK_SETTER(CursorPosCallback,            cursorPosCb,    GLFWcursorposfun)
CALLBACK_SETTER(CursorEnterCallback,          cursorEnterCb,  GLFWcursorenterfun)
CALLBACK_SETTER(ScrollCallback,               scrollCb,       GLFWscrollfun)
CALLBACK_SETTER(KeyCallback,                  keyCb,          GLFWkeyfun)
CALLBACK_SETTER(CharCallback,                 charCb,         GLFWcharfun)
CALLBACK_SETTER(CharModsCallback,             charModsCb,     GLFWcharmodsfun)
CALLBACK_SETTER(DropCallback,                 dropCb,         GLFWdropfun)