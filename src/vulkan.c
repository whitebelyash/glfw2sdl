/*
 * GLFW support layer on top of SDL3 -- Vulkan integration.
 *
 * SDL3 owns the Vulkan loader and any window/logical-device surface wiring;
 * this module only routes the GLFW Vulkan calls onto SDL3.
 *
 * SPDX-License-Identifier: Zlib
 */

/* glfw3.h only declares the Vulkan-taking functions when the app (or this
 * module) pulls in the Vulkan headers first.  We need the declarations here
 * so the definitions below match. */
#define GLFW_INCLUDE_VULKAN

#include "internal.h"

#include <SDL3/SDL_vulkan.h>

/* glfw3.h guards these two with VK_VERSION_1_0, which the include above
 * provides.  Implementations must live in this module because their
 * parameter types only exist when the Vulkan headers are included. */

GLFWAPI void glfwInitVulkanLoader(PFN_vkGetInstanceProcAddr loader)
{
    /* SDL owns the Vulkan loader; a custom loader cannot be handed to it
     * before initialization.  Accept and remember for API compatibility. */
    _glfw.vkLoader = (void *)loader;
}

GLFWAPI GLFWvkproc glfwGetInstanceProcAddress(VkInstance instance,
                                              const char *procname)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }

    /* SDL3 exposes no way to resolve instance/device entry points, so no
     * Vulkan function can be handed out beyond what the loader or the
     * application's own vkGetInstanceProcAddr already provides. */
    _glfwInputError(GLFW_API_UNAVAILABLE,
                    "Instance function loading is not supported through the "
                    "SDL-backed GLFW shim (use vkGetInstanceProcAddr directly)");
    return NULL;
}

GLFWAPI int glfwVulkanSupported(void)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_FALSE;
    }

    if (!SDL_Vulkan_LoadLibrary(NULL))
        return GLFW_FALSE;

    _glfw.vkLoaded = true;
    return GLFW_TRUE;
}

GLFWAPI const char **glfwGetRequiredInstanceExtensions(uint32_t *count)
{
    *count = 0;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return NULL;
    }

    if (!_glfw.vkLoaded)
    {
        if (!SDL_Vulkan_LoadLibrary(NULL))
        {
            _glfwInputError(GLFW_API_UNAVAILABLE,
                            "No Vulkan loader available: %s", SDL_GetError());
            return NULL;
        }
        _glfw.vkLoaded = true;
    }

    Uint32 n = 0;
    const char * const *names = SDL_Vulkan_GetInstanceExtensions(&n);
    if (!names)
        return NULL;

    /* GLFW returns a NULL-terminated array; SDL returns only the count. */
    const Uint32 cap = sizeof(_glfw.vkText) / sizeof(_glfw.vkText[0]);
    if (n >= cap)
        n = cap - 1;

    _glfw.vkTextCount = n;
    for (Uint32 i = 0; i < n; i++)
        _glfw.vkText[i] = names[i];
    _glfw.vkText[n] = NULL;

    *count = n;
    return _glfw.vkText;
}

GLFWAPI int glfwGetPhysicalDevicePresentationSupport(VkInstance instance,
                                                     VkPhysicalDevice device,
                                                     uint32_t queuefamily)
{
    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return GLFW_FALSE;
    }

    if (!instance || !device)
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid Vulkan instance or device");
        return GLFW_FALSE;
    }

    if (!_glfw.vkLoaded)
    {
        if (!SDL_Vulkan_LoadLibrary(NULL))
        {
            _glfwInputError(GLFW_API_UNAVAILABLE,
                            "No Vulkan loader available: %s", SDL_GetError());
            return GLFW_FALSE;
        }
        _glfw.vkLoaded = true;
    }

    return SDL_Vulkan_GetPresentationSupport(instance, device, queuefamily)
               ? GLFW_TRUE : GLFW_FALSE;
}

GLFWAPI VkResult glfwCreateWindowSurface(VkInstance instance,
                                         GLFWwindow *handle,
                                         const VkAllocationCallbacks *allocator,
                                         VkSurfaceKHR *surface)
{
    _GLFWwindow *window = (_GLFWwindow *)handle;

    if (!_glfw.initialized)
    {
        _glfwInputError(GLFW_NOT_INITIALIZED, "GLFW is not initialized");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!window)
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid window handle");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!surface)
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Invalid surface pointer");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!_glfw.vkLoaded)
    {
        if (!SDL_Vulkan_LoadLibrary(NULL))
        {
            _glfwInputError(GLFW_API_UNAVAILABLE,
                            "No Vulkan loader available: %s", SDL_GetError());
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        _glfw.vkLoaded = true;
    }

    if (!SDL_Vulkan_CreateSurface(window->sdlWindow, instance, allocator, surface))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}