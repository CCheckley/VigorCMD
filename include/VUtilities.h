#pragma once

#include <iostream>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "VDefinitions.h"

namespace Vigor
{
#if VULKAN_VALIDATION_LAYERS_ENABLED
    static VKAPI_ATTR VkBool32 VKAPI_CALL VkValidationLayerCallback
    (
        VkDebugReportFlagsEXT flags,
        [[maybe_unused]] VkDebugReportObjectTypeEXT objectType,
        [[maybe_unused]] uint64_t object,
        [[maybe_unused]] size_t location,
        [[maybe_unused]] int32_t messageCode,
        [[maybe_unused]] const char* pLayerPrefix,
        const char* pMessage,
        [[maybe_unused]] void* pUserData
    ) noexcept
    {
        if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        {
            std::cout << ("[VULKAN ERROR]: %s\n\n", pMessage);
        }
        else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
        {
            std::cout << ("[VULKAN WARNING]: %s\n\n", pMessage);
        }
        else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
        {
            std::cout << ("[VULKAN PERFORMANCE WARNING]: %s\n\n", pMessage);
        }
#if VULKAN_VALIDATION_LAYER_VERBOSE_LOGGING
        else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
        {
            std::cout << ("[VULKAN INFO]: %s\n\n", pMessage);
        }
        else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
        {
            std::cout << ("[VULKAN DEBUG]: %s\n\n", pMessage);

        }
#endif // VULKAN_VALIDATION_LAYER_VERBOSE_LOGGING

        // Don't abort whatever call triggered this. This is normally only returned as true when
        // testing validation layers themselves, according to the Vulkan docs:
        // https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers
        return VK_FALSE;
    }
#endif // VULKAN_VALIDATION_LAYERS_ENABLED
}