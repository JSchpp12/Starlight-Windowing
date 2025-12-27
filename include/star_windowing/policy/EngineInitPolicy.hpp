#pragma once

#include <star_windowing/WindowingContext.hpp>
#include <starlight/core/RenderingInstance.hpp>
#include <starlight/core/device/StarDevice.hpp>
#include <starlight/core/renderer/RendererBase.hpp>
#include <starlight/enums/Enums.hpp>
#include <starlight/service/Service.hpp>

#include <star_common/FrameTracker.hpp>
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

    common::FrameTracker::Setup getFrameInFlightTrackingSetup(core::device::StarDevice &device);

    void cleanup(vk::Instance);

    void init(uint8_t requestedNumFramesInFlight);

    std::vector<service::Service> getAdditionalDeviceServices();

  private:
    WindowingContext &m_winContext;
    uint8_t m_maxNumFramesInFlight = 0;

    RenderingSurface createRenderingSurface(vk::Instance instance, StarWindow &window) const;

    StarWindow createWindow() const;

    std::vector<const char *> getRequiredDisplayExtensions() const;

    void getNumSupportedSwapchainImages(core::device::StarDevice &device, uint8_t &min, uint8_t &max) const;

    service::Service createSwapchainService();
};
} // namespace star::windowing