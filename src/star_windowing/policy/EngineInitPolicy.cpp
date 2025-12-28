#include "star_windowing/policy/EngineInitPolicy.hpp"

#include "star_windowing/SwapChainRenderer.hpp"
#include "star_windowing/service/SwapChainControllerService.hpp"
#include <starlight/common/ConfigFile.hpp>

#include <GLFW/glfw3.h>

namespace star::windowing
{
core::RenderingInstance EngineInitPolicy::createRenderingInstance(std::string appName)
{
    glfwInit();

    auto extensions = getRequiredDisplayExtensions();
    extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
    core::RenderingInstance instance{appName, extensions};

    m_winContext.window = createWindow();
    m_winContext.surface = createRenderingSurface(instance.getVulkanInstance(), m_winContext.window);

    return instance;
}

void EngineInitPolicy::cleanup(core::RenderingInstance &instance)
{
    m_winContext.window.cleanupRender();
    m_winContext.surface.cleanupRender(instance.getVulkanInstance());
}

core::device::StarDevice EngineInitPolicy::createNewDevice(
    core::RenderingInstance &renderingInstance, std::set<star::Rendering_Features> &engineRenderingFeatures,
    std::set<Rendering_Device_Features> &engineRenderingDeviceFeatures)
{
    vk::SurfaceKHR vkSurface = m_winContext.surface.getVulkanSurface();
    return core::device::StarDevice(renderingInstance, engineRenderingFeatures, engineRenderingDeviceFeatures,
                                    {VK_KHR_SWAPCHAIN_EXTENSION_NAME}, &vkSurface);
}

RenderingSurface EngineInitPolicy::createRenderingSurface(vk::Instance instance, StarWindow &window) const
{
    RenderingSurface surface;
    surface.init(instance, window);

    return surface;
}

vk::Extent2D EngineInitPolicy::getEngineRenderingResolution()
{
    return m_winContext.window.getWindowFramebufferSize();
}

StarWindow EngineInitPolicy::createWindow() const
{
    int width{std::stoi(star::ConfigFile::getSetting(Config_Settings::resolution_x))};
    int height{std::stoi(star::ConfigFile::getSetting(Config_Settings::resolution_y))};
    std::string name = star::ConfigFile::getSetting(Config_Settings::app_name);

    return StarWindow::Builder().setWidth(width).setHeight(height).setTitle(name).build();
}

std::vector<const char *> EngineInitPolicy::getRequiredDisplayExtensions() const
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    return std::vector<const char *>(glfwExtensions, glfwExtensions + glfwExtensionCount);
}

common::FrameTracker::Setup EngineInitPolicy::getFrameInFlightTrackingSetup(core::device::StarDevice &device)
{
    uint8_t min, max;
    getNumSupportedSwapchainImages(device, min, max);

    uint8_t numSwapChainImages = min + 1;
    return {m_maxNumFramesInFlight, numSwapChainImages};
}

std::vector<service::Service> EngineInitPolicy::getAdditionalDeviceServices()
{
    std::vector<service::Service> service;
    service.emplace_back(createSwapchainService());

    return service;
}

void EngineInitPolicy::init(uint8_t requestedNumFramesInFlight)
{
    m_maxNumFramesInFlight = requestedNumFramesInFlight;
}

void EngineInitPolicy::getNumSupportedSwapchainImages(core::device::StarDevice &device, uint8_t &min,
                                                      uint8_t &max) const
{
    const auto result = device.getPhysicalDevice().getSurfaceCapabilities2KHR(m_winContext.surface.getVulkanSurface());

    min = (uint8_t)result.surfaceCapabilities.minImageCount;
    max = (uint8_t)result.surfaceCapabilities.maxImageCount;
}

service::Service EngineInitPolicy::createSwapchainService()
{
    return service::Service{SwapChainControllerService{m_winContext}};
}
} // namespace star::windowing
