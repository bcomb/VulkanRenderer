#include "VulkanContext.h"


#include <assert.h>
#include <stdio.h>


/******************************************************************************/
void VulkanContext::createInstance()
{
	// TODO : Check vulkan version available via vkEnumerateInstanceVersion
	VkApplicationInfo lAppInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	lAppInfo.apiVersion = VK_API_VERSION_1_1;

	// Create the VulkanInstance
	VkInstanceCreateInfo lCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	lCreateInfo.pApplicationInfo = &lAppInfo;

#ifdef _DEBUG
	// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers
	const char* lVulkanLayers[] =
	{
		"VK_LAYER_KHRONOS_validation",
	};

	lCreateInfo.enabledLayerCount = ARRAY_COUNT(lVulkanLayers);
	lCreateInfo.ppEnabledLayerNames = lVulkanLayers;
#endif

	// Extension
	const char* lVulkanExtensions[] =
	{
#ifdef _DEBUG
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
		VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
	};

	lCreateInfo.enabledExtensionCount = ARRAY_COUNT(lVulkanExtensions);
	lCreateInfo.ppEnabledExtensionNames = lVulkanExtensions;
	VK_CHECK(vkCreateInstance(&lCreateInfo, nullptr, &mVulkanInstance));

	// 
	volkLoadInstance(mVulkanInstance);
}

/******************************************************************************/
void VulkanContext::enumeratePhysicalDevices()
{
	uint32_t lPhysicalDevicesCount;
	VK_CHECK(vkEnumeratePhysicalDevices(mVulkanInstance, &lPhysicalDevicesCount, NULL));
	mPhysicalDevices.resize(lPhysicalDevicesCount);
	VK_CHECK(vkEnumeratePhysicalDevices(mVulkanInstance, &lPhysicalDevicesCount, mPhysicalDevices.data()));
}

/******************************************************************************/
VkPhysicalDevice VulkanContext::pickPhysicalDevice(VkQueueFlags pCapableBits)
{
	// Search for a DISCRETE_GPU
	for (uint32_t i = 0; i < mPhysicalDevices.size(); ++i)
	{
		VkPhysicalDeviceProperties lProperties;
		vkGetPhysicalDeviceProperties(mPhysicalDevices[i], &lProperties);
		if (lProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			printf("Picking discrete GPU : %s\n", lProperties.deviceName);
			return mPhysicalDevices[i];
		}
	}

	// Falback return the first device
	if (mPhysicalDevices.size() > 0)
	{
		VkPhysicalDeviceProperties lProperties;
		vkGetPhysicalDeviceProperties(mPhysicalDevices[0], &lProperties);
		printf("Picking fallback GPU : %s\n", lProperties.deviceName);
		return mPhysicalDevices[0];
	}

	printf("No physical device available\n");
	return VK_NULL_HANDLE;
}