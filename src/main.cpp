#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <format>
#include <set>
#include <algorithm>
#include <fstream>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define VULKAN_VALIDATION_LAYERS_ENABLED 1
#define VULKAN_VALIDATION_LAYER_VERBOSE_LOGGING 1

constexpr uint32_t WINDOW_WIDTH = 640;
constexpr uint32_t WINDOW_HEIGHT = 480;

static constexpr const char* const KHRONOS_VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

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

    namespace Utilities
    {
        static bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice, const std::vector<const char*>& deviceExtensions)
        {
            uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
            for (const auto& availableExtension : availableExtensions)
            {
                requiredExtensions.erase(availableExtension.extensionName);
            }

            return requiredExtensions.empty();
        }

        static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
        {
            for (const auto& availableFormat : availableFormats)
            {
                bool bIsFormatSupported = availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB; // 8 Bit SRGB BGRA
                if (!bIsFormatSupported)
                {
                    continue;
                }

                bool bIsColorSpaceSupported = availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                if (!bIsColorSpaceSupported)
                {
                    continue;
                }

                return availableFormat;
            }

            return availableFormats[0];
        }

        static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
        {
            for (const auto& availablePresentMode : availablePresentModes)
            {
                bool bIsPresentModeSupported = availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR;
                if (!bIsPresentModeSupported)
                {
                    continue;
                }

                return availablePresentMode;
            }

            return VK_PRESENT_MODE_FIFO_KHR;
        }

        static VkExtent2D ChooseSwapExtent(SDL_Window* window, const VkSurfaceCapabilitiesKHR& capabilities)
        {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            {
                return capabilities.currentExtent;
            }

            int width, height;
            SDL_Vulkan_GetDrawableSize(window, &width, &height);

            // TODO - The below can be simplified
            VkExtent2D actualExtent =
            {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }

        static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, SDL_vulkanSurface surface)
        {
            SwapChainSupportDetails swapChainSupportDetails;

            // Capabilities
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &swapChainSupportDetails.capabilities);

            // Formats
            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

            if (formatCount != 0)
            {
                swapChainSupportDetails.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, swapChainSupportDetails.formats.data());
            }

            // Present Modes
            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

            if (presentModeCount != 0)
            {
                swapChainSupportDetails.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, swapChainSupportDetails.presentModes.data());
            }

            return swapChainSupportDetails;
        }

        static bool IsDeviceSuitable(VkPhysicalDevice physicalDevice, const SDL_vulkanSurface& surface, const std::vector<const char*>& deviceExtensions)
        {
            VkPhysicalDeviceProperties physicalDeviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

            VkPhysicalDeviceFeatures physicalDeviceFeatures;
            vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

            bool bResult = false;
            bResult |= (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU); // Force dedicated GPU requirement
            bResult |= (physicalDeviceFeatures.geometryShader); // Force Geo-shader requirement
            bResult |= (CheckDeviceExtensionSupport(physicalDevice, deviceExtensions));

            bool bSwapchainAcceptable = false;
            if (bResult)
            {
                bResult |= QuerySwapChainSupport(physicalDevice, surface).IsComplete();
            }

            return bResult;
        }

        static QueueFamilyIndicies GetQueueFamilies(VkPhysicalDevice physicalDevice, const SDL_vulkanSurface& surface)
        {
            QueueFamilyIndicies queueFamilyIndicies;

            uint32_t queueFamilyCount;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

            uint32_t i = 0;
            for (const VkQueueFamilyProperties& queueFamily : queueFamilies)
            {
                if (queueFamilyIndicies.IsComplete())
                {
                    break;
                }

                // Graphics
                if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    queueFamilyIndicies.graphicsFamily = i;
                }

                // Compute
                VkBool32 support;
                if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
                {
                    queueFamilyIndicies.computeFamily = i;
                }

                if (!queueFamilyIndicies.presentFamily.has_value())
                {
                    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &support);
                    if (support)
                    {
                        queueFamilyIndicies.presentFamily = i;
                    }
                }

                ++i;
            }

            return queueFamilyIndicies;
        }

        static VkShaderModule CreateShaderModule(const std::vector<char>& code, VkDevice logicalDevice)
        {
            VkShaderModuleCreateInfo shaderModuleCreateInfo{};
            shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCreateInfo.codeSize = code.size();
            shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); // TODO - get rid of this reinterpret_cast when possible, its BAD

            VkShaderModule shaderModule;

            VkResult createShaderModuleRes = vkCreateShaderModule(logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
            if (createShaderModuleRes != VK_SUCCESS)
            {
                std::string errorMsg = std::format("Failed to create shader module Error: {}\n\n", (int)createShaderModuleRes);
            }

            return shaderModule;
        }

        namespace File
        {
            static std::vector<char> Read(const std::string& filename)
            {
                std::ifstream file(filename, std::ios::ate | std::ios::binary);

                if (!file.is_open())
                {
                    throw std::runtime_error(("Failed to open file! File Path: %s\n\n", filename));
                }

                // get size
                size_t fileSize = (size_t)file.tellg();
                std::vector<char> buffer(fileSize);

                // seek to file start and read all bytes
                file.seekg(0);
                file.read(buffer.data(), fileSize);

                // close and return data
                file.close();
                return buffer;
            }
        }
    }

    class VigorCMD
    {
    public:

        VigorCMD(uint16_t width = 640, uint16_t height = 480)
            : pipelineLayout(VK_NULL_HANDLE)
            , swapChain(VK_NULL_HANDLE)
            , swapChainExtent()
            , swapChainImageFormat(VK_FORMAT_UNDEFINED)
            , renderPass()
            , graphicsPipeline()
        {
            WindowWidth = width;
            WindowHeight = height;

            // TODO - RAII
        }

        ~VigorCMD()
        {
            // TODO - RAII
        }

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

        void Init()
        {
            // Order of Init here is important
            InitWindow();
            InitVKInstance();
            InitSurface();
            InitVKPhysicalDevice();
            InitQueueFamilies();
            InitLogicalDevice();
            InitSwapChain();
            InitImageViews();
            InitRenderPass();
            InitGraphicsPipeline();
        }

        void Run()
        {
            // Main Loop
            bool running = true;
            while (running)
            {
                SDL_Event windowEvent;
                while (SDL_PollEvent(&windowEvent))
                {
                    if (windowEvent.type == SDL_QUIT)
                    {
                        running = false;
                        break;
                    }
                }
            }
        }

        void Shutdown()
        {
            vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
            vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
            vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

            for (auto imageView : swapChainImageViews)
            {
                vkDestroyImageView(logicalDevice, imageView, nullptr);
            }

            vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
            vkDestroySurfaceKHR(instance, surface, nullptr);
            vkDestroyDevice(logicalDevice, nullptr);
            vkDestroyInstance(instance, nullptr);
            SDL_DestroyWindow(window);

            SDL_Vulkan_UnloadLibrary();
            SDL_Quit();

            SDL_Log("Cleaned up with errors: %s", SDL_GetError());
        }

    private:

        void InitWindow()
        {
            SDL_Init(SDL_INIT_EVERYTHING);

            int initError = 0;
            initError = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
            if (initError < 0)
            {
                throw std::runtime_error(("SDL_Init Error: %d, %s\n\n", initError, SDL_GetError()));
            }

            window = SDL_CreateWindow("VigorCMD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
            if (window == nullptr)
            {
                throw std::runtime_error(("SDL_CreateWindow Error: %s\n\n", SDL_GetError()));
            }
        }

        void InitVKInstance()
        {
            VkApplicationInfo appInfo{}; // !! YOU NEED TO DEFAULT INIT LIKE THIS !!
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.apiVersion = VK_API_VERSION_1_3;
            appInfo.pApplicationName = "VigorCMD";
            appInfo.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
            appInfo.pEngineName = "Vigor";
            appInfo.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);

            unsigned int extensionCount = 0;
            SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL);
            std::vector<const char*> extentionNames(extensionCount);
            SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extentionNames.data());

            std::cout << "Instance Extention's:\n\n";
            for (unsigned int i = 0; i < extensionCount; i++)
            {
                std::cout << ("%u: %s\n\n", i, extentionNames[i]);
            }
            std::cout << "----- ----- ----- ----- ----- -----\n\n";

#if VULKAN_VALIDATION_LAYERS_ENABLED
            // Request validation layers and the debug utils extension (for reporting) if required
            extentionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            extentionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

            // This is required by the 'VK_KHR_portability_subset' device extension used for validation on MacOS.
            extentionNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            validationLayerNames.push_back(KHRONOS_VALIDATION_LAYER_NAME);

            VkDebugReportCallbackCreateInfoEXT dbgReportCallbackCInfo{}; // !! YOU NEED TO DEFAULT INIT LIKE THIS !!
            dbgReportCallbackCInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            dbgReportCallbackCInfo.pfnCallback = VkValidationLayerCallback;
            dbgReportCallbackCInfo.flags =
                (
                    VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
                    VK_DEBUG_REPORT_WARNING_BIT_EXT |
                    VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                    VK_DEBUG_REPORT_ERROR_BIT_EXT |
                    VK_DEBUG_REPORT_DEBUG_BIT_EXT
                    );
#endif // VULKAN_VALIDATION_LAYERS_ENABLED

            // Fill out this struct with details about the instance we want, required extensions, API layers and so on:
            VkInstanceCreateInfo instInfo{}; // !! YOU NEED TO DEFAULT INIT LIKE THIS !!
            instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instInfo.pApplicationInfo = &appInfo;
            instInfo.ppEnabledExtensionNames = extentionNames.data();
            instInfo.enabledExtensionCount = (uint32_t)extentionNames.size();
            instInfo.ppEnabledLayerNames = validationLayerNames.data();
            instInfo.enabledLayerCount = (uint32_t)validationLayerNames.size();

#if VULKAN_VALIDATION_LAYERS_ENABLED
            // This line here enables validation layer reporting for instance creation and destruction.
            // Normally you must setup your own reporting object but that requires an instance and thus won't be used for instance create/destroy.
            // If we point this list to a debug report 'create info' then the API will do reporting for instance create/destroy accordingly.
            instInfo.pNext = &dbgReportCallbackCInfo;
#endif // VULKAN_VALIDATION_LAYERS_ENABLED

            VkResult instanceCreationResult = vkCreateInstance(&instInfo, NULL, &instance);
            if (instanceCreationResult != VK_SUCCESS)
            {
                std::string errorMsg = std::format("Failed to create instance Error: {}\n\n", (int)instanceCreationResult);
                throw std::runtime_error(errorMsg);
            }
        }

        void InitVKPhysicalDevice()
        {
            uint32_t physicalDeviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

            if (physicalDeviceCount == 0)
            {
                throw std::runtime_error("Cannot find Physical Device with VK support");
            }

            std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
            vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

            for (const auto& availablePhysicalDevice : physicalDevices)
            {
                if (!Vigor::Utilities::IsDeviceSuitable(availablePhysicalDevice, surface, deviceExtensions))
                {
                    continue;
                }

                physicalDevice = availablePhysicalDevice;
                break;
            }

            if (physicalDevice == VK_NULL_HANDLE)
            {
                throw std::runtime_error("Cannot find suitable Physical Device");
            }
        }

        void InitQueueFamilies()
        {
            queueFamilyIndicies = Vigor::Utilities::GetQueueFamilies(physicalDevice, surface);
        }

        void InitSurface()
        {
            if (SDL_FALSE == SDL_Vulkan_CreateSurface(window, instance, &surface))
            {
                throw std::runtime_error(("Failed to create surface, SDL Error: %s\n\n", SDL_GetError()));
            }
        }

        void InitLogicalDevice()
        {
            float queuePriority = 1.0f;
            VkDeviceQueueCreateInfo deviceQueueCreateInfo = { // TODO - should be seperate for individual queue families and stored in a vector
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
                nullptr,                                    // pNext
                0,                                          // flags
                queueFamilyIndicies.graphicsFamily.value(), // graphicsQueueIndex
                1,                                          // queueCount
                &queuePriority,                             // pQueuePriorities
            };

            // VK Device Setup
            VkPhysicalDeviceFeatures deviceFeatures = {};
            VkDeviceCreateInfo deviceCreateInfo =
            {
                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,           // sType
                nullptr,                                        // pNext
                0,                                              // flags
                1,                                              // queueCreateInfoCount // TODO - should be size of vector of deviceQueueCreateInfos (doesnt exist yet)
                &deviceQueueCreateInfo,                         // pQueueCreateInfos // TODO - can point to data ptr of vector
                0,                                              // enabledLayerCount
                nullptr,                                        // ppEnabledLayerNames
                static_cast<uint32_t>(deviceExtensions.size()), // enabledExtensionCount
                deviceExtensions.data(),                        // ppEnabledExtensionNames
                &deviceFeatures,                                // pEnabledFeatures
            };

#if VULKAN_VALIDATION_LAYERS_ENABLED
            deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayerNames.size());
            deviceCreateInfo.ppEnabledLayerNames = validationLayerNames.data();
#endif // VULKAN_VALIDATION_LAYERS_ENABLED

            if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create Logical VK Device");
            }

            VkQueue presentQueue;
            vkGetDeviceQueue(logicalDevice, queueFamilyIndicies.presentFamily.value(), 0, &presentQueue);

            VkQueue graphicsQueue;
            vkGetDeviceQueue(logicalDevice, queueFamilyIndicies.graphicsFamily.value(), 0, &graphicsQueue);

            SDL_Log("Initialized with errors: %s", SDL_GetError());
        }

        void InitSwapChain()
        {
            SwapChainSupportDetails swapChainSupportDetails = Utilities::QuerySwapChainSupport(physicalDevice, surface);

            VkSurfaceFormatKHR surfaceFormat = Utilities::ChooseSwapSurfaceFormat(swapChainSupportDetails.formats);
            VkPresentModeKHR presentMode = Utilities::ChooseSwapPresentMode(swapChainSupportDetails.presentModes);
            VkExtent2D extent = Utilities::ChooseSwapExtent(window, swapChainSupportDetails.capabilities);

            uint32_t imageCount = swapChainSupportDetails.capabilities.minImageCount + 1; // sticking to this minimum means that we may sometimes have to wait on the driver to complete internal operations before we can acquire another image to render to. Therefore it is recommended to request at least one more image than the minimum.
            if (swapChainSupportDetails.capabilities.maxImageCount > 0 && imageCount > swapChainSupportDetails.capabilities.maxImageCount)
            {
                imageCount = swapChainSupportDetails.capabilities.maxImageCount;
            }

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = surface;
            createInfo.minImageCount = imageCount;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            Vigor::QueueFamilyIndicies indices = Utilities::GetQueueFamilies(physicalDevice, surface);
            uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

            if (indices.graphicsFamily != indices.presentFamily)
            {
                createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                createInfo.queueFamilyIndexCount = 2;
                createInfo.pQueueFamilyIndices = queueFamilyIndices;
            }
            else
            {
                createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                createInfo.queueFamilyIndexCount = 0; // Optional
                createInfo.pQueueFamilyIndices = nullptr; // Optional
            }

            createInfo.preTransform = swapChainSupportDetails.capabilities.currentTransform;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.presentMode = presentMode;
            createInfo.clipped = VK_TRUE;
            createInfo.oldSwapchain = VK_NULL_HANDLE;

            if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create swap chain!");
            }

            vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
            swapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

            swapChainImageFormat = surfaceFormat.format;
            swapChainExtent = extent;
        }

        void InitImageViews()
        {
            swapChainImageViews.resize(swapChainImages.size());

            for (size_t i = 0; i < swapChainImages.size(); i++)
            {
                VkImageViewCreateInfo imageViewCreateInfo{};
                imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                imageViewCreateInfo.image = swapChainImages[i];
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // Can be 1d,2d,3d, arrays of the former, etc.
                imageViewCreateInfo.format = swapChainImageFormat;

                /*
                * The components field allows you to swizzle the color channels around.
                * For example, you can map all of the channels to the red channel for a monochrome texture.
                * You can also map constant values of 0 and 1 to a channel. In our case we'll stick to the default mapping.
                */
                imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

                /*
                * The subresourceRange field describes what the image's purpose is and which part of the image should be accessed.
                * Our images will be used as color targets without any mipmapping levels or multiple layers.
                */
                imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
                imageViewCreateInfo.subresourceRange.levelCount = 1;
                imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
                imageViewCreateInfo.subresourceRange.layerCount = 1;
                /* - NOTE -
                * If you were working on a stereographic 3D application, then you would create a swap chain with multiple layers.
                * You could then create multiple image views for each image representing the views for the left and right eyes by accessing different layers.
                */

                VkResult createImageViewRes = vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &swapChainImageViews[i]);
                if (createImageViewRes != VK_SUCCESS)
                {
                    std::string errorMsg = std::format("Failed to create image view Error: {}, Index: {}\n\n", (int)createImageViewRes, i);
                    throw std::runtime_error(errorMsg);
                }
            }
        }

        void InitGraphicsPipeline()
        {
            // Shader Module Setup
            auto VertexShaderCode = Vigor::Utilities::File::Read("./shaders/TriVS.spv");
            auto PixelShaderCode = Vigor::Utilities::File::Read("./shaders/TriPS.spv");

            VkShaderModule vertexShaderModule = Vigor::Utilities::CreateShaderModule(VertexShaderCode, logicalDevice);
            VkShaderModule pixelShaderModule = Vigor::Utilities::CreateShaderModule(PixelShaderCode, logicalDevice);

            VkPipelineShaderStageCreateInfo createInfoVertexShaderStage{};
            createInfoVertexShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            createInfoVertexShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
            createInfoVertexShaderStage.module = vertexShaderModule;
            createInfoVertexShaderStage.pName = "main";

            VkPipelineShaderStageCreateInfo createInfoPixelShaderStage{};
            createInfoPixelShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            createInfoPixelShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            createInfoPixelShaderStage.module = pixelShaderModule;
            createInfoPixelShaderStage.pName = "main";

            VkPipelineShaderStageCreateInfo shaderStages[] = { createInfoVertexShaderStage, createInfoPixelShaderStage };

            // Dynamic State Setup
            std::vector<VkDynamicState> dynamicStates =
            {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };

            VkPipelineDynamicStateCreateInfo createInfoDynamicState{};
            createInfoDynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            createInfoDynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()); // TODO - remove cast if possible
            createInfoDynamicState.pDynamicStates = dynamicStates.data();

            // Vertex Input
            VkPipelineVertexInputStateCreateInfo createInfoVertexInput{};
            createInfoVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            createInfoVertexInput.vertexBindingDescriptionCount = 0; // not currently sending a vertex buffer
            createInfoVertexInput.pVertexBindingDescriptions = nullptr;
            createInfoVertexInput.vertexAttributeDescriptionCount = 0; // not currently sending a vertex buffer
            createInfoVertexInput.pVertexAttributeDescriptions = nullptr;

            // Input Assembly
            VkPipelineInputAssemblyStateCreateInfo createInfoInputAssembly{};
            createInfoInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            createInfoInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            createInfoInputAssembly.primitiveRestartEnable = VK_FALSE; // if VK_TRUE, then it's possible to break up lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF.

            // Viewport and Scissor
            /* -- NOTE --
            * Viewport defines the region that pixels transformed from an image to the framebuffer
            * Scissor defines the region of pixels that are stored, anything outside this within the viewport are discarded by the rasterizer
            */

            /* -- NOTE --
            * These can be setup as static as part of pipeline state but are supported as dynamic
            * This generally doesnt have a noticeable perf hit and is worthwhile as commonly needed
            */

            // Viewport
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)swapChainExtent.width; // -- NOTE -- Swap chain width and height can differ from window height/width, but will be what we use as framebuffers
            viewport.height = (float)swapChainExtent.height; // -- NOTE -- Swap chain width and height can differ from window height/width, but will be what we use as framebuffers
            viewport.minDepth = 0.0f; // -- NOTE -- these values are normalised within 0 - 1
            viewport.maxDepth = 1.0f; // -- NOTE -- these values are normalised within 0 - 1

            // Scissor
            VkRect2D scissor{};
            scissor.offset = { 0,0 };
            scissor.extent = swapChainExtent; // This scissor covers the entire range of the vieport so no pixels are discarded by the rstr

            /* -- NOTE --
            * With dynamic state it's even possible to specify different viewports and or scissor rectangles within a single command buffer.
            * 
            * Without dynamic state, the viewport and scissor rectangle need to be set in the pipeline using the VkPipelineViewportStateCreateInfo struct.
            * This makes the viewport and scissor rectangle for this pipeline immutable.
            * Any changes required to these values would require a new pipeline to be created with the new values.
            * 
            * Using multiple requires enabling a GPU feature (see logical device init).
            */

            // Viewport Pipeline
            VkPipelineViewportStateCreateInfo createInfoViewportState{};
            createInfoViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            createInfoViewportState.viewportCount = 1;
            createInfoViewportState.pViewports = &viewport;
            createInfoViewportState.scissorCount = 1;
            createInfoViewportState.pScissors = &scissor;

            // Rasterizer
            VkPipelineRasterizationStateCreateInfo createInfoRasterizerState{};
            createInfoRasterizerState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            /*
            * if VK_TRUE, then fragments that are beyond the near and far planes are clamped to them as opposed to discarding them.
            * This is useful in some special cases like shadow maps.
            * Using this requires enabling a GPU feature.
            */
            createInfoRasterizerState.depthClampEnable = VK_FALSE;
            createInfoRasterizerState.rasterizerDiscardEnable = VK_FALSE; // if VK_TRUE, then geometry never passes through the rasterizer stage. This disables any output to the framebuffer.
            createInfoRasterizerState.polygonMode = VK_POLYGON_MODE_FILL; // Using any mode other than fill requires enabling a GPU feature.
            createInfoRasterizerState.lineWidth = 1.0f; // describes the thickness of lines in number of fragments. The maximum line width depends on the hardware, any line thicker than 1.0f requires the "wideLines" GPU feature.
            createInfoRasterizerState.cullMode = VK_CULL_MODE_BACK_BIT;
            createInfoRasterizerState.frontFace = VK_FRONT_FACE_CLOCKWISE;
            /*
            * The rasterizer can alter the depth values by adding a constant value or biasing them based on a fragment's slope.
            * This is sometimes used for shadow mapping, but we won't be using it so depthBiasEnable is set to VK_FALSE
            */
            createInfoRasterizerState.depthBiasEnable = VK_FALSE;
            createInfoRasterizerState.depthBiasConstantFactor = 0.0f; // Optional
            createInfoRasterizerState.depthBiasClamp = 0.0f; // Optional
            createInfoRasterizerState.depthBiasSlopeFactor = 0.0f; // Optional

            // MSAA (Multisampling) - Disabled with this config
            VkPipelineMultisampleStateCreateInfo createInfoMultisampling{};
            createInfoMultisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            createInfoMultisampling.sampleShadingEnable = VK_FALSE;
            createInfoMultisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            createInfoMultisampling.minSampleShading = 1.0f; // Optional
            createInfoMultisampling.pSampleMask = nullptr; // Optional
            createInfoMultisampling.alphaToCoverageEnable = VK_FALSE; // Optional
            createInfoMultisampling.alphaToOneEnable = VK_FALSE; // Optional

            /* Depth and Stencil testing
            * To configure this VkPipelineDepthStencilStateCreateInfo must be used but is ignored for now
            */

            // Color Blending - (At the moment the following config doesnt perform any color blending with the chosen settings)
            /* -- NOTE --
            * There are two types of structs to configure color blending.
            * The first struct, VkPipelineColorBlendAttachmentState contains the configuration per attached framebuffer.
            * The second struct, VkPipelineColorBlendStateCreateInfo contains the global color blending settings.
            * 
            * ATM we only have one framebuffer, so we do the following:
            */

            // Per frame buffer
            VkPipelineColorBlendAttachmentState createInfoColorBlendAttachment{};
            createInfoColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            // Blending Disabled:
            createInfoColorBlendAttachment.blendEnable = VK_FALSE; // the new color from the fragment shader is passed through unmodified, otherwise the resulting color is AND'd with the colorWriteMask to determine which channels are actually passed through
            createInfoColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
            createInfoColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
            createInfoColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
            createInfoColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
            createInfoColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
            createInfoColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

            // Alpha blending:
            //createInfoColorBlendAttachment.blendEnable = VK_TRUE;
            //createInfoColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            //createInfoColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            //createInfoColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            //createInfoColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            //createInfoColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            //createInfoColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            // Global blend settings
            VkPipelineColorBlendStateCreateInfo createInfoColorBlending{};
            createInfoColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            createInfoColorBlending.logicOpEnable = VK_FALSE; // set to true to utilize bitwise blending, will act as if each attachment has blendEnable set to false
            createInfoColorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
            createInfoColorBlending.attachmentCount = 1;
            createInfoColorBlending.pAttachments = &createInfoColorBlendAttachment;
            createInfoColorBlending.blendConstants[0] = 0.0f; // Optional
            createInfoColorBlending.blendConstants[1] = 0.0f; // Optional
            createInfoColorBlending.blendConstants[2] = 0.0f; // Optional
            createInfoColorBlending.blendConstants[3] = 0.0f; // Optional

            // Pipeline Layout - empty for now, need to specificy uniforms here when used
            VkPipelineLayoutCreateInfo createInfoPipelineLayout{};
            createInfoPipelineLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            createInfoPipelineLayout.setLayoutCount = 0; // Optional
            createInfoPipelineLayout.pSetLayouts = nullptr; // Optional
            createInfoPipelineLayout.pushConstantRangeCount = 0; // Optional
            createInfoPipelineLayout.pPushConstantRanges = nullptr; // Optional

            if (vkCreatePipelineLayout(logicalDevice, &createInfoPipelineLayout, nullptr, &pipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create pipeline layout!");
            }

            // Create the pipeline
            VkGraphicsPipelineCreateInfo createInfoGraphicsPipeline{};
            createInfoGraphicsPipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            createInfoGraphicsPipeline.stageCount = 2; // number of shader stages
            createInfoGraphicsPipeline.pStages = shaderStages;
            createInfoGraphicsPipeline.pVertexInputState = &createInfoVertexInput;
            createInfoGraphicsPipeline.pInputAssemblyState = &createInfoInputAssembly;
            createInfoGraphicsPipeline.pViewportState = &createInfoViewportState;
            createInfoGraphicsPipeline.pRasterizationState = &createInfoRasterizerState;
            createInfoGraphicsPipeline.pMultisampleState = &createInfoMultisampling;
            createInfoGraphicsPipeline.pDepthStencilState = nullptr; // not used yet
            createInfoGraphicsPipeline.pColorBlendState = &createInfoColorBlending;
            createInfoGraphicsPipeline.pDynamicState = &createInfoDynamicState;
            createInfoGraphicsPipeline.layout = pipelineLayout;

            // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chap8.html#renderpass-compatibility
            createInfoGraphicsPipeline.renderPass = renderPass;
            createInfoGraphicsPipeline.subpass = 0; // idx

            //Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline
            createInfoGraphicsPipeline.basePipelineHandle = VK_NULL_HANDLE;
            createInfoGraphicsPipeline.basePipelineIndex = -1;

            // pipeline cache and batch pipeline creation can also be configured
            if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &createInfoGraphicsPipeline, nullptr, &graphicsPipeline) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create graphics pipeline!");
            }

            // Shader Module Cleanup
            vkDestroyShaderModule(logicalDevice, vertexShaderModule, nullptr);
            vkDestroyShaderModule(logicalDevice, pixelShaderModule, nullptr);
        }

        void InitRenderPass()
        {
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = swapChainImageFormat;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // no msaa yet so stick with 1
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // not doing anything with stencil yet so dont care
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // not doing anything with stencil yet so dont care
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            // Sub-passes and attachment refs
            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = 0; // specifies which attachment to reference by its index
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // supports compute subpasses so need to be explicit
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;

            /* -- NOTE --
            * The index of the attachment in this array is directly referenced from the fragment shader with "layout(location = 0) out vec4 outColor" in OpenGL
            * 
            * The following other types of attachments can be referenced by a subpass:
            *   - pInputAttachments: Attachments that are read from a shader
            *   - pResolveAttachments: Attachments used for multisampling color attachments
            *   - pDepthStencilAttachment: Attachment for depth and stencil data
            *   - pPreserveAttachments: Attachments that are not used by this subpass, but for which the data must be preserved
            */

            VkRenderPassCreateInfo createInfoRenderPass{};
            createInfoRenderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            createInfoRenderPass.attachmentCount = 1;
            createInfoRenderPass.pAttachments = &colorAttachment;
            createInfoRenderPass.subpassCount = 1;
            createInfoRenderPass.pSubpasses = &subpass;

            VkResult createRenderPassRes = vkCreateRenderPass(logicalDevice, &createInfoRenderPass, nullptr, &renderPass);
            if (createRenderPassRes != VK_SUCCESS)
            {
                std::string errorMsg = std::format("Failed to create render pass Error: {}\n\n", (int)createRenderPassRes);
                throw std::runtime_error(errorMsg);
            }
        }

    private:

        uint16_t WindowWidth = 640;
        uint16_t WindowHeight = 480;

        SDL_Window* window = nullptr;

        VkDevice logicalDevice = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        SDL_vulkanSurface surface = VK_NULL_HANDLE;
        SDL_vulkanInstance instance = VK_NULL_HANDLE;

        QueueFamilyIndicies queueFamilyIndicies;

        VkSwapchainKHR swapChain;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;

        VkRenderPass renderPass;
        VkPipelineLayout pipelineLayout;

        VkPipeline graphicsPipeline;

        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;

#if VULKAN_VALIDATION_LAYERS_ENABLED
        std::vector<const char*> validationLayerNames;
#endif // VULKAN_VALIDATION_LAYERS_ENABLED
    };
}

int main(int argc, char* argv[])
{
    Vigor::VigorCMD vigorCMD;

    vigorCMD.Init();
    vigorCMD.Run();
    vigorCMD.Shutdown();

	return 0;
}