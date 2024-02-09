#include "Window.h"

#include "assert.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <VulkanHelper.h>

struct MyExampleWindow : public Window
{
	MyExampleWindow(VulkanContext pVulkanContext, WindowAttributes pRequestAttribs, const char* pName = nullptr) : Window(pVulkanContext, pRequestAttribs, pName) {}

    void initialize()
    {

    }

    void finalize()
    {

    }

	virtual void render() override
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

        float flash = (float)fabs(sin(mCurrentFrame / 120.0f));
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


        VkSemaphoreSubmitInfo lWaitInfo = vkh::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, lCurrentFrame.mSwapchainSemaphore);
        VkSemaphoreSubmitInfo lSignalInfo = vkh::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, lCurrentFrame.mRenderSemaphore);
        VkSubmitInfo2 lSubmitInfo = vkh::submitInfo(&lCmdSubmitInfo, &lSignalInfo, &lWaitInfo);

        // Submit command buffer to the queue and execute it.
        // The command buffer fence will now block until the graphic commands finish execution
        VK_CHECK(vkQueueSubmit2(mVulkanContext.mDevice->getQueue(VulkanQueueType::Graphics), 1, &lSubmitInfo, lCurrentFrame.mCommandBuffer.mFence));

        // Present the image (wait submit finished with the mRenderSemaphore)
        mVulkanSwapchain->queuePresent(mVulkanContext.mDevice->getQueue(VulkanQueueType::Graphics), lCurrentFrame.mRenderSemaphore);
        ++mCurrentFrame;
    }
};



int main(int argc, const char* argv[])
{
	VK_CHECK(volkInitialize());

	printf("Hello VulkanRenderer\n");
	int lSuccess = glfwInit();
	assert(lSuccess);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// MUST BE SET or SwapChain creation fail
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);


	VulkanInstance* lVulkanInstance = new VulkanInstance;
	lVulkanInstance->createInstance(VK_API_VERSION_1_3, true);
	lVulkanInstance->enumeratePhysicalDevices();
	VkPhysicalDevice lPhysicalDevice = lVulkanInstance->pickPhysicalDevice(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

	VulkanDevice* lDevice = new VulkanDevice(lPhysicalDevice);
	lDevice->createLogicalDevice(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, lVulkanInstance->mVulkanInstance);

	VulkanContext lContext = { lVulkanInstance, lDevice };

	WindowAttributes lWinAttr;
    MyExampleWindow* lWindow = new MyExampleWindow(lContext, lWinAttr, "VulkanTest");

    lWindow->initialize();

	while (!lWindow->shouldClose())
	{
		glfwPollEvents();
		lWindow->render();
	}

    lWindow->finalize();

	return 0;
}