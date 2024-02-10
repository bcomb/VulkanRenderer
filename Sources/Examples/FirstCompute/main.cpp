#include <Window.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <VulkanHelper.h>
#include <VulkanImage.h>
#include <VulkanDescriptor.h>
#include <VulkanShader.h>
#include <VulkanPipeline.h>

#include "assert.h"
#include <vector>
#include <functional>

// Imgui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"


struct CommandBuffer
{
    VkCommandPool   mCommandPool;       // We use one command pool in case we want to use multiple threads (command pool are not thread safe)
    VkCommandBuffer mCommandBuffer;
    VkFence mFence;                     // Fence signaled when queue submit finished

    // Create vulkan object
    void initialize(VulkanDevice* pDevice)
    {
        uint32_t lGraphicsQueueFamily = pDevice->getQueueFamilyIndex(VulkanQueueType::Graphics);

        // Command pool
        // Note that the pool allow reseting of individual command buffer (VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        mCommandPool = vkh::createCommandPool(pDevice->mLogicalDevice, lGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        // Command buffer use for rendering
        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo lCmdAllocInfo = vkh::commandBufferAllocateInfo(mCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        VK_CHECK(vkAllocateCommandBuffers(pDevice->mLogicalDevice, &lCmdAllocInfo, &mCommandBuffer));

        mFence = vkh::createFence(pDevice->mLogicalDevice);
    }

    //void finalize()
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

    //
    CommandBuffer mImmediateCommandBuffer;
    DescriptorAllocator mGlobalDescriptorAllocator;

    // Vulkan per frame data
    std::vector<FrameData> mFrameData;
    uint32_t mCurrentFrame = 0;

    // Care to alignment
    struct PushConstantData
    {
        float data0[4];
        float data1[4];
        float data2[4];
        float data3[4];
    } mPushConstants = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Descriptors
    VkDescriptorSet mDrawImageDescriptors;
    VkDescriptorSetLayout mDrawImageDescriptorLayout;    

    // Resources
    VulkanImage mDrawImage;


    VulkanShader mGradientComputeShader; 
    VkPipelineLayout mGradientPipelineLayout;
    VkPipeline mGradientPipeline;


    std::string getShaderPath()
    {
        return std::string("../Shaders/");
    }

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

        mImmediateCommandBuffer.initialize(mDevice);
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
            lFrameData.mCommandBuffer.initialize(mDevice);

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

    void initPipelines()
    {
        mGradientComputeShader = VulkanShader::loadFromFile(mDevice->mLogicalDevice, getShaderPath() + "gradient.comp.glsl.spv");
        assert(mGradientComputeShader.isValid());

        // Create a compute pipeline
        VkPushConstantRange lPushConstantRange = vkh::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(PushConstantData), 0);
        VkPipelineLayoutCreateInfo gradientPipelineLayout = vkh::pipelineLayoutCreateInfo(&mDrawImageDescriptorLayout, 1, &lPushConstantRange, 0);

        VK_CHECK(vkCreatePipelineLayout(mDevice->mLogicalDevice, &gradientPipelineLayout, nullptr, &mGradientPipelineLayout));
        VkPipelineShaderStageCreateInfo gradientShaderStage = vkh::pipelineShaderStageCreateInfo(mGradientComputeShader.mStage, mGradientComputeShader.mShaderModule);
        VkComputePipelineCreateInfo gradientPipeline = vkh::computePipelineCreateInfo(mGradientPipelineLayout, gradientShaderStage);
        VK_CHECK(vkCreateComputePipelines(mDevice->mLogicalDevice, VK_NULL_HANDLE, 1, &gradientPipeline, nullptr, &mGradientPipeline));
        // TODO : Can destroy the shader module now
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

    void initImGui()
    {
        // 1: create descriptor pool for IMGUI
        //  the size of the pool is very oversize, but it's copied from imgui demo
        //  itself.
        VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        VkDescriptorPool imguiPool;
        VK_CHECK(vkCreateDescriptorPool(mDevice->mLogicalDevice, &pool_info, nullptr, &imguiPool));

        // 2: initialize imgui library

        // this initializes the core structures of imgui
        ImGui::CreateContext();          

        // this initializes imgui for GLFW
        ImGui_ImplGlfw_InitForVulkan(mWindow->getGLFWwindow(), true);

        // this initializes imgui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = mInstance->mVulkanInstance;
        init_info.PhysicalDevice = mDevice->mPhysicalDevice;
        init_info.Device = mDevice->mLogicalDevice;
        init_info.Queue = mDevice->getQueue(VulkanQueueType::Graphics);
        init_info.DescriptorPool = imguiPool;
        init_info.MinImageCount = 3;
        init_info.ImageCount = 3;
        init_info.UseDynamicRendering = true;
        init_info.ColorAttachmentFormat = mSwapchain->mColorFormat;

        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        // Use the volk loaded function to load the other vulkan functions
        ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
            return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkan_instance)), function_name);
            }, &mInstance->mVulkanInstance);
        ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

        // execute a gpu command to upload imgui font textures
        //immediateSubmit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });
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
   
    // dispatch the compute shader
    // Copy the mDrawImage.mImage to the swapchain image
    // Make the swapchain image ready for presentation
    void draw_background_with_gradient_compute_shader(VkCommandBuffer pCmd)
    {
        // Make the swapchain image ready for dst copy 
        vkh::transitionImage(pCmd, mSwapchain->getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // VK_IMAGE_LAYOUT_GENERAL to do clearing
        vkh::transitionImage(pCmd, mDrawImage.mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        // Bind the pipeline
        vkCmdBindPipeline(pCmd, VK_PIPELINE_BIND_POINT_COMPUTE, mGradientPipeline);

        // Bind the descriptor set
        vkCmdBindDescriptorSets(pCmd, VK_PIPELINE_BIND_POINT_COMPUTE, mGradientPipelineLayout, 0, 1, &mDrawImageDescriptors, 0, nullptr);

        // Push constant
        vkCmdPushConstants(pCmd, mGradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantData), &mPushConstants);
        
        // Dispatch
        vkCmdDispatch(pCmd, (uint32_t)ceil(mDrawImage.mExtent.width / 16.0), (uint32_t)ceil(mDrawImage.mExtent.height / 16.0), 1);

        // Ready fo src transfert
        vkh::transitionImage(pCmd, mDrawImage.mImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // Copy the mDrawImage.mImage to the swapchain image
        vkh::copyImageToImage(pCmd, mDrawImage.mImage, mSwapchain->getImage(), { mDrawImage.mExtent.width, mDrawImage.mExtent.height }, { mDrawImage.mExtent.width, mDrawImage.mExtent.height });

        // Make the swapchain ready for presentation
        vkh::transitionImage(pCmd, mSwapchain->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
    {               
        VK_CHECK(vkResetFences(mDevice->mLogicalDevice, 1, &mImmediateCommandBuffer.mFence));
        VK_CHECK(vkResetCommandBuffer(mImmediateCommandBuffer.mCommandBuffer, 0));

        VkCommandBufferBeginInfo cmdBeginInfo = vkh::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VK_CHECK(vkBeginCommandBuffer(mImmediateCommandBuffer.mCommandBuffer, &cmdBeginInfo));

        function(mImmediateCommandBuffer.mCommandBuffer);

        VK_CHECK(vkEndCommandBuffer(mImmediateCommandBuffer.mCommandBuffer));

        VkCommandBufferSubmitInfo lCmdInfo = vkh::commandBufferSubmitInfo(mImmediateCommandBuffer.mCommandBuffer);
        VkSubmitInfo2 lSubmitInfo = vkh::submitInfo(&lCmdInfo, nullptr, nullptr);

        VK_CHECK(vkQueueSubmit2(mDevice->getQueue(VulkanQueueType::Graphics), 1, &lSubmitInfo, mImmediateCommandBuffer.mFence));

        VK_CHECK(vkWaitForFences(mDevice->mLogicalDevice, 1, &mImmediateCommandBuffer.mFence, true, 9999999999));
    }

    void drawImGui(VkCommandBuffer pCmd, VkImageView pTargetImageView)
    {
        VkRenderingAttachmentInfo colorAttachment = vkh::renderingAttachmentInfo(pTargetImageView, VK_IMAGE_LAYOUT_GENERAL, nullptr);
        VkRenderingInfo renderInfo = vkh::renderingInfo(VkRect2D{ VkOffset2D { 0, 0 }, {mSwapchain->mWidth, mSwapchain->mHeight} }, &colorAttachment, nullptr);

        vkCmdBeginRendering(pCmd, &renderInfo);
        
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), pCmd);

        vkCmdEndRendering(pCmd);
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

        
        //draw_background_by_clearing_image(lCommandBuffer);
        draw_background_with_gradient_compute_shader(lCommandBuffer);
        
        vkh::transitionImage(lCommandBuffer, mSwapchain->getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
        drawImGui(lCommandBuffer, mSwapchain->getImageView());
        vkh::transitionImage(lCommandBuffer, mSwapchain->getImage(), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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

    void UI_PushConstantWidget()
    {
        if (ImGui::Begin("background")) {
            ImGui::Text("PushConstant data");
            ImGui::ColorEdit4("data0", (float*)&mPushConstants.data0);
            ImGui::ColorEdit4("data1", (float*)&mPushConstants.data0);
            ImGui::ColorEdit4("data2", (float*)&mPushConstants.data0);
            ImGui::ColorEdit4("data3", (float*)&mPushConstants.data0);
            ImGui::End();
        }
    }

    int run()
    {
        init();
        initSwapchain();        
        initResources();
        initDescriptors();
        initPipelines();

        initImGui();

        while (!mWindow->shouldClose())
        {
            glfwPollEvents();

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            //some imgui UI to test
            //ImGui::ShowDemoWindow();

            UI_PushConstantWidget();

            //make imgui calculate internal draw structures
            ImGui::Render();

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