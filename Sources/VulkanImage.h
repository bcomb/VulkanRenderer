#pragma once

#include "vk_common.h"
#include "VulkanDevice.h"


struct VulkanImage
{
    VkImage mImage;
    VkImageView mImageView;
    VmaAllocation mAllocation;
    VkExtent3D mImageExtent;
    VkFormat mImageFormat;
};