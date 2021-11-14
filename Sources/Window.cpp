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

    mAcquireSemaphore = vkh::createSemaphore(mVulkanContext.mDevice->mLogicalDevice);
    mReleaseSemaphore = vkh::createSemaphore(mVulkanContext.mDevice->mLogicalDevice);


    // Command buffer
    mCommandPool = vkh::createCommandPool(lDevice, mVulkanContext.mDevice->getQueueFamilyIndex(VulkanQueueType::Graphics));

    VkCommandBufferAllocateInfo lAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    lAllocateInfo.commandPool = mCommandPool;
    lAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    lAllocateInfo.commandBufferCount = COMMAND_BUFFER_COUNT;
    VK_CHECK(vkAllocateCommandBuffers(lDevice, &lAllocateInfo, mCommandBuffers));

    
    for (int i = 0; i < COMMAND_BUFFER_COUNT; ++i)
        mCommandBufferFences[i] = vkh::createFence(lDevice);

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
uint32_t Window::beginFrame()
{    
    mVulkanSwapchain->acquireNextImage(mAcquireSemaphore, &mSwapchainImageIndex);
    return mSwapchainImageIndex;
}

/******************************************************************************/
void Window::render()
{
    mCommandBufferIndex = (++mCommandBufferIndex) % COMMAND_BUFFER_COUNT;
    VkCommandBuffer lCommandBuffer = mCommandBuffers[mCommandBufferIndex];
    VkFence lCommandBufferFence = mCommandBufferFences[mCommandBufferIndex];

    VkDevice lDevice = mVulkanContext.mDevice->mLogicalDevice;

    VK_CHECK(vkWaitForFences(lDevice, 1, &lCommandBufferFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(lDevice, 1, &lCommandBufferFence));
    VK_CHECK(vkResetCommandBuffer(lCommandBuffer, 0));

    VkCommandBufferBeginInfo lBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    lBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(lCommandBuffer, &lBeginInfo));


    VkClearValue lClearColor = { 0.3f, 0.2f, 0.3f, 1.0f };

    VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassInfo.renderPass = mRenderPass;
    renderPassInfo.framebuffer = mSwapchainFramebuffers[mSwapchainImageIndex];
    renderPassInfo.renderArea = { {0,0} , {(uint32_t)mAttribs.mWidth, (uint32_t)mAttribs.mHeight} };
    renderPassInfo.clearValueCount = 1;	// MRT
    renderPassInfo.pClearValues = &lClearColor;

    // barrier not needed, LayoutTransition are done during BeginRenderPass/EndRenderPass
    //VkImageMemoryBarrier renderBeginBarrier = imageBarrier(lSwapChainImages[lImageIndex], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
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
}

/******************************************************************************/
void Window::present()
{
    mVulkanSwapchain->queuePresent(mVulkanContext.mDevice->getQueue(VulkanQueueType::Graphics), mSwapchainImageIndex, mReleaseSemaphore);
}