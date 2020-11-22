#include "VulkanSwapchain.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanHelper.h"

#include <assert.h>
#include <algorithm>

/******************************************************************************/
VulkanSwapchain::VulkanSwapchain(struct VulkanInstance* pContext, struct VulkanDevice* pDevice)
: mContext(pContext)
, mDevice(pDevice)
{
    assert(mContext);
}

/******************************************************************************/
void VulkanSwapchain::initSurface(void* pPlatformHandle, void* pPlatformWindow)
{
    // Create the surface (just an handle to query info, no ressource really created)
    VkWin32SurfaceCreateInfoKHR lInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    lInfo.hinstance = (HINSTANCE)pPlatformHandle;
    lInfo.hwnd = (HWND)pPlatformWindow;
    VK_CHECK(vkCreateWin32SurfaceKHR(mContext->mVulkanInstance, &lInfo, nullptr, &mSurface));

    // Present support
    VkBool32 lPresentationSupported = false;
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(mDevice->mPhysicalDevice, mDevice->getQueueFamilyIndex(VulkanQueueType::Graphics), mSurface, &lPresentationSupported));
    assert(lPresentationSupported);

    // Surface capabilities
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mDevice->mPhysicalDevice, mSurface, &mSurfaceCapabilities));

    // Supported format
    uint32_t lFormatCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(mDevice->mPhysicalDevice, mSurface, &lFormatCount, NULL));
    mSurfaceSupportedFormats.resize(lFormatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(mDevice->mPhysicalDevice, mSurface, &lFormatCount, mSurfaceSupportedFormats.data()));

    // Supported present mode
    uint32_t lPresentModeCount;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mDevice->mPhysicalDevice, mSurface, &lPresentModeCount, NULL));
    mSurfaceSupportedPresentModes.resize(lPresentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mDevice->mPhysicalDevice, mSurface, &lPresentModeCount, mSurfaceSupportedPresentModes.data()));

	// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
	// there is no preferred format, so we assume VK_FORMAT_B8G8R8A8_UNORM
	if ((mSurfaceSupportedFormats.size() == 1) && (mSurfaceSupportedFormats[0].format == VK_FORMAT_UNDEFINED))
	{
		mColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
		mColorSpace = mSurfaceSupportedFormats[0].colorSpace;
	}
	else
	{
		// iterate over the list of available surface format and
		// check for the presence of VK_FORMAT_B8G8R8A8_UNORM
		bool lHas_B8G8R8A8_UNORM = false;
		for (auto&& surfaceFormat : mSurfaceSupportedFormats)
		{
			if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
			{
				mColorFormat = surfaceFormat.format;
				mColorSpace = surfaceFormat.colorSpace;
				lHas_B8G8R8A8_UNORM = true;
				break;
			}
		}

		// in case VK_FORMAT_B8G8R8A8_UNORM is not available
		// select the first available color format
		if (!lHas_B8G8R8A8_UNORM)
		{
			mColorFormat = mSurfaceSupportedFormats[0].format;
			mColorSpace = mSurfaceSupportedFormats[0].colorSpace;
		}
	}
}

/******************************************************************************/
void VulkanSwapchain::createSwapchain(uint32_t pWidth, uint32_t pHeight, uint32_t pDesiredImages, bool pVSync)
{
	VkSwapchainKHR lOldSwapchain = mSwapchain;

	// On android VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR is generally not supported
	// Spec said : at least one must be supported
	VkCompositeAlphaFlagBitsKHR lSurfaceCompositeAlpha =
		(mSurfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
		: (mSurfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
		: (mSurfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
		: VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

	// 
	uint32_t lSwapchainImageCount = mSurfaceCapabilities.minImageCount;
	if (pDesiredImages > lSwapchainImageCount)
	{
		lSwapchainImageCount = std::min(pDesiredImages, mSurfaceCapabilities.maxImageCount);
	}

	// Swap mode
	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")
	// In FIFO mode the presentation requests are stored in a queue.
	// If the queue is full the application will have to wait until an image is ready to be acquired again.
	// This is a normal operating mode for mobile, which automatically locks the framerate to 60 FPS.
	//VK_PRESENT_MODE_MAILBOX_KHR, tiled device?
	VkPresentModeKHR lSwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
	if (!pVSync)
	{
		for (size_t i = 0; i < mSurfaceSupportedPresentModes.size(); i++)
		{
			if (mSurfaceSupportedPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				lSwapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
			if ((lSwapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (mSurfaceSupportedPresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
			{
				lSwapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}
	}

	// TODO check width/height caps
	mWidth = pWidth;
	mHeight = pHeight;

	// Create
	VkSwapchainCreateInfoKHR lSwapchainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	lSwapchainInfo.surface = mSurface;
	lSwapchainInfo.minImageCount = lSwapchainImageCount;
	lSwapchainInfo.imageFormat = mColorFormat; // some devices only support BGRA
	lSwapchainInfo.imageColorSpace = mColorSpace;
	lSwapchainInfo.imageExtent.width = pWidth;
	lSwapchainInfo.imageExtent.height = pHeight;
	lSwapchainInfo.imageArrayLayers = 1;
	lSwapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	// we don't share swapchain image across multiple queue
	lSwapchainInfo.queueFamilyIndexCount = 0;
	lSwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; 
	lSwapchainInfo.pQueueFamilyIndices = NULL;
	// Don't care about rotate at the moment
	lSwapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	lSwapchainInfo.presentMode = lSwapchainPresentMode;  
	// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
	lSwapchainInfo.clipped = VK_TRUE;
	// The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system.You'll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
	lSwapchainInfo.compositeAlpha = lSurfaceCompositeAlpha;
	lSwapchainInfo.oldSwapchain = lOldSwapchain;

	// Finally create the swapchain
	VK_CHECK(vkCreateSwapchainKHR(mDevice->mLogicalDevice, &lSwapchainInfo, nullptr, &mSwapchain));

	// Destroy previous ressources
	if
		(lOldSwapchain)
	{
		for (auto&& lImageView : mImageViews)
			vkDestroyImageView(mDevice->mLogicalDevice, lImageView, NULL);

		vkDestroySwapchainKHR(mDevice->mLogicalDevice, lOldSwapchain, NULL);
	}

	// Retrieve Image and create view for this
	uint32_t lImageCount = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(mDevice->mLogicalDevice, mSwapchain, &lImageCount, nullptr));
	mImages.resize(lImageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(mDevice->mLogicalDevice, mSwapchain, &lImageCount, mImages.data()));

	mImageViews.resize(lImageCount);
	for (uint32_t i = 0; i < mImageViews.size(); ++i)
	{
		mImageViews[i] = vkh::createImageView(mDevice->mLogicalDevice, mImages[i], mColorFormat);
	}


	// Also create the render pass ?
	
}

/******************************************************************************/
void VulkanSwapchain::resizeSwapchain(uint32_t pWidth, uint32_t pHeight, uint32_t pDesiredImages, bool pVSync)
{
	VK_CHECK(vkDeviceWaitIdle(mDevice->mLogicalDevice));
	createSwapchain(pWidth, pHeight, pDesiredImages, pVSync);
}

/******************************************************************************/
VkResult VulkanSwapchain::acquireNextImage(VkSemaphore pAcquireSemaphore, uint32_t* pImageIndex)
{
	VkResult lResult = vkAcquireNextImageKHR(mDevice->mLogicalDevice, mSwapchain, ~0ull, pAcquireSemaphore, VK_NULL_HANDLE, pImageIndex);
	VK_CHECK(lResult);
	return lResult;
}

/******************************************************************************/
VkResult VulkanSwapchain::queuePresent(VkQueue pQueue, uint32_t pImageIndex, VkSemaphore pReleaseSemaphore)
{
	VkPresentInfoKHR lPresentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	lPresentInfo.waitSemaphoreCount = pReleaseSemaphore != VK_NULL_HANDLE;
	lPresentInfo.pWaitSemaphores = pReleaseSemaphore != VK_NULL_HANDLE  ? &pReleaseSemaphore : NULL;
	lPresentInfo.swapchainCount = 1;		// one per window?
	lPresentInfo.pSwapchains = &mSwapchain;
	lPresentInfo.pImageIndices = &pImageIndex;
	
	VkResult lResult = vkQueuePresentKHR(pQueue, &lPresentInfo);
	VK_CHECK(lResult);
	return lResult;
}

/******************************************************************************/
void VulkanSwapchain::cleanUp()
{
	if
		(mSwapchain)
	{
		for (auto&& lImageView : mImageViews)
			vkDestroyImageView(mDevice->mLogicalDevice, lImageView, NULL);
	}

	if
		(mSurface)
	{
		vkDestroySwapchainKHR(mDevice->mLogicalDevice, mSwapchain, NULL);
		
		vkDestroySurfaceKHR(mContext->mVulkanInstance, mSurface, nullptr);

		mSurface = VK_NULL_HANDLE;
		mSwapchain = VK_NULL_HANDLE;
	}
}