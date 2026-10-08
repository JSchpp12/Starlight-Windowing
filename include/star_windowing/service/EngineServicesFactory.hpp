#pragma once

#include <star_windowing/WindowingContext.hpp>

#include <starlight/service/EngineServices.hpp>
#include <starlight/service/Service.hpp>

namespace star::windowing
{
/// Builds the services required by presenting (windowed) configurations.
///
/// The services are appended to the given engine services storage, which is
/// expected to already contain the core engine services. This keeps a single
/// source of truth for the core set while allowing windowing to own what
/// presentation requires.
class EngineServicesFactory
{
  public:
    /// Append the services required by presenting configurations to the given
    /// engine services storage.
    static void addDefaultServices(service::EngineServices &engineServices, WindowingContext &winContext);

    static service::Service createSwapChainControllerService(WindowingContext &winContext);
    static service::Service createGuiControllerService(WindowingContext &winContext);
};
} // namespace star::windowing
