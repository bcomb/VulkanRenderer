#include <VulkanPipeline.h>
#include <VulkanHelper.h>

/*****************************************************************************/
void PipelineBuilder::clear()
{
	//clear all of the structs we need back to 0 with their correct stype	
	mInputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	mRasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	mColorBlendAttachment = {};
	mMultisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	mPipelineLayout = {};
	mDepthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	mRenderInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	mShaderStages.clear();
}

/*****************************************************************************/
VkPipeline PipelineBuilder::buildPipeline(VkDevice pDevice)
{
    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo lViewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    lViewportState.viewportCount = 1;
    lViewportState.scissorCount = 1;

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo lColorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    lColorBlending.logicOpEnable = VK_FALSE;
    lColorBlending.logicOp = VK_LOGIC_OP_COPY;
    lColorBlending.attachmentCount = 1;
    lColorBlending.pAttachments = &mColorBlendAttachment;

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo lVertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one
    // to create the pipeline
    VkGraphicsPipelineCreateInfo lPipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    //connect the renderInfo to the pNext extension mechanism
    lPipelineInfo.pNext = &mRenderInfo;

    lPipelineInfo.stageCount = (uint32_t)mShaderStages.size();
    lPipelineInfo.pStages = mShaderStages.data();
    lPipelineInfo.pVertexInputState = &lVertexInputInfo;
    lPipelineInfo.pInputAssemblyState = &mInputAssembly;
    lPipelineInfo.pViewportState = &lViewportState;
    lPipelineInfo.pRasterizationState = &mRasterizer;
    lPipelineInfo.pMultisampleState = &mMultisampling;
    lPipelineInfo.pColorBlendState = &lColorBlending;
    lPipelineInfo.pDepthStencilState = &mDepthStencil;
    lPipelineInfo.layout = mPipelineLayout;

    // Commonly supported (todo check)
    VkDynamicState lDynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo lDynamicInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    lDynamicInfo.pDynamicStates = &lDynamicState[0];
    lDynamicInfo.dynamicStateCount = 2;

    lPipelineInfo.pDynamicState = &lDynamicInfo;

    // its easy to error out on create graphics pipeline, so we handle it a bit
    // better than the common VK_CHECK case
    VkPipeline lPipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(pDevice, VK_NULL_HANDLE, 1, &lPipelineInfo, nullptr, &lPipeline));
    return lPipeline;
}
