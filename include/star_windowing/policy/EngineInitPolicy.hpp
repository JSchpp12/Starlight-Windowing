#pragma once

#include <star_windowing/WindowingContext.hpp>
#include <starlight/core/Exceptions.hpp>
#include <starlight/core/RenderingInstance.hpp>
#include <starlight/core/device/StarDevice.hpp>
#include <starlight/core/device/IStartupDeviceRequirementsProvider.hpp>
#include <starlight/core/renderer/RendererBase.hpp>
#include <starlight/enums/Enums.hpp>
#include <starlight/service/Service.hpp>

#include <star_common/FrameTracker.hpp>
#include <star_common/Renderer.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <utility>

namespace star::windowing
{
class EngineInitPolicy
{
  public:
    using LoadAdditionalServices = std::function<std::vector<service::Service>()>;
    using StartupDeviceRequirementsProvider = core::device::IStartupDeviceRequirementsProvider;

    explicit EngineInitPolicy(WindowingContext &winContext) : m_winContext(winContext) {};
    EngineInitPolicy(WindowingContext &winContext, LoadAdditionalServices addServiceLoader)
        : m_winContext(winContext), m_addServiceLoader(std::move(addServiceLoader))
    {
    }
    EngineInitPolicy(WindowingContext &winContext, int overrideRenderingDeviceIndex)
        : m_winContext(winContext), m_overrideRenderingDeviceIndex(std::move(overrideRenderingDeviceIndex))
    {
    }
    explicit EngineInitPolicy(WindowingContext &winContext,
                              std::unique_ptr<StartupDeviceRequirementsProvider> startupDeviceRequirements)
        : m_winContext(winContext), m_startupDeviceRequirements(std::move(startupDeviceRequirements))
    {
    }
    EngineInitPolicy(WindowingContext &winContext, int overrideRenderingDeviceIndex,
                     std::unique_ptr<StartupDeviceRequirementsProvider> startupDeviceRequirements)
        : m_winContext(winContext), m_overrideRenderingDeviceIndex(std::move(overrideRenderingDeviceIndex)),
          m_startupDeviceRequirements(std::move(startupDeviceRequirements))
    {
    }
    EngineInitPolicy(WindowingContext &winContext, LoadAdditionalServices addServiceLoader,
                     int overrideRenderingDeviceIndex)
        : m_winContext(winContext), m_addServiceLoader(std::move(addServiceLoader)),
          m_overrideRenderingDeviceIndex(std::move(overrideRenderingDeviceIndex))
    {
    }

    core::RenderingInstance createRenderingInstance(std::string appName);

    core::device::StarDevice createNewDevice(
        core::RenderingInstance &renderingInstance,
        std::set<Rendering_Device_Features> &engineRenderingDeviceFeatures);
    vk::Extent2D getEngineRenderingResolution();

    common::FrameTracker::Setup getFrameInFlightTrackingSetup(core::device::StarDevice &device);

    void cleanup(core::RenderingInstance &instance);

    void init(uint8_t requestedNumFramesInFlight);

    std::vector<service::Service> getAdditionalDeviceServices();

  protected:
    /// Consume the startup requirements provider. The provider is released as part of this call.
    core::device::DeviceRequirements consumeStartupDeviceRequirements()
    {
        if (m_startupRequirementsConsumed)
        {
            STAR_THROW("Startup device requirements have already been consumed");
        }

        m_startupRequirementsConsumed = true;
        auto provider = std::move(m_startupDeviceRequirements);
        return provider ? provider->getRequirements() : core::device::DeviceRequirements{};
    }

  private:
    WindowingContext &m_winContext;
    LoadAdditionalServices m_addServiceLoader;
    std::optional<int> m_overrideRenderingDeviceIndex{std::nullopt};
    std::unique_ptr<StartupDeviceRequirementsProvider> m_startupDeviceRequirements;
    bool m_startupRequirementsConsumed{false};
    uint8_t m_maxNumFramesInFlight = 0;

    RenderingSurface createRenderingSurface(vk::Instance instance, StarWindow &window) const;

    StarWindow createWindow() const;

    std::vector<const char *> getRequiredDisplayExtensions() const;

    void getNumSupportedSwapchainImages(core::device::StarDevice &device, uint8_t &min, uint8_t &max) const;

    service::Service createSwapchainService();
};
} // namespace star::windowing
