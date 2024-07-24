#pragma once

#include <set>
#include <memory>
#include <vector>
#include <cassert>
#include <iostream>
#include <optional>
#include <algorithm>
#include <functional>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "VErrors.h"
#include "VWindow.h"
#include "VUtilities.h"
#include "VDefinitions.h"
#include "VEngineTypes.h"

namespace Vigor
{
	class VEngine
	{
	public:
		VEngine(uint8_t _windowCount = 1)
			: windowCount(_windowCount)
		{
			// TODO[CC] Initialize with delegates for more elegant setup

			InitSDL();

			InitWindows();

			InitVkInstance();

			InitWindowSurfaces();

			InitVKPhysicalDevice();

			InitQueueFamilies();

			InitLogicalDevice();

			// Handle Frame Data
			for (auto& window : windows)
			{
				window->InitSwapChain(vkDevice, vkPhysicalDevice, swapChainSupportDetails, queueFamilyIndicies);
				window->InitImageViews(vkDevice);
				window->InitRenderPass(vkDevice, vkPhysicalDevice);
				window->InitDescriptorSetLayout(vkDevice);
				window->InitGraphicsPipelineAndLayoutAndShaderModules(vkDevice);

				FrameData& frameData = window->GetFrameData();

				frameData.InitCommandPool(vkDevice, queueFamilyIndicies);
				frameData.InitCommandPoolTransient(vkDevice, queueFamilyIndicies);

				window->InitDepthBufferResources(vkDevice, vkPhysicalDevice);
				window->InitFrameBuffers(vkDevice);
				window->InitTextureImage(vkDevice, vkPhysicalDevice);
				window->InitTextureImageView(vkDevice, vkPhysicalDevice);
				window->InitTextureSampler(vkDevice, vkPhysicalDevice);
				window->InitVertexBuffer(vkDevice, vkPhysicalDevice); // HANDLE VERTEX BUFFER INIT
				window->InitIndexBuffer(vkDevice, vkPhysicalDevice); // HANDLE INDEX BUFFER INIT
				window->InitUniformBuffers(vkDevice, vkPhysicalDevice);
				window->InitDescriptorPool(vkDevice, vkPhysicalDevice);
				window->InitDescriptorSets(vkDevice, vkPhysicalDevice);

				frameData.InitCommandBuffers(vkDevice);
				frameData.InitSyncObjects(vkDevice);
			}
		}

		~VEngine()
		{
			ShutdownWindows();
			windows.clear();

			vkDestroyDevice(vkDevice, nullptr);

#if VULKAN_VALIDATION_LAYERS_ENABLED
			//DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr); TODO[CC]
#endif

			vkDestroyInstance(vkInstance, nullptr);

			SDL_Vulkan_UnloadLibrary();
			SDL_Quit();

			SDL_Log("Cleaned up with errors: %s", SDL_GetError());
		}

		// Runtime
		void Run()
		{
			// Main Loop
			// TODO[CC] Handle individual window closure
			bool running = true;
			while (running)
			{
				SDL_Event windowEvent;
				while (SDL_PollEvent(&windowEvent))
				{
					if (windowEvent.type == SDL_WINDOWEVENT)
					{
						auto const& itWindow = std::find_if(windows.begin(), windows.end(), [&windowEvent = windowEvent](std::unique_ptr<VWindow>& window) { return window->GetSDLWindowID() == windowEvent.window.windowID; });
						if (itWindow != windows.end())
						{
							// Found window, do stuff

							switch (windowEvent.window.event)
							{
							case SDL_WINDOWEVENT_CLOSE:
								(*itWindow)->Shutdown(vkInstance, vkDevice);
								windows.erase(itWindow);
								break;
							case SDL_WINDOWEVENT_MINIMIZED:
								(*itWindow)->bIsMinimized = true;
								break;
							case SDL_WINDOWEVENT_RESTORED:
								(*itWindow)->bIsMinimized = false;
								break;
							case SDL_WINDOWEVENT_SIZE_CHANGED:
								(*itWindow)->FrameBufferResized(
									windowEvent.window.data1, // width
									windowEvent.window.data2 // height
								);
									break;
							default:
								break;
							}
						}
					}

					if (windowEvent.type == SDL_QUIT)
					{
						running = false;
						break;
					}
				}

				// TODO[CC] make 1 line-r
				for (auto& window : windows)
				{
					if (!window->bIsMinimized)
					{
						window->DrawFrame(vkDevice, vkPhysicalDevice, swapChainSupportDetails, queueFamilyIndicies);
					}
				}
			}

			vkDeviceWaitIdle(vkDevice);
		}

	private:
		// Initialization
		void InitSDL()
		{
			int initError = 0;
			initError = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
			if (initError < 0)
			{
				throw std::runtime_error(("SDL_Init Error: %d, %s\n\n", initError, SDL_GetError()));
			}
		}

		void InitWindows()
		{
			assert(windowCount > 0);
			for (size_t i = 0; i < windowCount; i++)
			{
				windows.push_back(std::make_unique<VWindow>());
			}
		}

		void InitVkInstance()
		{
			SDL_Window* firstWindowPtr = windows[0]->GetSDLWindow();
			VkApplicationInfo appInfo{}; // !! YOU NEED TO DEFAULT INIT LIKE THIS !!
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.apiVersion = VK_API_VERSION_1_3;
			appInfo.pApplicationName = "VigorCMD";
			appInfo.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
			appInfo.pEngineName = "Vigor";
			appInfo.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);

			unsigned int extensionCount = 0;
			SDL_Vulkan_GetInstanceExtensions(firstWindowPtr, &extensionCount, NULL);
			std::vector<const char*> extentionNames(extensionCount);
			SDL_Vulkan_GetInstanceExtensions(firstWindowPtr, &extensionCount, extentionNames.data());

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
			dbgReportCallbackCInfo.pfnCallback = Vigor::VkValidationLayerCallback;
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

#if VULKAN_VALIDATION_LAYERS_ENABLED
			// This line here enables validation layer reporting for instance creation and destruction.
			// Normally you must setup your own reporting object but that requires an instance and thus won't be used for instance create/destroy.
			// If we point this list to a debug report 'create info' then the API will do reporting for instance create/destroy accordingly.
			instInfo.pNext = &dbgReportCallbackCInfo;

			// Use the setup layer names
			instInfo.ppEnabledLayerNames = validationLayerNames.data();
			instInfo.enabledLayerCount = (uint32_t)validationLayerNames.size();
#endif // VULKAN_VALIDATION_LAYERS_ENABLED

			VkResult instanceCreationResult = vkCreateInstance(&instInfo, NULL, &vkInstance);
			if (instanceCreationResult != VK_SUCCESS)
			{
				Vigor::Errors::RaiseRuntimeError("Failed to create instance Error: {}\n\n", (int)instanceCreationResult);
			}
		}

		void InitWindowSurfaces()
		{
			// TODO[CC] make 1 line-r
			for (auto& window : windows)
			{
				window->InitSurface(vkInstance);
			}
		}

		void InitVKPhysicalDevice()
		{
			uint32_t physicalDeviceCount = 0;
			vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr);

			if (physicalDeviceCount == 0)
			{
				Vigor::Errors::RaiseRuntimeError("Cannot find Physical Device with VK support");
			}

			std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
			vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, physicalDevices.data());

			auto IsDeviceSuitable = [&deviceExtensions = deviceExtensions, &swapChainSupportDetails = swapChainSupportDetails](const VkPhysicalDevice& vkPhysicalDevice, const VkSurfaceKHR& surface)
				{
					VkPhysicalDeviceProperties physicalDeviceProperties;
					vkGetPhysicalDeviceProperties(vkPhysicalDevice, &physicalDeviceProperties);

					VkPhysicalDeviceFeatures physicalDeviceFeatures;
					vkGetPhysicalDeviceFeatures(vkPhysicalDevice, &physicalDeviceFeatures);

					// Force dedicated GPU requirement
					// TODO[CC] Do this more elegantly
					bool bResult = (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

					// Force Geo-shader AND anisotropic filtering requirements
					if (physicalDeviceFeatures.geometryShader != VK_TRUE || physicalDeviceFeatures.samplerAnisotropy != VK_TRUE) // TODO[CC] Do this more elegantly
					{
						return false;
					}

					// CheckDeviceExtensionSupport
					if (bResult) // TODO[CC] Do this more elegantly
					{
						uint32_t extensionCount = 0;
						vkEnumerateDeviceExtensionProperties(vkPhysicalDevice, nullptr, &extensionCount, nullptr);

						std::vector<VkExtensionProperties> availableExtensions(extensionCount);
						vkEnumerateDeviceExtensionProperties(vkPhysicalDevice, nullptr, &extensionCount, availableExtensions.data());

						std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
						for (const auto& availableExtension : availableExtensions)
						{
							requiredExtensions.erase(availableExtension.extensionName);
						}

						bResult = requiredExtensions.empty();
					}

					// QuerySwapChainSupport
					bool bSwapchainAcceptable = false;
					if (bResult) // TODO[CC] Do this more elegantly
					{
						// Capabilities
						vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice, surface, &swapChainSupportDetails.capabilities);

						// Formats
						uint32_t formatCount;
						vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, surface, &formatCount, nullptr);

						if (formatCount != 0)
						{
							swapChainSupportDetails.formats.resize(formatCount);
							vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, surface, &formatCount, swapChainSupportDetails.formats.data());
						}

						// Present Modes
						uint32_t presentModeCount;
						vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice, surface, &presentModeCount, nullptr);

						if (presentModeCount != 0)
						{
							swapChainSupportDetails.presentModes.resize(presentModeCount);
							vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice, surface, &presentModeCount, swapChainSupportDetails.presentModes.data());
						}

						bResult = swapChainSupportDetails.IsComplete();
					}

					return bResult;
				};

			for (const auto& availablePhysicalDevice : physicalDevices)
			{
				if (!IsDeviceSuitable(availablePhysicalDevice, windows[0]->GetSurface()))
				{
					continue;
				}

				vkPhysicalDevice = availablePhysicalDevice;
				break;
			}

			if (vkPhysicalDevice == VK_NULL_HANDLE)
			{
				Vigor::Errors::RaiseRuntimeError("Cannot find suitable Physical Device");
			}
		}

		void InitQueueFamilies()
		{
			uint32_t queueFamilyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount, nullptr);

			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount, queueFamilies.data());

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
					vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice, i, windows[0]->GetSurface(), &support);
					if (support)
					{
						queueFamilyIndicies.presentFamily = i;
					}
				}

				++i;
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
			deviceFeatures.samplerAnisotropy = VK_TRUE; // enable anisotropic filtering support on samplers
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

			if (vkCreateDevice(vkPhysicalDevice, &deviceCreateInfo, nullptr, &vkDevice) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create Logical VK Device");
			}

			// TODO[CC] make 1 line-r
			for (auto& window : windows)
			{
				window->InitDeviceQueues(vkDevice, queueFamilyIndicies);
			}

			SDL_Log("Initialized with errors: %s", SDL_GetError());
		}

		// Shutdown
		void ShutdownWindows()
		{
			// TODO[CC] make 1 line-r
			for (auto& window : windows)
			{
				window->Shutdown(vkInstance, vkDevice);
			}
		}

	private:
		uint8_t windowCount = 1;

		VkInstance vkInstance;

		VkDevice vkDevice;
		VkPhysicalDevice vkPhysicalDevice;

		std::vector<std::unique_ptr<VWindow>> windows; // allow for multiple windows

		QueueFamilyIndicies queueFamilyIndicies;
		SwapChainSupportDetails swapChainSupportDetails;

		std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#if VULKAN_VALIDATION_LAYERS_ENABLED
		std::vector<const char*> validationLayerNames;
#endif // VULKAN_VALIDATION_LAYERS_ENABLED
	};
}