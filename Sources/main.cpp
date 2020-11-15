
#include "vk_common.h"
#include "Shaders.h"

#include <algorithm>
#include <vector>

#include <assert.h>
#include <malloc.h>
#include <stdio.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <meshoptimizer.h>


#define FAST_OBJ_IMPLEMENTATION
#include <../meshoptimizer/extern/fast_obj.h>

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanHelper.h"

struct vec4
{
	float data[4];
	float& operator[](uint32_t i) { return data[i];  }
};

struct mat4
{
	union
	{
		float data[16];
		vec4 vdata[4];
	};
	//float& operator[](uint32_t i) { return data[i]; }
	vec4& operator[](uint32_t i) { return vdata[i]; }
};
#include <../../Shaders/mesh.h>


// Test image
uint8_t rgb2x2[][3] =
{
	{ 0xFF, 0x00, 0x00 }, { 0x00, 0xFF, 0x00 },
	{ 0x00, 0x00, 0xFF }, { 0xFF, 0xFF, 0xFF }
};

uint8_t rgba2x2[][4] =
{
	{ 0xFF, 0x00, 0x00, 0xFF }, { 0x00, 0xFF, 0x00, 0xFF },
	{ 0x00, 0x00, 0xFF, 0xFF }, { 0xFF, 0xFF, 0xFF, 0xFF }
};

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

/******************************************************************************/
VkDebugUtilsMessengerEXT gDebugMessenger;
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
	void* pUserData)
{
	const char* errorTypeStr =
		(messageType & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		? "[VERBOSE] "
		: (messageType & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		? "[INFO] "
		: (messageType & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		? "[WARNING] "
		: (messageType & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		? "[ERROR] "
		: "[UNKNOW] ";


	printf("%s%s\n", errorTypeStr, pCallbackData->pMessage);

#if WIN32
	// MSVC ouput
	OutputDebugString(errorTypeStr); OutputDebugString(pCallbackData->pMessage); OutputDebugString("\n");
#endif

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		assert(!"ERROR");

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

VkSwapchainKHR createSwapchain(VkDevice pDevice, VkSurfaceKHR pSurface, VkSurfaceFormatKHR pSurfaceFormat, VkSurfaceCapabilitiesKHR pSurfaceCaps, uint32_t pFamilyIndex, uint32_t pWidth, uint32_t pHeight, uint32_t lSwapchainImageCount, VkSwapchainKHR pOldSwapChain)
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

	// In FIFO mode the presentation requests are stored in a queue.
	// If the queue is full the application will have to wait until an image is ready to be acquired again.
	// This is a normal operating mode for mobile, which automatically locks the framerate to 60 FPS.


	lSwapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;  //VK_PRESENT_MODE_MAILBOX_KHR, tiled device?
	//lSwapchainInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; // no vsync?
	// The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system.You'll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
	lSwapchainInfo.compositeAlpha = surfaceCompositeAlpha;
	lSwapchainInfo.oldSwapchain = pOldSwapChain;

	VkSwapchainKHR lSwapChain;
	VK_CHECK(vkCreateSwapchainKHR(pDevice, &lSwapchainInfo, nullptr, &lSwapChain));
	return lSwapChain;
}

// Find a queue that support graphics operation
bool findQueueFamilyIndex(VkPhysicalDevice pDevice, uint32_t* pQueueFamilyIndex, VkQueueFlags pFlags )
{
	uint32_t lQueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &lQueueFamilyCount, nullptr);

	VkQueueFamilyProperties* lQueueFamilies = STACK_ALLOC(VkQueueFamilyProperties, lQueueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &lQueueFamilyCount, lQueueFamilies);

	for (uint32_t i = 0; i < lQueueFamilyCount; ++i)
	{
		if (lQueueFamilies[i].queueFlags & pFlags)
		{
			*pQueueFamilyIndex = i;

			assert(lQueueFamilies[i].timestampValidBits != 0); // can't use vkCmdWriteTimestamp
			return true;
		}
	}

	assert(false);
	return false;
}

VkQueryPool createQueryPool(VkDevice pDevice, uint32_t pQueryCount)
{
	VkQueryPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	//VkQueryPoolCreateFlags           flags;
	createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	createInfo.queryCount = pQueryCount;
	//VkQueryPipelineStatisticFlags    pipelineStatistics;

	VkQueryPool query;
	VK_CHECK(vkCreateQueryPool(pDevice, &createInfo, nullptr, &query));
	return query;
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
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
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

struct Vec3
{	
	float x, y, z;

	Vec3() {}
	Vec3(float _x, float _y, float _z) { x = _x; y = _y; z = _z; }
	Vec3(const Vec3& other) { x = other.x; y = other.y; z = other.z; }

	inline Vec3& operator-=(const Vec3& other)
	{
		x -= other.x; y -= other.y; z -= other.z;
		return *this;
	}

	inline Vec3 operator-(const Vec3& other) const
	{
		return Vec3(x - other.x, y - other.y, z - other.z);
	}

	inline Vec3& operator+=(const Vec3& other)
	{
		x += other.x; y += other.y; z += other.z;
		return *this;
	}

	inline Vec3 operator+(const Vec3& other) const
	{
		return Vec3( x + other.x, y + other.y, z + other.z );
	}

	inline Vec3& operator*=(float scalar)
	{
		x *= scalar; y *= scalar; z *= scalar;
		return *this;
	}

	inline Vec3 operator*(float scalar) const
	{
		return Vec3(x * scalar, y * scalar, z * scalar);
	}

	inline Vec3& operator/=(float scalar)
	{
		x /= scalar; y /= scalar; z /= scalar;
		return *this;
	}

	inline Vec3 operator/(float scalar) const
	{
		return Vec3(x / scalar, y / scalar, z / scalar);
	}
};

// AABB
struct Box
{
	Box() {};
	Vec3 min, max;

	Vec3 getCenter() const
	{
		return (min + max) * 0.5f;
	}

	Vec3 getExtent() const
	{
		return (max - min);
	}

	inline void setInfinite()
	{
		min.x = min.y = min.z = FLT_MIN;
		max.x = max.y = max.z = FLT_MAX;
	}

	inline void setMinMax(const Vec3& p)
	{
		if (p.x < min.x) min.x = p.x;
		if (p.y < min.y) min.y = p.y;
		if (p.z < min.z) min.z = p.z;
		if (p.x > max.x) max.x = p.x;
		if (p.y > max.y) max.y = p.y;
		if (p.z > max.z) max.z = p.z;
	}
};

struct Vertex
{
	float px, py, pz;	// position
	float nx, ny, nz;	// normal
	float tu, tv;		// texture coord
};

// Mesh representation VertexBuffer + IndexBuffer
struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

// Triangle as a mesth
void loadTriangleMesh(Mesh& pMesh)
{
	pMesh.vertices = { {-0.5,-0.5,0.0}, {0.5,-0.5,0.0}, {0.0,0.5,0.0} };
	pMesh.indices = { 0, 1, 2 };
}

// Triangle as a quad textured
void loadQuadMesh(Mesh& pMesh)
{
	pMesh.vertices = { {-0.5,-0.5,0.0,0.0,0.0,0.0,0.0,0.0}, {0.5,-0.5,0.0,0.0,0.0,0.0,1.0,0.0}, {0.5,0.5,0.0,0.0,0.0,0.0,1.0,1.0}, {-0.5,0.5,0.0,0.0,0.0,0.0,0.0,1.0} };
	pMesh.indices = { 0, 1, 2, 0, 2, 3 };
}

// Simple obj mesh loader
bool loadMesh(Mesh& pMesh, const char* pPath, bool pNormalized = false)
{
	static_assert(sizeof(fastObjUInt) == sizeof(uint32_t),"typeid !=");

	fastObjMesh* lMesh = fast_obj_read(pPath);
	if (!lMesh)
		return false;
		
	// Can be different of lMesh->face_count in case of Strip
	size_t triangleCount = 0;
	for (size_t i = 0; i < lMesh->face_count; ++i)
		triangleCount += (lMesh->face_vertices[i] - 2); // Strip
	pMesh.vertices.resize(3 * triangleCount);


	Box boundingBox;
	boundingBox.min = Vec3( FLT_MAX, FLT_MAX, FLT_MAX );
	boundingBox.max = Vec3( FLT_MIN, FLT_MIN, FLT_MIN );


	size_t vertexOffset = 0;
	size_t indexOffset = 0;
	for (uint32_t i = 0; i < lMesh->face_count; ++i)
	{
		for (uint32_t j = 0; j < lMesh->face_vertices[i]; ++j)
		{
			fastObjIndex dataIndex = lMesh->indices[indexOffset + j];

			// Strip triangulation on the fly
			if (j >= 3)
			{
				pMesh.vertices[vertexOffset + 0] = pMesh.vertices[vertexOffset - 3];
				pMesh.vertices[vertexOffset + 1] = pMesh.vertices[vertexOffset - 1];
				vertexOffset += 2;
			}
			Vertex& v = pMesh.vertices[vertexOffset++];
			v.px = lMesh->positions[dataIndex.p * 3 + 0];
			v.py = lMesh->positions[dataIndex.p * 3 + 1];
			v.pz = lMesh->positions[dataIndex.p * 3 + 2];
			v.nx = lMesh->normals[dataIndex.n * 3 + 0];
			v.ny = lMesh->normals[dataIndex.n * 3 + 1];
			v.nz = lMesh->normals[dataIndex.n * 3 + 2];

			v.tu = lMesh->texcoords[dataIndex.t * 3 + 0];
			v.tv = lMesh->texcoords[dataIndex.t * 3 + 1];

			boundingBox.setMinMax({ v.px, v.py, v.pz });
		}
			
		indexOffset += lMesh->face_vertices[i];
	}
	assert(vertexOffset == triangleCount * 3);

	if (pNormalized)
	{
		Vec3 lExtent = boundingBox.getExtent();
		Vec3 lCenter = boundingBox.getCenter();
		float lMaxExtent = std::max(lExtent.x, std::max(lExtent.y, lExtent.z));
		for (Vertex& v : pMesh.vertices)
		{
			Vec3 np = Vec3(v.px, v.py, v.pz) - lCenter;
			np /= lMaxExtent;

			v.px = np.x;
			v.py = np.y;
			v.pz = np.z;
		}
	}



	// Generate useless indices
	pMesh.indices.resize(triangleCount * 3);
	for (int i = 0; i < triangleCount * 3; ++i)
		pMesh.indices[i] = i;


	std::vector<uint32_t> remap(triangleCount * 3);
	size_t lVertexCount = meshopt_generateVertexRemap(remap.data(), 0, remap.size(), pMesh.vertices.data(), remap.size(), sizeof(Vertex));
	std::vector<Vertex> opt_vertices(lVertexCount);
	std::vector<uint32_t> opt_indices(triangleCount * 3);
	meshopt_remapVertexBuffer(opt_vertices.data(), pMesh.vertices.data(), pMesh.vertices.size(), sizeof(Vertex), remap.data());
	meshopt_remapIndexBuffer(opt_indices.data(), pMesh.indices.data(), pMesh.indices.size(), remap.data());

	// Test cache optimizer
	// random triangle order
	//struct Triangle { uint32_t i[3]; };
	//std::random_shuffle((Triangle*)opt_indices.data(), (Triangle*)(opt_indices.data() + opt_indices.size()));

	if
		(1)
	{
		meshopt_optimizeVertexCache(opt_indices.data(), opt_indices.data(), opt_indices.size(), opt_vertices.size());
		meshopt_optimizeVertexFetch(opt_vertices.data(), opt_indices.data(), opt_indices.size(), opt_vertices.data(), opt_vertices.size(), sizeof(Vertex));
	}

	std::swap(opt_vertices, pMesh.vertices);
	std::swap(opt_indices, pMesh.indices);

	fast_obj_destroy(lMesh);
	return true;
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
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // shorcut as we only have color at the moment
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;//VK_REMAINING_MIP_LEVELS;	// Seem android have bug with this constant
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;//VK_REMAINING_ARRAY_LAYERS;

	return barrier;
}

VkBufferMemoryBarrier bufferBarrier(VkBuffer pBuffer, VkAccessFlags pSrcAccessMask, VkAccessFlags pDstAccessMask)
{
	VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
	barrier.srcAccessMask = pSrcAccessMask;
	barrier.dstAccessMask = pDstAccessMask;
	// TODO : check GraphicsQueueIndex == TransfertQueueIndex
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;;
	barrier.buffer = pBuffer;
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;

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

struct Buffer
{
	VkBuffer buffer;
	VkDeviceMemory memory;

	void* data;	// if not null, persistant mapped data (in COHERENT)
	size_t size;
};


void createBuffer(Buffer& result, VulkanDevice& pVulkanDevice, size_t pSize, VkBufferUsageFlags pUsage, VkMemoryPropertyFlags pProperties)
{
	VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	//VkBufferCreateFlags    flags;
	createInfo.size = pSize;
	createInfo.usage = pUsage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;	
	//uint32_t               queueFamilyIndexCount;	// to fill if buffer is used in multiple queue families (VK_SHARING_MODE_CONCURRENT)
	//const uint32_t* pQueueFamilyIndices;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(pVulkanDevice.mLogicalDevice, &createInfo, nullptr, &buffer));

	VkMemoryRequirements memoryRequirements = {};
	vkGetBufferMemoryRequirements(pVulkanDevice.mLogicalDevice, buffer, &memoryRequirements);

	uint32_t memoryTypeIndex = pVulkanDevice.selectMemoryType(memoryRequirements.memoryTypeBits, pProperties);

	VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = memoryTypeIndex;
	VkDeviceMemory memory = {};
	VK_CHECK(vkAllocateMemory(pVulkanDevice.mLogicalDevice, &allocateInfo, nullptr, &memory));

	// Here we can bind multiple data on same buffer with different offset
	// It is recommended practice to allocate bigger chunks of memory and assign parts of them to particular resources
	// Functions that allocate memory blocks, reserve and return parts of them (VkDeviceMemory + offset + size) to the user
	// check vmaAllocator for this job
	VK_CHECK(vkBindBufferMemory(pVulkanDevice.mLogicalDevice, buffer, memory, 0)); // If the offset is non-zero, then it is required to be divisible by memoryRequirements.alignment

	void* data = 0;
	// Here we do Persistent mapping (only if flag HOST_VISIBLE is present)
	// https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/memory_mapping.html
	// note: some AMD device have special chunk of memory with DEVICE_LOCAL + HOST_VISIBLE flags
	if
		(pProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		VK_CHECK(vkMapMemory(pVulkanDevice.mLogicalDevice, memory, 0ull, VK_WHOLE_SIZE, 0ull, &data));
	}

	result.buffer = buffer;
	result.memory = memory;
	result.size = pSize;
	result.data = data;
}


struct Image
{
	VkFormat format;
	VkImage image;
	VkDeviceMemory memory;
	uint32_t width, height;

	void* data; // at the moment persistent map, if not, u have to Map/Unmap in the uploadBufferToImage
	size_t size;
};

void createImage(Image& result, VulkanDevice pDevice, VkFormat pFormat, uint32_t pWidth, uint32_t pHeight)
{
	VkImageCreateInfo lImageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	//const void* pNext;
	//VkImageCreateFlags       flags;
	lImageInfo.imageType = VK_IMAGE_TYPE_2D;
	lImageInfo.format = pFormat;
	lImageInfo.extent.width = pWidth;
	lImageInfo.extent.height = pHeight;
	lImageInfo.extent.depth = 1;
	lImageInfo.mipLevels = 1;
	lImageInfo.arrayLayers = 1;
	lImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	lImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	lImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT for render pass?
	lImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // not shared trough multiple queue
	//uint32_t                 queueFamilyIndexCount; // not need in VK_SHARING_MODE_EXCLUSIVE
	//const uint32_t* pQueueFamilyIndices;
	lImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImage lImage = {};
	VK_CHECK(vkCreateImage(pDevice, &lImageInfo, NULL, &lImage));

	VkMemoryRequirements lImageMemoryRequirements;
	vkGetImageMemoryRequirements(pDevice, lImage, &lImageMemoryRequirements);

	VkMemoryAllocateInfo lImageAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	//VkStructureType    sType;
	//const void* pNext;
	lImageAllocateInfo.allocationSize = lImageMemoryRequirements.size;
	lImageAllocateInfo.memoryTypeIndex = pDevice.selectMemoryType(lImageMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkDeviceMemory lImageMemory;
	VK_CHECK(vkAllocateMemory(pDevice, &lImageAllocateInfo, NULL, &lImageMemory));

	VK_CHECK(vkBindImageMemory(pDevice, lImage, lImageMemory, 0));

	
	result.image = lImage;
	result.memory = lImageMemory;
	result.format = pFormat;	
	result.width = pWidth;
	result.height = pHeight;
	result.data = NULL;
	result.size = lImageMemoryRequirements.size;
}

// Copy pHostData to the pSrc.data stage buffer into pDst using vkCmdCopyBuffer
void uploadBufferToImage(VkDevice pDevice, VkCommandPool pCommandPool, VkCommandBuffer pCommandBuffer, VkQueue pCopyQueue, const Buffer& pSrc, const Image& pDst, void* pHostData, size_t pHostDataSize)
{
#pragma message("TODO : robust way to identify persistent map or not")

	assert(pHostDataSize <= pSrc.size);
	// pDst.data is a persistent mapped buffer
	if
		(pSrc.data)
	{
		// At the moment we conisder the data is already map in VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		memcpy(pSrc.data, pHostData, pHostDataSize);
	}
	else
	{
		void* lData = NULL;
		VK_CHECK(vkMapMemory(pDevice, pSrc.memory, 0, VK_WHOLE_SIZE, 0, &lData));
		memcpy(lData, pHostData, pHostDataSize);
		vkUnmapMemory(pDevice, pSrc.memory);
	}

	VK_CHECK(vkResetCommandPool(pDevice, pCommandPool, 0));
	VK_CHECK(vkResetCommandBuffer(pCommandBuffer, 0));

	VkCommandBufferBeginInfo lBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	lBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(pCommandBuffer, &lBeginInfo));

	VkBufferImageCopy regions;
	regions.bufferOffset = 0;
	regions.bufferRowLength = 0;
	regions.bufferImageHeight = 0;
	regions.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	regions.imageSubresource.mipLevel = 0;
	regions.imageSubresource.baseArrayLayer = 0;
	regions.imageSubresource.layerCount = 1;
	regions.imageOffset = { 0, 0, 0 };
	regions.imageExtent = { pDst.width, pDst.height, 1 };

	
	// In our actual case we are always in VK_IMAGE_LAYOUT_UNDEFINED cause we jsut upload it one time and never touch it
	// Later we should store the layout and create a different barrier.
	// for example if we wan't to update the image, it will be probably in a VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL layout
	VkImageMemoryBarrier lImageBarrier = imageBarrier(pDst.image,	0, VK_ACCESS_TRANSFER_WRITE_BIT,
																	VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkCmdPipelineBarrier(pCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, NULL, 0, NULL, 1, &lImageBarrier);

	vkCmdCopyBufferToImage(pCommandBuffer, pSrc.buffer, pDst.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions);

	lImageBarrier = imageBarrier(pDst.image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#pragma message("TODO : manage texture access in vertex pipeline")
	vkCmdPipelineBarrier(pCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, /*VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |*/ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, NULL, 0, NULL, 1, &lImageBarrier);

	VK_CHECK(vkEndCommandBuffer(pCommandBuffer));


	// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
	VkPipelineStageFlags lSubmitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // why
	VkSubmitInfo lSubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	lSubmitInfo.commandBufferCount = 1;
	lSubmitInfo.pCommandBuffers = &pCommandBuffer;
	VK_CHECK(vkQueueSubmit(pCopyQueue, 1, &lSubmitInfo, nullptr));

#pragma message ("WARNING hard lock to remove")
	VK_CHECK(vkDeviceWaitIdle(pDevice));

}

// Copy pHostData to the pSrc.data stage buffer into pDst using vkCmdCopyBuffer
void uploadBuffer(VkDevice pDevice, VkCommandPool pCommandPool, VkCommandBuffer pCommandBuffer, VkQueue pCopyQueue, const Buffer& pSrc, const Buffer& pDst, const void* pHostData, size_t pHostDataSize)
{
#pragma message("TODO : robust way to identify persistent map or not")
	// pDst.data is a persistent mapped buffer
	if
		(pSrc.data)
	{
		// At the moment we conisder the data is already map in VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		memcpy(pSrc.data, pHostData, pHostDataSize);
	}
	else
	{
		void* lData = NULL;
		VK_CHECK(vkMapMemory(pDevice, pSrc.memory, 0, VK_WHOLE_SIZE, 0, &lData));
		memcpy(lData, pHostData, pHostDataSize);
		vkUnmapMemory(pDevice, pSrc.memory);
	}
	
	VK_CHECK(vkResetCommandPool(pDevice, pCommandPool, 0));
	VK_CHECK(vkResetCommandBuffer(pCommandBuffer, 0));

	VkCommandBufferBeginInfo lBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	lBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(pCommandBuffer, &lBeginInfo));

#pragma message("Should be device data size ?")
	VkBufferCopy regions = { 0, 0, VkDeviceSize(pHostDataSize) };
	vkCmdCopyBuffer(pCommandBuffer, pSrc.buffer, pDst.buffer, 1, &regions);

	VkBufferMemoryBarrier copyBufferBarrier = bufferBarrier(pDst.buffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
	vkCmdPipelineBarrier(pCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &copyBufferBarrier, 0, nullptr);

	VK_CHECK(vkEndCommandBuffer(pCommandBuffer));


	// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
	VkPipelineStageFlags lSubmitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // why
	VkSubmitInfo lSubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	lSubmitInfo.commandBufferCount = 1;
	lSubmitInfo.pCommandBuffers = &pCommandBuffer;
	VK_CHECK(vkQueueSubmit(pCopyQueue, 1, &lSubmitInfo, nullptr));

#pragma message ("WARNING hard lock to remove")
	VK_CHECK(vkDeviceWaitIdle(pDevice));
}

void destroyBuffer(VkDevice pDevice, const Buffer& pBuffer)
{
	// do we need to unmap?
	//vkUnmapMemory(pDevice, pBuffer.memory); // for now we use persistent mapping

	vkDestroyBuffer(pDevice, pBuffer.buffer, nullptr);
	vkFreeMemory(pDevice, pBuffer.memory, nullptr);
}

void getWindowSize(GLFWwindow* pWindow, uint32_t& pWidth, uint32_t& pHeight)
{
	int lWidth, lHeight;
	glfwGetWindowSize(pWindow, &lWidth, &lHeight);
	pWidth = (uint32_t)lWidth;
	pHeight = (uint32_t)lHeight;
}


void destroyFramebuffers(VkDevice pDevice, std::vector<VkFramebuffer>& pFramebuffers)
{
	for (auto&& lFramebuffer : pFramebuffers)
	{
		vkDestroyFramebuffer(pDevice, lFramebuffer, NULL);
	}
	pFramebuffers.clear();
}

void createSwapchainFramebuffer(VkDevice pDevice, VkRenderPass pRenderPass, VulkanSwapchain& pSwapchain, std::vector<VkFramebuffer>& pFramebuffers)
{	
	assert(pFramebuffers.size() == 0); // possible lost of handle ref
	std::vector<VkFramebuffer> lSwapChainFramebuffer(pSwapchain.imageCount());
	for (uint32_t i = 0; i < pSwapchain.imageCount(); ++i)
	{
		lSwapChainFramebuffer[i] = vkh::createFramebuffer(pDevice, pRenderPass, &pSwapchain.mImageViews[i], 1, pSwapchain.mWidth, pSwapchain.mHeight);
	}

	pFramebuffers.swap(lSwapChainFramebuffer);
}

// Entry point
int main(int argc, const char* argv[])
{
	// Initial windows configuration
	uint32_t lWindowWidth = 512, lWindowHeight = 512;
	uint32_t lDesiredSwapchainImageCount = 2;
	bool lDesiredVSync = false;

	VK_CHECK(volkInitialize());

	printf("Hello VulkanRenderer\n");
	int lSuccess = glfwInit();
	assert(lSuccess);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// MUST BE SET or SwapChain creation fail
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);


	VulkanContext lVulkanInstance;
	lVulkanInstance.createInstance();
	lVulkanInstance.enumeratePhysicalDevices();
	VkPhysicalDevice lPhysicalDevice = lVulkanInstance.pickPhysicalDevice(VK_QUEUE_GRAPHICS_BIT /*| VK_QUEUE_TRANSFER_BIT*/);

#ifdef _DEBUG
	registerDebugMessenger(lVulkanInstance);
#endif

	VulkanDevice lDevice(lPhysicalDevice);
	lDevice.createLogicalDevice(VK_QUEUE_GRAPHICS_BIT /*| VK_QUEUE_TRANSFER_BIT*/); // For the moment use only one queue

	// Surface creation are platform specific
	GLFWwindow* lWindow = glfwCreateWindow(lWindowWidth, lWindowHeight, "VulkanRenderer", 0, 0);
	assert(lWindow);
	glfwSetWindowSizeCallback(lWindow, window_size_callback);


	
	getWindowSize(lWindow, lWindowWidth, lWindowHeight);

	VulkanSwapchain  lVulkanSwapchain(&lVulkanInstance, &lDevice);
	lVulkanSwapchain.initSurface((void*)GetModuleHandle(0), (void*)glfwGetWin32Window(lWindow));
	lVulkanSwapchain.createSwapchain(lWindowWidth, lWindowHeight, lDesiredSwapchainImageCount, lDesiredVSync);



	VkRenderPass lRenderPass = vkh::createRenderPass(lDevice, lVulkanSwapchain.mColorFormat);

	
	/*
	VkShaderModule lVertexShader = loadShader(lDevice, "../../Shaders/triangle.vert.glsl.spv");
	VkShaderModule lFragmentShader = loadShader(lDevice, "../../Shaders/triangle.frag.glsl.spv");


	// No vb/ib, let it empty
	VkPipelineVertexInputStateCreateInfo lTriangleVertexInputCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	//uint32_t                                    vertexBindingDescriptionCount;
	//const VkVertexInputBindingDescription* pVertexBindingDescriptions;
	//uint32_t                                    vertexAttributeDescriptionCount;
	//const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;

	VkPipelineLayout lTriangleLayout = createPipelineLayout(lDevice);
	VkPipelineCache lPipelineCache = 0;
	VkPipeline lTrianglePipeline = createGraphicsPipeline(lDevice, lPipelineCache, lTriangleLayout, lRenderPass, lVertexShader, lFragmentShader, lTriangleVertexInputCreateInfo);
	*/



	// Create need mesh ressources
	Mesh lMesh;
	//loadTriangleMesh(lMesh);
	loadQuadMesh(lMesh);
 	//bool lResult = loadMesh(lMesh, R"path(E:\Data\obj\bicycle.obj)path", true);	
	//bool lResult = loadMesh(lMesh, R"path(F:\Data\Models\stanford\kitten.obj)path");	
	//bool lResult = loadMesh(lMesh, R"path(F:\Data\Models\stanford\debug.obj)path");
	//assert(lResult);


	size_t lChunkSize = 16 * 1024 * 1024;


	// TODO : a stage buffer to copy data from CPU to GPU(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) memory trough vkCmdCopyBuffer
	// At the moment
	Buffer lStageBuffer = {};
	createBuffer(lStageBuffer, lDevice, lChunkSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	Buffer lMeshVertexBuffer = {};
	createBuffer(lMeshVertexBuffer, lDevice, lChunkSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	Buffer lMeshIndexBuffer = {};
	createBuffer(lMeshIndexBuffer, lDevice, lChunkSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkVertexInputBindingDescription binding = {};
	binding.binding = 0;
	binding.stride = sizeof(Vertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attrs[3] = {};
	// 3 float position
	attrs[0].location = 0;
	attrs[0].binding = 0;
	attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrs[0].offset = 0;

	// 3 float normal
	attrs[1].location = 1;
	attrs[1].binding = 0;
	attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrs[1].offset = attrs[0].offset + 3 * sizeof(float);

	// 2 float tex coord
	attrs[2].location = 2;
	attrs[2].binding = 0;
	attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
	attrs[2].offset = attrs[1].offset + 3 * sizeof(float);

	VkPipelineVertexInputStateCreateInfo lMeshVertexInputCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	//lMeshVertexInputCreateInfo.flags;
	lMeshVertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	lMeshVertexInputCreateInfo.pVertexBindingDescriptions = &binding;
	lMeshVertexInputCreateInfo.vertexAttributeDescriptionCount = 3;
	lMeshVertexInputCreateInfo.pVertexAttributeDescriptions = attrs;

	Shader lMeshVertexShader;
	lSuccess = loadShader(lMeshVertexShader, lDevice, "../../Shaders/mesh.vert.glsl.spv");
	assert(lSuccess && "Can't load vertex program");
	

	Shader lMeshFragmentShader;
	lSuccess = loadShader(lMeshFragmentShader, lDevice, "../../Shaders/mesh.frag.glsl.spv");
	assert(lSuccess && "Can't load fragment program");


	// Create Image	
	Image lImage;
	createImage(lImage, lDevice, VK_FORMAT_R8G8B8A8_SRGB, 2, 2);

	// Texture
	

	



	// For uniform?
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // VK_SHADER_STAGE_ALL_GRAPHICS (opengl fashion?)
	uboLayoutBinding.pImmutableSamplers = nullptr; // Optional The pImmutableSamplers field is only relevant for image sampling related descriptors

	VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboLayoutBinding;

	// Double buffered
	// TODO : as an exercise, allocate Only one UBO and use offset
	std::vector<Buffer> uniformBuffers(lVulkanSwapchain.imageCount());
	for (uint32_t i = 0; i < lVulkanSwapchain.imageCount(); ++i)
	{
		createBuffer(uniformBuffers[i], lDevice, sizeof(Object), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		((Object*)uniformBuffers[i].data)->color[0] = 1.0f;
		((Object*)uniformBuffers[i].data)->color[1] = 0.0f;
		((Object*)uniformBuffers[i].data)->color[2] = 0.0f;
		((Object*)uniformBuffers[i].data)->color[3] = 1.0f;
	}

	VkDescriptorSetLayout descriptorSetLayout;
	VK_CHECK(vkCreateDescriptorSetLayout(lDevice, &layoutInfo, nullptr, &descriptorSetLayout));

	// HERE we need to created pool of each kind of desciptor type (UBO/SBO/constant look at VkDescriptorType)
	// And for each SwapChainImage for double buffer (not sure????)
	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(lVulkanSwapchain.imageCount());

	VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	//The structure has an optional flag similar to command pools that determines if individual descriptor sets can be freed or not: VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT.
	// We're not going to touch the descriptor set after creating it, so we don't need this flag.You can leave flags to its default value of 0.
	//VkDescriptorPoolCreateFlags    flags;
	poolInfo.maxSets = lVulkanSwapchain.imageCount(); // can't access the same set from multiple CommandBuffer? creater one for each CommandBuffer
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;

	VkDescriptorPool descriptorPool;
	VK_CHECK(vkCreateDescriptorPool(lDevice, &poolInfo, nullptr, &descriptorPool));

	// In our case we will create one descriptor set for each swap chain image, all with the same layout.
	// Unfortunately we do need all the copies of the layout because the next function expects an array matching the number of sets.
	std::vector<VkDescriptorSetLayout> layouts(lVulkanSwapchain.imageCount(), descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = lVulkanSwapchain.imageCount();
	allocInfo.pSetLayouts = layouts.data();

	std::vector<VkDescriptorSet> descriptorSets;
	descriptorSets.resize(lVulkanSwapchain.imageCount());
	VK_CHECK(vkAllocateDescriptorSets(lDevice, &allocInfo, descriptorSets.data()));

	for (size_t i = 0; i < lVulkanSwapchain.imageCount(); i++) {
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = uniformBuffers[i].buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(Object);

		VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		descriptorWrite.dstSet = descriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;
		descriptorWrite.pImageInfo = nullptr; // Optional
		descriptorWrite.pTexelBufferView = nullptr; // Optional

		vkUpdateDescriptorSets(lDevice, 1, &descriptorWrite, 0, nullptr);
	}


	// HERE DESCRIPTOR LABOR END


	/*
	VkDescriptorSetLayout meshDescriptSetLayout = createDescriptorSetLayout(lDevice);

	VkPipelineLayoutCreateInfo meshLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	//VkPipelineLayoutCreateFlags     flags;
	meshLayoutCreateInfo.setLayoutCount = 1;
	meshLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
	//uint32_t                        pushConstantRangeCount;
	//const VkPushConstantRange* pPushConstantRanges;

	VkPipelineLayout lMeshLayout;
	VK_CHECK(vkCreatePipelineLayout(lDevice, &meshLayoutCreateInfo, nullptr, &lMeshLayout));
	*/

	VkDescriptorSetLayout meshDescriptorSetLayout = createDescriptorSetLayout(lDevice);
	VkPipelineLayout lMeshLayout = createPipelineLayout(lDevice, 1, &meshDescriptorSetLayout, 0, NULL);
	VkDescriptorUpdateTemplate meshUpdateTpl = createDescriptorUpdateTemplate(lDevice, VK_PIPELINE_BIND_POINT_GRAPHICS, lMeshLayout, meshDescriptorSetLayout);
	// TODO : check if safe to destroy here
	//vkDestroyDescriptorSetLayout(lDevice, meshDescriptorSetLayout, NULL);
	
	VkPipeline lMeshPipeline;
	VkPipelineCache lPipelineCache = 0; // TODO : learn that
	lMeshPipeline = createGraphicsPipeline(lDevice, lPipelineCache, lMeshLayout, lRenderPass, lMeshVertexShader, lMeshFragmentShader, lMeshVertexInputCreateInfo);

	//VkPipelineLayout lTriangleLayout = createPipelineLayout(lDevice);
	//VkPipelineCache lPipelineCache = 0;


	VkSemaphore lAcquireSemaphore = vkh::createSemaphore(lDevice);
	VkSemaphore lReleaseSemaphore = vkh::createSemaphore(lDevice);

	VkCommandPool lCommandPool = vkh::createCommandPool(lDevice, lDevice.getQueueFamilyIndex(VulkanQueueType::Graphics));


	const uint32_t COMMAND_BUFFER_COUNT = 2;
	VkCommandBufferAllocateInfo lAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	lAllocateInfo.commandPool = lCommandPool;
	lAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	lAllocateInfo.commandBufferCount = COMMAND_BUFFER_COUNT;
	VkCommandBuffer lCommandBuffers[COMMAND_BUFFER_COUNT];
	VK_CHECK(vkAllocateCommandBuffers(lDevice, &lAllocateInfo, lCommandBuffers));

	VkFence lCommandBufferFences[COMMAND_BUFFER_COUNT] = {};
	for (int i = 0; i < COMMAND_BUFFER_COUNT; ++i)
	{
		lCommandBufferFences[i] = vkh::createFence(lDevice);
	}

	uint32_t lQueryCount = 16;
	VkQueryPool lTimeStampQueries = createQueryPool(lDevice, lQueryCount);
	
	// At the moment do a hard lock upload
	uploadBuffer(lDevice, lCommandPool, lCommandBuffers[0], lDevice.getQueue(VulkanQueueType::Transfert), lStageBuffer, lMeshVertexBuffer, (void*)lMesh.vertices.data(), lMesh.vertices.size() * sizeof(Vertex));
	uploadBuffer(lDevice, lCommandPool, lCommandBuffers[0], lDevice.getQueue(VulkanQueueType::Transfert), lStageBuffer, lMeshIndexBuffer, (void*)lMesh.indices.data(), lMesh.indices.size() * sizeof(uint32_t));
	uploadBufferToImage(lDevice, lCommandPool, lCommandBuffers[0], lDevice.getQueue(VulkanQueueType::Transfert), lStageBuffer, lImage, rgba2x2, sizeof(rgba2x2));

	uint64_t frameCount = 0;
	double cpuTotalTime = 0.0;
	uint64_t gpuTotalTime = 0;
	double cpuTimeAvg = 0;


	std::vector<VkFramebuffer> lSwapchainFramebuffers;
	createSwapchainFramebuffer(lDevice, lRenderPass, lVulkanSwapchain, lSwapchainFramebuffers);


	// MainLoop
	uint32_t lCommandBufferIndex = COMMAND_BUFFER_COUNT-1;
	while (!glfwWindowShouldClose(lWindow))
	{
		double cpuFrameBegin = glfwGetTime() * 1000.0;
		glfwPollEvents();

		int lNewWindowWidth = 0, lNewWindowHeight = 0;
		glfwGetWindowSize(lWindow, &lNewWindowWidth, &lNewWindowHeight);
		if (lNewWindowWidth != lWindowWidth || lNewWindowHeight != lWindowHeight)
		{
			lWindowWidth = lNewWindowWidth;
			lWindowHeight = lNewWindowHeight;
			lVulkanSwapchain.resizeSwapchain(lWindowWidth, lWindowHeight, lDesiredSwapchainImageCount, lDesiredVSync);

			destroyFramebuffers(lDevice, lSwapchainFramebuffers);
			createSwapchainFramebuffer(lDevice, lRenderPass, lVulkanSwapchain, lSwapchainFramebuffers);
		}

		lCommandBufferIndex = (++lCommandBufferIndex) % COMMAND_BUFFER_COUNT;
		VK_CHECK(vkWaitForFences(lDevice, 1, &lCommandBufferFences[lCommandBufferIndex], VK_TRUE, UINT64_MAX));
		VK_CHECK(vkResetFences(lDevice, 1, &lCommandBufferFences[lCommandBufferIndex]));

		uint32_t lImageIndex = 0;
		lVulkanSwapchain.acquireNextImage(lAcquireSemaphore, &lImageIndex);

		//VK_CHECK(vkResetCommandPool(lDevice, lCommandPool, 0));
		VK_CHECK(vkResetCommandBuffer(lCommandBuffers[lCommandBufferIndex], 0));
		

		VkCommandBufferBeginInfo lBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		lBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(lCommandBuffers[lCommandBufferIndex], &lBeginInfo));

		vkCmdResetQueryPool(lCommandBuffers[lCommandBufferIndex], lTimeStampQueries, 0, lQueryCount);
		vkCmdWriteTimestamp(lCommandBuffers[lCommandBufferIndex], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, lTimeStampQueries, 0);
		//vkCmdBeginQuery()
		VkClearValue lClearColor = { 0.3f, 0.2f, 0.3f, 1.0f };

		VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		renderPassInfo.renderPass = lRenderPass;
		renderPassInfo.framebuffer = lSwapchainFramebuffers[lImageIndex];
		renderPassInfo.renderArea = { {0,0} , {(uint32_t)lWindowWidth, (uint32_t)lWindowHeight} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &lClearColor;

		// barrier not needed, LayoutTransition are done during BeginRenderPass/EndRenderPass
		//VkImageMemoryBarrier renderBeginBarrier = imageBarrier(lSwapChainImages[lImageIndex], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		//vkCmdPipelineBarrier(lCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);
		vkCmdBeginRenderPass(lCommandBuffers[lCommandBufferIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		//VkViewport viewport = { 0.0f, 0.0f,(float)lWindowWidth, (float)lWindowHeight, 0.0f, 1.0f };
		// Tricks negative viewport to invert
		VkViewport viewport = { 0.0f,(float)lWindowHeight,(float)lWindowWidth, -(float)lWindowHeight, 0.0f, 1.0f }; // Vulkan 1_1 extension to reverse Y axis
		vkCmdSetViewport(lCommandBuffers[lCommandBufferIndex], 0, 1, &viewport);

		VkRect2D scissor = { {0,0}, {(uint32_t)lWindowWidth,(uint32_t)lWindowHeight} };
		vkCmdSetScissor(lCommandBuffers[lCommandBufferIndex], 0, 1, &scissor);

		// Update the uniform
		// Test, should i have to use vkFlushMappedMemoryRanges?. or it's not necessary with COHERENT MEMORY
		((Object*)uniformBuffers[lCommandBufferIndex].data)->color[0] = lCommandBufferIndex == 0 ? (rand() / float(RAND_MAX)) : 0.0f;
		((Object*)uniformBuffers[lCommandBufferIndex].data)->color[1] = lCommandBufferIndex == 1 ? (rand() / float(RAND_MAX)) : 0.0f;
		((Object*)uniformBuffers[lCommandBufferIndex].data)->color[2] = 0.0f;
		((Object*)uniformBuffers[lCommandBufferIndex].data)->color[3] = 1.0f;
		/* Don't needed as UBO are HOST_VISIBLE
		VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		range.memory = uniformBuffers[lCommandBufferIndex].memory;
		range.offset = 0;
		range.size = VK_WHOLE_SIZE;
		VK_CHECK(vkFlushMappedMemoryRanges(lDevice, 1, &range)); // probably a bad idea to do that in Begin/EndRenderpass
		*/

		// Draw (Bind API)
		/*
		vkCmdBindPipeline(lCommandBuffers[lCommandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, lMeshPipeline);
		VkDeviceSize dummyOffset = 0;
		vkCmdBindVertexBuffers(lCommandBuffers[lCommandBufferIndex], 0, 1, &lMeshVertexBuffer.buffer, &dummyOffset);
		vkCmdBindIndexBuffer(lCommandBuffers[lCommandBufferIndex], lMeshIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(lCommandBuffers[lCommandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, lMeshLayout, 0, 1, &descriptorSets[lCommandBufferIndex], 0, nullptr);

		for (int i = 0; i < 100; ++i)
		{
			vkCmdDrawIndexed(lCommandBuffers[lCommandBufferIndex], (uint32_t)lMesh.indices.size(), 1, 0, 0, 0);
		}
		*/

		vkCmdBindPipeline(lCommandBuffers[lCommandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, lMeshPipeline);
		VkDeviceSize dummyOffset = 0;
		vkCmdBindVertexBuffers(lCommandBuffers[lCommandBufferIndex], 0, 1, &lMeshVertexBuffer.buffer, &dummyOffset);
		vkCmdBindIndexBuffer(lCommandBuffers[lCommandBufferIndex], lMeshIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		//vkCmdBindDescriptorSets(lCommandBuffers[lCommandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, lMeshLayout, 0, 1, &descriptorSets[lCommandBufferIndex], 0, nullptr);

		DescriptorInfo descriptorInfos[] =
		{ 
			DescriptorInfo{uniformBuffers[lCommandBufferIndex].buffer, 0, VK_WHOLE_SIZE}
		};
		vkCmdPushDescriptorSetWithTemplateKHR(lCommandBuffers[lCommandBufferIndex], meshUpdateTpl, lMeshLayout, 0, descriptorInfos);
		vkCmdDrawIndexed(lCommandBuffers[lCommandBufferIndex], (uint32_t)lMesh.indices.size(), 1, 0, 0, 0);




		// BindLessAPI (vk 1.1)
		//vkCmdPushDescriptorSetWithTemplateKHR
		//vkCmdPushDescriptorSetWithTemplateKHR(commandBuffer, descriptorUpdateTemplate, layout, set, data)
		//vkUpdateDescriptorSetWithTemplate or vkCmdPushDescriptorSetWithTemplateKHR


		//VkImageMemoryBarrier presentBarrier = imageBarrier(lSwapChainImages[lImageIndex], 0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		//vkCmdPipelineBarrier(lCommandBuffer, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &presentBarrier);

		vkCmdEndRenderPass(lCommandBuffers[lCommandBufferIndex]);

		vkCmdWriteTimestamp(lCommandBuffers[lCommandBufferIndex], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, lTimeStampQueries, 1);
		VK_CHECK(vkEndCommandBuffer(lCommandBuffers[lCommandBufferIndex]));


		// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
		VkPipelineStageFlags lSubmitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // why
		VkSubmitInfo lSubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		lSubmitInfo.waitSemaphoreCount = 1;
		lSubmitInfo.pWaitSemaphores = &lAcquireSemaphore;
		lSubmitInfo.pWaitDstStageMask = &lSubmitStageMask;
		lSubmitInfo.commandBufferCount = 1;
		lSubmitInfo.pCommandBuffers = &lCommandBuffers[lCommandBufferIndex];
		lSubmitInfo.signalSemaphoreCount = 1;
		lSubmitInfo.pSignalSemaphores = &lReleaseSemaphore;
		VK_CHECK(vkQueueSubmit(lDevice.getQueue(VulkanQueueType::Graphics), 1, &lSubmitInfo, lCommandBufferFences[lCommandBufferIndex]));


		lVulkanSwapchain.queuePresent(lDevice.getQueue(VulkanQueueType::Graphics), lImageIndex, lReleaseSemaphore);

		// TODO : Barrier/Fence on CommandBuffer or DoubleBuffered CommandBuffer with sync ?
		//VK_CHECK(vkDeviceWaitIdle(lDevice));


		++frameCount;
		VkPerformanceCounterResultKHR queryResults[2] = {};
		vkGetQueryPoolResults(lDevice, lTimeStampQueries, 0, 2, sizeof(queryResults), queryResults, sizeof(queryResults[0]), VK_QUERY_RESULT_64_BIT /*| VK_QUERY_RESULT_WAIT_BIT*/);

		double gpuFrameBegin = double(queryResults[0].uint64) * lDevice.mPhysicalDeviceProperties.limits.timestampPeriod * 1e-6; // ms
		double gpuFrameEnd = double(queryResults[1].uint64) * lDevice.mPhysicalDeviceProperties.limits.timestampPeriod * 1e-6;

		double cpuFrameEnd = glfwGetTime() * 1000.0; // ms
		cpuTotalTime += cpuFrameEnd - cpuFrameBegin;
		gpuTotalTime += (queryResults[1].uint64 - queryResults[0].uint64);
		double cpuTimeAvg = 0;

		if(cpuTotalTime > 1000.0f)
		{
			double avgCpu = cpuTotalTime / frameCount;
			double avgGpu = (double(gpuTotalTime) * lDevice.mPhysicalDeviceProperties.limits.timestampPeriod * 1e-6) / frameCount;

			char title[256];
			sprintf(title, "cpu=%.1f ms; gpu: %.1f ms", avgCpu, avgGpu);
			glfwSetWindowTitle(lWindow, title);

			cpuTotalTime = 0;
			gpuTotalTime = 0;
			frameCount = 0;			
		}
	}
	
	VK_CHECK(vkDeviceWaitIdle(lDevice));


	vkDestroyDescriptorPool(lDevice, descriptorPool, nullptr);

	vkDestroyDescriptorSetLayout(lDevice, descriptorSetLayout, nullptr);

	vkDestroyCommandPool(lDevice, lCommandPool, nullptr);

	vkDestroyDescriptorSetLayout(lDevice, meshDescriptorSetLayout, nullptr);
	vkDestroyDescriptorUpdateTemplate(lDevice, meshUpdateTpl, nullptr);

	vkDestroyPipeline(lDevice, lMeshPipeline, nullptr);
	vkDestroyPipelineLayout(lDevice, lMeshLayout, nullptr);

	destroyShader(lDevice, lMeshVertexShader);
	destroyShader(lDevice, lMeshFragmentShader);

	destroyBuffer(lDevice, lMeshVertexBuffer);
	destroyBuffer(lDevice, lMeshIndexBuffer);
	destroyBuffer(lDevice, lStageBuffer);

	for (int i = 0; i < uniformBuffers.size(); ++i)
		destroyBuffer(lDevice, uniformBuffers[i]);


	vkDestroyRenderPass(lDevice, lRenderPass, nullptr);

	vkDestroySemaphore(lDevice, lReleaseSemaphore, nullptr);
	vkDestroySemaphore(lDevice, lAcquireSemaphore, nullptr);
	
	for (int i = 0; i < ARRAY_COUNT(lCommandBufferFences); ++i)
		vkDestroyFence(lDevice, lCommandBufferFences[i], nullptr);

	vkDestroyQueryPool(lDevice, lTimeStampQueries, nullptr);


	destroyFramebuffers(lDevice, lSwapchainFramebuffers);
	lVulkanSwapchain.cleanUp();
	

	vkDestroyDevice(lDevice, nullptr);


#ifdef _DEBUG
	unregisterDebugMessenger(lVulkanInstance, gDebugMessenger);
#endif

	vkDestroyInstance(lVulkanInstance, nullptr);

	glfwDestroyWindow(lWindow);
	glfwTerminate();
	return 0;
}

