#include "VulkanHelper.h"

#include <assert.h>

namespace vkh
{
/******************************************************************************/
VkSemaphore createSemaphore(VkDevice pDevice)
{
	VkSemaphoreCreateInfo lSemaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkSemaphore lSemaphore;
	VK_CHECK(vkCreateSemaphore(pDevice, &lSemaphoreCreateInfo, nullptr, &lSemaphore));

	return lSemaphore;
}

/******************************************************************************/
VkFence createFence(VkDevice pDevice)
{
	VkFenceCreateInfo createInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence fence;
	vkCreateFence(pDevice, &createInfo, nullptr, &fence);
	return fence;
}

/******************************************************************************/
VkCommandPool createCommandPool(VkDevice pDevice, uint32_t pFamilyIndex)
{
	VkCommandPoolCreateInfo lCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	lCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	lCreateInfo.queueFamilyIndex = pFamilyIndex;
	VkCommandPool lCommandPool;
	VK_CHECK(vkCreateCommandPool(pDevice, &lCreateInfo, nullptr, &lCommandPool));
	return lCommandPool;
}

/******************************************************************************/
VkImageView createImageView(VkDevice pDevice, VkImage pImage, VkFormat pFormat)
{
	VkImageViewCreateInfo lImageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	//VkImageViewCreateFlags     flags;
	lImageViewCreateInfo.image = pImage;
	lImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	lImageViewCreateInfo.format = pFormat;
	lImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	lImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	lImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	lImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	lImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	lImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	lImageViewCreateInfo.subresourceRange.levelCount = 1;
	lImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	lImageViewCreateInfo.subresourceRange.layerCount = 1;
	VkImageView imageView;
	VK_CHECK(vkCreateImageView(pDevice, &lImageViewCreateInfo, nullptr, &imageView));
	return imageView;
}

/******************************************************************************/
VkRenderPass createRenderPass(VkDevice pDevice, VkFormat pFormat)
{
	VkAttachmentDescription attachmentDesc[1] = {};
	attachmentDesc[0].flags;
	attachmentDesc[0].format = pFormat; // swapChainImageFormat
	attachmentDesc[0].samples = VK_SAMPLE_COUNT_1_BIT; // no msaa
	attachmentDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//Using VK_IMAGE_LAYOUT_UNDEFINED for initialLayout means that we don't care
	// what previous layout the image was in. The caveat of this special value is 
	// that the contents of the image are not guaranteed to be preserved,
	// but that doesn't matter since we're going to clear it anyway
	attachmentDesc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//attachmentDesc[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // require a imageBarrier layout transition before vkCmdBeginRenderPass
	attachmentDesc[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;	// Index in VkRenderPassCreateInfo::pAttachments
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDesc = {};
	//VkSubpassDescriptionFlags       flags;
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	//subpassDesc.inputAttachmentCount = 1;	// Attachments that are read from a shader
	//subpassDesc.pInputAttachments = &colorAttachmentRef;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorAttachmentRef;
	//subpassDesc.pResolveAttachments;	// Attachments used for multisampling color attachments
	//subpassDesc.pDepthStencilAttachment;
	//uint32_t                        preserveAttachmentCount;	// Attachments that are not used by this subpass, but for which the data must be preserved
	//const uint32_t* pPreserveAttachments;

	// Explicit dependency

	// How to fill this to be ready for presentation?
	// No need this for only one pass
	VkSubpassDependency dependencies[2] = { };
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// implicitly defined dependency would cover this, but let's replace it with this explicitly defined dependency!
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = 0;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	//VkRenderPassCreateFlags           flags;
	createInfo.attachmentCount = 1;
	createInfo.pAttachments = attachmentDesc;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpassDesc;
	createInfo.dependencyCount = ARRAY_COUNT(dependencies);
	createInfo.pDependencies = dependencies;

	VkRenderPass renderPass;
	VK_CHECK(vkCreateRenderPass(pDevice, &createInfo, nullptr, &renderPass));

	return renderPass;
}

/******************************************************************************/
VkFramebuffer createFramebuffer(VkDevice pDevice, VkRenderPass pRenderPass, VkImageView* imageViews, uint32_t imageViewCount, uint32_t pWidth, uint32_t pHeight)
{
	VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	//VkFramebufferCreateFlags    flags;
	createInfo.renderPass = pRenderPass;
	createInfo.attachmentCount = imageViewCount;
	createInfo.pAttachments = imageViews;
	createInfo.width = pWidth;
	createInfo.height = pHeight;
	createInfo.layers = 1;

	VkFramebuffer framebuffer;
	VK_CHECK(vkCreateFramebuffer(pDevice, &createInfo, nullptr, &framebuffer));

	return framebuffer;
}

/******************************************************************************/
VkSampler createTextureSampler(VkDevice pDevice)
{
	VkSamplerCreateInfo createInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	//VkStructureType         sType;
	//const void* pNext;
	//VkSamplerCreateFlags    flags = 0; // osef? VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT if image was create with this flag (dunno what is it)
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; // have to generate it manually
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.mipLodBias = 0.0f;
	createInfo.anisotropyEnable = VK_FALSE; // to use this need to enable the feature on the logical device
	createInfo.maxAnisotropy = 0.0f;
	createInfo.compareEnable = VK_FALSE; // Depth compare for depth map test i suppose
	createInfo.compareOp = VK_COMPARE_OP_NEVER;
	createInfo.minLod = 0.0f;
	createInfo.maxLod = 0.0f;
	createInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK; // INT/FLOAT? maybe should be a good idea to pass VK_FORMAT of the image to auto this
	createInfo.unnormalizedCoordinates = VK_FALSE;

	VkSampler sampler;
	VK_CHECK(vkCreateSampler(pDevice, &createInfo, NULL, &sampler));
	return sampler;
}

} // ns vkh