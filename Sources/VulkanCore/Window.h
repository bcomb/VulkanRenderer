#pragma once
#include "vk_common.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include <stdint.h>

struct WindowAttributes
{
    uint32_t mWidth = 1024;
    uint32_t mHeight = 768;
    uint32_t mSwapchainImageCount = 2; // generally value between 1-3
    VkFormat mColorFormat = VK_FORMAT_B8G8R8A8_UNORM; 
    VkFormat mDepthFormat = VK_FORMAT_UNDEFINED; // VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT
    bool mVSync = false; // vsync enable or not
    bool mResizable = false;
};


// Create a GLFW window
// Create a surface attached to the window id given by GLFW
// Create a swapchain on the surface
// Manage resizing (it try...)
class VulkanGLFWWindow
{
public:
    VulkanGLFWWindow(VulkanInstance* pVulkanInstance, VulkanDevice* pVulkanDevice, WindowAttributes pRequestAttribs, const char* pName = nullptr);
    ~VulkanGLFWWindow();
    bool shouldClose() const;
    uint32_t width() const { return mAttribs.mWidth; }
    uint32_t height() const { return mAttribs.mHeight; }
    // Native handle
    void* winId() const;
    inline bool shouldClose() { return mShouldClose; }
    VulkanSwapchain* getSwapchain() { return mVulkanSwapchain; }

protected:
    // GLFW callback
    static void WindowCloseCallback(struct GLFWwindow* pWindow);
    void onWindowClose();
    static void WindowSizeCallback(GLFWwindow* pWindow, int pWidth, int pHeight);
    void onWindowSize(uint32_t pWidth, uint32_t pHeight);

protected:
    WindowAttributes mRequestedAttribs; // What we ask for
    WindowAttributes mAttribs;          // What we obtain
    struct GLFWwindow* mGLFWwindow = nullptr;

    VulkanInstance* mVulkanInstance;
    VulkanDevice*   mVulkanDevice;
    VulkanSwapchain* mVulkanSwapchain = nullptr;

    bool mShouldClose = false;
};