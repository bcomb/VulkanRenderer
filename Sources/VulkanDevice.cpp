#include "VulkanDevice.h"

#include <assert.h>

/******************************************************************************/
VulkanDevice::VulkanDevice(VkPhysicalDevice pPhysicalDevice)
: mPhysicalDevice(pPhysicalDevice)
{
	assert(mPhysicalDevice);

	// Store Properties features, limits and properties of the physical device for later use
	// Device properties also contain limits and sparse properties
	vkGetPhysicalDeviceProperties(mPhysicalDevice, &mPhysicalDeviceProperties);
	// Features should be checked by the examples before using them
	vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mPhysicalDeviceFeatures);
	// Memory properties are used regularly for creating all kinds of buffers
	vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mPhysicalDeviceMemoryProperties);

	// Queues properties
	uint32_t lQueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &lQueueFamilyCount, nullptr);
	mQueueFamilyProperties.resize(lQueueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &lQueueFamilyCount, mQueueFamilyProperties.data());


	for (int i = 0; i < VulkanQueueType::Count; ++i)
	{
		mQueueFamilyIndices[i] = findQueueFamilyIndex(ToVkQueueFlagBits((VulkanQueueType::Enum)i));
	}
}

/******************************************************************************/
void VulkanDevice::createLogicalDevice(VkQueueFlags pRequestedQueueTypes)
{
#pragma message ("TODO : Manage features/extension")

	VkPhysicalDeviceFeatures lRequiredDeviceFeatures = {};


	// Get queue family indices for the requested queue family types
	// Note that the indices may overlap depending on the implementation

	const float defaultQueuePriority(0.0f);
	std::vector<VkDeviceQueueCreateInfo> lQueueCreateInfos;
	// Graphics queue
	if (pRequestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
	{		
		VkDeviceQueueCreateInfo lQueueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		lQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		lQueueInfo.queueFamilyIndex = mQueueFamilyIndices[VulkanQueueType::Graphics];
		lQueueInfo.queueCount = 1;
		lQueueInfo.pQueuePriorities = &defaultQueuePriority;
		lQueueCreateInfos.push_back(lQueueInfo);
	}
	else
	{
		mQueueFamilyIndices[VulkanQueueType::Graphics] = VK_NULL_HANDLE;
	}

	// Dedicated compute queue
	if (pRequestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
	{		
		if (mQueueFamilyIndices[VulkanQueueType::Graphics] != mQueueFamilyIndices[VulkanQueueType::Compute])
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo lQueueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
			lQueueInfo.queueFamilyIndex = mQueueFamilyIndices[VulkanQueueType::Compute];
			lQueueInfo.queueCount = 1;
			lQueueInfo.pQueuePriorities = &defaultQueuePriority;
			lQueueCreateInfos.push_back(lQueueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		mQueueFamilyIndices[VulkanQueueType::Compute] = mQueueFamilyIndices[VulkanQueueType::Graphics];
	}

	// Dedicated transfer queue
	if (pRequestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
	{		
		if ((mQueueFamilyIndices[VulkanQueueType::Transfert] != mQueueFamilyIndices[VulkanQueueType::Graphics]) && (mQueueFamilyIndices[VulkanQueueType::Transfert] != mQueueFamilyIndices[VulkanQueueType::Compute]))
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo lQueueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO  };
			lQueueInfo.queueFamilyIndex = mQueueFamilyIndices[VulkanQueueType::Transfert];
			lQueueInfo.queueCount = 1;
			lQueueInfo.pQueuePriorities = &defaultQueuePriority;
			lQueueCreateInfos.push_back(lQueueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		mQueueFamilyIndices[VulkanQueueType::Transfert] = mQueueFamilyIndices[VulkanQueueType::Graphics];
	}



	VkDeviceCreateInfo lDeviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	lDeviceCreateInfo.pQueueCreateInfos = lQueueCreateInfos.data();
	lDeviceCreateInfo.queueCreateInfoCount = (uint32_t)lQueueCreateInfos.size();

	const char* lDeviceExtensions[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	};
	lDeviceCreateInfo.enabledExtensionCount = ARRAY_COUNT(lDeviceExtensions);
	lDeviceCreateInfo.ppEnabledExtensionNames = lDeviceExtensions;

	// Previous implementations of Vulkan made a distinction between instance and device specific validation layers,
	// but this is no longer the case. That means that the enabledLayerCountand ppEnabledLayerNames fields 
	// of VkDeviceCreateInfo are ignored by up - to - date implementations.However,
	// it is still a good idea to set them anyway to be compatible with older implementations
	VK_CHECK(vkCreateDevice(mPhysicalDevice, &lDeviceCreateInfo, nullptr, &mLogicalDevice));
	
	// Obtain queue handle
	for (unsigned i = 0; i < VulkanQueueType::Count; ++i)
	{
		if (mQueueFamilyIndices[i] >= 0)
			vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices[i], 0, &mQueues[i]);
		else
			mQueues[i] = VK_NULL_HANDLE;
	}
}

/******************************************************************************/
uint32_t VulkanDevice::findQueueFamilyIndex(VkQueueFlagBits pQueueFlags)
{
	// Dedicated queue for compute
	// Try to find a queue family index that supports compute but not graphics
	if (pQueueFlags & VK_QUEUE_COMPUTE_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(mQueueFamilyProperties.size()); i++)
		{
			if ((mQueueFamilyProperties[i].queueFlags & pQueueFlags) && ((mQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				return i;
			}
		}
	}

	// Dedicated queue for transfer
	// Try to find a queue family index that supports transfer but not graphics and compute
	if (pQueueFlags & VK_QUEUE_TRANSFER_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(mQueueFamilyProperties.size()); i++)
		{
			if ((mQueueFamilyProperties[i].queueFlags & pQueueFlags) && ((mQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((mQueueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				return i;
			}
		}
	}

	// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
	for (uint32_t i = 0; i < static_cast<uint32_t>(mQueueFamilyProperties.size()); i++)
	{
		if (mQueueFamilyProperties[i].queueFlags & pQueueFlags)
		{
			return i;
		}
	}
	return -1;
}

/******************************************************************************/
uint32_t VulkanDevice::selectMemoryType(uint32_t pMemoryTypeFilter, VkMemoryPropertyFlags pProperties)
{
	// The VkPhysicalDeviceMemoryProperties structure has two arrays memoryTypes and memoryHeaps.
	// Memory heaps are distinct memory resources like dedicated VRAM and swap space in RAM for when VRAM runs out.
	// The different types of memory exist within these heaps.
	uint32_t memoryTypeIndex = ~0u;
	for (uint32_t i = 0; i < mPhysicalDeviceMemoryProperties.memoryTypeCount; ++i)
	{
		if ((pMemoryTypeFilter & (1 << i))
			&& (mPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & pProperties) == pProperties)
		{
			memoryTypeIndex = i;
			break;
		}
	}

	assert(memoryTypeIndex != ~0u && "give optional flag and try to be less restrictive on memory type");
	return memoryTypeIndex;
}