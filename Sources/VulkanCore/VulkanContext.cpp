#include "VulkanContext.h"


#include <assert.h>
#include <stdio.h>

//#if WIN32
//#	include <debugapi.h>
//#endif

/******************************************************************************/
VkDebugUtilsMessengerEXT gDebugMessenger;
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	const char* errorTypeStr =
		(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		? "[VERBOSE] "
		: (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		? "[INFO] "
		: (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		? "[WARNING] "
		: (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		? "[ERROR] "
		: "[UNKNOW] ";


	printf("%s%s\n", errorTypeStr, pCallbackData->pMessage);

//#if WIN32
//	// MSVC ouput
//	OutputDebugString(errorTypeStr); OutputDebugString(pCallbackData->pMessage); OutputDebugString("\n");
//#endif

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
		&& !(messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)	// VUID-VkSamplerCreateInfo-anisotropyEnable-01070: Validation raise this error on vkCreateSwapchainKHR (which is a bug of validation layer)
		)
	{
		assert(!"ERROR");
	}

	return VK_FALSE;
}

void registerDebugMessenger(VkInstance pInstance)
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	VK_CHECK(vkCreateDebugUtilsMessengerEXT(pInstance, &createInfo, nullptr, &gDebugMessenger));
}

void unregisterDebugMessenger(VkInstance pInstance, VkDebugUtilsMessengerEXT pDebugMessenger)
{
	vkDestroyDebugUtilsMessengerEXT(pInstance, pDebugMessenger, nullptr);
}

/******************************************************************************/
void VulkanInstance::createInstance(uint32_t pVersion, bool pUseValidationLayers)
{
	// TODO : Check vulkan version available via vkEnumerateInstanceVersion
	VkApplicationInfo lAppInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	lAppInfo.apiVersion = pVersion;

	// Create the VulkanInstance
	VkInstanceCreateInfo lCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	lCreateInfo.pApplicationInfo = &lAppInfo;

	if (pUseValidationLayers)
	{
#ifdef _DEBUG
		// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers
		const char* lVulkanLayers[] =
		{
			"VK_LAYER_KHRONOS_validation",
		};

		lCreateInfo.enabledLayerCount = ARRAY_COUNT(lVulkanLayers);
		lCreateInfo.ppEnabledLayerNames = lVulkanLayers;
#endif
	}

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

#ifdef _DEBUG
	registerDebugMessenger(mVulkanInstance);
#endif
}

/******************************************************************************/
void VulkanInstance::enumeratePhysicalDevices()
{
	uint32_t lPhysicalDevicesCount;
	VK_CHECK(vkEnumeratePhysicalDevices(mVulkanInstance, &lPhysicalDevicesCount, NULL));
	mPhysicalDevices.resize(lPhysicalDevicesCount);
	VK_CHECK(vkEnumeratePhysicalDevices(mVulkanInstance, &lPhysicalDevicesCount, mPhysicalDevices.data()));
}

/******************************************************************************/
VkPhysicalDevice VulkanInstance::pickPhysicalDevice(VkQueueFlags pCapableBits)
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