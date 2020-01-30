#include <vulkan/vulkan.h>

#include <assert.h>
#include <malloc.h>
#include <stdio.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define VK_CHECK(call_) do { VkResult result_ = call_;	assert(result_ == VK_SUCCESS); } while(0);
#define VK_CHECK_HANDLE(handle_) assert(handle_ != VK_NULL_HANDLE);
#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof(array_[0]))

// Allocation on stack
// Automatically destroyed when exit function
#define STACK_ALLOC(TYPE_,COUNT_) (TYPE_*)_alloca(sizeof(TYPE_) * COUNT_)

VkDebugUtilsMessengerEXT debugMessenger;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
	void* pUserData)
{
	printf("validation layer: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void setupDebugMessenger(VkInstance pInstance)
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	VK_CHECK(CreateDebugUtilsMessengerEXT(pInstance, &createInfo, nullptr, &debugMessenger));
}


// Search for the discrete GPU
VkPhysicalDevice pickPhysicalDevice(uint32_t pPhysicalDevicesCount, const VkPhysicalDevice* pPhysicalDevices)
{
	for (uint32_t i = 0; i < pPhysicalDevicesCount; ++i)
	{
		VkPhysicalDeviceProperties lProperties;
		vkGetPhysicalDeviceProperties(pPhysicalDevices[i], &lProperties);
		if (lProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			printf("Picking discrete GPU : %s\n", lProperties.deviceName);
			return pPhysicalDevices[i];
		}
	}

	if (pPhysicalDevicesCount > 0)
	{
		VkPhysicalDeviceProperties lProperties;
		vkGetPhysicalDeviceProperties(pPhysicalDevices[0], &lProperties);
		if (lProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			printf("Picking fallback GPU : %s\n", lProperties.deviceName);
			return pPhysicalDevices[0];
		}
	}

	printf("No physical device available\n");
	return VK_NULL_HANDLE;
}

VkInstance createInstance()
{
	// TODO : Check vulkan version available via vkEnumerateInstanceVersion
	VkApplicationInfo lAppInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	lAppInfo.apiVersion = VK_API_VERSION_1_1;

	// Create the VulkanInstance
	VkInstance lVulkanInstance;
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
	VK_CHECK(vkCreateInstance(&lCreateInfo, nullptr, &lVulkanInstance));

	return lVulkanInstance;
}

VkSurfaceKHR createSurface(VkInstance pVkInstance, GLFWwindow* pWindow)
{
	VkSurfaceKHR lSurface = VK_NULL_HANDLE;
#ifdef VK_USE_PLATFORM_WIN32_KHR
	VkWin32SurfaceCreateInfoKHR lInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	lInfo.hinstance = GetModuleHandle(0);
	lInfo.hwnd = glfwGetWin32Window(pWindow);
	VK_CHECK(vkCreateWin32SurfaceKHR(pVkInstance, &lInfo, nullptr, &lSurface));
#else
#	error Unsupported platform
#endif

	return lSurface;
}

VkSwapchainKHR createSwapchain(VkDevice pDevice, VkSurfaceKHR pSurface, uint32_t pFamilyIndex, uint32_t pWidth, uint32_t pHeight, uint32_t lSwapchainImageCount)
{	
	VkSwapchainCreateInfoKHR lSwapchainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	lSwapchainInfo.surface = pSurface;
	lSwapchainInfo.minImageCount = lSwapchainImageCount;		// double buffer
	lSwapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;		// some devices only support BGRA
	lSwapchainInfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	lSwapchainInfo.imageExtent.width = pWidth;
	lSwapchainInfo.imageExtent.height = pHeight;
	lSwapchainInfo.imageArrayLayers = 1;
	lSwapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	lSwapchainInfo.queueFamilyIndexCount = 1;
	//lSwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	lSwapchainInfo.pQueueFamilyIndices = &pFamilyIndex;
	lSwapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	lSwapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	// The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system.You'll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
	lSwapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	VkSwapchainKHR lSwapChain;
	VK_CHECK(vkCreateSwapchainKHR(pDevice, &lSwapchainInfo, nullptr, &lSwapChain));
	return lSwapChain;
}

// Find a queue that support graphics operation
bool findQueueFamilyIndex(VkPhysicalDevice pDevice, uint32_t* pQueueFamilyIndex )
{
	uint32_t lQueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &lQueueFamilyCount, nullptr);

	VkQueueFamilyProperties* lQueueFamilies = STACK_ALLOC(VkQueueFamilyProperties, lQueueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &lQueueFamilyCount, lQueueFamilies);

	for (uint32_t i = 0; i < lQueueFamilyCount; ++i)
	{
		if (lQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			*pQueueFamilyIndex = i;
			return true;
		}
	}

	assert(false);
	return false;
}

VkDevice createDevice(VkInstance pInstance, VkPhysicalDevice pPhysicalDevice, uint32_t* pFamilyIndex)
{
	VkPhysicalDeviceFeatures lRequiredDeviceFeatures = {};

	const float lQueuePriority[1] = { 1.0f };
	VkDeviceQueueCreateInfo lDeviceQueueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	lDeviceQueueCreateInfo.queueFamilyIndex = *pFamilyIndex; // TODO : Compute from queue properties
	lDeviceQueueCreateInfo.queueCount = 1;
	lDeviceQueueCreateInfo.pQueuePriorities = lQueuePriority;

	VkDeviceCreateInfo lDeviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	lDeviceCreateInfo.pQueueCreateInfos = &lDeviceQueueCreateInfo;
	lDeviceCreateInfo.queueCreateInfoCount = 1;

	const char* lDeviceExtensions[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};
	lDeviceCreateInfo.enabledExtensionCount = ARRAY_COUNT(lDeviceExtensions);
	lDeviceCreateInfo.ppEnabledExtensionNames = lDeviceExtensions;
	// Previous implementations of Vulkan made a distinction between instance and device specific validation layers,
	// but this is no longer the case. That means that the enabledLayerCountand ppEnabledLayerNames fields 
	// of VkDeviceCreateInfo are ignored by up - to - date implementations.However,
	// it is still a good idea to set them anyway to be compatible with older implementations

	VkDevice lDevice;
	VK_CHECK(vkCreateDevice(pPhysicalDevice, &lDeviceCreateInfo, nullptr, &lDevice));

	return lDevice;
}

VkSemaphore createSemaphore(VkDevice pDevice)
{
	VkSemaphoreCreateInfo lSemaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkSemaphore lSemaphore;
	VK_CHECK(vkCreateSemaphore(pDevice, &lSemaphoreCreateInfo, nullptr, &lSemaphore));

	return lSemaphore;
}

VkCommandPool createCommandPool(VkDevice pDevice, uint32_t pFamilyIndex)
{
	VkCommandPoolCreateInfo lCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	lCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	lCreateInfo.queueFamilyIndex = pFamilyIndex;
	VkCommandPool lCommandPool;
	VK_CHECK(vkCreateCommandPool(pDevice, &lCreateInfo, nullptr, &lCommandPool));
	return lCommandPool;
}

// Entry point
int main(int argc, char* argv[])
{
	printf("Hello VulkanRenderer\n");
	int lSuccess = glfwInit();
	assert(lSuccess);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// MUST BE SET or SwapChain creation fail
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

	VkInstance lVulkanInstance = createInstance();
#ifdef _DEBUG
	//setupDebugMessenger(lVulkanInstance);
#endif

	// Enumerate Device
	VkPhysicalDevice lPhysicalDevices[16];
	uint32_t lPhysicalDevicesCount = ARRAY_COUNT(lPhysicalDevices);
	VK_CHECK(vkEnumeratePhysicalDevices(lVulkanInstance, &lPhysicalDevicesCount, lPhysicalDevices));
	VkPhysicalDevice lPhysicalDevice = pickPhysicalDevice(lPhysicalDevicesCount, lPhysicalDevices);

	// Find a graphics capable queue
	uint32_t lQueueFamilyIndex = 0;
	findQueueFamilyIndex(lPhysicalDevice, &lQueueFamilyIndex);

	// Create logical device
	VkDevice lDevice = createDevice(lVulkanInstance, lPhysicalDevice, &lQueueFamilyIndex);

	// Retrieve the graphics queue
	VkQueue lGraphicsQueue;
	vkGetDeviceQueue(lDevice, lQueueFamilyIndex, 0, &lGraphicsQueue);

	// Surface creation are platform specific
	GLFWwindow* lWindow = glfwCreateWindow(512, 512, "VulkanRenderer", 0, 0);
	assert(lWindow);
	VkSurfaceKHR lSurface = createSurface(lVulkanInstance, lWindow);

	VkBool32 lPresentationSupported = false;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(lPhysicalDevice, lQueueFamilyIndex, lSurface, &lPresentationSupported));
	assert(lPresentationSupported);

	VkSurfaceCapabilitiesKHR lPhysicalDeviceSurfaceCapabilities;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(lPhysicalDevice, lSurface, &lPhysicalDeviceSurfaceCapabilities));
	uint32_t lFormatCount;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(lPhysicalDevice, lSurface, &lFormatCount, nullptr));
	VkSurfaceFormatKHR* lFormats = (VkSurfaceFormatKHR*)_alloca(lFormatCount * sizeof(VkSurfaceFormatKHR));
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(lPhysicalDevice, lSurface, &lFormatCount, lFormats));
	uint32_t lPresentModeCount;
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(lPhysicalDevice, lSurface, &lPresentModeCount, nullptr));
	VkPresentModeKHR* lPresentModes = (VkPresentModeKHR*)_alloca(lPresentModeCount * sizeof(VkPresentModeKHR));
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(lPhysicalDevice, lSurface, &lPresentModeCount, lPresentModes));

	// Create the swap chain
	int lWindowWidth = 0, lWindowHeight = 0;
	glfwGetWindowSize(lWindow, &lWindowWidth, &lWindowHeight);
	uint32_t lSwapchainImageCount = 2;
	VkSwapchainKHR lSwapChain = createSwapchain(lDevice, lSurface, lQueueFamilyIndex, lWindowWidth, lWindowHeight, lSwapchainImageCount);

	VkImage lSwapChainImages[16] = {};
	VK_CHECK(vkGetSwapchainImagesKHR(lDevice, lSwapChain, &lSwapchainImageCount, lSwapChainImages));

	VkSemaphore lAcquireSemaphore = createSemaphore(lDevice);
	VkSemaphore lReleaseSemaphore = createSemaphore(lDevice);

	VkCommandPool lCommandPool = createCommandPool(lDevice, lQueueFamilyIndex);

	VkCommandBufferAllocateInfo lAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	lAllocateInfo.commandPool = lCommandPool;
	lAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	lAllocateInfo.commandBufferCount = 1;
	VkCommandBuffer lCommandBuffer;
	VK_CHECK(vkAllocateCommandBuffers(lDevice, &lAllocateInfo, &lCommandBuffer));

	// MainLoop
	while (!glfwWindowShouldClose(lWindow))
	{
		glfwPollEvents();

		uint32_t lImageIndex = 0;
		VK_CHECK(vkAcquireNextImageKHR(lDevice, lSwapChain, ~0ull, lAcquireSemaphore, VK_NULL_HANDLE, &lImageIndex));

		VK_CHECK(vkResetCommandPool(lDevice, lCommandPool, 0));

		VkCommandBufferBeginInfo lBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		lBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(lCommandBuffer, &lBeginInfo));

		VkClearColorValue lClearColor = { 1, 0, 0, 1 };
		VkImageSubresourceRange lRange = {};
		lRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		lRange.levelCount = 1;
		lRange.layerCount = 1;
		//VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
		vkCmdClearColorImage(lCommandBuffer, lSwapChainImages[lImageIndex], VK_IMAGE_LAYOUT_GENERAL, &lClearColor, 1, &lRange);
		VK_CHECK(vkEndCommandBuffer(lCommandBuffer));


		// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
		VkPipelineStageFlags lSubmitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // why
		VkSubmitInfo lSubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		lSubmitInfo.waitSemaphoreCount = 1;
		lSubmitInfo.pWaitSemaphores = &lAcquireSemaphore;
		lSubmitInfo.pWaitDstStageMask = &lSubmitStageMask;
		lSubmitInfo.commandBufferCount = 1;
		lSubmitInfo.pCommandBuffers = &lCommandBuffer;
		lSubmitInfo.signalSemaphoreCount = 1;
		lSubmitInfo.pSignalSemaphores = &lReleaseSemaphore;
		VK_CHECK(vkQueueSubmit(lGraphicsQueue, 1, &lSubmitInfo, VK_NULL_HANDLE));

		VkPresentInfoKHR lPresentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		lPresentInfo.swapchainCount = 1;		// one per window?
		lPresentInfo.pSwapchains = &lSwapChain; 
		lPresentInfo.pImageIndices = &lImageIndex;
		lPresentInfo.pWaitSemaphores = &lReleaseSemaphore;
		VK_CHECK(vkQueuePresentKHR(lGraphicsQueue, &lPresentInfo));

		VK_CHECK(vkDeviceWaitIdle(lDevice));
	}
	
	vkDestroyInstance(lVulkanInstance, nullptr);

	glfwDestroyWindow(lWindow);
	glfwTerminate();
	return 0;
}