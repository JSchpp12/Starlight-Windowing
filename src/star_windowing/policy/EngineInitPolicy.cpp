#include "star_windowing/policy/EngineInitPolicy.hpp"

#include "star_windowing/SwapChainRenderer.hpp"
#include <starlight/common/ConfigFile.hpp>

#include <GLFW/glfw3.h>

namespace star::windowing
{
core::RenderingInstance EngineInitPolicy::createRenderingInstance(std::string appName)
{
    glfwInit();

    auto extensions = getRequiredDisplayExtensions();
    core::RenderingInstance instance{appName, extensions};

    m_winContext.window = createWindow();
    m_winContext.surface = createRenderingSurface(instance.getVulkanInstance(), m_winContext.window);

    return instance;
}

void EngineInitPolicy::cleanup(vk::Instance instance)
{
    m_winContext.window.cleanupRender();
    m_winContext.surface.cleanupRender(instance);
}

core::device::StarDevice EngineInitPolicy::createNewDevice(
    core::RenderingInstance &renderingInstance, std::set<star::Rendering_Features> &engineRenderingFeatures,
    std::set<Rendering_Device_Features> &engineRenderingDeviceFeatures)
{
    vk::SurfaceKHR vkSurface = m_winContext.surface.getVulkanSurface();
    return core::device::StarDevice(renderingInstance, engineRenderingFeatures, engineRenderingDeviceFeatures,
                                    &vkSurface);
}

RenderingSurface EngineInitPolicy::createRenderingSurface(vk::Instance instance, StarWindow &window) const
{
    RenderingSurface surface;
    surface.init(instance, window);

    return surface;
}

vk::Extent2D EngineInitPolicy::getEngineRenderingResolution() const
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
} // namespace star::windowing
