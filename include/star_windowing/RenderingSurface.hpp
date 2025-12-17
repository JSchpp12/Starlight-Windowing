#pragma once

#include "StarWindow.hpp"

#include <vulkan/vulkan.hpp>

namespace star::windowing
{
class RenderingSurface
{
  public:
    RenderingSurface() = default;

    RenderingSurface(const RenderingSurface &) = delete;
    RenderingSurface &operator=(const RenderingSurface &) = delete;
    RenderingSurface(RenderingSurface &&other) = default;
    RenderingSurface &operator=(RenderingSurface &&other) = default;
    ~RenderingSurface() = default;

    void init(vk::Instance instance, StarWindow &window);

    void cleanupRender(vk::Instance instance);

    vk::SurfaceKHR &getVulkanSurface()
    {
        return m_surface;
    }
    const vk::SurfaceKHR &getVulkanSurface() const
    {
        return m_surface;
    }

  private:
    vk::SurfaceKHR m_surface;

    static vk::SurfaceKHR CreateSurface(vk::Instance instance, StarWindow &window);
};
} // namespace star::windowing