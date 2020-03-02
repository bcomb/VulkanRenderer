#pragma once

// Structure use to pass descriptor data to vkPushDescriptorTemplate
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

struct Shader
{
	VkShaderModule module;
	VkShaderStageFlagBits stage;
};


bool loadShader(Shader& pShader, VkDevice pDevice, const char* pFilename);
void destroyShader(VkDevice pDevice, Shader& pShader);

VkPipelineLayout createPipelineLayout(VkDevice pDevice, uint32_t setLayoutCount, const VkDescriptorSetLayout* pSetLayouts, uint32_t pushConstantRangeCount, const VkPushConstantRange* pPushConstantRanges);
VkPipeline createGraphicsPipeline(VkDevice pDevice, VkPipelineCache pPipelineCache, VkPipelineLayout pPipelineLayout, VkRenderPass pRenderPass, Shader& pVertexShader, Shader& pFragmentShader, VkPipelineVertexInputStateCreateInfo& pInputState);

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice pDevice);
VkDescriptorUpdateTemplate createDescriptorUpdateTemplate(VkDevice pDevice, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout pipelineLayout, VkDescriptorSetLayout descriptorSetLayout);



