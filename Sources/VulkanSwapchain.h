#include "vk_common.h"

#include <vector>

// Link between the windowing system and vulkan
struct VulkanSwapchain
{
    explicit VulkanSwapchain(struct VulkanContext* pContext, struct VulkanDevice* pDevice);
    
#ifdef VK_USE_PLATFORM_WIN32_KHR
    void initSurface(void* pPlatformHandle, void* pPlatformWindow);
#endif // VK_USE_PLATFORM_WIN32_KHR

    void createSwapchain(uint32_t pWidth, uint32_t pHeight, uint32_t pDesiredImages, bool pVSync);
    void resizeSwapchain(uint32_t pWidth, uint32_t pHeight, uint32_t pDesiredImages, bool pVSync);
    void destroySwapchain();

    inline uint32_t imageCount() const { return (uint32_t)mImages.size();  }
    VkResult acquireNextImage(VkSemaphore pAcquireSemaphore, uint32_t* pImageIndex);
    VkResult queuePresent(VkQueue pQueue, uint32_t pImageIndex, VkSemaphore pReleaseSemaphore = VK_NULL_HANDLE);

    void cleanUp();


    struct VulkanContext* mContext;
    struct VulkanDevice* mDevice;

    VkSurfaceKHR mSurface;
    VkSurfaceCapabilitiesKHR mSurfaceCapabilities;
    VkFormat           mColorFormat;
    VkColorSpaceKHR    mColorSpace;
    std::vector<VkSurfaceFormatKHR> mSurfaceSupportedFormats;
    std::vector<VkPresentModeKHR> mSurfaceSupportedPresentModes;

    // Current swapchain information
    VkSwapchainKHR mSwapchain = {};
    uint32_t mWidth, mHeight;           // The real size of Width/heigh, can be different from requested
    std::vector<VkImage> mImages;
    std::vector<VkImageView> mImageViews;
    std::vector<VkFramebuffer> mFramebuffers;
};