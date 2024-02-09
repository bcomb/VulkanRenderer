#include "vk_common.h"

struct VulkanBuffer
{
	VkDevice mDevice;					// The logical device
	VkBuffer mBuffer;					// The VkBuffer can be a huge buffer
	VkDeviceMemory mMemory;				// The memory bind to this buffer
	VkDescriptorBufferInfo mDescriptor;	// The region of the buffer concern by this buffer
	VkBufferUsageFlags	mUsageFlags;	// Set at creation time
	VkMemoryPropertyFlags mMemoryPropertyFlags;
	void* mMappedData;					// The pointer of currently mapped data
	VkDeviceSize mSize;					// The buffer size
	VkDeviceSize mAlignment;			// Required alignment

	//void map(VkDeviceSize pSize = VK_WHOLE_SIZE, VkDeviceSize pOffset = 0);
	//void unmap();
	//void destroy();
};
