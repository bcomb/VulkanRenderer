#pragma once

#include "vk_common.h"
#include <vk_mem_alloc.h>


struct VulkanImage
{
    //static VulkanImage* create(VulkanContext& pContext, uint32_t pWidth, uint32_t pHeight );

    VkImage mImage;
    VkImageView mView;
    VkExtent3D mExtent;
    VkFormat mFormat;
    VmaAllocation mAllocation;
};