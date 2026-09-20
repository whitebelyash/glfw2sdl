/*
 * GLFW support layer on top of SDL3 -- OpenGL contexts and function loading.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "internal.h"

/* Create the GL context for a window (window->clientAPI != GLFW_NO_API).
 * Returns true on success.  A newly created context, like GLFW, is not
 * automatically made current. */
bool _glfwCreateContext(_GLFWwindow *window, _GLFWwindow *share)
{
    if (window->clientAPI == GLFW_NO_API)
        return true;

    if (!window->sdlWindow)
        return false;

    if (share && share->context)
    {
        /* SDL shares the new context with the *current* one, so make the
         * share window's context current first. */
        SDL_GL_MakeCurrent(share->sdlWindow, share->context);
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    }

    window->context = SDL_GL_CreateContext(window->sdlWindow);
    if (!window->context)
    {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    return true;
}

GLFWAPI void glfwMakeContextCurrent(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }

    if (window && window->clientAPI == GLFW_NO_API)
    {
        _glfwInputError(GLFW_NO_WINDOW_CONTEXT,
                        "Window has no client API");
        return;
    }

    if (!window)
    {
        /* Release the current context. */
        SDL_Window *current = SDL_GL_GetCurrentWindow();
        if (current)
            SDL_GL_MakeCurrent(current, NULL);
        else
            SDL_GL_MakeCurrent(NULL, NULL);
        return;
    }

    if (!SDL_GL_MakeCurrent(window->sdlWindow, window->context))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_GL_MakeCurrent failed: %s", SDL_GetError());
    }
}

GLFWAPI GLFWwindow *glfwGetCurrentContext(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    SDL_Window *current = SDL_GL_GetCurrentWindow();
    if (!current)
        return NULL;
    return (GLFWwindow *)_glfwFindWindow(current);
}

GLFWAPI void glfwSwapBuffers(GLFWwindow *handle)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    SDL_GL_SwapWindow(window->sdlWindow);
}

GLFWAPI void glfwSwapInterval(int interval)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return;
    }
    if (!SDL_GL_SetSwapInterval(interval))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_GL_SetSwapInterval(%i) failed: %s",
                        interval, SDL_GetError());
    }
}

GLFWAPI int glfwExtensionSupported(const char *extension)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_FALSE;
    }
    if (!extension)
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Extension name is NULL");
        return GLFW_FALSE;
    }
    return SDL_GL_ExtensionSupported(extension) ? GLFW_TRUE : GLFW_FALSE;
}

GLFWAPI GLFWglproc glfwGetProcAddress(const char *procname)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }
    return (GLFWglproc)SDL_GL_GetProcAddress(procname);
}