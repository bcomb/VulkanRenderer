#pragma once

#include <volk.h>

#define VK_CHECK(call_) do { VkResult result_ = call_;	assert(result_ == VK_SUCCESS); } while(0);
#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof(array_[0]))

// Don't change order/count or report modification everywhere
struct VulkanQueueType
{
    enum Enum : uint8_t
    {
        Graphics,
        Compute,
        Transfert,
        Count
    };
};

inline VkQueueFlagBits ToVkQueueFlagBits(VulkanQueueType::Enum pType)
{    
    const VkQueueFlagBits cTo[] = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT };
    static_assert(ARRAY_COUNT(cTo) == (int)VulkanQueueType::Count, "");
    return cTo[(int)pType];
}