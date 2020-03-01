#pragma once

VkShaderModule loadShader(VkDevice pDevice, const char* pFilename);
VkPipelineLayout createPipelineLayout(VkDevice pDevice);
VkPipeline createGraphicsPipeline(VkDevice pDevice, VkPipelineCache pPipelineCache, VkPipelineLayout pPipelineLayout, VkRenderPass pRenderPass, VkShaderModule pVertexShader, VkShaderModule pFragmentShader, VkPipelineVertexInputStateCreateInfo& pInputState);