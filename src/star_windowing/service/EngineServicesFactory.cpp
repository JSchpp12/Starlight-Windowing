#include "star_windowing/service/EngineServicesFactory.hpp"

#include "star_windowing/service/GuiControllerService.hpp"
#include "star_windowing/service/SwapChainControllerService.hpp"

namespace star::windowing
{
void EngineServicesFactory::addDefaultServices(service::EngineServices &engineServices, WindowingContext &winContext)
{
    // The SwapChainControllerService is the frame tracker provider for all
    // presenting (windowed) configurations. It must be initialized FIRST among
    // user services since base engine services (e.g. CommandOrderService)
    // resolve the frame tracker during their own initialization. Inserting it
    // at the front guarantees it is always set up when it is needed.
    engineServices.addFirst(createSwapChainControllerService(winContext));

    // The gui controller owns the ImGui overlay lifecycle and the single input
    // capture decision. It is appended after the swapchain controller because it
    // must be able to request the swapchain during its own initialization.
    engineServices.add(createGuiControllerService(winContext));
}

service::Service EngineServicesFactory::createSwapChainControllerService(WindowingContext &winContext)
{
    return service::Service{SwapChainControllerService{winContext}};
}

service::Service EngineServicesFactory::createGuiControllerService(WindowingContext &winContext)
{
    return service::Service{GuiControllerService{winContext}};
}
} // namespace star::windowing