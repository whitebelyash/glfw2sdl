/*
 * GLFW support layer on top of SDL3 -- diagnostics logging.
 *
 * Emits a one-line boot banner from glfwInit and redirects SDL_Log output
 * to stdout.  The SDL log sink in effect before glfwInit (the default or a
 * host-customised one) is saved and restored at glfwTerminate so host
 * applications get their sink back.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "internal.h"

/* ------------------------------------------------------------------ */
/* Output callback: forward SDL_Log messages to stdout.                */
/* ------------------------------------------------------------------ */

static const char *priorityTag(SDL_LogPriority priority)
{
    switch (priority)
    {
        case SDL_LOG_PRIORITY_TRACE:    return "trace";
        case SDL_LOG_PRIORITY_VERBOSE:  return "verbose";
        case SDL_LOG_PRIORITY_DEBUG:    return "debug";
        case SDL_LOG_PRIORITY_INFO:     return "info";
        case SDL_LOG_PRIORITY_WARN:     return "warn";
        case SDL_LOG_PRIORITY_ERROR:    return "error";
        case SDL_LOG_PRIORITY_CRITICAL: return "critical";
        default:                        return "?";
    }
}

static void SDLCALL _glfwSdlLogOutput(void *userdata, int category,
                                      SDL_LogPriority priority,
                                      const char *message)
{
    (void) userdata;
    (void) category;

    fprintf(stdout, "[SDL3:%s] %s\n", priorityTag(priority), message);
    fflush(stdout);
}

void _glfwInstallLogOutput(void)
{
    if (_glfw.logRedirected)
        return;

    /* Save whatever sink the host had so glfwTerminate can restore it. */
    SDL_GetLogOutputFunction(&_glfw.prevLogOutput, &_glfw.prevLogUserdata);
    SDL_SetLogOutputFunction(_glfwSdlLogOutput, NULL);
    _glfw.logRedirected = true;
}

void _glfwRestoreLogOutput(void)
{
    if (!_glfw.logRedirected)
        return;

    SDL_SetLogOutputFunction(_glfw.prevLogOutput, _glfw.prevLogUserdata);
    _glfw.logRedirected = false;
}

/* ------------------------------------------------------------------ */
/* Boot banner emitted by glfwInit.                                    */
/* ------------------------------------------------------------------ */

static const char *platformName(int platform)
{
    switch (platform)
    {
        case GLFW_PLATFORM_X11:     return "x11";
        case GLFW_PLATFORM_WAYLAND: return "wayland";
        case GLFW_PLATFORM_COCOA:   return "cocoa";
        case GLFW_PLATFORM_NULL:    return "null";
        default:                    return "unknown";
    }
}

void _glfwLogBootInfo(void)
{
    /* SDL 3.2+ packs the version as major*10^6 + minor*10^3 + patch. */
    const int v = SDL_GetVersion();
    const char *driver = SDL_GetCurrentVideoDriver();

    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO,
                   "%s initialized (SDL %d.%d.%d, video driver: %s, "
                   "platform: %s)",
                   glfwGetVersionString(),
                   v / 1000000, (v / 1000) % 1000, v % 1000,
                   driver ? driver : "unknown",
                   platformName(_glfw.resolvedPlatform));
}