#pragma once
#include "vk_common.h"

#include <vector>

// Link between the windowing system and vulkan
struct VulkanSwapchain
{
    explicit VulkanSwapchain(struct VulkanInstance* pContext, struct VulkanDevice* pDevice);
    
#ifdef VK_USE_PLATFORM_WIN32_KHR
    void initSurface(void* pPlatformHandle, void* pPlatformWindow);
#endif // VK_USE_PLATFORM_WIN32_KHR

    void createSwapchain(uint32_t pWidth, uint32_t pHeight, uint32_t pDesiredImages, bool pVSync);
    void resizeSwapchain(uint32_t pWidth, uint32_t pHeight, uint32_t pDesiredImages, bool pVSync);
    void destroySwapchain();

    inline uint32_t imageCount() const { return (uint32_t)mImages.size();  }

    // Acquire the next image in the swapchain
    VkResult acquireNextImage(VkSemaphore pAcquireSemaphore);

    // Present the current image to the queue (the last one acquired)
    VkResult queuePresent(VkQueue pQueue, VkSemaphore pWaitSemaphore = VK_NULL_HANDLE);

    // Get the current image
    inline VkImage getImage() { return mImages[mSwapchainImageIndex]; }

    void cleanUp();

    struct VulkanInstance* mContext = nullptr;
    struct VulkanDevice* mDevice = nullptr;

    uint32_t mSwapchainImageIndex = 0;  // The last requested image index

    VkSurfaceKHR mSurface;
    VkSurfaceCapabilitiesKHR mSurfaceCapabilities;
    VkFormat           mColorFormat;
    VkColorSpaceKHR    mColorSpace;
    std::vector<VkSurfaceFormatKHR> mSurfaceSupportedFormats;
    std::vector<VkPresentModeKHR> mSurfaceSupportedPresentModes;

    // Current swapchain information
    VkSwapchainKHR mSwapchain = {};
    uint32_t mWidth, mHeight;           // The real size of Width/height, can be different from requested
    std::vector<VkImage> mImages;
    std::vector<VkImageView> mImageViews;
    std::vector<VkFramebuffer> mFramebuffers;   // No more used with Vulkan 1.3
};