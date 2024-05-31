#include <iostream>
#include <vector>
#include <string>
#include <optional>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define GENERAL_ERROR           666
#define WINDOW_CREATE_ERROR     901
#define INSTANCE_CREATE_ERROR   902
#define SURFACE_CREATE_ERROR    903

#define VULKAN_VALIDATION_LAYERS_ENABLED true

constexpr uint32_t WINDOW_WIDTH  = 640;
constexpr uint32_t WINDOW_HEIGHT = 480;

static constexpr const char* const KHRONOS_VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanValidationLayerCallback(
    VkDebugReportFlagsEXT flags,
    [[maybe_unused]] VkDebugReportObjectTypeEXT objectType,
    [[maybe_unused]] uint64_t object,
    [[maybe_unused]] size_t location,
    [[maybe_unused]] int32_t messageCode,
    [[maybe_unused]] const char* pLayerPrefix,
    const char* pMessage,
    [[maybe_unused]] void* pUserData
) noexcept {
    // Set to '1' for more verbose logging
#define VULKAN_VALIDATION_LAYER_VERBOSE_LOGGING 1

// See what type of message we are dealing with:
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        std::cout << ("[VULKAN ERROR]: %s\n\n", pMessage);
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        std::cout << ("[VULKAN WARNING]: %s\n\n", pMessage);
    }
    else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
        std::cout << ("[VULKAN PERFORMANCE WARNING]: %s\n\n", pMessage);
    }
    else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
#if VULKAN_VALIDATION_LAYER_VERBOSE_LOGGING
        std::cout << ("[VULKAN INFO]: %s\n\n", pMessage);
#endif
    }
    else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
#if VULKAN_VALIDATION_LAYER_VERBOSE_LOGGING
        std::cout << ("[VULKAN DEBUG]: %s\n\n", pMessage);
#endif
    }

    // Don't abort whatever call triggered this. This is normally only returned as true when
    // testing validation layers themselves, according to the Vulkan docs:
    // https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers
    return VK_FALSE;
}

void WindowInit(SDL_Window** window)
{
    SDL_Init(SDL_INIT_EVERYTHING);

    int initError = 0;
    initError = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (initError < 0) {
        std::cout << "VIDEO INIT ERROR: " << SDL_GetError() << std::endl;
        throw initError;
    }

    *window = SDL_CreateWindow("VigorCMD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (window == nullptr) {
        std::cout << "WINDOW CREATION ERROR: " << SDL_GetError() << std::endl;
        throw WINDOW_CREATE_ERROR;
    }
}

void SetupVulkanInstance(SDL_Window* window, SDL_vulkanInstance* instance)
{
    VkApplicationInfo appInfo{}; // !! YOU NEED TO DEFAULT INIT LIKE THIS !!
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pApplicationName = "test-app";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
    appInfo.pEngineName = "test-engine";
    appInfo.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);

    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL);
    std::vector<const char*> extentionNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extentionNames.data());

    for (unsigned int i = 0; i < extensionCount; i++)
    {
        printf("%u: %s\n", i, extentionNames[i]);
    }

    // The required API layers: none by default, unless we want validation layers
    std::vector<const char*> reqLayerNames;

    // Request validation layers and the debug utils extension (for reporting) if required
    if (VULKAN_VALIDATION_LAYERS_ENABLED) {
        extentionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extentionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

        // This is required by the 'VK_KHR_portability_subset' device extension used for validation on MacOS.
        extentionNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        reqLayerNames.push_back(KHRONOS_VALIDATION_LAYER_NAME);
    }

    // Fill out this struct with details about the instance we want, required extensions, API layers and so on:
    VkInstanceCreateInfo instInfo{}; // !! YOU NEED TO DEFAULT INIT LIKE THIS !!
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.ppEnabledExtensionNames = extentionNames.data();
    instInfo.enabledExtensionCount = (uint32_t)extentionNames.size();
    instInfo.ppEnabledLayerNames = reqLayerNames.data();
    instInfo.enabledLayerCount = (uint32_t)reqLayerNames.size();

    VkDebugReportCallbackCreateInfoEXT dbgReportCallbackCInfo{}; // !! YOU NEED TO DEFAULT INIT LIKE THIS !!
    dbgReportCallbackCInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    dbgReportCallbackCInfo.pfnCallback = vulkanValidationLayerCallback;
    dbgReportCallbackCInfo.flags = (
        VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
        VK_DEBUG_REPORT_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_ERROR_BIT_EXT |
        VK_DEBUG_REPORT_DEBUG_BIT_EXT
        );
    if (VULKAN_VALIDATION_LAYERS_ENABLED) {
        // This line here enables validation layer reporting for instance creation and destruction.
        // Normally you must setup your own reporting object but that requires an instance and thus won't be used for instance create/destroy.
        // If we point this list to a debug report 'create info' then the API will do reporting for instance create/destroy accordingly.

        instInfo.pNext = &dbgReportCallbackCInfo;
    }

    if (0 > vkCreateInstance(&instInfo, NULL, instance))
    {
        printf("failed to create instance\n");
        throw GENERAL_ERROR;
    }
}

bool IsDeviceSuitable(VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

    // Force dedicated GPU requirement and Geo-shader requirement
    return physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && physicalDeviceFeatures.geometryShader;
}

VkPhysicalDevice PickPhysicalDevice(SDL_vulkanInstance* instance)
{
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(*instance, &physicalDeviceCount, nullptr);

    if (physicalDeviceCount == 0)
    {
        throw std::runtime_error("CANNOT FIND PHYSICAL DEVICE WITH VK SUPPORT");
    }

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(*instance, &physicalDeviceCount, physicalDevices.data());

    VkPhysicalDevice pickedPhysicalDevice = VK_NULL_HANDLE;
    for (const auto& physicalDevice : physicalDevices)
    {
        if (!IsDeviceSuitable(physicalDevice))
        {
            continue;
        }

        pickedPhysicalDevice = physicalDevice;
        break;
    }

    if (pickedPhysicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("CANNOT FIND SUITABLE PHYSICAL DEVICE");
    }

    return pickedPhysicalDevice;
}

struct QueueFamilyIndicies
{
    std::optional<uint32_t> presentFamily;

    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> graphicsFamily;

    bool IsComplete()
    {
        return presentFamily.has_value() && computeFamily.has_value() && graphicsFamily.has_value();
    }
};

QueueFamilyIndicies FindQueueFamilies(VkPhysicalDevice physicalDevice, SDL_vulkanSurface* surface)
{
    QueueFamilyIndicies indicies;

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t i = 0;
    for (VkQueueFamilyProperties queueFamily : queueFamilies)
    {
        if (indicies.IsComplete())
        {
            break;
        }

        // Graphics
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indicies.graphicsFamily = i;
        }

        // Compute
        VkBool32 support;
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            indicies.computeFamily = i;
        }

        if (!indicies.presentFamily.has_value())
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, *surface, &support);
            if (support)
            {
                indicies.presentFamily = i;
            }
        }

        ++i;
    }

    return indicies;
}

void SetupVulkan(SDL_Window** window, SDL_vulkanInstance instance, VkDevice device)
{
    SetupVulkanInstance(*window, &instance);

    VkPhysicalDevice physicalDevice = PickPhysicalDevice(&instance);

    SDL_vulkanSurface surface = VK_NULL_HANDLE;
    if (SDL_FALSE == SDL_Vulkan_CreateSurface(*window, instance, &surface))
    {
        printf("failed to create surface, SDL Error: %s", SDL_GetError());
        throw GENERAL_ERROR;
    }

    QueueFamilyIndicies queueFamilyIndicies = FindQueueFamilies(physicalDevice, &surface);

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
        nullptr,                                    // pNext
        0,                                          // flags
        queueFamilyIndicies.graphicsFamily.value(), // graphicsQueueIndex
        1,                                          // queueCount
        &queuePriority,                             // pQueuePriorities
    };

    // -- SETUP VK DEVICE --
    VkPhysicalDeviceFeatures deviceFeatures = {};
    const char* deviceExtensionNames[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,   // sType
        nullptr,                                // pNext
        0,                                      // flags
        1,                                      // queueCreateInfoCount
        &queueInfo,                             // pQueueCreateInfos
        0,                                      // enabledLayerCount
        nullptr,                                // ppEnabledLayerNames
        1,                                      // enabledExtensionCount
        deviceExtensionNames,                   // ppEnabledExtensionNames
        &deviceFeatures,                        // pEnabledFeatures
    };
    vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);

    VkQueue presentQueue;
    vkGetDeviceQueue(device, queueFamilyIndicies.presentFamily.value(), 0, &presentQueue);

    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, queueFamilyIndicies.graphicsFamily.value(), 0, &graphicsQueue);

    SDL_Log("Initialized with errors: %s", SDL_GetError());
}

void MainLoop()
{
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

void Cleanup(SDL_Window** window, SDL_vulkanInstance instance, VkDevice device)
{
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(*window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    SDL_Log("Cleaned up with errors: %s", SDL_GetError());
}

int main(int argc, char* argv[]) {    
    SDL_Window** window = (SDL_Window**)malloc(sizeof(SDL_Window*)); // Need the doube ptr malloc to init within a func
    WindowInit(window);

    SDL_vulkanInstance instance = VK_NULL_HANDLE;
    VkDevice device = nullptr;
    SetupVulkan(window, instance, device);

    MainLoop();

    Cleanup(window, instance, device);

	return 0;
}