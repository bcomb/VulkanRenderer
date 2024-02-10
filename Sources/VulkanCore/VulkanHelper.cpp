#include "VulkanHelper.h"
#include "VulkanSwapchain.h"

#include <assert.h>

namespace vkh
{
/******************************************************************************/
void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize)
{
	// Vulkan has 2 main ways of copying one image to another.
	// you can use VkCmdCopyImage or VkCmdBlitImage.
	// CopyImage is faster, but its much more restricted, for example the resolution on both images must match.
	// Meanwhile, blit image lets you copy images of different formats and different sizes into one another.
	// You have a source rectangle and a target rectangle, and the system copies it into its position.
	// Those two functions are useful when setting up the engine, but later its best to ignore them and 
	// write your own version that can do extra logic on a fullscreen fragment shader.
	VkImageBlit2 blitRegion = { VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
	blitRegion.srcOffsets[1].x = srcSize.width;
	blitRegion.srcOffsets[1].y = srcSize.height;
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstOffsets[1].x = dstSize.width;
	blitRegion.dstOffsets[1].y = dstSize.height;
	blitRegion.dstOffsets[1].z = 1;

	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;

	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blitInfo = { VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2 };
	blitInfo.dstImage = destination;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.srcImage = source;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.filter = VK_FILTER_LINEAR;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;

	vkCmdBlitImage2(cmd, &blitInfo);
}

/******************************************************************************/
VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask)
{
	VkImageSubresourceRange subImage = {};
	subImage.aspectMask = aspectMask;
	subImage.baseMipLevel = 0;
	subImage.levelCount = VK_REMAINING_MIP_LEVELS;
	subImage.baseArrayLayer = 0;
	subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;
	return subImage;
}

/******************************************************************************/
void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier2 imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	imageBarrier.pNext = nullptr;

	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

	imageBarrier.oldLayout = currentLayout;
	imageBarrier.newLayout = newLayout;

	VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange = imageSubresourceRange(aspectMask);
	imageBarrier.image = image;

	VkDependencyInfo depInfo = {};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.pNext = nullptr;

	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

/******************************************************************************/
VkRenderingAttachmentInfo renderingAttachmentInfo(VkImageView pView, VkImageLayout pLayout, VkClearValue* pClearValue)
{
	VkRenderingAttachmentInfo lInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	lInfo.imageView = pView;
	lInfo.imageLayout = pLayout;
	//VkResolveModeFlagBits    resolveMode;				// MSAA render target mode
	//VkImageView              resolveImageView;
	//VkImageLayout            resolveImageLayout;
	lInfo.loadOp = pClearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	lInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	if(pClearValue)
		lInfo.clearValue = *pClearValue;

	return lInfo;
}

/******************************************************************************/
VkRenderingInfo renderingInfo(const VkRect2D& pRenderArea, const VkRenderingAttachmentInfo* pColorAttachments, const VkRenderingAttachmentInfo* pDepthAttachment, const VkRenderingAttachmentInfo* pStencilAttachment)
{
	VkRenderingInfo lInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
	//VkRenderingFlags                    flags;
	lInfo.renderArea = pRenderArea;
	lInfo.layerCount = 1;
	//uint32_t                            viewMask;
	lInfo.colorAttachmentCount = 1;
	lInfo.pColorAttachments = pColorAttachments;
	lInfo.pDepthAttachment = pDepthAttachment;
	lInfo.pStencilAttachment = pStencilAttachment;
	return lInfo;
}

/******************************************************************************/
VkPushConstantRange pushConstantRange(VkShaderStageFlags pShaderStageFlags, uint32_t pSize, uint32_t pOffset)
{
	VkPushConstantRange lConstantRange = {};
	lConstantRange.stageFlags = pShaderStageFlags;
	lConstantRange.offset = pOffset;
	lConstantRange.size = pSize;
	return lConstantRange;
}

/******************************************************************************/
VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(const VkDescriptorSetLayout* pSetLayouts, uint32_t pSetLayoutCount, const VkPushConstantRange* pPushConstantRanges, uint32_t pPushConstantRangeCount)
{
	VkPipelineLayoutCreateInfo lCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    //VkPipelineLayoutCreateFlags        flags;
    lCreateInfo.setLayoutCount = pSetLayoutCount;
    lCreateInfo.pSetLayouts = pSetLayouts;
    lCreateInfo.pushConstantRangeCount = pPushConstantRangeCount;
    lCreateInfo.pPushConstantRanges = pPushConstantRanges;
    return lCreateInfo;
}

/******************************************************************************/
VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char* pEntryName)
{
	VkPipelineShaderStageCreateInfo lInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
//	VkPipelineShaderStageCreateFlags    flags;
	lInfo.stage = stage;
	lInfo.module = shaderModule;
	lInfo.pName = pEntryName;
	//const VkSpecializationInfo* pSpecializationInfo; // What is it?

	return lInfo;
}

/******************************************************************************/
VkComputePipelineCreateInfo computePipelineCreateInfo(VkPipelineLayout layout, VkPipelineShaderStageCreateInfo shaderStageInfo)
{
    VkComputePipelineCreateInfo lInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	//VkPipelineCreateFlags              flags;
	lInfo.stage = shaderStageInfo;
	lInfo.layout = layout;
	//VkPipeline                         basePipelineHandle;
	//int32_t                            basePipelineIndex;

    return lInfo;
}

/******************************************************************************/
VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
	VkSemaphoreSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.semaphore = semaphore;
	submitInfo.stageMask = stageMask;
	submitInfo.deviceIndex = 0;
	submitInfo.value = 1;

	return submitInfo;
}

/******************************************************************************/
VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd)
{
	VkCommandBufferSubmitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = nullptr;
	info.commandBuffer = cmd;
	info.deviceMask = 0;

	return info;
}

/******************************************************************************/
VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo)
{
	VkSubmitInfo2 info = {};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	info.pNext = nullptr;
	info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
	info.pWaitSemaphoreInfos = waitSemaphoreInfo;
	info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
	info.pSignalSemaphoreInfos = signalSemaphoreInfo;
	info.commandBufferInfoCount = 1;
	info.pCommandBufferInfos = cmd;
	return info;
}

/******************************************************************************/
VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent)
{
	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;

	info.imageType = VK_IMAGE_TYPE_2D;

	info.format = format;
	info.extent = extent;

	info.mipLevels = 1;
	info.arrayLayers = 1;

	//for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
	info.samples = VK_SAMPLE_COUNT_1_BIT;

	//optimal tiling, which means the image is stored on the best gpu format
	info.tiling = VK_IMAGE_TILING_OPTIMAL;			// VK_IMAGE_TILING_LINEAR is for CPU access as 2D Array linear data (for readback?)
	info.usage = usageFlags;

	return info;
}

/******************************************************************************/
VkImageViewCreateInfo imageViewCreateInfo(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	// build a image-view for the depth image to use for rendering
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;

	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.image = image;
	info.format = format;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.aspectMask = aspectFlags;

	return info;
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
VkCommandPoolCreateInfo commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags /*= 0*/)
{
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	info.queueFamilyIndex = queueFamilyIndex;
	return info;
}

/******************************************************************************/
VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count /*= 1*/, VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/)
{
	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.pNext = nullptr;
	info.commandPool = pool;
	info.commandBufferCount = count;
	info.level = level;
	return info;
}

/******************************************************************************/
VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags /*= 0*/)
{
	VkCommandBufferBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.pNext = nullptr;
	info.pInheritanceInfo = nullptr;
	info.flags = flags;
	return info;
}

/******************************************************************************/
VkFramebufferCreateInfo framebufferCreateInfo(VkRenderPass renderPass, VkExtent2D extent)
{
	VkFramebufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.pNext = nullptr;
	info.renderPass = renderPass;
	info.attachmentCount = 1;
	info.width = extent.width;
	info.height = extent.height;
	info.layers = 1;
	return info;
}

/******************************************************************************/
VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags /*= 0*/)
{
	VkFenceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	return info;
}

/******************************************************************************/
VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags /*= 0*/)
{
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	return info;
}

/******************************************************************************/
VkPresentInfoKHR presentInfo()
{
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.pNext = nullptr;
	info.swapchainCount = 0;
	info.pSwapchains = nullptr;
	info.pWaitSemaphores = nullptr;
	info.waitSemaphoreCount = 0;
	info.pImageIndices = nullptr;
	return info;
}

/******************************************************************************/
VkRenderPassBeginInfo renderpassBeginInfo(VkRenderPass renderPass, VkExtent2D windowExtent, VkFramebuffer framebuffer)
{
	VkRenderPassBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.pNext = nullptr;
	info.renderPass = renderPass;
	info.renderArea.offset.x = 0;
	info.renderArea.offset.y = 0;
	info.renderArea.extent = windowExtent;
	info.clearValueCount = 1;
	info.pClearValues = nullptr;
	info.framebuffer = framebuffer;
	return info;
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
VkSemaphore createSemaphore(VkDevice pDevice)
{
	VkSemaphoreCreateInfo lSemaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkSemaphore lSemaphore;
	VK_CHECK(vkCreateSemaphore(pDevice, &lSemaphoreCreateInfo, nullptr, &lSemaphore));
	return lSemaphore;
}

/******************************************************************************/
VkFence createFence(VkDevice pDevice, VkFenceCreateFlags flags)
{
	VkFenceCreateInfo createInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	createInfo.flags = flags;
	VkFence fence;
	VK_CHECK(vkCreateFence(pDevice, &createInfo, nullptr, &fence));
	return fence;
}

/******************************************************************************/
VkCommandPool createCommandPool(VkDevice pDevice, uint32_t pFamilyIndex, VkCommandPoolCreateFlags pFlags = 0)
{
	VkCommandPoolCreateInfo lCreateInfo = commandPoolCreateInfo(pFamilyIndex, pFlags);
	VkCommandPool lCommandPool;
	VK_CHECK(vkCreateCommandPool(pDevice, &lCreateInfo, nullptr, &lCommandPool));
	return lCommandPool;
}

/******************************************************************************/
VkImageView createImageView(VkDevice pDevice, VkImage pImage, VkFormat pFormat, VkImageAspectFlags pAspectFlags)
{
	VkImageViewCreateInfo lImageViewCreateInfo = imageViewCreateInfo(pImage, pFormat, pAspectFlags);
	VkImageView lImageView;
	VK_CHECK(vkCreateImageView(pDevice, &lImageViewCreateInfo, nullptr, &lImageView));
	return lImageView;
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
VkFramebuffer createFramebuffer(VkDevice pDevice, VkRenderPass pRenderPass, const VkImageView* imageViews, uint32_t imageViewCount, uint32_t pWidth, uint32_t pHeight)
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

/******************************************************************************/
std::vector<VkFramebuffer> createSwapchainFramebuffer(VkDevice pDevice, VkRenderPass pRenderPass, const VulkanSwapchain& pSwapchain)
{
	std::vector<VkFramebuffer> lSwapChainFramebuffer(pSwapchain.imageCount());
	for (uint32_t i = 0; i < pSwapchain.imageCount(); ++i)
	{
		lSwapChainFramebuffer[i] = vkh::createFramebuffer(pDevice, pRenderPass, &pSwapchain.mImageViews[i], 1, pSwapchain.mWidth, pSwapchain.mHeight);
	}

	return lSwapChainFramebuffer;
}

} // ns vkh