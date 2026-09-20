/*
 * GLFW support layer on top of SDL3 -- native access functions.
 *
 * Every native handle getter is a stub.  The shim is completely SDL-backed:
 * windows, contexts, monitors and cursors live inside SDL, so there is no
 * X11/Wayland/EGL/Cocoa handle we can hand back, and SDL3 provides no
 * window-system-info escape hatch (SDL_GetWindowWMInfo was removed).
 *
 * All functions therefore validate initialization and then raise
 * GLFW_PLATFORM_UNAVAILABLE, returning NULL / 0 as appropriate.  The
 * platform typedefs below are ABI-compatible stand-ins for the real ones
 * (they are all pointers or 64-bit integers), so code that links against
 * the real glfw3native.h signatures will resolve and behave like a GLFW
 * build that lacks the requested backend.
 *
 * SPDX-License-Identifier: Zlib
 */

/*
 * Expose every native function group so the shared library exports the
 * full GLFW 3.5 native symbol set, whatever the application requests.
 * GLFW_NATIVE_INCLUDE_NONE keeps the real system headers (X11, EGL, ...)
 * out of this translation unit; the typedefs below stand in for them.
 */
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#define GLFW_EXPOSE_NATIVE_WAYLAND
#define GLFW_EXPOSE_NATIVE_EGL
#define GLFW_EXPOSE_NATIVE_OSMESA
#define GLFW_NATIVE_INCLUDE_NONE

#include "internal.h"

/* ------------------------------------------------------------------ */
/* ABI-compatible stand-in types (see file comment).  These must be    */
/* declared before glfw3native.h so its prototypes compile (it does    */
/* not include any system headers thanks to GLFW_NATIVE_INCLUDE_NONE). */
/* ------------------------------------------------------------------ */

#include <stdint.h>

/* Win32 */
typedef void *HWND;
typedef void *HGLRC;

/* Cocoa / NSGL.  `id` is a pointer to an Objective-C object; we never
 * dereference it, and CGDirectDisplayID is a 32-bit integer. */
typedef void *id;
typedef uint32_t CGDirectDisplayID;

/* X11 (Xlib/Xrandr) */
typedef struct _XDisplay Display;
typedef unsigned long Window;
typedef unsigned long RRCrtc;
typedef unsigned long RROutput;

/* GLX */
typedef void *GLXContext;
typedef unsigned long GLXWindow;
typedef void *GLXFBConfig;

/* Wayland (opaque structs only) */
struct wl_display;
struct wl_output;
struct wl_surface;

/* EGL */
typedef void *EGLDisplay;
typedef void *EGLContext;
typedef void *EGLSurface;
typedef void *EGLConfig;

/* OSMesa */
typedef void *OSMesaContext;

#include <GLFW/glfw3native.h>

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static void nativeUnavailable(void)
{
    _glfwInputError(GLFW_PLATFORM_UNAVAILABLE,
                    "Native window-system access is not available through "
                    "the SDL-backed GLFW shim");
}

#define NATIVE_ENTER()               \
    do {                             \
        if (!_glfw.initialized)      \
        {                            \
            _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized"); \
            return NULL;             \
        }                            \
        nativeUnavailable();         \
        return NULL;                 \
    } while (0)

#define NATIVE_ENTER_ZERO()          \
    do {                             \
        if (!_glfw.initialized)      \
        {                            \
            _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized"); \
            return 0;                \
        }                            \
        nativeUnavailable();         \
        return 0;                    \
    } while (0)

#define NATIVE_ENTER_FALSE()         \
    do {                             \
        if (!_glfw.initialized)      \
        {                            \
            _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized"); \
            return GLFW_FALSE;       \
        }                            \
        nativeUnavailable();         \
        return GLFW_FALSE;           \
    } while (0)

#define NATIVE_ENTER_VOID()          \
    do {                             \
        if (!_glfw.initialized)      \
        {                            \
            _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized"); \
            return;                  \
        }                            \
        nativeUnavailable();         \
    } while (0)

/* ------------------------------------------------------------------ */
/* Win32                                                               */
/* ------------------------------------------------------------------ */

GLFWAPI const char *glfwGetWin32Adapter(GLFWmonitor *monitor)
{
    NATIVE_ENTER();
}

GLFWAPI const char *glfwGetWin32Monitor(GLFWmonitor *monitor)
{
    NATIVE_ENTER();
}

GLFWAPI HWND glfwGetWin32Window(GLFWwindow *window)
{
    NATIVE_ENTER();
}

/* ------------------------------------------------------------------ */
/* WGL                                                                 */
/* ------------------------------------------------------------------ */

GLFWAPI HGLRC glfwGetWGLContext(GLFWwindow *window)
{
    NATIVE_ENTER();
}

/* ------------------------------------------------------------------ */
/* Cocoa                                                               */
/* ------------------------------------------------------------------ */

GLFWAPI CGDirectDisplayID glfwGetCocoaMonitor(GLFWmonitor *monitor)
{
    NATIVE_ENTER_ZERO();
}

GLFWAPI id glfwGetCocoaWindow(GLFWwindow *window)
{
    NATIVE_ENTER();
}

GLFWAPI id glfwGetCocoaView(GLFWwindow *window)
{
    NATIVE_ENTER();
}

/* ------------------------------------------------------------------ */
/* NSGL                                                                */
/* ------------------------------------------------------------------ */

GLFWAPI id glfwGetNSGLContext(GLFWwindow *window)
{
    NATIVE_ENTER();
}

/* ------------------------------------------------------------------ */
/* X11                                                                 */
/* ------------------------------------------------------------------ */

GLFWAPI Display *glfwGetX11Display(void)
{
    NATIVE_ENTER();
}

GLFWAPI RRCrtc glfwGetX11Adapter(GLFWmonitor *monitor)
{
    NATIVE_ENTER_ZERO();
}

GLFWAPI RROutput glfwGetX11Monitor(GLFWmonitor *monitor)
{
    NATIVE_ENTER_ZERO();
}

GLFWAPI Window glfwGetX11Window(GLFWwindow *window)
{
    NATIVE_ENTER_ZERO();
}

/* Primary-selection access has no parallel in SDL; raise the platform
 * error like the other stubs.  The string is ignored. */
GLFWAPI void glfwSetX11SelectionString(const char *string)
{
    NATIVE_ENTER_VOID();
}

GLFWAPI const char *glfwGetX11SelectionString(void)
{
    NATIVE_ENTER();
}

/* ------------------------------------------------------------------ */
/* GLX                                                                 */
/* ------------------------------------------------------------------ */

GLFWAPI GLXContext glfwGetGLXContext(GLFWwindow *window)
{
    NATIVE_ENTER();
}

GLFWAPI GLXWindow glfwGetGLXWindow(GLFWwindow *window)
{
    NATIVE_ENTER_ZERO();
}

GLFWAPI int glfwGetGLXFBConfig(GLFWwindow *window, GLXFBConfig *config)
{
    NATIVE_ENTER_FALSE();
}

/* ------------------------------------------------------------------ */
/* Wayland                                                             */
/* ------------------------------------------------------------------ */

GLFWAPI struct wl_display *glfwGetWaylandDisplay(void)
{
    NATIVE_ENTER();
}

GLFWAPI struct wl_output *glfwGetWaylandMonitor(GLFWmonitor *monitor)
{
    NATIVE_ENTER();
}

GLFWAPI struct wl_surface *glfwGetWaylandWindow(GLFWwindow *window)
{
    NATIVE_ENTER();
}

/* ------------------------------------------------------------------ */
/* EGL                                                                 */
/* ------------------------------------------------------------------ */

GLFWAPI EGLDisplay glfwGetEGLDisplay(void)
{
    NATIVE_ENTER();
}

GLFWAPI EGLContext glfwGetEGLContext(GLFWwindow *window)
{
    NATIVE_ENTER();
}

GLFWAPI EGLSurface glfwGetEGLSurface(GLFWwindow *window)
{
    NATIVE_ENTER();
}

GLFWAPI int glfwGetEGLConfig(GLFWwindow *window, EGLConfig *config)
{
    NATIVE_ENTER_FALSE();
}

/* ------------------------------------------------------------------ */
/* OSMesa                                                              */
/* ------------------------------------------------------------------ */

GLFWAPI int glfwGetOSMesaColorBuffer(GLFWwindow *window, int *width,
                                     int *height, int *format, void **buffer)
{
    NATIVE_ENTER_FALSE();
}

GLFWAPI int glfwGetOSMesaDepthBuffer(GLFWwindow *window, int *width,
                                     int *height, int *bytesPerValue,
                                     void **buffer)
{
    NATIVE_ENTER_FALSE();
}

GLFWAPI OSMesaContext glfwGetOSMesaContext(GLFWwindow *window)
{
    NATIVE_ENTER();
}