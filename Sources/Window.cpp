#include "Window.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanHelper.h"

#include <assert.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

/******************************************************************************/
void Window::WindowCloseCallback(GLFWwindow* pWindow)
{
    Window* lWindow = (Window*)glfwGetWindowUserPointer(pWindow);
    if(lWindow) {
        lWindow->onWindowClose();
    }
}

/******************************************************************************/
void Window::WindowSizeCallback(GLFWwindow* pWindow, int pWidth, int pHeight)
{
    Window* lWindow = (Window*)glfwGetWindowUserPointer(pWindow);
    if (lWindow) {
        lWindow->onWindowSize((uint32_t)pWidth, (uint32_t)pHeight);
    }
}

/******************************************************************************/
Window::Window(VulkanContext pVulkanContext, WindowAttributes pRequestAttrib, const char* pName)
: mVulkanContext(pVulkanContext)
, mRequestedAttribs(pRequestAttrib)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// MUST BE SET or SwapChain creation fail
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

    VkDevice lDevice = mVulkanContext.mDevice->mLogicalDevice;

    mGLFWwindow = glfwCreateWindow((int)mRequestedAttribs.mWidth, (int)mRequestedAttribs.mHeight, pName, 0, 0);
    glfwSetWindowUserPointer(mGLFWwindow, this);
    glfwSetWindowCloseCallback(mGLFWwindow, &Window::WindowCloseCallback);
    glfwSetWindowSizeCallback(mGLFWwindow, &Window::WindowSizeCallback);

    mVulkanSwapchain = new VulkanSwapchain(mVulkanContext.mInstance, mVulkanContext.mDevice);
    mVulkanSwapchain->initSurface((void*)GetModuleHandle(0), winId());
    mVulkanSwapchain->createSwapchain(mRequestedAttribs.mWidth, mRequestedAttribs.mHeight, mRequestedAttribs.mSwapchainImageCount, mRequestedAttribs.mVSync);

    mRenderPass = vkh::createRenderPass(mVulkanContext.mDevice->mLogicalDevice, mVulkanSwapchain->mColorFormat);

    mSwapchainFramebuffers = vkh::createSwapchainFramebuffer(mVulkanContext.mDevice->mLogicalDevice, mRenderPass, *mVulkanSwapchain);

    // Create FrameData
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

    // TODO : check what we obtain
    mAttribs = mRequestedAttribs;
}

/******************************************************************************/
Window::~Window()
{

}

/******************************************************************************/
void* Window::winId() const
{
    return (void*)glfwGetWin32Window(mGLFWwindow);
}

/******************************************************************************/
void Window::onWindowClose()
{
    mShouldClose = true;
}

/******************************************************************************/
void Window::onWindowSize(uint32_t pWidth, uint32_t pHeight)
{   
    // Probably necessary
    //VK_CHECK(vkDeviceWaitIdle(lDevice));

    mRequestedAttribs.mWidth = pWidth;
    mRequestedAttribs.mHeight = pHeight;
    mVulkanSwapchain->resizeSwapchain(mRequestedAttribs.mWidth, mRequestedAttribs.mHeight, mRequestedAttribs.mSwapchainImageCount, mRequestedAttribs.mVSync);

    for (auto&& lFramebuffer : mSwapchainFramebuffers)
        vkDestroyFramebuffer(mVulkanContext.mDevice->mLogicalDevice, lFramebuffer, NULL);
    mSwapchainFramebuffers.clear();

    mSwapchainFramebuffers = vkh::createSwapchainFramebuffer(mVulkanContext.mDevice->mLogicalDevice, mRenderPass, *mVulkanSwapchain);

#pragma message("TODO : check what we obtain")
    mAttribs = mRequestedAttribs;
}

/******************************************************************************/
void Window::beginFrame()
{    
    //mVulkanSwapchain->acquireNextImage(getCurrentFrame().mSwapchainSemaphore);
}

/******************************************************************************/
void Window::render()
{
    FrameData& lCurrentFrame = getCurrentFrame();    
    VkCommandBuffer lCommandBuffer = lCurrentFrame.mCommandBuffer.mCommandBuffer;

    // Wait until the gpu has finished rendering the last frame.
    VK_CHECK(vkWaitForFences(mVulkanContext.mDevice->mLogicalDevice, 1, &lCurrentFrame.mCommandBuffer.mFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(mVulkanContext.mDevice->mLogicalDevice, 1, &lCurrentFrame.mCommandBuffer.mFence));

    mVulkanSwapchain->acquireNextImage(getCurrentFrame().mSwapchainSemaphore);

    VK_CHECK(vkResetCommandBuffer(lCommandBuffer, 0));

    // Begin recording, tell vulkan we are going to use this command buffer exactly once
    VkCommandBufferBeginInfo lCmdBeginInfo = vkh::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(lCommandBuffer, &lCmdBeginInfo));

    // Make the swapchain image ready for rendering
    vkh::transitionImage(lCommandBuffer, mVulkanSwapchain->getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    float flash = fabs(sin(mCurrentFrame / 120.0f));
    VkClearColorValue lClearColor = { 0.3f, 0.3f, flash, 1.0f };

    VkImageSubresourceRange lClearRange = vkh::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(lCommandBuffer, mVulkanSwapchain->getImage(), VK_IMAGE_LAYOUT_GENERAL, &lClearColor, 1, &lClearRange);

    // Make the swapchain ready form presentation
    vkh::transitionImage(lCommandBuffer, mVulkanSwapchain->getImage(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(lCommandBuffer));

    // Prepare the submission to the queue. 
    // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    // we will signal the _renderSemaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo lCmdSubmitInfo = vkh::commandBufferSubmitInfo(lCommandBuffer);

    // 
    VkSemaphoreSubmitInfo lWaitInfo = vkh::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, lCurrentFrame.mSwapchainSemaphore);
    VkSemaphoreSubmitInfo lSignalInfo = vkh::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, lCurrentFrame.mRenderSemaphore);

    VkSubmitInfo2 lSubmitInfo = vkh::submitInfo(&lCmdSubmitInfo, &lSignalInfo, &lWaitInfo);

    // Submit command buffer to the queue and execute it.
    // The command buffer fence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(mVulkanContext.mDevice->getQueue(VulkanQueueType::Graphics), 1, &lSubmitInfo, lCurrentFrame.mCommandBuffer.mFence));


    /*
    //VkDevice lDevice = mVulkanContext.mDevice->mLogicalDevice;

    //VK_CHECK(vkWaitForFences(lDevice, 1, &lCommandBufferFence, VK_TRUE, UINT64_MAX));
    //VK_CHECK(vkResetFences(lDevice, 1, &lCommandBufferFence));
    //VK_CHECK(vkResetCommandBuffer(lCommandBuffer, 0));

    VkCommandBufferBeginInfo lBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    lBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(lCommandBuffer, &lBeginInfo));


    VkClearValue lClearColor = { 0.3f, 0.3f, 0.3f, 1.0f };

    VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassInfo.renderPass = mRenderPass;
    renderPassInfo.framebuffer = mSwapchainFramebuffers[mSwapchainImageIndex];
    renderPassInfo.renderArea = { {0,0} , {(uint32_t)mAttribs.mWidth, (uint32_t)mAttribs.mHeight} };
    renderPassInfo.clearValueCount = 1;	// MRT
    renderPassInfo.pClearValues = &lClearColor;

    // barrier not needed, LayoutTransition are done during BeginRenderPass/EndRenderPass
    //VkImageMemoryBarrier renderBeginBarrier = imageBarrier(lSwapChainImages[lImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    //vkCmdPipelineBarrier(lCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);
    vkCmdBeginRenderPass(lCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    //VkViewport viewport = { 0.0f, 0.0f,(float)lWindowWidth, (float)lWindowHeight, 0.0f, 1.0f };
    // Tricks negative viewport to invert
    VkViewport viewport = { 0.0f,(float)mAttribs.mHeight,(float)mAttribs.mWidth, -(float)mAttribs.mHeight, 0.0f, 1.0f }; // Vulkan 1_1 extension to reverse Y axis
    vkCmdSetViewport(lCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor = { {0,0}, {(uint32_t)mAttribs.mWidth,(uint32_t)mAttribs.mHeight} };
    vkCmdSetScissor(lCommandBuffer, 0, 1, &scissor);


    // TODO : Draw call


    vkCmdEndRenderPass(lCommandBuffer);

    VK_CHECK(vkEndCommandBuffer(lCommandBuffer));


    // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
    VkPipelineStageFlags lSubmitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //  Pipeline stages used to wait at for graphics queue submissions
    VkSubmitInfo lSubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    lSubmitInfo.waitSemaphoreCount = 1;
    lSubmitInfo.pWaitSemaphores = &mAcquireSemaphore;
    lSubmitInfo.pWaitDstStageMask = &lSubmitStageMask;
    lSubmitInfo.commandBufferCount = 1;
    lSubmitInfo.pCommandBuffers = &lCommandBuffer;
    lSubmitInfo.signalSemaphoreCount = 1;
    lSubmitInfo.pSignalSemaphores = &mReleaseSemaphore;
    VK_CHECK(vkQueueSubmit(mVulkanContext.mDevice->getQueue(VulkanQueueType::Graphics), 1, &lSubmitInfo, lCommandBufferFence));
    */

   // Submit the command buffer to the graphics queue
}

/******************************************************************************/
void Window::present()
{
    FrameData& lCurrentFrame = getCurrentFrame();
    mVulkanSwapchain->queuePresent(mVulkanContext.mDevice->getQueue(VulkanQueueType::Graphics), lCurrentFrame.mRenderSemaphore);
    ++mCurrentFrame;
}