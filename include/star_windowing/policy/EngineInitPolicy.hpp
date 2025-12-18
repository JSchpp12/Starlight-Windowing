#pragma once

#include <star_windowing/WindowingContext.hpp>
#include <starlight/core/RenderingInstance.hpp>
#include <starlight/core/device/StarDevice.hpp>
#include <starlight/core/renderer/RendererBase.hpp>
#include <starlight/enums/Enums.hpp>

#include <star_common/Renderer.hpp>

#include <set>

namespace star::windowing
{
class EngineInitPolicy
{
  public:
    explicit EngineInitPolicy(WindowingContext &winContext) : m_winContext(winContext){};

    core::RenderingInstance createRenderingInstance(std::string appName);

    core::device::StarDevice createNewDevice(core::RenderingInstance &renderingInstance,
                                             std::set<star::Rendering_Features> &engineRenderingFeatures,
                                             std::set<Rendering_Device_Features> &engineRenderingDeviceFeatures);
    vk::Extent2D getEngineRenderingResolution() const;

    void cleanup(vk::Instance);

  private:
    WindowingContext &m_winContext;

    RenderingSurface createRenderingSurface(vk::Instance instance, StarWindow &window) const;

    StarWindow createWindow() const;

    std::vector<const char *> getRequiredDisplayExtensions() const;
};
} // namespace star::windowing