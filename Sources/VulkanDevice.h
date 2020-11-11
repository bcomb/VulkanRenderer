#include "vk_common.h"

#include <vector>

// Helper class to create/access logical device from a physical device
struct VulkanDevice
{
    explicit VulkanDevice(VkPhysicalDevice pPhysical);
    operator VkDevice() const { return mLogicalDevice; }

    // Create the logical device
    void createLogicalDevice(VkQueueFlags pRequestedQueueTypes);

    // Convenient function to get device queue
    // LogicalDevice must be created
    // return VK_NULL_HANDLE if not exist
    inline VkQueue getQueue(VulkanQueueType::Enum pQueueType) const
    {
        return mQueues[pQueueType];
    }    
    inline uint32_t getQueueFamilyIndex(VulkanQueueType::Enum pQueueType) const
    {
        return mQueueFamilyIndices[pQueueType];
    }

    // Find a queue, try to find a dedicated queue first
    uint32_t findQueueFamilyIndex(VkQueueFlagBits pQueueFlags);
    uint32_t selectMemoryType(uint32_t pMemoryTypeFilter, VkMemoryPropertyFlags pProperties);

    VkDevice mLogicalDevice;
    VkPhysicalDevice mPhysicalDevice;

    VkQueue     mQueues[VulkanQueueType::Count];
    uint32_t    mQueueFamilyIndices[VulkanQueueType::Count];

    std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;

    VkPhysicalDeviceProperties mPhysicalDeviceProperties;
    VkPhysicalDeviceFeatures mPhysicalDeviceFeatures;
    VkPhysicalDeviceFeatures mEnabledDeviceFeatures;
    VkPhysicalDeviceMemoryProperties mPhysicalDeviceMemoryProperties;
};