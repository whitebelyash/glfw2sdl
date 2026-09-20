/*
 * GLFW support layer on top of SDL3 -- monitors, video modes, gamma.
 *
 * SDL3 has no gamma-ramp API, so gamma ramps are stored and returned by
 * the shim; the values are honored by the compositor/native driver where
 * possible and are otherwise bookkeeping (games rarely depend on them).
 *
 * SPDX-License-Identifier: Zlib
 */

#include "internal.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

/* SDL3 (as of 3.5.x) has no public physical-size API, so we estimate
 * the size in millimetres from the pixel bounds and the content scale,
 * assuming a nominal 96 DPI baseline (25.4 mm per inch).  This matches
 * the numbers most desktop environments report for standard displays. */
static void estimatePhysicalSize(SDL_DisplayID id, int *widthMM, int *heightMM)
{
    SDL_Rect bounds = {0, 0, 0, 0};
    float scale = SDL_GetDisplayContentScale(id);
    if (scale <= 0.0f)
        scale = 1.0f;

    *widthMM = *heightMM = 0;
    if (SDL_GetDisplayBounds(id, &bounds))
    {
        const double mmPerPixel = 25.4 / (96.0 * (double)scale);
        *widthMM  = (int)((double)bounds.w * mmPerPixel + 0.5);
        *heightMM = (int)((double)bounds.h * mmPerPixel + 0.5);
    }
}

static int countBits(Uint32 value)
{
    int bits = 0;
    while (value)
    {
        bits += (int)(value & 1u);
        value >>= 1;
    }
    return bits;
}

static GLFWvidmode sdlModeToGLFW(const SDL_DisplayMode *mode)
{
    GLFWvidmode result;
    const SDL_PixelFormatDetails *details;

    memset(&result, 0, sizeof(result));
    result.width  = mode->w;
    result.height = mode->h;
    result.refreshRate = (int)(mode->refresh_rate + 0.5f);

    details = SDL_GetPixelFormatDetails(mode->format);
    if (details)
    {
        result.redBits   = countBits(details->Rmask);
        result.greenBits = countBits(details->Gmask);
        result.blueBits  = countBits(details->Bmask);
    }
    else
    {
        result.redBits = result.greenBits = result.blueBits = 8;
    }

    return result;
}

_GLFWmonitor *_glfwFindMonitor(SDL_DisplayID id)
{
    for (_GLFWmonitor *m = _glfw.monitorList; m; m = m->next)
    {
        if (m->displayID == id)
            return m;
    }
    return NULL;
}

static _GLFWmonitor *createMonitor(SDL_DisplayID id)
{
    _GLFWmonitor *m = calloc(1, sizeof(_GLFWmonitor));
    if (!m)
        return NULL;

    m->displayID = id;
    const char *name = SDL_GetDisplayName(id);
    if (name)
        SDL_strlcpy(m->name, name, sizeof(m->name));
    else
        SDL_strlcpy(m->name, "Display", sizeof(m->name));

    m->ramp.red   = m->rampRed;
    m->ramp.green = m->rampGreen;
    m->ramp.blue  = m->rampBlue;
    m->ramp.size  = 256;
    m->rampValid  = false;
    return m;
}

static void destroyMonitor(_GLFWmonitor *m)
{
    if (m->modes)
        free(m->modes);
    if (m->sdlModes)
        SDL_free(m->sdlModes);
    free(m);
}

/* Refresh the monitor list to match SDL's current display enumeration.
 * Preserves monitor handle identity across refreshes. */
void _glfwRefreshMonitors(void)
{
    int count = 0;
    SDL_DisplayID *ids = SDL_GetDisplays(&count);
    _GLFWmonitor *head = NULL, *tail = NULL;
    int i;

    if (ids)
    {
        for (i = 0; i < count; i++)
        {
            _GLFWmonitor *m = _glfwFindMonitor(ids[i]);
            if (!m)
            {
                m = createMonitor(ids[i]);
                if (!m)
                    continue;
                if (_glfw.initialized && _glfw.monitorCallback)
                    _glfw.monitorCallback((GLFWmonitor *)m, GLFW_CONNECTED);
            }

            /* Re-link into the new list (keeps SDL display order). */
            if (tail)
                tail->next = m;
            else
                head = m;
            tail = m;
            m->next = NULL;
        }
        SDL_free(ids);
    }

    /* Destroy monitors that disappeared (not present in the new list). */
    _GLFWmonitor *m = _glfw.monitorList;
    while (m)
    {
        _GLFWmonitor *next = m->next;

        _GLFWmonitor *cur = head;
        bool found = false;
        for (; cur; cur = cur->next)
        {
            if (cur == m) { found = true; break; }
        }

        if (!found)
        {
            if (_glfw.initialized && _glfw.monitorCallback)
                _glfw.monitorCallback((GLFWmonitor *)m, GLFW_DISCONNECTED);
            destroyMonitor(m);
        }
        m = next;
    }

    _glfw.monitorList = head;

    /* Rebuild the glfwGetMonitors array. */
    if (_glfw.monitorArray)
        free(_glfw.monitorArray);
    _glfw.monitorCount = 0;
    for (m = head; m; m = m->next)
        _glfw.monitorCount++;

    _glfw.monitorArray = calloc(_glfw.monitorCount ? _glfw.monitorCount : 1,
                                sizeof(GLFWmonitor *));
    if (_glfw.monitorArray)
    {
        int idx = 0;
        for (m = head; m; m = m->next)
            _glfw.monitorArray[idx++] = (GLFWmonitor *)m;
    }
}

void _glfwDestroyMonitors(void)
{
    _GLFWmonitor *m = _glfw.monitorList;
    while (m)
    {
        _GLFWmonitor *next = m->next;
        destroyMonitor(m);
        m = next;
    }
    _glfw.monitorList = NULL;
    if (_glfw.monitorArray)
        free(_glfw.monitorArray);
    _glfw.monitorArray = NULL;
    _glfw.monitorCount = 0;
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */

GLFWAPI GLFWmonitor **glfwGetMonitors(int *count)
{
    *count = 0;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    if (!_glfw.monitorArray)
        _glfwRefreshMonitors();
    *count = _glfw.monitorCount;
    return _glfw.monitorArray;
}

GLFWAPI GLFWmonitor *glfwGetPrimaryMonitor(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    return (GLFWmonitor *)_glfwFindMonitor(primary);
}

GLFWAPI void glfwGetMonitorPos(GLFWmonitor *handle, int *xpos, int *ypos)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    if (xpos) *xpos = 0;
    if (ypos) *ypos = 0;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_Rect bounds = {0, 0, 0, 0};
    if (!SDL_GetDisplayBounds(monitor->displayID, &bounds))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR, "SDL_GetDisplayBounds failed: %s",
                        SDL_GetError());
        return;
    }
    if (xpos) *xpos = bounds.x;
    if (ypos) *ypos = bounds.y;
}

GLFWAPI void glfwGetMonitorWorkarea(GLFWmonitor *handle, int *xpos, int *ypos,
                                    int *width, int *height)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    if (xpos) *xpos = 0;
    if (ypos) *ypos = 0;
    if (width) *width = 0;
    if (height) *height = 0;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_Rect r = {0, 0, 0, 0};
    if (!SDL_GetDisplayUsableBounds(monitor->displayID, &r))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR, "SDL_GetDisplayUsableBounds failed: %s",
                        SDL_GetError());
        return;
    }
    if (xpos) *xpos = r.x;
    if (ypos) *ypos = r.y;
    if (width) *width = r.w;
    if (height) *height = r.h;
}

GLFWAPI void glfwGetMonitorPhysicalSize(GLFWmonitor *handle, int *widthMM, int *heightMM)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    if (widthMM) *widthMM = 0;
    if (heightMM) *heightMM = 0;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    int w = 0, h = 0;
    estimatePhysicalSize(monitor->displayID, &w, &h);
    /* SDL3 provides no physical-size query (checked against 3.4 and 3.5);
     * the millimetre estimate above is the best available. GLFW expects
     * millimetres, which is what we hand back. */
    if (widthMM) *widthMM = w;
    if (heightMM) *heightMM = h;
}

GLFWAPI void glfwGetMonitorContentScale(GLFWmonitor *handle,
                                        float *xscale, float *yscale)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    if (xscale) *xscale = 1.0f;
    if (yscale) *yscale = 1.0f;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    const float scale = SDL_GetDisplayContentScale(monitor->displayID);
    if (xscale) *xscale = scale;
    if (yscale) *yscale = scale;
}

GLFWAPI const char *glfwGetMonitorName(GLFWmonitor *handle)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    return monitor->name;
}

GLFWAPI void glfwSetMonitorUserPointer(GLFWmonitor *handle, void *pointer)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    monitor->userPointer = pointer;
}

GLFWAPI void *glfwGetMonitorUserPointer(GLFWmonitor *handle)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    return monitor->userPointer;
}

GLFWAPI GLFWmonitorfun glfwSetMonitorCallback(GLFWmonitorfun callback)
{
    const GLFWmonitorfun previous = _glfw.monitorCallback;
    _glfw.monitorCallback = callback;
    return previous;
}

GLFWAPI const GLFWvidmode *glfwGetVideoModes(GLFWmonitor *handle, int *count)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    *count = 0;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }

    int sdlCount = 0;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(monitor->displayID, &sdlCount);
    if (!modes && sdlCount > 0)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR, "SDL_GetFullscreenDisplayModes failed: %s",
                        SDL_GetError());
        return NULL;
    }

    if (monitor->sdlModes)
        SDL_free(monitor->sdlModes);
    monitor->sdlModes = modes;
    monitor->sdlModeCount = sdlCount;

    if (monitor->modes)
        free(monitor->modes);

    monitor->modes = calloc(sdlCount ? sdlCount : 1, sizeof(GLFWvidmode));
    for (int i = 0; i < sdlCount; i++)
        monitor->modes[i] = sdlModeToGLFW(modes[i]);

    monitor->modeCount = sdlCount;
    *count = sdlCount;
    return monitor->modes;
}

GLFWAPI const GLFWvidmode *glfwGetVideoMode(GLFWmonitor *handle)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(monitor->displayID);
    if (!mode)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR, "SDL_GetCurrentDisplayMode failed: %s",
                        SDL_GetError());
        return NULL;
    }
    monitor->currentMode = sdlModeToGLFW(mode);
    return &monitor->currentMode;
}

/* ------------------------------------------------------------------ */
/* Gamma                                                               */
/* ------------------------------------------------------------------ */

static void defaultGammaRamp(_GLFWmonitor *monitor)
{
    for (int i = 0; i < 256; i++)
    {
        const unsigned short value = (unsigned short)((i * 65535) / 255);
        monitor->rampRed[i]   = value;
        monitor->rampGreen[i] = value;
        monitor->rampBlue[i]  = value;
    }
    monitor->rampValid = true;
}

GLFWAPI void glfwSetGamma(GLFWmonitor *handle, float gamma)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (gamma <= 0.0f || gamma != gamma) /* NaN check */
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid gamma value %f", gamma);
        return;
    }

    GLFWgammaramp ramp;
    unsigned short red[256], green[256], blue[256];
    ramp.red = red; ramp.green = green; ramp.blue = blue; ramp.size = 256;

    for (int i = 0; i < 256; i++)
    {
        const double value = pow((double)i / 255.0, (double)gamma) * 65535.0;
        red[i] = green[i] = blue[i] = (unsigned short)(value + 0.5);
    }

    glfwSetGammaRamp(handle, &ramp);
}

GLFWAPI const GLFWgammaramp *glfwGetGammaRamp(GLFWmonitor *handle)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    if (!monitor->rampValid)
        defaultGammaRamp(monitor);
    return &monitor->ramp;
}

GLFWAPI void glfwSetGammaRamp(GLFWmonitor *handle, const GLFWgammaramp *ramp)
{
    _GLFWmonitor *monitor = (_GLFWmonitor *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (!ramp || ramp->size != 256)
    {
        _glfwInputError(GLFW_INVALID_VALUE,
                        "Gamma ramp must have exactly 256 entries");
        return;
    }

    for (int i = 0; i < 256; i++)
    {
        monitor->rampRed[i]   = ramp->red[i];
        monitor->rampGreen[i] = ramp->green[i];
        monitor->rampBlue[i]  = ramp->blue[i];
    }
    monitor->rampValid = true;
}