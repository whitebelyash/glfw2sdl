/*
 * GLFW support layer on top of SDL3 -- error handling.
 *
 * Mirrors the GLFW error model: an error code + description are stored
 * per-thread and reported to the error callback (if any).
 *
 * SPDX-License-Identifier: Zlib
 */

#include "internal.h"

#include <stdarg.h>

/* Per-thread error state, like GLFW does. */
static _Thread_local int      tlsErrorCode  = GLFW_NO_ERROR;
static _Thread_local char     tlsErrorMsg[1024];

/* GLFW's error callback is global (shared by all threads in real GLFW too). */
static GLFWerrorfun gErrorCallback = NULL;

void _glfwInputError(int error, const char *format, ...)
{
    tlsErrorCode = error;
    if (format)
    {
        va_list args;
        va_start(args, format);
        vsnprintf(tlsErrorMsg, sizeof(tlsErrorMsg), format, args);
        va_end(args);
    }
    else
    {
        tlsErrorMsg[0] = '\0';
    }

    if (gErrorCallback)
        gErrorCallback(error, tlsErrorMsg);
}

GLFWAPI int glfwGetError(const char **description)
{
    if (description)
        *description = tlsErrorMsg;

    const int error = tlsErrorCode;
    tlsErrorCode = GLFW_NO_ERROR;
    tlsErrorMsg[0] = '\0';
    return error;
}

GLFWAPI GLFWerrorfun glfwSetErrorCallback(GLFWerrorfun callback)
{
    const GLFWerrorfun previous = gErrorCallback;
    gErrorCallback = callback;
    return previous;
}