#include "Window.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"

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
Window::Window(VulkanContext* pVulkanContext, WindowAttributes pRequestAttrib, const char* pName)
: mVulkanContext(pVulkanContext)
, mRequestedAttribs(pRequestAttrib)
{
    assert(pVulkanContext);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// MUST BE SET or SwapChain creation fail
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

    mGLFWwindow = glfwCreateWindow((int)mRequestedAttribs.mWidth, (int)mRequestedAttribs.mHeight, pName, 0, 0);
    glfwSetWindowUserPointer(mGLFWwindow, this);
    glfwSetWindowCloseCallback(mGLFWwindow, &Window::WindowCloseCallback);
    glfwSetWindowSizeCallback(mGLFWwindow, &Window::WindowSizeCallback);

    mVulkanSwapchain = new VulkanSwapchain(mVulkanContext->mInstance, mVulkanContext->mDevice);
    mVulkanSwapchain->initSurface((void*)GetModuleHandle(0), winId());
    mVulkanSwapchain->createSwapchain(mRequestedAttribs.mWidth, mRequestedAttribs.mHeight, mRequestedAttribs.mSwapchainImageCount, mRequestedAttribs.mVSync);

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
    mRequestedAttribs.mWidth = pWidth;
    mRequestedAttribs.mHeight = pHeight;
    mVulkanSwapchain->resizeSwapchain(mRequestedAttribs.mWidth, mRequestedAttribs.mHeight, mRequestedAttribs.mSwapchainImageCount, mRequestedAttribs.mVSync);
    // TODO : check what we obtain
    mAttribs = mRequestedAttribs;
}

/******************************************************************************/
void Window::beginFrame()
{    
    mVulkanSwapchain->acquireNextImage(mAcquireSemaphore, &mSwapchainImageIndex);
}

/******************************************************************************/
void Window::present()
{
    mVulkanSwapchain->queuePresent(mVulkanContext->mDevice->getQueue(VulkanQueueType::Graphics), mSwapchainImageIndex, mReleaseSemaphore);
}