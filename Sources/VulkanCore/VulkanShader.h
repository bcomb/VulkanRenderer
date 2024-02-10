#pragma once

#include "vk_common.h"
#include <string>

struct VulkanShader
{
    // Load a shader from a file (Spirv file)
    static VulkanShader loadFromFile(VkDevice pDevice, const std::string& pFilename);

    VkShaderModule mShaderModule;
    VkShaderStageFlagBits mStage;

    inline bool isValid() const { return mShaderModule != VK_NULL_HANDLE; }
};