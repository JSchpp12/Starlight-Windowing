#pragma once

#include <star_common/IEvent.hpp>
#include <vulkan/vulkan.hpp>

namespace star::windowing::event
{
constexpr std::string_view GetRequestSwapChainFromServiceEventTypeName = "star::windowing::RequestSwapChain";

class RequestSwapChainFromService : public common::IEvent
{
  public:
    RequestSwapChainFromService(vk::SwapchainKHR &resultSwapchain);

    virtual ~RequestSwapChainFromService() = default;

    vk::SwapchainKHR *getResultSwapChain() const
    {
        return m_resultSwapchain;
    }

  private:
    mutable vk::SwapchainKHR *m_resultSwapchain = nullptr;
};
} // namespace star::windowing::event