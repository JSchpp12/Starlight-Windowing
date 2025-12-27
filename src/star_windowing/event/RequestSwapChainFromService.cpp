#include "star_windowing/event/RequestSwapChainFromService.hpp"

#include <star_common/HandleTypeRegistry.hpp>

namespace star::windowing::event
{
RequestSwapChainFromService::RequestSwapChainFromService(vk::SwapchainKHR &resultSwapchain)
    : common::IEvent(common::HandleTypeRegistry::instance().registerType(GetRequestSwapChainFromServiceEventTypeName)),
      m_resultSwapchain(&resultSwapchain)
{
}
} // namespace star::windowing::event
