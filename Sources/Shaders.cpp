#include "vk_common.h"
#include "Shaders.h"

#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <vector>



/*****************************************************************************/
VkShaderModule loadShader(VkDevice pDevice, const char* pFilename)
{
	//char path[256];
	//DWORD r = GetCurrentirectory(256, path);

	FILE* file = fopen(pFilename, "rb");
	if (file)
	{
		fseek(file, 0, SEEK_END);
		long bytesSize = ftell(file);
		assert(bytesSize);
		fseek(file, 0, SEEK_SET);

		char* code = (char*)malloc(bytesSize);
		assert(code);
		size_t bytesRead = fread(code, 1, bytesSize, file);
		assert(bytesRead == bytesSize);
		assert(bytesRead % 4 == 0);

		VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		//VkShaderModuleCreateFlags    flags;
		createInfo.codeSize = bytesSize;			// size in bytes
		createInfo.pCode = (const uint32_t*)code;	// must be an array of codeSize/4

		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(pDevice, &createInfo, nullptr, &shaderModule));

		free(code);

		return shaderModule;
	}

	return VK_NULL_HANDLE;
}


/*****************************************************************************/
VkDescriptorSetLayout createDescriptorSetLayout(VkDevice pDevice)
{
	// TODO : This is the thing to automated with reflection
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1; // for array
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // VK_SHADER_STAGE_ALL_GRAPHICS (opengl fashion?)
	uboLayoutBinding.pImmutableSamplers = nullptr; // Optional The pImmutableSamplers field is only relevant for image sampling related descriptors

	VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboLayoutBinding;

	VkDescriptorSetLayout descriptorSetLayout = 0;
	VK_CHECK(vkCreateDescriptorSetLayout(pDevice, &layoutInfo, nullptr, &descriptorSetLayout));
	return descriptorSetLayout;
}

/*****************************************************************************/
VkPipelineLayout createPipelineLayout(VkDevice pDevice, uint32_t setLayoutCount, const VkDescriptorSetLayout* pSetLayouts, uint32_t pushConstantRangeCount, const VkPushConstantRange* pPushConstantRanges)
{
	VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	//VkPipelineLayoutCreateFlags     flags;
	createInfo.setLayoutCount = setLayoutCount;
	createInfo.pSetLayouts = pSetLayouts;
	createInfo.pushConstantRangeCount = pushConstantRangeCount;
	createInfo.pPushConstantRanges = pPushConstantRanges;

	VkPipelineLayout pipelineLayout;
	VK_CHECK(vkCreatePipelineLayout(pDevice, &createInfo, nullptr, &pipelineLayout));

	return pipelineLayout;
}

/*****************************************************************************/
VkPipeline createGraphicsPipeline(VkDevice pDevice, VkPipelineCache pPipelineCache, VkPipelineLayout pPipelineLayout, VkRenderPass pRenderPass, VkShaderModule pVertexShader, VkShaderModule pFragmentShader, VkPipelineVertexInputStateCreateInfo& pInputState)
{
	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	//const void* pNext;
	//VkPipelineShaderStageCreateFlags    flags;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = pVertexShader;
	stages[0].pName = "main";
	//const VkSpecializationInfo* pSpecializationInfo;

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = pFragmentShader;
	stages[1].pName = "main";


	VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	//VkPipelineInputAssemblyStateCreateFlags    flags;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = 0;

	VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	//VkPipelineViewportStateCreateFlags    flags;
	viewportState.viewportCount = 1;
	//const VkViewport* pViewports;
	viewportState.scissorCount = 1;
	//const VkRect2D* pScissors;

	VkPipelineRasterizationStateCreateInfo rasterState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	//VkPipelineRasterizationStateCreateFlags    flags;
	rasterState.depthClampEnable = VK_FALSE;
	rasterState.rasterizerDiscardEnable = VK_FALSE;
	rasterState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterState.cullMode = VK_CULL_MODE_NONE;
	rasterState.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterState.depthBiasEnable = VK_FALSE;
	rasterState.depthBiasConstantFactor = 0.0;;
	rasterState.depthBiasClamp = 0.0;
	rasterState.depthBiasSlopeFactor = 0.0;
	rasterState.lineWidth = 1.0;

	VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	//VkPipelineMultisampleStateCreateFlags    multisampleState.flags;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.sampleShadingEnable = VK_FALSE;
	//float                                    multisampleState.minSampleShading = 1.0;
	//const VkSampleMask* multisampleState.pSampleMask;
	multisampleState.alphaToCoverageEnable = VK_FALSE;
	multisampleState.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	//VkPipelineDepthStencilStateCreateFlags    flags;
	//VkBool32                                  depthTestEnable;
	//VkBool32                                  depthWriteEnable;
	//VkCompareOp                               depthCompareOp;
	//VkBool32                                  depthBoundsTestEnable;
	//VkBool32                                  stencilTestEnable;
	//VkStencilOpState                          front;
	//VkStencilOpState                          back;
	depthStencilState.minDepthBounds = 0.0f;
	depthStencilState.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	//VkBool32                 blendEnable;
	//VkBlendFactor            srcColorBlendFactor;
	//VkBlendFactor            dstColorBlendFactor;
	//VkBlendOp                colorBlendOp;
	//VkBlendFactor            srcAlphaBlendFactor;
	//VkBlendFactor            dstAlphaBlendFactor;
	//VkBlendOp                alphaBlendOp;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	//VkPipelineColorBlendStateCreateFlags          flags;
	//VkBool32                                      logicOpEnable = VK_FALSE;
	//VkLogicOp                                     logicOp;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorBlendAttachment;
	//float                                         blendConstants[4];


	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	//VkPipelineDynamicStateCreateFlags    flags;
	dynamicState.dynamicStateCount = ARRAY_COUNT(dynamicStates);
	dynamicState.pDynamicStates = dynamicStates;


	VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	//VkPipelineCreateFlags                            flags;
	createInfo.stageCount = ARRAY_COUNT(stages);
	createInfo.pStages = stages;
	createInfo.pVertexInputState = &pInputState;
	createInfo.pInputAssemblyState = &inputAssembly;
	//const VkPipelineTessellationStateCreateInfo* pTessellationState;
	createInfo.pViewportState = &viewportState;
	createInfo.pRasterizationState = &rasterState;
	createInfo.pMultisampleState = &multisampleState;
	createInfo.pDepthStencilState = &depthStencilState;
	createInfo.pColorBlendState = &colorBlendState;
	createInfo.pDynamicState = &dynamicState;
	createInfo.layout = pPipelineLayout;
	createInfo.renderPass = pRenderPass;
	//uint32_t                                         createInfo.subpass;
	//VkPipeline                                       createInfo.basePipelineHandle;
	//int32_t                                          createInfo.basePipelineIndex;

	VkPipeline pipeline = 0;
	VK_CHECK(vkCreateGraphicsPipelines(pDevice, pPipelineCache, 1, &createInfo, nullptr, &pipeline));
	return pipeline;
}



/*****************************************************************************/
VkDescriptorUpdateTemplate createDescriptorUpdateTemplate(VkDevice pDevice, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout pipelineLayout, VkDescriptorSetLayout descriptorSetLayout)
{
	std::vector<VkDescriptorUpdateTemplateEntry> entries;
	entries.resize(1);
	entries[0].dstBinding = 0;
	entries[0].dstArrayElement = 0;
	entries[0].descriptorCount = 1;
	entries[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	// TODO : DescriptorInfo 
	// This is the data layout given to the vkUpdateDescriptorSetWithTemplate
	entries[0].offset = sizeof(DescriptorInfo) * 0;
	entries[0].stride = sizeof(DescriptorInfo);

	VkDescriptorUpdateTemplateCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
	//const void* pNext;
	//VkDescriptorUpdateTemplateCreateFlags     flags;
	createInfo.descriptorUpdateEntryCount = (uint32_t)entries.size();
	createInfo.pDescriptorUpdateEntries = entries.data();
	createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
	createInfo.descriptorSetLayout = descriptorSetLayout;
	createInfo.pipelineBindPoint = pipelineBindPoint;
	createInfo.pipelineLayout = pipelineLayout;
	//uint32_t                                  set;

	VkDescriptorUpdateTemplate updateTpl = 0;
	VK_CHECK(vkCreateDescriptorUpdateTemplate(pDevice, &createInfo, NULL, &updateTpl));
	return updateTpl;
}