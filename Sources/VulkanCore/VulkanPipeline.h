#pragma once

#include "vk_common.h"

#include <vector>

struct PipelineBuilder
{
    std::vector<VkPipelineShaderStageCreateInfo> mShaderStages;

    VkPipelineInputAssemblyStateCreateInfo mInputAssembly;
    VkPipelineRasterizationStateCreateInfo mRasterizer;
    VkPipelineColorBlendAttachmentState mColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo mMultisampling;
    VkPipelineLayout mPipelineLayout;
    VkPipelineDepthStencilStateCreateInfo mDepthStencil;
    VkPipelineRenderingCreateInfo mRenderInfo;
    VkFormat mColorAttachmentformat;

    PipelineBuilder() { clear(); }
    void clear();

    VkPipeline buildPipeline(VkDevice device);
};