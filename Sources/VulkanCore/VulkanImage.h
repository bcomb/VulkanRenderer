#pragma once

#include "vk_common.h"
#include <vk_mem_alloc.h>

struct VulkanImage
{
    VkImage mImage;
    VkImageView mView;
    VkExtent3D mExtent;
    VkFormat mFormat;
    VmaAllocation mAllocation;
};