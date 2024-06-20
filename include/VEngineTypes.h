#pragma once

#include <vector>
#include <optional>

#include <vulkan/vulkan.h>

namespace Vigor
{
    struct QueueFamilyIndicies
    {
        std::optional<uint32_t> presentFamily;

        std::optional<uint32_t> computeFamily;
        std::optional<uint32_t> graphicsFamily;

        bool IsComplete()
        {
            return
                presentFamily.has_value() &&
                computeFamily.has_value() &&
                graphicsFamily.has_value();
        }
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;

        bool IsComplete()
        {
            return
                !formats.empty() &&
                !presentModes.empty();
        }
    };
}