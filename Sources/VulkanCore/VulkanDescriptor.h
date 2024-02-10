#pragma once

#include "vk_common.h"
#include <vector>

// Helper to create a descriptor layout
struct DescriptorLayoutBuilder
{
    std::vector<VkDescriptorSetLayoutBinding> mBindings;
    void addBinding(uint32_t pBinding, VkDescriptorType pType);
    VkDescriptorSetLayout build(VkDevice pDevice, VkShaderStageFlags pStageFlags);
    void clear();
};

// Helper to create allocate descriptor set
struct DescriptorAllocator
{
    struct PoolSize
    {
        VkDescriptorType mType;
        uint32_t mCount;
    };

    VkDescriptorPool mPool;

    void initPool(VkDevice pDevice, uint32_t pMaxSets, const std::vector<PoolSize>& pPoolSizes);
    void clearDescriptors(VkDevice pDevice);
    void destroyPool(VkDevice pDevice);
    VkDescriptorSet allocate(VkDevice pDevice, VkDescriptorSetLayout pLayout);
};