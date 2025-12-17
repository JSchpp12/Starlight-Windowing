#include "star_windowing/RenderingSurface.hpp"

namespace star::windowing
{
vk::SurfaceKHR RenderingSurface::CreateSurface(vk::Instance instance, StarWindow &window)
{
    VkSurfaceKHR surfaceTmp = VkSurfaceKHR();

    auto createResult = glfwCreateWindowSurface(instance, window.getGLFWWindow(), nullptr, &surfaceTmp);
    if (createResult != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface");
    }
    return surfaceTmp;
}

void RenderingSurface::cleanupRender(vk::Instance instance)
{
    instance.destroySurfaceKHR(m_surface);
}

void RenderingSurface::init(vk::Instance instance, StarWindow &window)
{
    m_surface = CreateSurface(instance, window);
}
} // namespace star::windowing
