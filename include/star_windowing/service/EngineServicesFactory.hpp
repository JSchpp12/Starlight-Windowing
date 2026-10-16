#pragma once

#include <star_windowing/WindowingContext.hpp>

#include <starlight/service/EngineServices.hpp>
#include <starlight/service/Service.hpp>

namespace star::windowing
{

/// @brief Builds the services required by presenting (windowed) configurations. The services are appended to the given
/// engine services storage.
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
