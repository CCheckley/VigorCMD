#include <iostream>
#include <vector>
#include <string>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define GENERAL_ERROR 666
#define WINDOW_CREATE_ERROR 901
#define INSTANCE_CREATE_ERROR 902
#define SURFACE_CREATE_ERROR 903

#define VULKAN_VALIDATION_LAYERS_ENABLED true

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

int main(int argc, char* argv[]) {
	//SDL_Init(SDL_INIT_EVERYTHING);
    
	SDL_Window* window = nullptr;

	int initError = 0;
	initError = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	if (initError < 0) {
		std::cout << "VIDEO INIT ERROR: " << SDL_GetError() << std::endl;
		return initError;
	}

	window = SDL_CreateWindow("VigorCMD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
	if (window == nullptr) {
		std::cout << "WINDOW CREATION ERROR: " << SDL_GetError() << std::endl;
		return WINDOW_CREATE_ERROR;
	}

    VkApplicationInfo appInfo{}; // !! YOU NEED TO DEFAULT INIT LIKE THIS !!
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pApplicationName = "test-app";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
    appInfo.pEngineName = "test-engine";
    appInfo.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);

    unsigned int count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &count, NULL);
    std::vector<const char*> extentionNames;
    extentionNames.resize(count);
    SDL_Vulkan_GetInstanceExtensions(window, &count, extentionNames.data());

    for (unsigned int i = 0; i < count; i++)
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
    VkInstanceCreateInfo createInfo{}; // !! YOU NEED TO DEFAULT INIT LIKE THIS !!
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.ppEnabledExtensionNames = extentionNames.data();
    createInfo.enabledExtensionCount = (uint32_t)extentionNames.size();
    createInfo.ppEnabledLayerNames = reqLayerNames.data();
    createInfo.enabledLayerCount = (uint32_t) reqLayerNames.size();

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
        
        createInfo.pNext = &dbgReportCallbackCInfo;
    }

    SDL_vulkanInstance instance = VK_NULL_HANDLE;
    if (0 > vkCreateInstance(&createInfo, NULL, &instance))
    {
        printf("failed to create instance\n");
    }

    SDL_vulkanSurface surface = VK_NULL_HANDLE;
    if (SDL_FALSE == SDL_Vulkan_CreateSurface(window, instance, &surface))
    {
        printf("failed to create surface, SDL Error: %s", SDL_GetError());
    }

	SDL_UpdateWindowSurface(window);
	SDL_Delay(1000);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}