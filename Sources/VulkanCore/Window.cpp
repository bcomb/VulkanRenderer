#include "Window.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanHelper.h"

#include <assert.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

/******************************************************************************/
void VulkanGLFWWindow::WindowCloseCallback(GLFWwindow* pWindow)
{
    VulkanGLFWWindow* lWindow = (VulkanGLFWWindow*)glfwGetWindowUserPointer(pWindow);
    if(lWindow) {
        lWindow->onWindowClose();
    }
}

/******************************************************************************/
void VulkanGLFWWindow::WindowSizeCallback(GLFWwindow* pWindow, int pWidth, int pHeight)
{
    VulkanGLFWWindow* lWindow = (VulkanGLFWWindow*)glfwGetWindowUserPointer(pWindow);
    if (lWindow) {
        lWindow->onWindowSize((uint32_t)pWidth, (uint32_t)pHeight);
    }
}

/******************************************************************************/
VulkanGLFWWindow::VulkanGLFWWindow(VulkanInstance* pVulkanInstance, VulkanDevice* pVulkanDevice, WindowAttributes pRequestAttrib, const char* pName)
: mVulkanInstance(pVulkanInstance)
, mVulkanDevice(pVulkanDevice)
, mRequestedAttribs(pRequestAttrib)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// MUST BE SET or SwapChain creation fail
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

    mGLFWwindow = glfwCreateWindow((int)mRequestedAttribs.mWidth, (int)mRequestedAttribs.mHeight, pName, 0, 0);
    glfwSetWindowUserPointer(mGLFWwindow, this);
    glfwSetWindowCloseCallback(mGLFWwindow, &VulkanGLFWWindow::WindowCloseCallback);
    glfwSetWindowSizeCallback(mGLFWwindow, &VulkanGLFWWindow::WindowSizeCallback);

    mVulkanSwapchain = new VulkanSwapchain(mVulkanInstance, mVulkanDevice);
    mVulkanSwapchain->initSurface((void*)GetModuleHandle(0), winId());
    mVulkanSwapchain->createSwapchain(mRequestedAttribs.mWidth, mRequestedAttribs.mHeight, mRequestedAttribs.mSwapchainImageCount, mRequestedAttribs.mVSync);

    //mRenderPass = vkh::createRenderPass(mVulkanContext.mDevice->mLogicalDevice, mVulkanSwapchain->mColorFormat);
    //mSwapchainFramebuffers = vkh::createSwapchainFramebuffer(mVulkanContext.mDevice->mLogicalDevice, mRenderPass, *mVulkanSwapchain);

    // Create FrameData
    /*
    uint32_t lGraphicsQueueFamily = mVulkanContext.mDevice->getQueueFamilyIndex(VulkanQueueType::Graphics);
    for (int i = 0; i < FRAME_BUFFER_COUNT; ++i)
    {
        // Command pool
        // Note that the pool allow reseting of individual command buffer (VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        mFrames[i].mCommandBuffer.mCommandPool = vkh::createCommandPool(lDevice, lGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        // Command buffer use for rendering
        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo lCmdAllocInfo = vkh::commandBufferAllocateInfo(mFrames[i].mCommandBuffer.mCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        VK_CHECK(vkAllocateCommandBuffers(lDevice, &lCmdAllocInfo, &mFrames[i].mCommandBuffer.mCommandBuffer));

        mFrames[i].mCommandBuffer.mFence = vkh::createFence(lDevice);

        // The semaphore is used to synchronize the image acquisition and the rendering
        mFrames[i].mSwapchainSemaphore = vkh::createSemaphore(mVulkanContext.mDevice->mLogicalDevice);
        mFrames[i].mRenderSemaphore = vkh::createSemaphore(mVulkanContext.mDevice->mLogicalDevice);
    };        
    */

    // TODO : check what we obtain
    mAttribs = mRequestedAttribs;
}

/******************************************************************************/
VulkanGLFWWindow::~VulkanGLFWWindow()
{

}

/******************************************************************************/
void* VulkanGLFWWindow::winId() const
{
    return (void*)glfwGetWin32Window(mGLFWwindow);
}

/******************************************************************************/
void VulkanGLFWWindow::onWindowClose()
{
    mShouldClose = true;
}

/******************************************************************************/
void VulkanGLFWWindow::onWindowSize(uint32_t pWidth, uint32_t pHeight)
{   
    // Probably necessary
    VK_CHECK(vkDeviceWaitIdle(mVulkanDevice->mLogicalDevice));

    mRequestedAttribs.mWidth = pWidth;
    mRequestedAttribs.mHeight = pHeight;
    mVulkanSwapchain->resizeSwapchain(mRequestedAttribs.mWidth, mRequestedAttribs.mHeight, mRequestedAttribs.mSwapchainImageCount, mRequestedAttribs.mVSync);

    // No more needed with Vulkan 1.3.0
    //for (auto&& lFramebuffer : mSwapchainFramebuffers)
    //    vkDestroyFramebuffer(mVulkanDevice->mLogicalDevice, lFramebuffer, NULL);
    //mSwapchainFramebuffers.clear();
    //mSwapchainFramebuffers = vkh::createSwapchainFramebuffer(mVulkanDevice->mLogicalDevice, mRenderPass, *mVulkanSwapchain);

#pragma message("TODO : check what we obtain")
    mAttribs = mRequestedAttribs;
}