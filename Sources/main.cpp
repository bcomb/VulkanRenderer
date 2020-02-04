#include <vulkan/vulkan.h>

#include <assert.h>
#include <malloc.h>
#include <stdio.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define VK_CHECK(call_) do { VkResult result_ = call_;	assert(result_ == VK_SUCCESS); } while(0);
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
	const char* errorTypeStr =
		(messageType & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		? "[VERBOSE] "
		: (messageType & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		? "[INFO] "
		: (messageType & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		? "[WARNING] "
		: "[ERROR] ";

	printf("%s%s\n", errorTypeStr, pCallbackData->pMessage);
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		assert(!"ERROR");

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
		printf("Picking fallback GPU : %s\n", lProperties.deviceName);
		return pPhysicalDevices[0];
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

VkSwapchainKHR createSwapchain(VkDevice pDevice, VkSurfaceKHR pSurface, VkSurfaceFormatKHR pSurfaceFormat, VkSurfaceCapabilitiesKHR pSurfaceCaps, uint32_t pFamilyIndex, uint32_t pWidth, uint32_t pHeight, uint32_t lSwapchainImageCount)
{	
	// On android VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR is generally not supported
	// Spec said : at least one must be supported
	VkCompositeAlphaFlagBitsKHR surfaceCompositeAlpha =
		(pSurfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
		: (pSurfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
		: (pSurfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
		: VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

	VkSwapchainCreateInfoKHR lSwapchainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	lSwapchainInfo.surface = pSurface;
	lSwapchainInfo.minImageCount = lSwapchainImageCount;		// double buffer
	lSwapchainInfo.imageFormat = pSurfaceFormat.format;		// some devices only support BGRA
	lSwapchainInfo.imageColorSpace = pSurfaceFormat.colorSpace;
	lSwapchainInfo.imageExtent.width = pWidth;
	lSwapchainInfo.imageExtent.height = pHeight;
	lSwapchainInfo.imageArrayLayers = 1;
	lSwapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	lSwapchainInfo.queueFamilyIndexCount = 1;
	//lSwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	lSwapchainInfo.pQueueFamilyIndices = &pFamilyIndex;
	lSwapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	lSwapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;  //VK_PRESENT_MODE_MAILBOX_KHR, tiled device?
	// The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system.You'll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
	lSwapchainInfo.compositeAlpha = surfaceCompositeAlpha;

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

VkFence createFence(VkDevice pDevice)
{
	VkFenceCreateInfo createInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence fence;
	vkCreateFence(pDevice, &createInfo, nullptr, &fence);
	return fence;
}

VkCommandPool createCommandPool(VkDevice pDevice, uint32_t pFamilyIndex)
{
	VkCommandPoolCreateInfo lCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	lCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	lCreateInfo.queueFamilyIndex = pFamilyIndex;
	VkCommandPool lCommandPool;
	VK_CHECK(vkCreateCommandPool(pDevice, &lCreateInfo, nullptr, &lCommandPool));
	return lCommandPool;
}

VkImageView createImageView(VkDevice pDevice, VkImage pImage, VkFormat pFormat)
{
	VkImageViewCreateInfo lImageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	//VkImageViewCreateFlags     flags;
	lImageViewCreateInfo.image = pImage;
	lImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	lImageViewCreateInfo.format = pFormat;
	lImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	lImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	lImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	lImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	lImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	lImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	lImageViewCreateInfo.subresourceRange.levelCount = 1;
	lImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	lImageViewCreateInfo.subresourceRange.layerCount = 1;
	VkImageView imageView;
	VK_CHECK(vkCreateImageView(pDevice, &lImageViewCreateInfo, nullptr, &imageView));
	return imageView;
}

VkRenderPass createRenderPass(VkDevice pDevice, VkFormat pFormat)
{
	VkAttachmentDescription attachmentDesc[1] = {};
	attachmentDesc[0].flags;
	attachmentDesc[0].format = pFormat; // swapChainImageFormat
	attachmentDesc[0].samples = VK_SAMPLE_COUNT_1_BIT; // no msaa
	attachmentDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//Using VK_IMAGE_LAYOUT_UNDEFINED for initialLayout means that we don't care
	// what previous layout the image was in. The caveat of this special value is 
	// that the contents of the image are not guaranteed to be preserved,
	// but that doesn't matter since we're going to clear it anyway
	attachmentDesc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//attachmentDesc[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // require a imageBarrier layout transition before vkCmdBeginRenderPass
	attachmentDesc[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;	// Index in VkRenderPassCreateInfo::pAttachments
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDesc = {};
	//VkSubpassDescriptionFlags       flags;
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	//subpassDesc.inputAttachmentCount = 1;	// Attachments that are read from a shader
	//subpassDesc.pInputAttachments = &colorAttachmentRef;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorAttachmentRef;
	//subpassDesc.pResolveAttachments;	// Attachments used for multisampling color attachments
	//subpassDesc.pDepthStencilAttachment;
	//uint32_t                        preserveAttachmentCount;	// Attachments that are not used by this subpass, but for which the data must be preserved
	//const uint32_t* pPreserveAttachments;

	// Explicit dependency

	// How to fill this to be ready for presentation?
	// No need this for only one pass
	VkSubpassDependency dependencies[2] = { };
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// implicitly defined dependency would cover this, but let's replace it with this explicitly defined dependency!
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = 0;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	//VkRenderPassCreateFlags           flags;
	createInfo.attachmentCount = 1;
	createInfo.pAttachments = attachmentDesc;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpassDesc;
	createInfo.dependencyCount = ARRAY_COUNT(dependencies);
	createInfo.pDependencies = dependencies;

	VkRenderPass renderPass;
	VK_CHECK(vkCreateRenderPass(pDevice, &createInfo, nullptr, &renderPass));

	return renderPass;
}

VkFramebuffer createFramebuffer(VkDevice pDevice, VkRenderPass pRenderPass, VkImageView* imageViews, uint32_t imageViewCount, uint32_t pWidth, uint32_t pHeight)
{
	VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	//VkFramebufferCreateFlags    flags;
	createInfo.renderPass = pRenderPass;
	createInfo.attachmentCount = imageViewCount;
	createInfo.pAttachments = imageViews;
	createInfo.width = pWidth;
	createInfo.height = pHeight;
	createInfo.layers = 1;

	VkFramebuffer framebuffer;
	VK_CHECK(vkCreateFramebuffer(pDevice, &createInfo, nullptr, &framebuffer));

	return framebuffer;
}

VkShaderModule loadShader(VkDevice pDevice, const char* pFilename)
{	
	//char path[256];
	//DWORD r = GetCurrentirectory(256, path);

	FILE* file = fopen(pFilename, "rb");
	if (file)
	{
		fseek(file, 0, SEEK_END);
		long bytesSize = ftell(file);
		assert(bytesSize);
		fseek(file, 0, SEEK_SET);

		char* code = (char*)malloc(bytesSize);
		assert(code);
		size_t bytesRead = fread(code, 1, bytesSize, file);
		assert(bytesRead == bytesSize);
		assert(bytesRead % 4 == 0);

		VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		//VkShaderModuleCreateFlags    flags;
		createInfo.codeSize = bytesSize;			// size in bytes
		createInfo.pCode = (const uint32_t*)code;	// must be an array of codeSize/4

		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(pDevice, &createInfo, nullptr, &shaderModule));

		free(code);

		return shaderModule;
	}

	return VK_NULL_HANDLE;
}

VkPipelineLayout createPipelineLayout(VkDevice pDevice)
{
	VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	//VkPipelineLayoutCreateFlags     flags;
	//uint32_t                        setLayoutCount;
	//const VkDescriptorSetLayout* pSetLayouts;
	//uint32_t                        pushConstantRangeCount;
	//const VkPushConstantRange* pPushConstantRanges;

	VkPipelineLayout layout;
	VK_CHECK(vkCreatePipelineLayout(pDevice, &createInfo, nullptr, &layout));
	return layout;
}

VkPipeline createGraphicsPipeline(VkDevice pDevice, VkPipelineCache pPipelineCache, VkPipelineLayout pPipelineLayout, VkRenderPass pRenderPass, VkShaderModule pVertexShader, VkShaderModule pFragmentShader)
{
	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	//const void* pNext;
	//VkPipelineShaderStageCreateFlags    flags;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = pVertexShader;
	stages[0].pName = "main";
	//const VkSpecializationInfo* pSpecializationInfo;

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = pFragmentShader;
	stages[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	//VkPipelineVertexInputStateCreateFlags       flags;
	//uint32_t                                    vertexBindingDescriptionCount;
	//const VkVertexInputBindingDescription* pVertexBindingDescriptions;
	//uint32_t                                    vertexAttributeDescriptionCount;
	//const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	//VkPipelineInputAssemblyStateCreateFlags    flags;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = 0;

	VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	//VkPipelineViewportStateCreateFlags    flags;
	viewportState.viewportCount = 1;
	//const VkViewport* pViewports;
	viewportState.scissorCount = 1;
	//const VkRect2D* pScissors;

	VkPipelineRasterizationStateCreateInfo rasterState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	//VkPipelineRasterizationStateCreateFlags    flags;
	rasterState.depthClampEnable = VK_FALSE;
	rasterState.rasterizerDiscardEnable = VK_FALSE;
	rasterState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterState.cullMode = VK_CULL_MODE_NONE;
	rasterState.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterState.depthBiasEnable = VK_FALSE;
	rasterState.depthBiasConstantFactor = 0.0;;
	rasterState.depthBiasClamp = 0.0;
	rasterState.depthBiasSlopeFactor = 0.0;
	rasterState.lineWidth = 1.0;

	VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };	
	//VkPipelineMultisampleStateCreateFlags    multisampleState.flags;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.sampleShadingEnable = VK_FALSE;
	//float                                    multisampleState.minSampleShading = 1.0;
	//const VkSampleMask* multisampleState.pSampleMask;
	multisampleState.alphaToCoverageEnable = VK_FALSE;
	multisampleState.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	//VkPipelineDepthStencilStateCreateFlags    flags;
	//VkBool32                                  depthTestEnable;
	//VkBool32                                  depthWriteEnable;
	//VkCompareOp                               depthCompareOp;
	//VkBool32                                  depthBoundsTestEnable;
	//VkBool32                                  stencilTestEnable;
	//VkStencilOpState                          front;
	//VkStencilOpState                          back;
	depthStencilState.minDepthBounds = 0.0f;
	depthStencilState.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	//VkBool32                 blendEnable;
	//VkBlendFactor            srcColorBlendFactor;
	//VkBlendFactor            dstColorBlendFactor;
	//VkBlendOp                colorBlendOp;
	//VkBlendFactor            srcAlphaBlendFactor;
	//VkBlendFactor            dstAlphaBlendFactor;
	//VkBlendOp                alphaBlendOp;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	//VkPipelineColorBlendStateCreateFlags          flags;
	//VkBool32                                      logicOpEnable = VK_FALSE;
	//VkLogicOp                                     logicOp;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorBlendAttachment;
	//float                                         blendConstants[4];


	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	//VkPipelineDynamicStateCreateFlags    flags;
	dynamicState.dynamicStateCount = ARRAY_COUNT(dynamicStates);
	dynamicState.pDynamicStates = dynamicStates;

	
	VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	//VkPipelineCreateFlags                            flags;
	createInfo.stageCount = ARRAY_COUNT(stages);
	createInfo.pStages = stages;
	createInfo.pVertexInputState = &vertexInput;
	createInfo.pInputAssemblyState = &inputAssembly;
	//const VkPipelineTessellationStateCreateInfo* pTessellationState;
	createInfo.pViewportState = &viewportState;
	createInfo.pRasterizationState = &rasterState;
	createInfo.pMultisampleState = &multisampleState;
	createInfo.pDepthStencilState = &depthStencilState;
	createInfo.pColorBlendState = &colorBlendState;
	createInfo.pDynamicState = &dynamicState;
	createInfo.layout = pPipelineLayout;
	createInfo.renderPass = pRenderPass;
	//uint32_t                                         createInfo.subpass;
	//VkPipeline                                       createInfo.basePipelineHandle;
	//int32_t                                          createInfo.basePipelineIndex;

	VkPipeline pipeline = 0;
	VK_CHECK(vkCreateGraphicsPipelines(pDevice, pPipelineCache, 1, &createInfo, nullptr, &pipeline));
	return pipeline;
}

void window_size_callback(GLFWwindow* window, int width, int height)
{

}

VkImageMemoryBarrier imageBarrier(VkImage pImage, 
	VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
	VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = pImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // shorcut as we onl have color at the moment
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;	// Seem android have bug with this constant
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return barrier;
}

// You should ONLY USE THIS FOR DEBUGGING - this is not something that should ever ship in real code, this will flushand invalidate all cachesand stall everything, it is a tool not to be used lightly!
// That said, it can be really handy if you think you have a race condition in your appand you just want to serialize everything so you can debug it.
// Note that this does not take care of image layouts - if you're debugging you can set the layout of all your images to GENERAL to overcome this, but again - do not do this in release code!
/*
void fullMemoryBarrier(VkCommandBuffer pCommandBuffer)
{
	VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	memoryBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
		VK_ACCESS_INDEX_READ_BIT |
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
		VK_ACCESS_UNIFORM_READ_BIT |
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_TRANSFER_READ_BIT |
		VK_ACCESS_TRANSFER_WRITE_BIT |
		VK_ACCESS_HOST_READ_BIT |
		VK_ACCESS_HOST_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
		VK_ACCESS_INDEX_READ_BIT |
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
		VK_ACCESS_UNIFORM_READ_BIT |
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_TRANSFER_READ_BIT |
		VK_ACCESS_TRANSFER_WRITE_BIT |
		VK_ACCESS_HOST_READ_BIT |
		VK_ACCESS_HOST_WRITE_BIT;

	vkCmdPipelineBarrier(pCommandBuffer,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // srcStageMask
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dstStageMask
		,									// dependencyFlags
		1,                                  // memoryBarrierCount
		&memoryBarrier,                     // pMemoryBarriers
		0,
		nullptr);
}
*/

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
	setupDebugMessenger(lVulkanInstance);
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

	glfwSetWindowSizeCallback(lWindow, window_size_callback);

	VkSurfaceKHR lSurface = createSurface(lVulkanInstance, lWindow);

	VkBool32 lPresentationSupported = false;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(lPhysicalDevice, lQueueFamilyIndex, lSurface, &lPresentationSupported));
	assert(lPresentationSupported);

	VkSurfaceCapabilitiesKHR lSurfaceCaps;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(lPhysicalDevice, lSurface, &lSurfaceCaps));

	uint32_t lFormatCount;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(lPhysicalDevice, lSurface, &lFormatCount, nullptr));
	VkSurfaceFormatKHR* lFormats = (VkSurfaceFormatKHR*)_alloca(lFormatCount * sizeof(VkSurfaceFormatKHR));	
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(lPhysicalDevice, lSurface, &lFormatCount, lFormats));
	VkSurfaceFormatKHR lSurfaceFormat = lFormats[0]; // todo : clean that
	uint32_t lPresentModeCount;
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(lPhysicalDevice, lSurface, &lPresentModeCount, nullptr));
	VkPresentModeKHR* lPresentModes = (VkPresentModeKHR*)_alloca(lPresentModeCount * sizeof(VkPresentModeKHR));
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(lPhysicalDevice, lSurface, &lPresentModeCount, lPresentModes));

	// Create the swap chain
	int lWindowWidth = 0, lWindowHeight = 0;
	glfwGetWindowSize(lWindow, &lWindowWidth, &lWindowHeight); // TODO: shortcut, window size and swapchain image size may be different !!!
	uint32_t lSwapchainImageCount = 2;
	VkSwapchainKHR lSwapChain = createSwapchain(lDevice, lSurface, lSurfaceFormat, lSurfaceCaps, lQueueFamilyIndex, lWindowWidth, lWindowHeight, lSwapchainImageCount);

	VkImage lSwapChainImages[16] = {};
	VK_CHECK(vkGetSwapchainImagesKHR(lDevice, lSwapChain, &lSwapchainImageCount, nullptr));
	VK_CHECK(vkGetSwapchainImagesKHR(lDevice, lSwapChain, &lSwapchainImageCount, lSwapChainImages));

	VkImageView lSwapChainImageViews[16] = {};
	for (uint32_t i = 0; i < lSwapchainImageCount; ++i)
	{
		lSwapChainImageViews[i] = createImageView(lDevice, lSwapChainImages[i], lSurfaceFormat.format);
	}

	VkRenderPass renderPass = createRenderPass(lDevice, lSurfaceFormat.format);

	VkFramebuffer swapChainframebuffers[16] = {};
	for (uint32_t i = 0; i < lSwapchainImageCount; ++i)
	{
		swapChainframebuffers[i] = createFramebuffer(lDevice, renderPass, &lSwapChainImageViews[i], 1, lWindowWidth, lWindowHeight);
	}
	
	VkShaderModule vertexShader = loadShader(lDevice, "../../Shaders/triangle.vert.spv");
	VkShaderModule fragmentShader = loadShader(lDevice, "../../Shaders/triangle.frag.spv");


	VkPipelineLayout triangleLayout = createPipelineLayout(lDevice);

	VkPipelineCache pipelineCache = 0;
	VkPipeline trianglePipeline = createGraphicsPipeline(lDevice, pipelineCache, triangleLayout, renderPass, vertexShader, fragmentShader);


	// GraphicsPipeline
	// VS/FS program
	// VertexInputLayout
	// FixedState (Blend/Rasterizer)

	// Compile GLSL to spirv



	VkSemaphore lAcquireSemaphore = createSemaphore(lDevice);
	VkSemaphore lReleaseSemaphore = createSemaphore(lDevice);

	VkCommandPool lCommandPool = createCommandPool(lDevice, lQueueFamilyIndex);


	const uint32_t COMMAND_BUFFER_COUNT = 2;

	VkCommandBufferAllocateInfo lAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	lAllocateInfo.commandPool = lCommandPool;
	lAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	lAllocateInfo.commandBufferCount = COMMAND_BUFFER_COUNT;
	VkCommandBuffer commandBuffers[COMMAND_BUFFER_COUNT];
	VK_CHECK(vkAllocateCommandBuffers(lDevice, &lAllocateInfo, commandBuffers));

	VkFence commandBufferFence[COMMAND_BUFFER_COUNT] = {};
	for (int i = 0; i < COMMAND_BUFFER_COUNT; ++i)
	{
		commandBufferFence[i] = createFence(lDevice);
	}
	

	// MainLoop
	uint32_t commandBufferIndex = COMMAND_BUFFER_COUNT-1;
	while (!glfwWindowShouldClose(lWindow))
	{
		glfwPollEvents();

		commandBufferIndex = (++commandBufferIndex) % COMMAND_BUFFER_COUNT;
		VK_CHECK(vkWaitForFences(lDevice, 1, &commandBufferFence[commandBufferIndex], VK_TRUE, UINT64_MAX));
		VK_CHECK(vkResetFences(lDevice, 1, &commandBufferFence[commandBufferIndex]));

		uint32_t lImageIndex = 0;
		VK_CHECK(vkAcquireNextImageKHR(lDevice, lSwapChain, ~0ull, lAcquireSemaphore, VK_NULL_HANDLE, &lImageIndex));

		//VK_CHECK(vkResetCommandBuffer(commandBuffers[commandBufferIndex], 0));
		//VK_CHECK(vkResetCommandPool(lDevice, lCommandPool, 0));

		VkCommandBufferBeginInfo lBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		lBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(commandBuffers[commandBufferIndex], &lBeginInfo));

		VkClearValue lClearColor = { 0.3f, 0.2f, 0.3f, 1.0f };

		VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChainframebuffers[lImageIndex];
		renderPassInfo.renderArea = { {0,0} , {(uint32_t)lWindowWidth, (uint32_t)lWindowHeight} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &lClearColor;

		// barrier not needed, LayoutTransition are done during BeginRenderPass/EndRenderPass
		//VkImageMemoryBarrier renderBeginBarrier = imageBarrier(lSwapChainImages[lImageIndex], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		//vkCmdPipelineBarrier(lCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);

		vkCmdBeginRenderPass(commandBuffers[commandBufferIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = { 0.0f, 0.0f,(float)lWindowWidth, (float)lWindowHeight, 0.0f, 1.0f };
		//VkViewport viewport = { 0.0f,(float)lWindowHeight,(float)lWindowWidth, -(float)lWindowHeight, 0.0f, 1.0f }; // Vulkan 1_1 extension to reverse Y axis
		vkCmdSetViewport(commandBuffers[commandBufferIndex], 0, 1, &viewport);

		VkRect2D scissor = { {0,0}, {(uint32_t)lWindowWidth,(uint32_t)lWindowHeight} };
		vkCmdSetScissor(commandBuffers[commandBufferIndex], 0, 1, &scissor);

		// Draw stuff here
		vkCmdBindPipeline(commandBuffers[commandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
		vkCmdDraw(commandBuffers[commandBufferIndex], 3, 1, 0, 0);

		//VkImageMemoryBarrier presentBarrier = imageBarrier(lSwapChainImages[lImageIndex], 0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		//vkCmdPipelineBarrier(lCommandBuffer, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &presentBarrier);

		vkCmdEndRenderPass(commandBuffers[commandBufferIndex]);
		VK_CHECK(vkEndCommandBuffer(commandBuffers[commandBufferIndex]));


		// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
		VkPipelineStageFlags lSubmitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // why
		VkSubmitInfo lSubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		lSubmitInfo.waitSemaphoreCount = 1;
		lSubmitInfo.pWaitSemaphores = &lAcquireSemaphore;
		lSubmitInfo.pWaitDstStageMask = &lSubmitStageMask;
		lSubmitInfo.commandBufferCount = 1;
		lSubmitInfo.pCommandBuffers = &commandBuffers[commandBufferIndex];
		lSubmitInfo.signalSemaphoreCount = 1;
		lSubmitInfo.pSignalSemaphores = &lReleaseSemaphore;
		VK_CHECK(vkQueueSubmit(lGraphicsQueue, 1, &lSubmitInfo, commandBufferFence[commandBufferIndex]));

		VkPresentInfoKHR lPresentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		lPresentInfo.waitSemaphoreCount = 1;
		lPresentInfo.pWaitSemaphores = &lReleaseSemaphore;
		lPresentInfo.swapchainCount = 1;		// one per window?
		lPresentInfo.pSwapchains = &lSwapChain;
		lPresentInfo.pImageIndices = &lImageIndex;
		//VkResult* pResults;
		VK_CHECK(vkQueuePresentKHR(lGraphicsQueue, &lPresentInfo));

		// TODO : Barrier/Fence on CommandBuffer or DoubleBuffered CommandBuffer with sync ?
		//VK_CHECK(vkDeviceWaitIdle(lDevice));
	}
	
	VK_CHECK(vkDeviceWaitIdle(lDevice));

	// TODO : destroy ressources

	vkDestroyInstance(lVulkanInstance, nullptr);

	glfwDestroyWindow(lWindow);
	glfwTerminate();
	return 0;
}