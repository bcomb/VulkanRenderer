#include <Window.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <VulkanHelper.h>
#include <VulkanImage.h>
#include <VulkanDescriptor.h>

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

    // Descriptors
    DescriptorAllocator mGlobalDescriptorAllocator;

    VkDescriptorSet mDrawImageDescriptors;
    VkDescriptorSetLayout mDrawImageDescriptorLayout;


    // Resources
    VulkanImage mDrawImage;


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

    void initDescriptors()
    {
        // Create a descriptor pool that will hold 10 sets with 1 image each
        std::vector<DescriptorAllocator::PoolSize> lPools =
        {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
        };
        mGlobalDescriptorAllocator.initPool(mDevice->mLogicalDevice, 10, lPools);

        DescriptorLayoutBuilder lBuilder;
        lBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        mDrawImageDescriptorLayout = lBuilder.build(mDevice->mLogicalDevice, VK_SHADER_STAGE_COMPUTE_BIT);

        mDrawImageDescriptors = mGlobalDescriptorAllocator.allocate(mDevice->mLogicalDevice, mDrawImageDescriptorLayout);

        VkDescriptorImageInfo lImgInfo{};
        lImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        lImgInfo.imageView = mDrawImage.mView;

        VkWriteDescriptorSet lDrawImageWrite = {};
        lDrawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lDrawImageWrite.pNext = nullptr;

        lDrawImageWrite.dstBinding = 0;
        lDrawImageWrite.dstSet = mDrawImageDescriptors;
        lDrawImageWrite.descriptorCount = 1;
        lDrawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        lDrawImageWrite.pImageInfo = &lImgInfo;

        vkUpdateDescriptorSets(mDevice->mLogicalDevice, 1, &lDrawImageWrite, 0, nullptr);
    }

    void initResources()
    {
        // draw image size will match the window
        mDrawImage.mExtent = { mWindow->width(), mWindow->height(), 1};
        mDrawImage.mFormat = VK_FORMAT_R8G8B8A8_UNORM;

        VkImageUsageFlags lDrawImageUsages{};
        lDrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        lDrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        lDrawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        lDrawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkImageCreateInfo rimg_info = vkh::imageCreateInfo(mDrawImage.mFormat, lDrawImageUsages, mDrawImage.mExtent);

        //for the draw image, we want to allocate it from gpu local memory
        VmaAllocationCreateInfo rimg_allocinfo = {};
        rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        //allocate and create the image
        VK_CHECK(vmaCreateImage(mDevice->mAllocator, &rimg_info, &rimg_allocinfo, &mDrawImage.mImage, &mDrawImage.mAllocation, nullptr));

        //build a image-view for the draw image to use for rendering
        VkImageViewCreateInfo rview_info = vkh::imageViewCreateInfo(mDrawImage.mImage, mDrawImage.mFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(mDevice->mLogicalDevice, &rview_info, nullptr, &mDrawImage.mView));       
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

    // Clear the mDrawImage.mImage
    // Copy the mDrawImage.mImage to the swapchain image
    // Make the swapchain image ready for presentation
    void draw_background_by_clearing_image(VkCommandBuffer pCmd)
    {
        // Make the swapchain image ready for dst copy 
        vkh::transitionImage(pCmd, mSwapchain->getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // VK_IMAGE_LAYOUT_GENERAL to do clearing
        vkh::transitionImage(pCmd, mDrawImage.mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        float flash = (float)fabs(sin(mCurrentFrame / 120.0f));
        VkClearColorValue lClearColor = { 0.3f, 0.3f, flash, 1.0f };
        VkImageSubresourceRange lClearRange = vkh::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        vkCmdClearColorImage(pCmd, mDrawImage.mImage, VK_IMAGE_LAYOUT_GENERAL, &lClearColor, 1, &lClearRange);

        // Ready fo src transfert
        vkh::transitionImage(pCmd, mDrawImage.mImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // Copy the mDrawImage.mImage to the swapchain image
        vkh::copyImageToImage(pCmd, mDrawImage.mImage, mSwapchain->getImage(), { mDrawImage.mExtent.width, mDrawImage.mExtent.height }, { mDrawImage.mExtent.width, mDrawImage.mExtent.height });

        // Make the swapchain ready for presentation
        vkh::transitionImage(pCmd, mSwapchain->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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

        
        
        
        draw_background_by_clearing_image(lCommandBuffer);



        //vkh::transitionImage(lCommandBuffer, mSwapchain->getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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
        initResources();
        initDescriptors();

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