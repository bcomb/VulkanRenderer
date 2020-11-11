#include "vk_common.h"
#include <vector>

struct VulkanContext
{	
	operator VkInstance() const { return mVulkanInstance; }

	void createInstance();
	void enumeratePhysicalDevices();

	// Pick a physcial device with capabilities
	// ie: VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT
	VkPhysicalDevice pickPhysicalDevice(VkQueueFlags pCapableBits);

	VkInstance mVulkanInstance;
	std::vector<VkPhysicalDevice> mPhysicalDevices;
};