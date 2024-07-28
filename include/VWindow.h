#pragma once

#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include <SDL.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "VShaders.h"
#include "VFilesystem.h"
#include "VEngineTypes.h"

namespace Vigor
{
	constexpr uint8_t MAX_FRAMES_IN_FLIGHT = 3;

	struct alignas(16) ModelViewProjectionBuffer
	{
		glm::mat4x4 Model;
		glm::mat4x4 View;
		glm::mat4x4 Projection;
	};

	class FrameData
	{
	public: // TODO[CC] Make RAII
		FrameData()
			: commandPool()
			, commandBuffers()
			, commandPoolTransient()
			, imageAvailableSemaphores()
			, renderFinishedSemaphores()
			, inFlightFences()
		{

		}

		void InitCommandPool(VkDevice vkDevice, QueueFamilyIndicies queueFamilyIndicies)
		{
			VkCommandPoolCreateInfo createInfoCommandPool{};
			createInfoCommandPool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			createInfoCommandPool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			createInfoCommandPool.queueFamilyIndex = queueFamilyIndicies.graphicsFamily.value();

			VkResult createCommandPoolRes = vkCreateCommandPool(vkDevice, &createInfoCommandPool, nullptr, &commandPool);
			if (createCommandPoolRes != VK_SUCCESS)
			{
				std::string errorMsg = std::format("Failed to create command pool Error: {}\n\n", (int)createCommandPoolRes);
				throw std::runtime_error(errorMsg);
			}
		}

		void InitCommandBuffers(VkDevice vkDevice)
		{
			commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

			VkCommandBufferAllocateInfo allocInfoCommandBuffer{};
			allocInfoCommandBuffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfoCommandBuffer.commandPool = commandPool;
			allocInfoCommandBuffer.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // primary can be submitted but not called from other buffers, secondary cannot be submitted but can be called from others
			allocInfoCommandBuffer.commandBufferCount = (uint32_t)commandBuffers.size();

			if (vkAllocateCommandBuffers(vkDevice, &allocInfoCommandBuffer, commandBuffers.data()) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to allocate command buffers!");
			}
		}

		void InitCommandPoolTransient(VkDevice vkDevice, QueueFamilyIndicies queueFamilyIndicies)
		{
			VkCommandPoolCreateInfo createInfoCommandPool{};
			createInfoCommandPool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			createInfoCommandPool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
			createInfoCommandPool.queueFamilyIndex = queueFamilyIndicies.graphicsFamily.value();

			VkResult createCommandPoolRes = vkCreateCommandPool(vkDevice, &createInfoCommandPool, nullptr, &commandPoolTransient);
			if (createCommandPoolRes != VK_SUCCESS)
			{
				std::string errorMsg = std::format("Failed to create command pool transient Error: {}\n\n", (int)createCommandPoolRes);
				throw std::runtime_error(errorMsg);
			}
		}

		void InitSyncObjects(VkDevice vkDevice)
		{
			imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
			renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
			inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

			VkSemaphoreCreateInfo createInfoSemaphore{};
			createInfoSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo createInfoFence{};
			createInfoFence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			createInfoFence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				if (vkCreateSemaphore(vkDevice, &createInfoSemaphore, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS)
				{
					throw std::runtime_error("Failed to create imageAvailableSemaphore!");
				}

				if (vkCreateSemaphore(vkDevice, &createInfoSemaphore, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
				{
					throw std::runtime_error("Failed to create renderFinishedSemaphore!");
				}

				if (vkCreateFence(vkDevice, &createInfoFence, nullptr, &inFlightFences[i]) != VK_SUCCESS)
				{
					throw std::runtime_error("Failed to create inFlightFence!");
				}
			}
		}

		void Shutdown(VkDevice vkDevice)
		{
			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				vkDestroySemaphore(vkDevice, imageAvailableSemaphores[i], nullptr);
				vkDestroySemaphore(vkDevice, renderFinishedSemaphores[i], nullptr);
				vkDestroyFence(vkDevice, inFlightFences[i], nullptr);
			}

			vkDestroyCommandPool(vkDevice, commandPool, nullptr);
			vkDestroyCommandPool(vkDevice, commandPoolTransient, nullptr);
		}

	private:
		VkCommandPool commandPool; // allocation and memory management for command buffers
		std::vector<VkCommandBuffer> commandBuffers;

		VkCommandPool commandPoolTransient; // allocation and memory management for command buffers

		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;

		friend class VWindow;
	};

	class VWindow
	{
	public:
		VWindow
		(
			uint16_t _windowWidth = 640,
			uint16_t _windowHeight = 480
		)
			: windowWidth(_windowWidth)
			, windowHeight(_windowHeight)
			, pipelineLayout(VK_NULL_HANDLE)
			, swapChain(VK_NULL_HANDLE)
			, swapChainExtent()
			, swapChainSurfaceFormat()
			, graphicsQueue()
			, presentQueue()
			, renderPass()
			, graphicsPipeline()
			, frameData()
			, descriptorPool()
			, descriptorSetLayout()
			, vkIndexBuffer()
			, vkVertexBuffer()
			, vkIndexBufferMemory()
			, vkVertexBufferMemory()
		{
			window = SDL_CreateWindow("VigorCMD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
			if (window == nullptr)
			{
				throw std::runtime_error(("SDL_CreateWindow Error: %s\n\n", SDL_GetError()));
			}

			sdlWindowId = SDL_GetWindowID(window);
		}

		~VWindow()
		{
			SDL_DestroyWindow(window);
		}

		// Callback's
		void FrameBufferResized(uint16_t _windowWidth, uint16_t _windowHeight)
		{
			windowWidth = _windowWidth;
			windowHeight = _windowHeight;
			bFrameBufferResized = true;
		}

		// Getter's
		/*
		* Get internal SDL window ptr
		*/
		SDL_Window* GetSDLWindow() const
		{
			return window;
		}

		/*
		* Get internal SDL window ID
		*/
		uint32_t GetSDLWindowID() const
		{
			return sdlWindowId;
		}

		/*
		* Get internal VkSurface
		*/
		VkSurfaceKHR& GetSurface()
		{
			return surface;
		}

		/*
		* Getter for frame data
		*/
		FrameData& GetFrameData()
		{
			return frameData;
		}

		// Utils
		static uint32_t FindMemoryType(VkPhysicalDevice vkPhysicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags memoryPropertyFlags)
		{
			/*
			* The VkPhysicalDeviceMemoryProperties structure has two arrays memoryTypes and memoryHeaps.
			* Memory heaps are distinct memory resources like dedicated VRAM and swap space in RAM for when
			*		VRAM runs out
			*/
			VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
			vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &physicalDeviceMemoryProperties);

			for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++)
			{
				if (
					(typeFilter & (1 << i)) && // check available memory type idx against filter
					(physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags))
				{
					return i;
				}
			}

			throw std::runtime_error("Failed to find suitable memory type!");
		}

		static void CreateBuffer(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
		{
			VkBufferCreateInfo createInfoBuffer{};
			createInfoBuffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			createInfoBuffer.size = size;
			createInfoBuffer.usage = usage;
			createInfoBuffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateBuffer(vkDevice, &createInfoBuffer, nullptr, &buffer) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create vertex buffer!");
			}

			VkMemoryRequirements memoryRequirements;
			vkGetBufferMemoryRequirements(vkDevice, buffer, &memoryRequirements);

			VkMemoryAllocateInfo memoryAllocateInfo{};
			memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			memoryAllocateInfo.allocationSize = memoryRequirements.size;
			memoryAllocateInfo.memoryTypeIndex = FindMemoryType
			(
				vkPhysicalDevice,
				memoryRequirements.memoryTypeBits,
				memoryPropertyFlags
			);

			if (vkAllocateMemory(vkDevice, &memoryAllocateInfo, nullptr, &bufferMemory) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to allocate vertex buffer memory!");
			}

			vkBindBufferMemory(vkDevice, buffer, bufferMemory, 0); // bind allocated memory
		}

		static void CreateImage
		(
			VkDevice vkDevice,
			VkPhysicalDevice vkPhysicalDevice,
			uint32_t texWidth,
			uint32_t texHeight,
			uint32_t mipLevels,
			VkFormat format,
			VkImageTiling tiling,
			VkImageUsageFlags usage,
			VkMemoryPropertyFlags properties,
			VkImage& image,
			VkDeviceMemory& imageMemory
		)
		{
			VkImageCreateInfo createInfoImage{};
			createInfoImage.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			createInfoImage.imageType = VK_IMAGE_TYPE_2D;
			createInfoImage.extent.width = static_cast<uint32_t>(texWidth);
			createInfoImage.extent.height = static_cast<uint32_t>(texHeight);
			createInfoImage.extent.depth = 1;
			createInfoImage.mipLevels = 1;
			createInfoImage.arrayLayers = 1;
			createInfoImage.format = format;
			createInfoImage.tiling = tiling;
			createInfoImage.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			createInfoImage.usage = usage;
			createInfoImage.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfoImage.samples = VK_SAMPLE_COUNT_1_BIT;
			createInfoImage.flags = 0; // Optional
			createInfoImage.mipLevels = mipLevels;

			if (vkCreateImage(vkDevice, &createInfoImage, nullptr, &image) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create image!");
			}

			VkMemoryRequirements memoryRequirements;
			vkGetImageMemoryRequirements(vkDevice, image, &memoryRequirements);

			VkMemoryAllocateInfo allocInfoMemory{};
			allocInfoMemory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfoMemory.allocationSize = memoryRequirements.size;
			allocInfoMemory.memoryTypeIndex = FindMemoryType
			(
				vkPhysicalDevice,
				memoryRequirements.memoryTypeBits,
				properties
			);

			if (vkAllocateMemory(vkDevice, &allocInfoMemory, nullptr, &imageMemory))
			{
				throw std::runtime_error("Failed to alloc image memory!");
			}

			vkBindImageMemory(vkDevice, image, imageMemory, 0);
		}

		static VkImageView CreateImageView(VkDevice vkDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
		{
			VkImageViewCreateInfo imageViewCreateInfo{};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = image;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = format;
			imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			imageViewCreateInfo.subresourceRange.levelCount = mipLevels;

			VkImageView imageView;
			if (vkCreateImageView(vkDevice, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create texture image view!");
			}

			return imageView;
		}

		static VkCommandBuffer BeginOneTimeCommands(VkDevice vkDevice, FrameData& frameData)
		{
			VkCommandBufferAllocateInfo allocInfoCommandBuffer{};
			allocInfoCommandBuffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfoCommandBuffer.commandPool = frameData.commandPoolTransient;
			allocInfoCommandBuffer.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // primary can be submitted but not called from other buffers, secondary cannot be submitted but can be called from others
			allocInfoCommandBuffer.commandBufferCount = 1;

			VkCommandBuffer commandBuffer;
			if (vkAllocateCommandBuffers(vkDevice, &allocInfoCommandBuffer, &commandBuffer) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to allocate command buffers transient!");
			}

			VkCommandBufferBeginInfo beginInfoCommandBuffer{};
			beginInfoCommandBuffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfoCommandBuffer.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(commandBuffer, &beginInfoCommandBuffer);

			return commandBuffer;
		}

		static void EndOneTimeCommands(VkDevice vkDevice, VkQueue graphicsQueue, FrameData& frameData, VkCommandBuffer commandBuffer)
		{
			vkEndCommandBuffer(commandBuffer);

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;

			vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
			/*
			* two possible ways to wait on this transfer to complete. We could use a fence and wait with vkWaitForFences,
			*	or simply wait for the transfer queue to become idle with vkQueueWaitIdle. A fence would allow you to schedule
			*	multiple transfers simultaneously and wait for all of them complete, instead of executing one at a time.
			*	That may give the driver more opportunities to optimize.
			*/
			vkQueueWaitIdle(graphicsQueue);
			vkFreeCommandBuffers(vkDevice, frameData.commandPoolTransient, 1, &commandBuffer);
		}

		static void TransitionImageLayout(VkDevice vkDevice, VkQueue graphicsQueue, FrameData& frameData, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
		{
			VkCommandBuffer commandBuffer = BeginOneTimeCommands(vkDevice, frameData);

			VkImageMemoryBarrier imageMemoryBarrier{};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.oldLayout = oldLayout;
			imageMemoryBarrier.newLayout = newLayout;
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.image = image;

			imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
			imageMemoryBarrier.subresourceRange.levelCount = 1;
			imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemoryBarrier.subresourceRange.layerCount = mipLevels;

			imageMemoryBarrier.srcAccessMask = 0; // TODO
			imageMemoryBarrier.dstAccessMask = 0; // TODO

			VkPipelineStageFlags srcStage;
			VkPipelineStageFlags dstStage;

			if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			{
				imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

				if (FormatHasStencilComponent(format))
				{
					imageMemoryBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				}
			}

			if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			{
				imageMemoryBarrier.srcAccessMask = 0;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

				srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			{
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			{
				imageMemoryBarrier.srcAccessMask = 0;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			}
			else
			{
				throw std::invalid_argument("Unsupported layout transition!");
			}

			vkCmdPipelineBarrier
			(
				commandBuffer,
				srcStage, dstStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier
			);

			EndOneTimeCommands(vkDevice, graphicsQueue, frameData, commandBuffer);
		}

		static void CopyBufferToImage(VkDevice vkDevice, VkQueue graphicsQueue, FrameData& frameData, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
		{
			VkCommandBuffer commandBuffer = BeginOneTimeCommands(vkDevice, frameData);

			VkBufferImageCopy bufferImageCopyRegion{};
			bufferImageCopyRegion.bufferOffset = 0;
			bufferImageCopyRegion.bufferRowLength = 0;
			bufferImageCopyRegion.bufferImageHeight = 0;

			bufferImageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferImageCopyRegion.imageSubresource.mipLevel = 0;
			bufferImageCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferImageCopyRegion.imageSubresource.layerCount = 1;

			bufferImageCopyRegion.imageOffset = {0, 0, 0};
			bufferImageCopyRegion.imageExtent = 
			{
				width,
				height,
				1
			};

			vkCmdCopyBufferToImage(
				commandBuffer,
				buffer,
				image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&bufferImageCopyRegion
			);

			EndOneTimeCommands(vkDevice, graphicsQueue, frameData, commandBuffer);
		}

		static VkFormat GetSupportedFormat(VkPhysicalDevice vkPhysicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
		{
			for(VkFormat format : candidates)
			{
				VkFormatProperties formatProperties;
				vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice, format, &formatProperties);

				if (tiling == VK_IMAGE_TILING_LINEAR &&
					(formatProperties.linearTilingFeatures & features) == features)
				{
					return format;
				}

				if (tiling == VK_IMAGE_TILING_OPTIMAL &&
					(formatProperties.optimalTilingFeatures & features) == features)
				{
					return format;
				}
			}

			throw std::runtime_error("Failed to find supported format!");
		}

		static VkFormat GetDepthFormat(VkPhysicalDevice vkPhysicalDevice)
		{
			return GetSupportedFormat
			(
				vkPhysicalDevice,
				{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
				VK_IMAGE_TILING_OPTIMAL,
				VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
			);
		}

		static bool FormatHasStencilComponent(VkFormat format)
		{
			return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
				format == VK_FORMAT_D24_UNORM_S8_UINT;
		}

		/*
		* It is uncommon in practice to generate the mipmap levels at runtime.
		* Usually they are pregenerated and stored in the texture file alongside the base level to improve 
		*	loading speed. Implementing resizing in software and loading multiple levels from a file is TODO
		*/
		static void GenerateMipmaps
		(
			VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice, VkQueue graphicsQueue, FrameData& frameData,
			VkImage image, VkFormat imageFormat, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels
		)
		{
			// Check if image format supports linear blitting
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice, imageFormat, &formatProperties);

			// image created with optimal tiling format, so we check against "optimalTilingFeatures"
			if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			{
				throw std::runtime_error("texture image format does not support linear blitting!");
			}

			VkCommandBuffer commandBuffer = BeginOneTimeCommands(vkDevice, frameData);

			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.levelCount = 1;

			int32_t mipWidth = texWidth;
			int32_t mipHeight = texHeight;

			for (uint32_t i = 1; i < mipLevels; i++)
			{
				barrier.subresourceRange.baseMipLevel = i - 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

				vkCmdPipelineBarrier
				(
					commandBuffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);

				VkImageBlit blit{};
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.mipLevel = i - 1;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.mipLevel = i;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;

				vkCmdBlitImage
				(
					commandBuffer,
					image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &blit,
					VK_FILTER_LINEAR
				);

				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				vkCmdPipelineBarrier
				(
					commandBuffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);

				if (mipWidth > 1) mipWidth /= 2;
				if (mipHeight > 1) mipHeight /= 2;
			}

			barrier.subresourceRange.baseMipLevel = mipLevels - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier
			(
				commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			EndOneTimeCommands(vkDevice, graphicsQueue, frameData, commandBuffer);
		}

		// Initializers
		/*
		* Init Window VK Surface
		*/
		void InitSurface(VkInstance instance)
		{
			if (SDL_Vulkan_CreateSurface(window, instance, &surface) == SDL_FALSE)
			{
				Vigor::Errors::RaiseRuntimeError("Failed to create surface, SDL Error: %s\n\n", SDL_GetError());
			}
		}

		/*
		* Initialize device queues using Vk device
		*/
		void InitDeviceQueues(VkDevice vkDevice, const QueueFamilyIndicies& queueFamilyIndicies)
		{
			vkGetDeviceQueue(vkDevice, queueFamilyIndicies.presentFamily.value(), 0, &presentQueue);
			vkGetDeviceQueue(vkDevice, queueFamilyIndicies.graphicsFamily.value(), 0, &graphicsQueue);
		}

		/*
		* Initialize local swapcahin data
		*/
		void InitSwapChain(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice, SwapChainSupportDetails swapChainSupportDetails, QueueFamilyIndicies queueFamilyIndicies)
		{
			// Choose Swap Chain Extent
			{
				if (swapChainSupportDetails.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
				{
					swapChainExtent = swapChainSupportDetails.capabilities.currentExtent;
				}
				else
				{
					int width, height;
					SDL_Vulkan_GetDrawableSize(window, &width, &height);

					// TODO - The below can be simplified
					swapChainExtent.width = std::clamp<uint32_t>(width, swapChainSupportDetails.capabilities.minImageExtent.width, swapChainSupportDetails.capabilities.maxImageExtent.width);
					swapChainExtent.height = std::clamp<uint32_t>(height, swapChainSupportDetails.capabilities.minImageExtent.height, swapChainSupportDetails.capabilities.maxImageExtent.height);
				}
			}

			// Choose Swap Chain Surface Format
			{
				for (const auto& availableFormat : swapChainSupportDetails.formats)
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

					swapChainSurfaceFormat = availableFormat;
					break;
				}

				swapChainSurfaceFormat = swapChainSupportDetails.formats[0];
			}

			// Choose Swap Present Mode
			VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
			{
				for (const auto& availablePresentMode : swapChainSupportDetails.presentModes)
				{
					bool bIsPresentModeSupported = availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR;
					if (!bIsPresentModeSupported)
					{
						continue;
					}

					presentMode = availablePresentMode;
				}
			}

			uint32_t imageCount = swapChainSupportDetails.capabilities.minImageCount + 1; // sticking to this minimum means that we may sometimes have to wait on the driver to complete internal operations before we can acquire another image to render to. Therefore it is recommended to request at least one more image than the minimum.
			if (swapChainSupportDetails.capabilities.maxImageCount > 0 && imageCount > swapChainSupportDetails.capabilities.maxImageCount)
			{
				imageCount = swapChainSupportDetails.capabilities.maxImageCount;
			}

			VkSwapchainCreateInfoKHR createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = surface;
			createInfo.minImageCount = imageCount;
			createInfo.imageFormat = swapChainSurfaceFormat.format;
			createInfo.imageColorSpace = swapChainSurfaceFormat.colorSpace;
			createInfo.imageExtent = swapChainExtent;
			createInfo.imageArrayLayers = 1;
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			uint32_t queueFamilyIndicesArray[] = { queueFamilyIndicies.graphicsFamily.value(), queueFamilyIndicies.presentFamily.value() };
			if (queueFamilyIndicies.graphicsFamily != queueFamilyIndicies.presentFamily)
			{
				createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				createInfo.queueFamilyIndexCount = 2;
				createInfo.pQueueFamilyIndices = queueFamilyIndicesArray;
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

			if (vkCreateSwapchainKHR(vkDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create swap chain!");
			}

			vkGetSwapchainImagesKHR(vkDevice, swapChain, &imageCount, nullptr);
			swapChainImages.resize(imageCount);
			vkGetSwapchainImagesKHR(vkDevice, swapChain, &imageCount, swapChainImages.data());
		}

		/*
		* Initialize local swapcahin image views
		*/
		void InitImageViews(VkDevice vkDevice)
		{
			swapChainImageViews.resize(swapChainImages.size());

			for (size_t i = 0; i < swapChainImages.size(); i++)
			{
				swapChainImageViews[i] = CreateImageView(vkDevice, swapChainImages[i], swapChainSurfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
			}
		}

		/*
		* Initialize render pass // TODO[CC] Handle Multiple
		*/
		void InitRenderPass(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
		{
			// COLOR
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = swapChainSurfaceFormat.format;
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

			// DEPTH
			VkAttachmentDescription depthAttachment{};
			depthAttachment.format = GetDepthFormat(vkPhysicalDevice);
			depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference depthAttachmentRef{};
			depthAttachmentRef.attachment = 1;
			depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // supports compute subpasses so need to be explicit
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;

			/* -- NOTE --
			* The index of the attachment in this array is directly referenced from the fragment shader with "layout(location = 0) out vec4 outColor" in OpenGL
			*
			* The following other types of attachments can be referenced by a subpass:
			*   - pInputAttachments: Attachments that are read from a shader
			*   - pResolveAttachments: Attachments used for multisampling color attachments
			*   - pDepthStencilAttachment: Attachment for depth and stencil data
			*   - pPreserveAttachments: Attachments that are not used by this subpass, but for which the data must be preserved
			*/

			VkSubpassDependency subpassDependency{};
			// dependant subpass and dependency
			subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			subpassDependency.dstSubpass = 0;
			// what do we wait on
			subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			subpassDependency.srcAccessMask = 0;
			// wait on color attachment stage and the writing of this, dont transition until allowed + necessary
			subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
			VkRenderPassCreateInfo createInfoRenderPass{};
			createInfoRenderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			createInfoRenderPass.attachmentCount = static_cast<uint32_t>(attachments.size());
			createInfoRenderPass.pAttachments = attachments.data();
			createInfoRenderPass.subpassCount = 1;
			createInfoRenderPass.pSubpasses = &subpass;
			// use subpass depenency
			createInfoRenderPass.dependencyCount = 1;
			createInfoRenderPass.pDependencies = &subpassDependency;

			VkResult createRenderPassRes = vkCreateRenderPass(vkDevice, &createInfoRenderPass, nullptr, &renderPass);
			if (createRenderPassRes != VK_SUCCESS)
			{
				std::string errorMsg = std::format("Failed to create render pass Error: {}\n\n", (int)createRenderPassRes);
				throw std::runtime_error(errorMsg);
			}
		}

		/*
		* Initialize Descriptor Set And Layout
		*/
		void InitDescriptorSetLayout(VkDevice vkDevice)
		{
			VkDescriptorSetLayoutBinding mvpBufferLayoutBinding{};
			mvpBufferLayoutBinding.binding = 0;
			mvpBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			mvpBufferLayoutBinding.descriptorCount = 1;
			mvpBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			mvpBufferLayoutBinding.pImmutableSamplers = nullptr;

			VkDescriptorSetLayoutBinding samplerLayoutBinding{};
			samplerLayoutBinding.binding = 1;
			samplerLayoutBinding.descriptorCount = 1;
			samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			samplerLayoutBinding.pImmutableSamplers = nullptr;
			samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			std::array<VkDescriptorSetLayoutBinding, 2> bindings = { mvpBufferLayoutBinding, samplerLayoutBinding };
			VkDescriptorSetLayoutCreateInfo createInfoDescriptorSetLayout{};
			createInfoDescriptorSetLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			createInfoDescriptorSetLayout.bindingCount = static_cast<uint32_t>(bindings.size());
			createInfoDescriptorSetLayout.pBindings = bindings.data();

			if (vkCreateDescriptorSetLayout(vkDevice, &createInfoDescriptorSetLayout, nullptr, &descriptorSetLayout) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create descriptor set layout!");
			}
		}

		/*
		* Initialize Graphics Pipeline And Layou And Shader Modules
		*/
		void InitGraphicsPipelineAndLayoutAndShaderModules(VkDevice vkDevice) // TODO[CC] - split this up
		{
			// Shader Module Setup
			auto VertexShaderCode = Filesystem::Read("./shaders/glsl/vert.spv");
			auto FragmentShaderCode = Filesystem::Read("./shaders/glsl/frag.spv");

			VkShaderModule vertexShaderModule = Shaders::CreateShaderModule(VertexShaderCode, vkDevice);
			VkShaderModule fragmentShaderModule = Shaders::CreateShaderModule(FragmentShaderCode, vkDevice);

			VkPipelineShaderStageCreateInfo createInfoVertexShaderStage{};
			createInfoVertexShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			createInfoVertexShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
			createInfoVertexShaderStage.module = vertexShaderModule;
			createInfoVertexShaderStage.pName = "main";

			VkPipelineShaderStageCreateInfo createInfoFragmentShaderStage{};
			createInfoFragmentShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			createInfoFragmentShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			createInfoFragmentShaderStage.module = fragmentShaderModule;
			createInfoFragmentShaderStage.pName = "main";

			VkPipelineShaderStageCreateInfo shaderStages[] = { createInfoVertexShaderStage, createInfoFragmentShaderStage };

			// Dynamic State Setup
			VkPipelineDynamicStateCreateInfo createInfoDynamicState{};
			createInfoDynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			createInfoDynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()); // TODO - remove cast if possible
			createInfoDynamicState.pDynamicStates = dynamicStates.data();

			// Get Vertex structure values
			auto bindingDescription = Vertex::GetBindingDescription();
			auto attributeDescriptions = Vertex::GetAttributeDescriptions();

			// Vertex Input
			VkPipelineVertexInputStateCreateInfo createInfoVertexInput{};
			createInfoVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			createInfoVertexInput.vertexBindingDescriptionCount = 1;
			createInfoVertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
			createInfoVertexInput.pVertexBindingDescriptions = &bindingDescription;
			createInfoVertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

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
			createInfoRasterizerState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
			createInfoPipelineLayout.setLayoutCount = 1;
			createInfoPipelineLayout.pSetLayouts = &descriptorSetLayout;
			createInfoPipelineLayout.pushConstantRangeCount = 0; // Optional
			createInfoPipelineLayout.pPushConstantRanges = nullptr; // Optional

			if (vkCreatePipelineLayout(vkDevice, &createInfoPipelineLayout, nullptr, &pipelineLayout) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create pipeline layout!");
			}

			VkPipelineDepthStencilStateCreateInfo depthStencil{};
			depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencil.depthTestEnable = VK_TRUE;
			depthStencil.depthWriteEnable = VK_TRUE;
			depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
			depthStencil.depthBoundsTestEnable = VK_FALSE;
			depthStencil.minDepthBounds = 0.0f; // Optional
			depthStencil.maxDepthBounds = 1.0f; // Optional
			depthStencil.stencilTestEnable = VK_FALSE;
			depthStencil.front = {}; // Optional
			depthStencil.back = {}; // Optional

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

			// use depth stencil state for depth testing of fragment shaders for discard
			createInfoGraphicsPipeline.pDepthStencilState = &depthStencil;

			// pipeline cache and batch pipeline creation can also be configured
			if (vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &createInfoGraphicsPipeline, nullptr, &graphicsPipeline) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create graphics pipeline!");
			}

			// Shader Module Cleanup
			vkDestroyShaderModule(vkDevice, vertexShaderModule, nullptr);
			vkDestroyShaderModule(vkDevice, fragmentShaderModule, nullptr);
		}

		/*
		* Initialize Frame Buffers
		*/
		void InitFrameBuffers(VkDevice vkDevice)
		{
			swapChainFramebuffers.resize(swapChainImageViews.size());

			for (size_t i = 0; i < swapChainImageViews.size(); i++)
			{
				std::array<VkImageView, 2> attachments = { swapChainImageViews[i], depthImageView };
				VkFramebufferCreateInfo createInfoFrameBuffer{};
				createInfoFrameBuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				createInfoFrameBuffer.renderPass = renderPass;
				createInfoFrameBuffer.attachmentCount = static_cast<uint32_t>(attachments.size());
				createInfoFrameBuffer.pAttachments = attachments.data();
				createInfoFrameBuffer.width = swapChainExtent.width;
				createInfoFrameBuffer.height = swapChainExtent.height;
				createInfoFrameBuffer.layers = 1;

				VkResult createFrameBufferRes = vkCreateFramebuffer(vkDevice, &createInfoFrameBuffer, nullptr, &swapChainFramebuffers[i]);
				if (createFrameBufferRes != VK_SUCCESS)
				{
					std::string errorMsg = std::format("Failed to create frame buffer Error: {}\n\n", (int)createFrameBufferRes);
					throw std::runtime_error(errorMsg);
				}
			}
		}

		/*
		* Initialize Depth Buffer Resources
		*/
		void InitDepthBufferResources(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
		{
			VkFormat depthFormat = GetDepthFormat(vkPhysicalDevice);
			CreateImage
			(
				vkDevice,
				vkPhysicalDevice,
				swapChainExtent.width,
				swapChainExtent.height,
				1,
				depthFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				depthImage,
				depthImageMemory
			);
			depthImageView = CreateImageView(vkDevice, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

			TransitionImageLayout
			(
				vkDevice,
				graphicsQueue,
				frameData,
				depthImage,
				depthFormat,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				1
			);
		}

		/*
		* Initialize Texture Image
		*/
		void InitTextureImage(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice, const std::string& texturePath)
		{
			int texWidth, texHeight, texChannels;
			stbi_uc* pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
			VkDeviceSize imageSize = texWidth * texHeight * 4;

			if (!pixels)
			{
				throw std::runtime_error("failed to load texture image!");
			}

			mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			CreateBuffer
			(
				vkDevice,
				vkPhysicalDevice,
				imageSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBuffer,
				stagingBufferMemory
			);

			void* data;
			vkMapMemory(vkDevice, stagingBufferMemory, 0, imageSize, 0, &data);
			memcpy(data, pixels, static_cast<size_t>(imageSize));
			vkUnmapMemory(vkDevice, stagingBufferMemory);

			stbi_image_free(pixels);

			CreateImage
			(
				vkDevice,
				vkPhysicalDevice,
				texWidth,
				texHeight,
				mipLevels,
				VK_FORMAT_R8G8B8A8_SRGB,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				textureImage,
				textureImageMemory
			);

			TransitionImageLayout
			(
				vkDevice,
				graphicsQueue,
				frameData,
				textureImage,
				VK_FORMAT_R8G8B8A8_SRGB,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				mipLevels
			);

			//transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

			CopyBufferToImage
			(
				vkDevice,
				graphicsQueue,
				frameData,
				stagingBuffer,
				textureImage,
				static_cast<uint32_t>(texWidth),
				static_cast<uint32_t>(texHeight)
			);

			// Cleanup
			vkDestroyBuffer(vkDevice, stagingBuffer, nullptr);
			vkFreeMemory(vkDevice, stagingBufferMemory, nullptr);

			GenerateMipmaps
			(
				vkDevice,
				vkPhysicalDevice,
				graphicsQueue,
				frameData,
				textureImage,
				VK_FORMAT_R8G8B8A8_SRGB,
				texWidth,
				texHeight,
				mipLevels
			);
		}

		/*
		* Initialize Texture Image View
		*/
		void InitTextureImageView(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
		{
			textureImageView = CreateImageView(vkDevice, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
		}

		/*
		* Initialize Texture Sampler
		*/
		void InitTextureSampler(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
		{
			VkSamplerCreateInfo createInfoSampler{};
			createInfoSampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			createInfoSampler.magFilter = VK_FILTER_LINEAR;
			createInfoSampler.minFilter = VK_FILTER_LINEAR;
			createInfoSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			createInfoSampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			createInfoSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

			// TODO[CC] - remove this and just collect during engine init and use dependancy injection to put query them here
			VkPhysicalDeviceProperties physicalDeviceProperties{};
			vkGetPhysicalDeviceProperties(vkPhysicalDevice, &physicalDeviceProperties);

			createInfoSampler.anisotropyEnable = VK_TRUE;
			createInfoSampler.maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy;

			// Can choose to ignore filtering in the sampler
			//createInfoSampler.anisotropyEnable = VK_FALSE;
			//createInfoSampler.maxAnisotropy = 1.0f;

			createInfoSampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

			// sample in the range of [0, 1)
			createInfoSampler.unnormalizedCoordinates = VK_FALSE;

			createInfoSampler.compareEnable = VK_FALSE; // mainly used for percentage-closer filtering on shadow maps
			createInfoSampler.compareOp = VK_COMPARE_OP_ALWAYS;

			createInfoSampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			createInfoSampler.mipLodBias = 0.0f;
			createInfoSampler.minLod = 0.0f;
			createInfoSampler.maxLod = static_cast<float>(mipLevels);


			if (vkCreateSampler(vkDevice, &createInfoSampler, nullptr, &textureSampler) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create texture sampler!");
			}
		}

		/*
		* Load model
		*/
		void LoadModel(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice, const std::string& modelPath)
		{
			tinyobj::attrib_t attrib;
			std::vector<tinyobj::shape_t> shapes;
			std::vector<tinyobj::material_t> materials;

			std::string warn, err;
			if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str()))
			{
				throw std::runtime_error(warn + err);
			}

			// combined indices/vertex map to filter non-unique verts
			std::unordered_map<Vertex, uint32_t> uniqueVertices{};

			for (const auto& shape : shapes)
			{
				for (const auto& index : shape.mesh.indices)
				{
					Vertex vertex{};

					vertex.pos = 
					{
						attrib.vertices[3 * index.vertex_index + 0],
						attrib.vertices[3 * index.vertex_index + 1],
						attrib.vertices[3 * index.vertex_index + 2]
					};

					// centered
					//vertex.texCoord = 
					//{
					//	attrib.texcoords[2 * index.texcoord_index + 0],
					//	attrib.texcoords[2 * index.texcoord_index + 1]
					//};

					// offset to account for the model
					vertex.texCoord = 
					{
						attrib.texcoords[2 * index.texcoord_index + 0],
						1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
					};

					vertex.color = { 1.0f, 1.0f, 1.0f };

					if (uniqueVertices.count(vertex) == 0)
					{
						uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
						vertices.push_back(vertex);
					}

					indices.push_back(uniqueVertices[vertex]);
				}
			}
		}

		/*
		* Initialize Vertex Buffer
		*/
		void InitVertexBuffer(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
		{
			VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

			// Staging buffer which can have data accessable via CPU
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			CreateBuffer(
				vkDevice,
				vkPhysicalDevice,
				bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBuffer,
				stagingBufferMemory
			);

			// MAP BUFFER MEMORY

			/*
			* Unfortunately the driver may not immediately copy the data into the buffer memory,
			*	for example because of caching. It is also possible that writes to the buffer are not
			*	visible in the mapped memory yet. There are two ways to deal with that problem:
			*
			*		- Use a memory heap that is host coherent, indicated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			*		- Call vkFlushMappedMemoryRanges after writing to the mapped memory,
			*			and call vkInvalidateMappedMemoryRanges before reading from the mapped memory
			*
			* Chosen approach below uses first method, may lead to slightly worse performance than explicit flushing
			*/
			void* bufferData;
			vkMapMemory(vkDevice, stagingBufferMemory, 0, bufferSize, 0, &bufferData);
			memcpy(bufferData, vertices.data(), (size_t)bufferSize);
			vkUnmapMemory(vkDevice, stagingBufferMemory);

			// Vertex buffer where its memory is GPU only and updated via the staging buffer
			CreateBuffer(
				vkDevice,
				vkPhysicalDevice,
				bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vkVertexBuffer,
				vkVertexBufferMemory
			);

			// transfer from staging over to gpu vertex buffer
			CopyBufferData(vkDevice, stagingBuffer, vkVertexBuffer, bufferSize);

			// cleanup staging buffer
			vkDestroyBuffer(vkDevice, stagingBuffer, nullptr);
			vkFreeMemory(vkDevice, stagingBufferMemory, nullptr);
		}

		/*
		* Initialize Index Buffer
		*/
		void InitIndexBuffer(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
		{
			VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			CreateBuffer
			(
				vkDevice,
				vkPhysicalDevice,
				bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBuffer,
				stagingBufferMemory
			);

			void* data;
			vkMapMemory(vkDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
			memcpy(data, indices.data(), (size_t)bufferSize);
			vkUnmapMemory(vkDevice, stagingBufferMemory);

			CreateBuffer
			(
				vkDevice,
				vkPhysicalDevice,
				bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vkIndexBuffer,
				vkIndexBufferMemory
			);

			CopyBufferData(vkDevice, stagingBuffer, vkIndexBuffer, bufferSize);

			vkDestroyBuffer(vkDevice, stagingBuffer, nullptr);
			vkFreeMemory(vkDevice, stagingBufferMemory, nullptr);
		}

		/*
		* Initialize Uniform Buffers
		*/
		void InitUniformBuffers(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
		{
			VkDeviceSize bufferSize = sizeof(ModelViewProjectionBuffer);

			uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
			uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
			uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				CreateBuffer
				(
					vkDevice,
					vkPhysicalDevice,
					bufferSize,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					uniformBuffers[i],
					uniformBuffersMemory[i]
				);
				vkMapMemory(vkDevice, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
			}
		}

		/*
		* Initialize Descriptor Pool
		*/
		void InitDescriptorPool(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
		{
			std::array<VkDescriptorPoolSize, 2> poolSizes{};
			poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
			poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

			VkDescriptorPoolCreateInfo createInfoDescriptorPool{};
			createInfoDescriptorPool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			createInfoDescriptorPool.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
			createInfoDescriptorPool.pPoolSizes = poolSizes.data();
			createInfoDescriptorPool.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

			if (vkCreateDescriptorPool(vkDevice, &createInfoDescriptorPool, nullptr, &descriptorPool) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create descriptor pool!");
			}
		}

		/*
		* Initialize Descriptor Sets
		*/
		void InitDescriptorSets(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
		{
			std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

			VkDescriptorSetAllocateInfo allocInfoDescriptorSet{};
			allocInfoDescriptorSet.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfoDescriptorSet.descriptorPool = descriptorPool;
			allocInfoDescriptorSet.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
			allocInfoDescriptorSet.pSetLayouts = layouts.data();

			descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
			if (vkAllocateDescriptorSets(vkDevice, &allocInfoDescriptorSet, descriptorSets.data()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to allocate descriptor sets!");
			}

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = uniformBuffers[i];
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(ModelViewProjectionBuffer);

				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = textureImageView;
				imageInfo.sampler = textureSampler;

				std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].dstSet = descriptorSets[i];
				descriptorWrites[0].dstBinding = 0;
				descriptorWrites[0].dstArrayElement = 0;
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[0].descriptorCount = 1;
				descriptorWrites[0].pBufferInfo = &bufferInfo;

				descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[1].dstSet = descriptorSets[i];
				descriptorWrites[1].dstBinding = 1;
				descriptorWrites[1].dstArrayElement = 0;
				descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[1].descriptorCount = 1;
				descriptorWrites[1].pImageInfo = &imageInfo;

				vkUpdateDescriptorSets(vkDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
			}
		}

		// Runtime
		void DrawFrame(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice, SwapChainSupportDetails swapChainSupportDetails, QueueFamilyIndicies queueFamilyIndicies)
		{
			vkWaitForFences(vkDevice, 1, &frameData.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX); // wait for previous frame to finish

			// TODO
			uint32_t imageIdx = 0;
			VkResult aquireNextImageRes = vkAcquireNextImageKHR(vkDevice, swapChain, UINT64_MAX, frameData.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIdx);

			if (aquireNextImageRes == VK_ERROR_OUT_OF_DATE_KHR)
			{
				ReInitSwapChain(vkDevice, vkPhysicalDevice, swapChainSupportDetails, queueFamilyIndicies);
				return;
			}
			else if (aquireNextImageRes != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to acquire swap chain image!");
			}

			UpdateConstantBuffer(currentFrame);

			vkResetFences(vkDevice, 1, &frameData.inFlightFences[currentFrame]);

			VkCommandBuffer& commandBuffer = frameData.commandBuffers[currentFrame];
			vkResetCommandBuffer(commandBuffer, 0);

			// Record Command Buffer
			{
				VkCommandBufferBeginInfo beginInfoCommandBuffer{};
				beginInfoCommandBuffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfoCommandBuffer.flags = 0;
				beginInfoCommandBuffer.pInheritanceInfo = nullptr; // specifies which state to inherit from the calling primary command buffers if the buffer is secondary

				VkResult beginCommandBufferRes = vkBeginCommandBuffer(commandBuffer, &beginInfoCommandBuffer);
				if (beginCommandBufferRes != VK_SUCCESS)
				{
					std::string errorMsg = std::format("Failed to begin command buffer Error: {}\n\n", (int)beginCommandBufferRes);
					throw std::runtime_error(errorMsg);
				}

				VkRenderPassBeginInfo beginInfoRenderPass{};
				beginInfoRenderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				beginInfoRenderPass.renderPass = renderPass;
				beginInfoRenderPass.framebuffer = swapChainFramebuffers[imageIdx];

				beginInfoRenderPass.renderArea.offset = { 0, 0 };
				beginInfoRenderPass.renderArea.extent = swapChainExtent;

				// the order of clearValues should be identical to the order of the render pass VkAttachment's
				std::array<VkClearValue, 2> clearValues{};
				clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
				clearValues[1].depthStencil = { 1.0f, 0 };

				beginInfoRenderPass.clearValueCount = static_cast<uint32_t>(clearValues.size());
				beginInfoRenderPass.pClearValues = clearValues.data();

				vkCmdBeginRenderPass(commandBuffer, &beginInfoRenderPass, VK_SUBPASS_CONTENTS_INLINE);
				{
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

					// Setup viewport dynamic
					VkViewport viewport{};
					viewport.x = 0.0f;
					viewport.y = 0.0f;
					viewport.width = (float)(swapChainExtent.width);
					viewport.height = (float)(swapChainExtent.height);
					viewport.minDepth = 0.0f;
					viewport.maxDepth = 1.0f;
					vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

					// Setup scissor dynamic
					VkRect2D scissor{};
					scissor.offset = { 0, 0 };
					scissor.extent = swapChainExtent;
					vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

					// Bind Vertex Buffers
					VkBuffer vertexBuffers[] = { vkVertexBuffer };
					VkDeviceSize offsets[] = { 0 };

					vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(commandBuffer, vkIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

					vkCmdBindDescriptorSets
					(
						commandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						pipelineLayout,
						0,
						1,
						&descriptorSets[currentFrame],
						0,
						nullptr
					);

					vkCmdDrawIndexed
					(
						commandBuffer,
						static_cast<uint32_t>(indices.size()), // idx count
						1, // instance count
						0, // first idx
						0, // vert offset
						0 // first instance
					);
				}
				vkCmdEndRenderPass(commandBuffer);

				VkResult endCommandBufferRes = vkEndCommandBuffer(commandBuffer);
				if (endCommandBufferRes != VK_SUCCESS)
				{
					std::string errorMsg = std::format("Failed to end command buffer Error: {}\n\n", (int)endCommandBufferRes);
					throw std::runtime_error(errorMsg);
				}
			}

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			VkSemaphore waitSemaphores[] = { frameData.imageAvailableSemaphores[currentFrame] };
			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;

			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &frameData.commandBuffers[currentFrame];

			VkSemaphore signalSemaphores[] = { frameData.renderFinishedSemaphores[currentFrame] };
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;

			if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameData.inFlightFences[currentFrame]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to submit command buffer draw!");
			}

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;

			VkSwapchainKHR swapChains[] = { swapChain };
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;
			presentInfo.pImageIndices = &imageIdx;

			presentInfo.pResults = nullptr; // specify an array of VkResult values to check for every individual swap chain if presentation was successful

			VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
			if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || bFrameBufferResized)
			{
				bFrameBufferResized = false;
				ReInitSwapChain(vkDevice, vkPhysicalDevice, swapChainSupportDetails, queueFamilyIndicies);
			}
			else if (presentResult != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to acquire swap chain image!");
			}

			currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT; // set current frame counter to loop when max reached
		}

		void CopyBufferData(VkDevice vkDevice, VkBuffer srcBuff, VkBuffer dstBuff, VkDeviceSize size)
		{
			VkCommandBuffer commandBuffer = BeginOneTimeCommands(vkDevice, frameData);

			// Do the copy
			VkBufferCopy copyBufferRegion{};
			copyBufferRegion.srcOffset = 0; // Optional
			copyBufferRegion.dstOffset = 0; // Optional
			copyBufferRegion.size = size;
			vkCmdCopyBuffer(commandBuffer, srcBuff, dstBuff, 1, &copyBufferRegion);

			EndOneTimeCommands(vkDevice, graphicsQueue, frameData, commandBuffer);
		}

		void UpdateConstantBuffer(uint32_t currentFrame)
		{
			static auto startTime = std::chrono::high_resolution_clock::now();

			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

			ModelViewProjectionBuffer mvpBuffer{};
			mvpBuffer.Model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			mvpBuffer.View = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			mvpBuffer.Projection = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);

			//mvpBuffer.Model = glm::identity<glm::mat4x4>();
			//mvpBuffer.View = glm::identity<glm::mat4x4>();
			//mvpBuffer.Projection = glm::identity<glm::mat4x4>();

			mvpBuffer.Projection[1][1] *= -1; // adjust clip co-ordinates

			memcpy(uniformBuffersMapped[currentFrame], &mvpBuffer, sizeof(mvpBuffer));
		}

		// Shutdown
		void Shutdown(VkInstance vkInstance, VkDevice vkDevice)
		{
			ShutdownSwapChain(vkDevice);

			vkDestroySampler(vkDevice, textureSampler, nullptr);
			vkDestroyImageView(vkDevice, textureImageView, nullptr);

			vkDestroyImage(vkDevice, textureImage, nullptr);
			vkFreeMemory(vkDevice, textureImageMemory, nullptr);

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				vkDestroyBuffer(vkDevice, uniformBuffers[i], nullptr);
				vkFreeMemory(vkDevice, uniformBuffersMemory[i], nullptr);
			}

			vkDestroyDescriptorPool(vkDevice, descriptorPool, nullptr);
			vkDestroyDescriptorSetLayout(vkDevice, descriptorSetLayout, nullptr);

			vkDestroyBuffer(vkDevice, vkIndexBuffer, nullptr);
			vkFreeMemory(vkDevice, vkIndexBufferMemory, nullptr);

			vkDestroyBuffer(vkDevice, vkVertexBuffer, nullptr);
			vkFreeMemory(vkDevice, vkVertexBufferMemory, nullptr);

			vkDestroyPipeline(vkDevice, graphicsPipeline, nullptr);
			vkDestroyPipelineLayout(vkDevice, pipelineLayout, nullptr);
			vkDestroyRenderPass(vkDevice, renderPass, nullptr);

			frameData.Shutdown(vkDevice);

			vkDestroySurfaceKHR(vkInstance, surface, nullptr);
		}

		void ShutdownSwapChain(VkDevice vkDevice)
		{
			// depth buffer cleanup
			vkDestroyImageView(vkDevice, depthImageView, nullptr);
			vkDestroyImage(vkDevice, depthImage, nullptr);
			vkFreeMemory(vkDevice, depthImageMemory, nullptr);

			for (auto framebuffer : swapChainFramebuffers)
			{
				vkDestroyFramebuffer(vkDevice, framebuffer, nullptr);
			}

			for (auto imageView : swapChainImageViews)
			{
				vkDestroyImageView(vkDevice, imageView, nullptr);
			}

			vkDestroySwapchainKHR(vkDevice, swapChain, nullptr);
		}

	private:
		/*
		* Re-initialize swapchains for events that invalidate them like window resize
		*/
		void ReInitSwapChain(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice, SwapChainSupportDetails swapChainSupportDetails, QueueFamilyIndicies queueFamilyIndicies)
		{
			/* !! NOTE !!
			* we don't recreate the renderpass here for simplicity.
			* it can be possible for the swap chain image format to change during an applications' lifetime
			* e.g. when moving a window from an standard range to an high dynamic range monitor.
			* This may require the application to recreate the renderpass to make sure the change
			* 	between dynamic ranges is properly reflected.
			*/

			vkDeviceWaitIdle(vkDevice);

			ShutdownSwapChain(vkDevice);

			InitSwapChain(vkDevice, vkPhysicalDevice, swapChainSupportDetails, queueFamilyIndicies);
			InitImageViews(vkDevice);
			InitDepthBufferResources(vkDevice, vkPhysicalDevice);
			InitFrameBuffers(vkDevice);
		}

	public:
		// Window Settings
		// TODO - are these better off as bitfields?
		bool bHasMouseFocus = false;
		bool bHasKeyboardFocus = false;
		bool bIsFullscreen = false;
		bool bIsMinimized = false;

	private:
		uint32_t sdlWindowId = 0;

		uint16_t windowWidth = 320;
		uint16_t windowHeight = 240;

		SDL_Window* window = nullptr;

		// Surface
		VkSurfaceKHR surface = VK_NULL_HANDLE;

		// Queue's
		VkQueue presentQueue;
		VkQueue graphicsQueue;

		// Swap Chain Data
		VkSwapchainKHR		swapChain;
		VkExtent2D			swapChainExtent;
		VkSurfaceFormatKHR	swapChainSurfaceFormat;

		std::vector<VkImage>		swapChainImages;
		std::vector<VkImageView>	swapChainImageViews;
		std::vector<VkFramebuffer>	swapChainFramebuffers;

		// Render Pass and Pipelines - TODO[CC] Enable multiple/re-use
		VkRenderPass renderPass;

		VkPipeline graphicsPipeline;
		VkPipelineLayout pipelineLayout;

		// Frame Data - TODO[CC] Enable multiple
		FrameData frameData;
		uint32_t currentFrame = 0;
		bool bFrameBufferResized = false;

		// Dynamic states for window
		std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		// TODO[CC] these are here for in-dev, create functionality to collect verts and attr's from "imported" meshes etc. for the scene to display
		std::vector<Vertex> vertices;

		// contents of index buffer
		std::vector<uint32_t> indices;

		// TODO[CC] support multiple
		VkBuffer vkVertexBuffer;
		VkDeviceMemory vkVertexBufferMemory;

		VkBuffer vkIndexBuffer;
		VkDeviceMemory vkIndexBufferMemory;

		/* !! NOTE !!
		* the memory type that allows us to access it from the CPU may not be the most optimal memory type for the graphics card
		*	to read from. The most optimal memory has the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT flag and is usually not accessible
		*	by the CPU on dedicated graphics cards. In this chapter we're going to create two vertex buffers.
		*	One staging buffer in CPU accessible memory to upload the data from the vertex array to, and the final vertex buffer
		*	in device local memory. We'll then use a buffer copy command to move the data from the staging buffer to the actual
		*	vertex buffer.
		*/

		std::vector<VkBuffer> uniformBuffers;
		std::vector<VkDeviceMemory> uniformBuffersMemory;
		std::vector<void*> uniformBuffersMapped;

		// Texture sampling
		uint32_t mipLevels;
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
		VkImageView textureImageView;
		VkSampler textureSampler;

		// Depth Buffering
		VkImage depthImage;
		VkDeviceMemory depthImageMemory;
		VkImageView depthImageView;

		// Descriptor data
		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> descriptorSets;
		VkDescriptorSetLayout descriptorSetLayout;
	};
}