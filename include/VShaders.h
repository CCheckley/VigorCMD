#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Vigor
{
	namespace Shaders
	{
        static VkShaderModule CreateShaderModule(const std::vector<char>& code, VkDevice vkDevice)
        {
            VkShaderModuleCreateInfo shaderModuleCreateInfo{};
            shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCreateInfo.codeSize = code.size();
            shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); // TODO - get rid of this reinterpret_cast when possible, its BAD

            VkShaderModule shaderModule;

            VkResult createShaderModuleRes = vkCreateShaderModule(vkDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
            if (createShaderModuleRes != VK_SUCCESS)
            {
                std::string errorMsg = std::format("Failed to create shader module Error: {}\n\n", (int)createShaderModuleRes);
            }

            return shaderModule;
        }
	}
}