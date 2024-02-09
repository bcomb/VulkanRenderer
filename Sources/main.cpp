#include "vk_common.h"
#include "Shaders.h"

#include <algorithm>
#include <vector>
#include <array>

#include <assert.h>
#include <malloc.h>
#include <stdio.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <meshoptimizer.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


#define FAST_OBJ_IMPLEMENTATION
#include <../meshoptimizer/extern/fast_obj.h>

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanHelper.h"

#include "Window.h"

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

// vkUpdateDescriptorSets -> Vulkan 1_0
// vkUpdateDescriptorSetWithTemplate -> Vulkan 1_1
// vkCmdPushDescriptorSetWithTemplateKHR -> require VK_KHR_descriptor_update_template
static bool pushDescriptorsSupported = false; // Bindless require //VK_KHR_push_descriptor
static bool useDescriptorTemplate = false;	//VK_KHR_descriptor_update_template

/******************************************************************************/
VkDebugUtilsMessengerEXT gDebugMessenger;
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
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

#if WIN32
	// MSVC ouput
	OutputDebugString(errorTypeStr); OutputDebugString(pCallbackData->pMessage); OutputDebugString("\n");
#endif

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

	/*if
		(1)
	{
		meshopt_optimizeVertexCache(opt_indices.data(), opt_indices.data(), opt_indices.size(), opt_vertices.size());
		meshopt_optimizeVertexFetch(opt_vertices.data(), opt_indices.data(), opt_indices.size(), opt_vertices.data(), opt_vertices.size(), sizeof(Vertex));
	}*/

	std::swap(opt_vertices, pMesh.vertices);
	std::swap(opt_indices, pMesh.indices);

	fast_obj_destroy(lMesh);
	return true;
}


void window_size_callback(GLFWwindow* window, int width, int height)
{

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
	VkBuffer mBuffer;					// The VkBuffer can be a huge buffer
	VkDeviceMemory mMemory;				// The memory bind to this buffer
	VkDescriptorBufferInfo mDescriptor;	// The region of the buffer concern by this buffer
	VkBufferUsageFlags	mUsageFlags;	// Set at creation time
	VkMemoryPropertyFlags mMemoryPropertyFlags;
	void* mMappedData;					// The pointer of currently mapped data
	VkDeviceSize mSize;					// The buffer size
	VkDeviceSize mAlignment;			// Required alignment
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
	if (pProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		VK_CHECK(vkMapMemory(pVulkanDevice.mLogicalDevice, memory, 0ull, VK_WHOLE_SIZE, 0ull, &data));
	}

	result.mBuffer = buffer;
	result.mMemory = memory;
	result.mUsageFlags = pUsage;
	result.mMemoryPropertyFlags = pProperties;
	result.mAlignment = memoryRequirements.alignment;
	result.mSize = pSize;
	result.mMappedData = data;

	result.mDescriptor.buffer = buffer;
	result.mDescriptor.offset = 0;
	result.mDescriptor.range = VK_WHOLE_SIZE;
}


struct Image
{
	VkFormat format;
	VkImage image;
	VkDeviceMemory memory;
	uint32_t width, height;	
	void* imageData = NULL;			// can be freed after staging

	void* data; // at the moment persistent map, if not, u have to Map/Unmap in the uploadBufferToImage
	size_t size;
};

void createImage(Image& result, VulkanDevice pDevice, VkFormat pFormat, uint32_t pWidth, uint32_t pHeight)
{
	VkImageCreateInfo lImageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	//const void* pNext;
	//VkImageCreateFlags       flags;	// advanced conf
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
/*
void uploadBufferToImage(VkDevice pDevice, VkCommandPool pCommandPool, VkCommandBuffer pCommandBuffer, VkQueue pCopyQueue, const Buffer& pSrc, Image& pDst, bool pDeleteImageData = true)
{
#pragma message("TODO : robust way to identify persistent map or not")
	assert(pDst.imageData != NULL);
	uint32_t lImageDataSize = pDst.width * pDst.height * 4;
	assert(lImageDataSize <= pSrc.mSize);
	// pDst.data is a persistent mapped buffer
	if (pSrc.mMappedData)
	{
		// At the moment we conisder the data is already map in VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		memcpy(pSrc.mMappedData, pDst.imageData, lImageDataSize);
	}
	else
	{
		void* lData = NULL;
		VK_CHECK(vkMapMemory(pDevice, pSrc.mMemory, 0, VK_WHOLE_SIZE, 0, &lData));
		memcpy(lData, pDst.imageData, lImageDataSize);
		vkUnmapMemory(pDevice, pSrc.mMemory);
	}

	if (pDeleteImageData)
	{
		free(pDst.imageData);
		pDst.imageData = NULL;
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
	VkImageMemoryBarrier lImageBarrier = vkh::imageBarrier(pDst.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																		0, VK_ACCESS_TRANSFER_WRITE_BIT);
	vkCmdPipelineBarrier(pCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, NULL, 0, NULL, 1, &lImageBarrier);

	vkCmdCopyBufferToImage(pCommandBuffer, pSrc.mBuffer, pDst.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions);

	lImageBarrier = vkh::imageBarrier(pDst.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
#pragma message("TODO : manage texture access in vertex pipeline")
	vkCmdPipelineBarrier(pCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, NULL, 0, NULL, 1, &lImageBarrier);

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
*/

// Copy pHostData to the pSrc.data stage buffer into pDst using vkCmdCopyBuffer
/*
void uploadBuffer(VkDevice pDevice, VkCommandPool pCommandPool, VkCommandBuffer pCommandBuffer, VkQueue pCopyQueue, const Buffer& pSrc, const Buffer& pDst, const void* pHostData, size_t pHostDataSize)
{
#pragma message("TODO : robust way to identify persistent map or not")
	// pDst.data is a persistent mapped buffer
	if (pSrc.mMappedData)
	{
		// At the moment we consider the data is already map in VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		memcpy(pSrc.mMappedData, pHostData, pHostDataSize);
	}
	else
	{
		void* lData = NULL;
		VK_CHECK(vkMapMemory(pDevice, pSrc.mMemory, 0, VK_WHOLE_SIZE, 0, &lData));
		memcpy(lData, pHostData, pHostDataSize);
		vkUnmapMemory(pDevice, pSrc.mMemory);
	}
	
	VK_CHECK(vkResetCommandPool(pDevice, pCommandPool, 0));
	VK_CHECK(vkResetCommandBuffer(pCommandBuffer, 0));

	VkCommandBufferBeginInfo lBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	lBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(pCommandBuffer, &lBeginInfo));

#pragma message("Should be device data size ?")
	VkBufferCopy regions = { 0, 0, VkDeviceSize(pHostDataSize) };
	vkCmdCopyBuffer(pCommandBuffer, pSrc.mBuffer, pDst.mBuffer, 1, &regions);

	VkBufferMemoryBarrier copyBufferBarrier = vkh::bufferBarrier(pDst.mBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
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
*/

void destroyBuffer(VkDevice pDevice, const Buffer& pBuffer)
{
	// do we need to unmap?
	//vkUnmapMemory(pDevice, pBuffer.memory); // for now we use persistent mapping

	vkDestroyBuffer(pDevice, pBuffer.mBuffer, nullptr);
	vkFreeMemory(pDevice, pBuffer.mMemory, nullptr);
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


// Create Image	
bool loadImage(VulkanDevice pDevice, Image& pImage, const char* pFilename)
{
	Image lTextureImage;
	int w, h, channels_in_file;
	void* data = stbi_load(pFilename, &w, &h, &channels_in_file, 4);
	assert(stbi_load != NULL && "fail to load images");
	if (data)
	{
		pImage.imageData = data;
		createImage(pImage, pDevice, VK_FORMAT_R8G8B8A8_UNORM, w, h);		
		return true;
	}

	return false;
}

// Refactor
int main(int argc, const char* argv[])
{
	VK_CHECK(volkInitialize());

	printf("Hello VulkanRenderer\n");
	int lSuccess = glfwInit();
	assert(lSuccess);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// MUST BE SET or SwapChain creation fail
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);


	VulkanInstance* lVulkanInstance = new VulkanInstance;
	lVulkanInstance->createInstance(VK_API_VERSION_1_3, true);
	lVulkanInstance->enumeratePhysicalDevices();
	VkPhysicalDevice lPhysicalDevice = lVulkanInstance->pickPhysicalDevice(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

#ifdef _DEBUG
	registerDebugMessenger(lVulkanInstance->mVulkanInstance);
#endif

	VulkanDevice* lDevice = new VulkanDevice(lPhysicalDevice);
	lDevice->createLogicalDevice(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, lVulkanInstance->mVulkanInstance);

	VulkanContext lContext = { lVulkanInstance, lDevice };

	WindowAttributes lWinAttr;
	Window* lWindow = new Window(lContext, lWinAttr, "VulkanTest");

	while (1)
	{
		glfwPollEvents();		
		lWindow->render();
	}

	return 0;
}


// Entry point
#if 0
int main(int argc, const char* argv[])
{
	mainLoop();

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


	VulkanInstance lVulkanInstance;
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
	//loadQuadMesh(lMesh);
 	bool lResult = loadMesh(lMesh, R"(i:\Data\obj\bicycle.obj)", true);	
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
	Image lTextureImage;
	loadImage(lDevice, lTextureImage, R"(E:\Data\Textures\color_wheel_rgb.png)");
	assert(lTextureImage.imageData);

	// Sampler
	VkSampler lTextureSampler = vkh::createTextureSampler(lDevice);

#pragma message("TODO : move pool creation to device object")
	// -- pool creation BEGIN 
	// HERE we need to created pool of each kind of desciptor type (UBO/SBO/constant look at VkDescriptorType)
	// And for each SwapChainImage for double buffer

	// VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER  -> texture and sampler
	// VK_DESCRIPTOR_TYPE_SAMPLER + VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE -> ??

	std::vector<VkDescriptorType> lPoolDescriptorTypes = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
	std::vector<VkDescriptorPoolSize> lDescriptorPoolSizes;

	for (VkDescriptorType lDescriptorType : lPoolDescriptorTypes)
	{		
		lDescriptorPoolSizes.emplace_back(VkDescriptorPoolSize({ lDescriptorType, lVulkanSwapchain.imageCount() }));
	}


	VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	//The structure has an optional flag similar to command pools that determines if individual descriptor sets can be freed or not: VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT.
	// We're not going to touch the descriptor set after creating it, so we don't need this flag.You can leave flags to its default value of 0.
	//VkDescriptorPoolCreateFlags    flags;
	poolInfo.maxSets = lVulkanSwapchain.imageCount(); // can't access the same set from multiple CommandBuffer? create one for each CommandBuffer
	poolInfo.poolSizeCount = (uint32_t)lDescriptorPoolSizes.size();
	poolInfo.pPoolSizes = lDescriptorPoolSizes.data();

	VkDescriptorPool lDescriptorPool;
	VK_CHECK(vkCreateDescriptorPool(lDevice, &poolInfo, nullptr, &lDescriptorPool));

	// -- pool creation END


	// -- binding point declaration BEGIN
	// i have same binding point for texture/ubo, one in VertexStage, the other in fragment, is it legal? what happen if texture
	// is accessed from vertex too?

	std::vector<VkDescriptorSetLayoutBinding> lDescriptorSetLayoutBinding;

	// For uniform?
	VkDescriptorSetLayoutBinding uboDescBind = {};
	uboDescBind.binding = 0;
	uboDescBind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboDescBind.descriptorCount = 1;
	uboDescBind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // VK_SHADER_STAGE_ALL_GRAPHICS (opengl fashion?)
	uboDescBind.pImmutableSamplers = nullptr; // Optional The pImmutableSamplers field is only relevant for image sampling related descriptors

	// Texture
	VkDescriptorSetLayoutBinding textureDescBind = {};
	textureDescBind.binding = 1;
	textureDescBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureDescBind.descriptorCount = 1;
	textureDescBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // VK_SHADER_STAGE_VERTEX_BIT
	//const VkSampler* pImmutableSamplers;


	lDescriptorSetLayoutBinding.push_back(uboDescBind);
	lDescriptorSetLayoutBinding.push_back(textureDescBind);

	VkDescriptorSetLayoutCreateInfo lDescriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };

	// VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : Setting this flag tells the descriptor set layouts that no actual descriptor sets are allocated but instead pushed at command buffer creation time
	lDescriptorSetLayoutCreateInfo.flags = pushDescriptorsSupported ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;
	lDescriptorSetLayoutCreateInfo.bindingCount = (uint32_t)lDescriptorSetLayoutBinding.size();
	lDescriptorSetLayoutCreateInfo.pBindings = lDescriptorSetLayoutBinding.data();

	VkDescriptorSetLayout lDescriptorSetLayout;
	VK_CHECK(vkCreateDescriptorSetLayout(lDevice, &lDescriptorSetLayoutCreateInfo, nullptr, &lDescriptorSetLayout));

	// -- binding point declaration END

	// -- Binding ressource creation BEGIN
	// Create the resource to bind on the shader
	/// Resources by imageCount in the swapChain
	// Texture (require as much view as swapChainImage (can't access the imageview from multiple command buffer)
	std::vector<VkImageView> lTextureImageViews(lVulkanSwapchain.imageCount());
	std::vector<Buffer> lUniformBuffers(lVulkanSwapchain.imageCount());

	for (uint32_t i = 0; i < lVulkanSwapchain.imageCount(); ++i)
	{
		// UBO
		createBuffer(lUniformBuffers[i], lDevice, sizeof(Object), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		((Object*)lUniformBuffers[i].mMappedData)->color[0] = 1.0f;
		((Object*)lUniformBuffers[i].mMappedData)->color[1] = 0.0f;
		((Object*)lUniformBuffers[i].mMappedData)->color[2] = 0.0f;
		((Object*)lUniformBuffers[i].mMappedData)->color[3] = 1.0f;

		// TextureImage view
		lTextureImageViews[i] = vkh::createImageView(lDevice, lTextureImage.image, lTextureImage.format);
	}
	// -- Binding ressource creation END

	// In our case we will create one descriptor set for each swap chain image, all with the same layout.
	// Unfortunately we do need all the copies of the layout because the next function expects an array matching the number of sets.
	std::vector<VkDescriptorSetLayout> layouts(lVulkanSwapchain.imageCount(), lDescriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = lDescriptorPool;
	allocInfo.descriptorSetCount = (uint32_t)layouts.size();
	allocInfo.pSetLayouts = layouts.data();

	// Allocate descriptor set (one per image in the swapchain)
	std::vector<VkDescriptorSet> lDescriptorSets;
	lDescriptorSets.resize(lVulkanSwapchain.imageCount());

	//vkAllocateDescriptorSets() was created with invalid flag VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR set.
	// The Vulkan spec states : Each element of pSetLayouts must not have been created with VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR set
	// (https ://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-VkDescriptorSetAllocateInfo-pSetLayouts-00308)
	if
		(useDescriptorTemplate)
	{
		if
			(pushDescriptorsSupported)
		{
		}
		else
		{

		}
	}
	else
	{
		//this is for Vulkan 1_0, we don't care at the moment, we use VkDescriptorUpdateTemplate instead
		VK_CHECK(vkAllocateDescriptorSets(lDevice, &allocInfo, lDescriptorSets.data()));

		// Fill the descriptor
		for (size_t i = 0; i < lDescriptorSets.size(); ++i)
		{
			std::vector<VkWriteDescriptorSet> lDescriptorWrites;

			// ----- 1st resource
			{
				VkDescriptorBufferInfo bufferInfo = {};
				bufferInfo.buffer = lUniformBuffers[i].mBuffer;
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(Object);

				VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				descriptorWrite.dstSet = lDescriptorSets[i];
				descriptorWrite.dstBinding = 0;
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrite.descriptorCount = 1;
				descriptorWrite.pBufferInfo = &bufferInfo;
				descriptorWrite.pImageInfo = NULL;
				descriptorWrite.pTexelBufferView = NULL;

				lDescriptorWrites.push_back(descriptorWrite);
			}

			// ----- 2nd resource
			{
				VkDescriptorImageInfo imageInfo = {};
				imageInfo.sampler = lTextureSampler;
				imageInfo.imageView = lTextureImageViews[i];
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				descriptorWrite.dstSet = lDescriptorSets[i];
				descriptorWrite.dstBinding = 1;
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrite.descriptorCount = 1;
				descriptorWrite.pBufferInfo = NULL;
				descriptorWrite.pImageInfo = &imageInfo;
				descriptorWrite.pTexelBufferView = NULL;

				lDescriptorWrites.push_back(descriptorWrite);
			}

			// indexation are done trough the descriptorWrite.dstSet
			vkUpdateDescriptorSets(lDevice, (uint32_t)lDescriptorWrites.size(), lDescriptorWrites.data(), 0, nullptr); // VkCopyDescriptorSet : what is this?	
		}
	}
	// HERE DESCRIPTOR LABOR END
	VkPipelineLayout lPipelineLayout = createPipelineLayout(lDevice, 1, &lDescriptorSetLayout, 0, NULL);

	// Bindless API
	VkDescriptorUpdateTemplate lDescriptorUpdateTemplate{};	
	lDescriptorUpdateTemplate = createDescriptorUpdateTemplate(lDevice, VK_PIPELINE_BIND_POINT_GRAPHICS, lPipelineLayout, lDescriptorSetLayout, useDescriptorTemplate && pushDescriptorsSupported);

	// TODO : check if safe to destroy here
	//vkDestroyDescriptorSetLayout(lDevice, meshDescriptorSetLayout, NULL);
	
	VkPipeline lPipeline;
	VkPipelineCache lPipelineCache = 0; // TODO : learn that
	lPipeline = createGraphicsPipeline(lDevice, lPipelineCache, lPipelineLayout, lRenderPass, lMeshVertexShader, lMeshFragmentShader, lMeshVertexInputCreateInfo);

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
	uploadBufferToImage(lDevice, lCommandPool, lCommandBuffers[0], lDevice.getQueue(VulkanQueueType::Transfert), lStageBuffer, lTextureImage, true);

	uint64_t frameCount = 0;
	double cpuTotalTime = 0.0;
	uint64_t gpuTotalTime = 0;
	double cpuTimeAvg = 0;


	std::vector<VkFramebuffer> lSwapchainFramebuffers = vkh::createSwapchainFramebuffer(lDevice, lRenderPass, lVulkanSwapchain);

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
			lSwapchainFramebuffers = vkh::createSwapchainFramebuffer(lDevice, lRenderPass, lVulkanSwapchain);
		}

		lCommandBufferIndex = (++lCommandBufferIndex) % COMMAND_BUFFER_COUNT;
		VK_CHECK(vkWaitForFences(lDevice, 1, &lCommandBufferFences[lCommandBufferIndex], VK_TRUE, UINT64_MAX));
		VK_CHECK(vkResetFences(lDevice, 1, &lCommandBufferFences[lCommandBufferIndex]));

		uint32_t lImageIndex = 0;
		lVulkanSwapchain.acquireNextImage(lAcquireSemaphore, &lImageIndex);

		//VK_CHECK(vkResetCommandPool(lDevice, lCommandPool, 0));
		VK_CHECK(vkResetCommandBuffer(lCommandBuffers[lCommandBufferIndex], 0));
		
		//VkDescriptorPoolResetFlags
		//VK_CHECK(vkResetDescriptorPool(lDevice, lDescriptorPool, 0)); // SHOULD HAVE one pool per swapchain, cause some descriptor are in use in the previouus command buufer

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
		renderPassInfo.clearValueCount = 1;	// MRT
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
		((Object*)lUniformBuffers[lCommandBufferIndex].mMappedData)->color[0] = lCommandBufferIndex == 0 ? (rand() / float(RAND_MAX)) : 0.0f;
		((Object*)lUniformBuffers[lCommandBufferIndex].mMappedData)->color[1] = lCommandBufferIndex == 1 ? (rand() / float(RAND_MAX)) : 0.0f;
		((Object*)lUniformBuffers[lCommandBufferIndex].mMappedData)->color[2] = 0.0f;
		((Object*)lUniformBuffers[lCommandBufferIndex].mMappedData)->color[3] = 1.0f;
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

		vkCmdBindPipeline(lCommandBuffers[lCommandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, lPipeline);
		VkDeviceSize dummyOffset = 0;
		vkCmdBindVertexBuffers(lCommandBuffers[lCommandBufferIndex], 0, 1, &lMeshVertexBuffer.mBuffer, &dummyOffset);
		vkCmdBindIndexBuffer(lCommandBuffers[lCommandBufferIndex], lMeshIndexBuffer.mBuffer, 0, VK_INDEX_TYPE_UINT32);

#pragma message("TODO : reactivate this optimal way to bind descriptor (PushTemplate)")
		if (useDescriptorTemplate)
		{
			// Bind resource
			DescriptorInfo descriptorInfos[] =
			{
				DescriptorInfo(lUniformBuffers[lCommandBufferIndex].mBuffer, 0, VK_WHOLE_SIZE),
				DescriptorInfo(lTextureSampler, lTextureImageViews[lCommandBufferIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			};

			if (pushDescriptorsSupported)
			{
				// BindLessAPI (vk 1.1) + extentions
				// Do i need to create lDescriptorUpdateTemplate one by swapchain????
				vkCmdPushDescriptorSetWithTemplateKHR(lCommandBuffers[lCommandBufferIndex], lDescriptorUpdateTemplate, lPipelineLayout, 0, descriptorInfos);
			}
			else
			{
				VkDescriptorSetAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
				allocateInfo.descriptorPool = lDescriptorPool;
				allocateInfo.descriptorSetCount = 1;
				allocateInfo.pSetLayouts = &lDescriptorSetLayout;

				VkDescriptorSet lSet = 0;
				VK_CHECK(vkAllocateDescriptorSets(lDevice, &allocateInfo, &lSet));

				vkUpdateDescriptorSetWithTemplate(lDevice, lSet, lDescriptorUpdateTemplate, descriptorInfos);

				vkCmdBindDescriptorSets(lCommandBuffers[lCommandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, lPipelineLayout, 0, 1, &lSet, 0, nullptr);
			}
		}
		else
		{
			// vkUpdateDescriptorSets


			// Vulkan 1.0
			vkCmdBindDescriptorSets(lCommandBuffers[lCommandBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, lPipelineLayout, 0, 1, &lDescriptorSets[lCommandBufferIndex], 0, nullptr);
		}		

		for(int i =0; i <100; ++i)
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
		VkPipelineStageFlags lSubmitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //  Pipeline stages used to wait at for graphics queue submissions
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
		vkGetQueryPoolResults(lDevice, lTimeStampQueries, 0, 2, sizeof(queryResults), queryResults, sizeof(queryResults[0]), VK_QUERY_RESULT_64_BIT /* | VK_QUERY_RESULT_WAIT_BIT*/);

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


	vkDestroyDescriptorPool(lDevice, lDescriptorPool, nullptr);
	
	vkDestroyDescriptorSetLayout(lDevice, lDescriptorSetLayout, nullptr);

	vkDestroyCommandPool(lDevice, lCommandPool, nullptr);

	vkDestroyDescriptorUpdateTemplate(lDevice, lDescriptorUpdateTemplate, nullptr);

	vkDestroyPipeline(lDevice, lPipeline, nullptr);
	vkDestroyPipelineLayout(lDevice, lPipelineLayout, nullptr);

	destroyShader(lDevice, lMeshVertexShader);
	destroyShader(lDevice, lMeshFragmentShader);

	destroyBuffer(lDevice, lMeshVertexBuffer);
	destroyBuffer(lDevice, lMeshIndexBuffer);
	destroyBuffer(lDevice, lStageBuffer);

	for (int i = 0; i < lUniformBuffers.size(); ++i)
		destroyBuffer(lDevice, lUniformBuffers[i]);


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


}
#endif