#pragma once
#include "vk_common.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"

#include <stdint.h>

struct VulkanContext
{
    VulkanInstance* mInstance;
    VulkanDevice* mDevice;
};

struct WindowAttributes
{
    uint32_t mWidth = 512;
    uint32_t mHeight = 512;
    uint32_t mSwapchainImageCount = 2; // generally value between 1-3
    bool mVSync = false; // vsync enable or not
    VkFormat mColorFormat = VK_FORMAT_B8G8R8A8_UNORM; 
    VkFormat mDepthFormat = VK_FORMAT_UNDEFINED; // VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT
};


class Window
{
public:
    Window(VulkanContext* pVulkanContext, WindowAttributes pRequestAttribs, const char* pName = nullptr);
    ~Window();
    bool shouldClose() const;
    uint32_t width() const;
    uint32_t height() const;

    // Native handle
    void* winId() const;

    void beginFrame(); // acquire the next image of the swapchain
    void present(); // swap

protected:
    // GLFW callback
    static void WindowCloseCallback(struct GLFWwindow* pWindow);
    void onWindowClose();
    static void WindowSizeCallback(GLFWwindow* pWindow, int pWidth, int pHeight);
    void onWindowSize(uint32_t pWidth, uint32_t pHeight);

protected:
    WindowAttributes mRequestedAttribs; // What we ask for
    WindowAttributes mAttribs; // What we obtain

    struct GLFWwindow* mGLFWwindow = nullptr;
    VulkanContext* mVulkanContext = nullptr;
    VulkanSwapchain* mVulkanSwapchain = nullptr;
    VkSemaphore mAcquireSemaphore = VK_NULL_HANDLE;
    VkSemaphore mReleaseSemaphore = VK_NULL_HANDLE;
    uint32_t mSwapchainImageIndex = 0;

    bool mShouldClose = false;
};