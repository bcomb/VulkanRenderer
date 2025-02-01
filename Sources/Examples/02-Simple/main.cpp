#include <Window.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <VulkanHelper.h>

#include "assert.h"

struct CommandBuffer
{
    VkCommandPool   mCommandPool;       // We use one command pool in case we want to use multiple threads (command pool are not thread safe)
    VkCommandBuffer mCommandBuffer;
    VkFence mFence;                     // Fence signaled when queue submit finished
};

struct FrameData
{
    CommandBuffer mCommandBuffer;       // Command buffer for rendering the frame
    VkSemaphore mSwapchainSemaphore;    // Wait the swapchain image to be available
    VkSemaphore mRenderSemaphore;       // Control presenting image
};

struct VulkanApp
{
    VulkanInstance* mInstance;
    VulkanDevice* mDevice;
    VulkanSwapchain* mSwapchain;
    VulkanGLFWWindow* mWindow;

    // Vulkan per frame data
    std::vector<FrameData> mFrameData;
    uint32_t mCurrentFrame = 0;

    FrameData& getCurrentFrame()
    {
        return mFrameData[mCurrentFrame % mFrameData.size()];
    }

    void init()
    {
        VK_CHECK(volkInitialize());

        printf("Hello VulkanRenderer\n");
        int lSuccess = glfwInit();
        assert(lSuccess);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// MUST BE SET or SwapChain creation fail
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);


        mInstance = new VulkanInstance;
        mInstance->createInstance(VK_API_VERSION_1_3, true);
        mInstance->enumeratePhysicalDevices();
        VkPhysicalDevice lPhysicalDevice = mInstance->pickPhysicalDevice(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

        mDevice = new VulkanDevice(lPhysicalDevice);
        mDevice->createLogicalDevice(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, mInstance->mVulkanInstance);
    }

    void initSwapchain()
    {
        WindowAttributes lWinAttr;
        mWindow = new VulkanGLFWWindow(mInstance, mDevice, lWinAttr, "VulkanTest");
        mSwapchain = mWindow->getSwapchain();

        // FrameData
        mSwapchain->imageCount();
        mFrameData.resize(mSwapchain->imageCount());

        uint32_t lGraphicsQueueFamily = mDevice->getQueueFamilyIndex(VulkanQueueType::Graphics);
        for (auto&& lFrameData : mFrameData)
        {
            // Command pool
            // Note that the pool allow reseting of individual command buffer (VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
            lFrameData.mCommandBuffer.mCommandPool = vkh::createCommandPool(mDevice->mLogicalDevice, lGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

            // Command buffer use for rendering
            // allocate the default command buffer that we will use for rendering
            VkCommandBufferAllocateInfo lCmdAllocInfo = vkh::commandBufferAllocateInfo(lFrameData.mCommandBuffer.mCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
            VK_CHECK(vkAllocateCommandBuffers(mDevice->mLogicalDevice, &lCmdAllocInfo, &lFrameData.mCommandBuffer.mCommandBuffer));

            lFrameData.mCommandBuffer.mFence = vkh::createFence(mDevice->mLogicalDevice);

            // The semaphore is used to synchronize the image acquisition and the rendering
            lFrameData.mSwapchainSemaphore = vkh::createSemaphore(mDevice->mLogicalDevice);
            lFrameData.mRenderSemaphore = vkh::createSemaphore(mDevice->mLogicalDevice);
        }
    }

    void draw_background_by_clearing_swapchain_image(VkCommandBuffer pCmd)
    {
        // Make the swapchain image ready for rendering
        vkh::transitionImage(pCmd, mSwapchain->getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        float flash = (float)fabs(sin(mCurrentFrame / 120.0f));
        VkClearColorValue lClearColor = { 0.3f, 0.3f, flash, 1.0f };

        VkImageSubresourceRange lClearRange = vkh::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        vkCmdClearColorImage(pCmd, mSwapchain->getImage(), VK_IMAGE_LAYOUT_GENERAL, &lClearColor, 1, &lClearRange);

        // Make the swapchain ready for presentation
        vkh::transitionImage(pCmd, mSwapchain->getImage(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    void render()
    {
        FrameData& lCurrentFrame = getCurrentFrame();
        VkCommandBuffer lCommandBuffer = lCurrentFrame.mCommandBuffer.mCommandBuffer;

        // Wait until the gpu has finished rendering the last frame.
        VK_CHECK(vkWaitForFences(mDevice->mLogicalDevice, 1, &lCurrentFrame.mCommandBuffer.mFence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(mDevice->mLogicalDevice, 1, &lCurrentFrame.mCommandBuffer.mFence));

        mSwapchain->acquireNextImage(getCurrentFrame().mSwapchainSemaphore);

        VK_CHECK(vkResetCommandBuffer(lCommandBuffer, 0));

        // Begin recording, tell vulkan we are going to use this command buffer exactly once
        VkCommandBufferBeginInfo lCmdBeginInfo = vkh::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(lCommandBuffer, &lCmdBeginInfo));



        draw_background_by_clearing_swapchain_image(lCommandBuffer);


        VK_CHECK(vkEndCommandBuffer(lCommandBuffer));

        // Prepare the submission to the queue. 
        // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
        // we will signal the _renderSemaphore, to signal that rendering has finished
        VkCommandBufferSubmitInfo lCmdSubmitInfo = vkh::commandBufferSubmitInfo(lCommandBuffer);


        VkSemaphoreSubmitInfo lWaitInfo = vkh::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, lCurrentFrame.mSwapchainSemaphore);
        VkSemaphoreSubmitInfo lSignalInfo = vkh::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, lCurrentFrame.mRenderSemaphore);
        VkSubmitInfo2 lSubmitInfo = vkh::submitInfo(&lCmdSubmitInfo, &lSignalInfo, &lWaitInfo);

        // Submit command buffer to the queue and execute it.
        // The command buffer fence will now block until the graphic commands finish execution
        VK_CHECK(vkQueueSubmit2(mDevice->getQueue(VulkanQueueType::Graphics), 1, &lSubmitInfo, lCurrentFrame.mCommandBuffer.mFence));

        // Present the image (wait submit finished with the mRenderSemaphore)
        mSwapchain->queuePresent(mDevice->getQueue(VulkanQueueType::Graphics), lCurrentFrame.mRenderSemaphore);
        ++mCurrentFrame;
    }

    int run()
    {
        init();
        initSwapchain();

        while (!mWindow->shouldClose())
        {
            glfwPollEvents();
            render();
        }

        return 0;
    }
};


int main(int argc, const char* argv[])
{
    VulkanApp app;
    return app.run();
}