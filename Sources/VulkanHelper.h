#include "vk_common.h"

// 
namespace vkh
{
	VkSemaphore createSemaphore(VkDevice pDevice);
	VkFence createFence(VkDevice pDevice);
	VkCommandPool createCommandPool(VkDevice pDevice, uint32_t pFamilyIndex);
	VkImageView createImageView(VkDevice pDevice, VkImage pImage, VkFormat pFormat);
	VkRenderPass createRenderPass(VkDevice pDevice, VkFormat pFormat);
	VkFramebuffer createFramebuffer(VkDevice pDevice, VkRenderPass pRenderPass, VkImageView* imageViews, uint32_t imageViewCount, uint32_t pWidth, uint32_t pHeight);
	VkSampler createTextureSampler(VkDevice pDevice);
}