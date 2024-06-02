#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <format>

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
    namespace Utilities
    {
        static bool IsDeviceSuitable(VkPhysicalDevice physicalDevice)
        {
            VkPhysicalDeviceProperties physicalDeviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

            VkPhysicalDeviceFeatures physicalDeviceFeatures;
            vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

            // Force dedicated GPU requirement and Geo-shader requirement
            return physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && physicalDeviceFeatures.geometryShader;
        }
    }

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

    class VigorCMD
    {
    public:

        VigorCMD(uint16_t width = 640, uint16_t height = 480)
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
            InitWindow();
            InitVKInstance();
            InitVKPhysicalDevice();
            InitSurface();
            InitQueueFamilies();
            InitLogicalDevice();
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
                if (!Vigor::Utilities::IsDeviceSuitable(availablePhysicalDevice))
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
            uint32_t queueFamilyCount;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

            uint32_t i = 0;
            for (VkQueueFamilyProperties queueFamily : queueFamilies)
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
            const char* deviceExtensionNames[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
            VkDeviceCreateInfo deviceCreateInfo =
            {
                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,   // sType
                nullptr,                                // pNext
                0,                                      // flags
                1,                                      // queueCreateInfoCount // TODO - should be size of vector of deviceQueueCreateInfos (doesnt exist yet)
                &deviceQueueCreateInfo,                 // pQueueCreateInfos // TODO - can point to data ptr of vector
                0,                                      // enabledLayerCount
                nullptr,                                // ppEnabledLayerNames
                1,                                      // enabledExtensionCount
                deviceExtensionNames,                   // ppEnabledExtensionNames
                &deviceFeatures,                        // pEnabledFeatures
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

    private:

        uint16_t WindowWidth = 640;
        uint16_t WindowHeight = 480;

        SDL_Window* window = nullptr;

        VkDevice logicalDevice = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        SDL_vulkanSurface surface = VK_NULL_HANDLE;
        SDL_vulkanInstance instance = VK_NULL_HANDLE;

        QueueFamilyIndicies queueFamilyIndicies;

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