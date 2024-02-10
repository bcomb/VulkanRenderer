#include "vk_common.h"

#include <vector>

// Forward declaration
struct VulkanSwapchain;

// Helper functions
namespace vkh
{
	// Some initializers helpers
	VkCommandPoolCreateInfo commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
	VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);
	VkFramebufferCreateInfo framebufferCreateInfo(VkRenderPass renderPass, VkExtent2D extent);
	VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);
	VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);
	VkPresentInfoKHR presentInfo();
	VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
	VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);
	VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);

	// Pipeline helpers
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(const VkDescriptorSetLayout* pSetLayouts, uint32_t pSetLayoutCount);
	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char* pEntryName = "main");
	VkComputePipelineCreateInfo computePipelineCreateInfo(VkPipelineLayout layout, VkPipelineShaderStageCreateInfo shaderStageInfo);

	// Image helpers
	VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask);
	VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
	VkImageViewCreateInfo imageViewCreateInfo(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

	void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);


	// Some create helpers
	VkSemaphore createSemaphore(VkDevice pDevice);
	VkFence createFence(VkDevice pDevice, uint32_t pFlags = VK_FENCE_CREATE_SIGNALED_BIT);
	VkCommandPool createCommandPool(VkDevice pDevice, uint32_t pFamilyIndex, VkCommandPoolCreateFlags pFlags);
	VkImageView createImageView(VkDevice pDevice, VkImage pImage, VkFormat pFormat, VkImageAspectFlags pAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
	VkRenderPass createRenderPass(VkDevice pDevice, VkFormat pFormat);
	VkFramebuffer createFramebuffer(VkDevice pDevice, VkRenderPass pRenderPass, VkImageView* imageViews, uint32_t imageViewCount, uint32_t pWidth, uint32_t pHeight);
	VkSampler createTextureSampler(VkDevice pDevice);
	std::vector<VkFramebuffer> createSwapchainFramebuffer(VkDevice pDevice, VkRenderPass pRenderPass, const VulkanSwapchain& pSwapchain);	// No more used with Vulkan 1.3

	
	// Barrier helpers
	// VK_KHR_synchronization2 : https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
	void transitionImage(VkCommandBuffer cmd, VkImage pImage, VkImageLayout oldLayout, VkImageLayout newLayout);
    //VkBufferMemoryBarrier bufferBarrier(VkBuffer pBuffer, VkAccessFlags pSrcAccessMask, VkAccessFlags pDstAccessMask);
}