#pragma once

VkShaderModule loadShader(VkDevice pDevice, const char* pFilename);
VkPipelineLayout createPipelineLayout(VkDevice pDevice, uint32_t setLayoutCount, const VkDescriptorSetLayout* pSetLayouts, uint32_t pushConstantRangeCount, const VkPushConstantRange* pPushConstantRanges);
VkPipeline createGraphicsPipeline(VkDevice pDevice, VkPipelineCache pPipelineCache, VkPipelineLayout pPipelineLayout, VkRenderPass pRenderPass, VkShaderModule pVertexShader, VkShaderModule pFragmentShader, VkPipelineVertexInputStateCreateInfo& pInputState);

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice pDevice);
VkDescriptorUpdateTemplate createDescriptorUpdateTemplate(VkDevice pDevice, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout pipelineLayout, VkDescriptorSetLayout descriptorSetLayout);


struct DescriptorInfo
{
	union
	{
		VkDescriptorImageInfo image;
		VkDescriptorBufferInfo buffer;
	};

	DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
	{
		image.sampler = sampler;
		image.imageView = imageView;
		image.imageLayout = imageLayout;
	}

	DescriptorInfo(VkBuffer buffer_, VkDeviceSize offset, VkDeviceSize range)
	{
		buffer.buffer = buffer_;
		buffer.offset = offset;
		buffer.range = range;
	}
};