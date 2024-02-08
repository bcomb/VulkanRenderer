#pragma once
#include "vk_common.h"

#include <vector>
#include <string>


//TODO Instance builder?
// VkInstanceBuilder instanceBuilder;
// instanceBuilder.setApplicationName
// instanceBuilder.setVersion(VK_MAKE_VERSION(1,3,0))
// instanceBuilder.addLayer("VK_LAYER_KHRONOS_validation");
// instanceBuilder.addExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
// instanceBuilder.addExtension(VK_KHR_SURFACE_EXTENSION_NAME);
// instanceBuilder.addExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
// instanceBuilder.build();


struct VulkanInstance
{	
	operator VkInstance() const { return mVulkanInstance; }

	void createInstance(uint32_t pVersion, bool pUseValidationLayers = true);
	void enumeratePhysicalDevices();

	// Pick a physcial device with capabilities
	// ie: VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT
	VkPhysicalDevice pickPhysicalDevice(VkQueueFlags pCapableBits);

	VkInstance mVulkanInstance;
	std::vector<VkPhysicalDevice> mPhysicalDevices;
};
