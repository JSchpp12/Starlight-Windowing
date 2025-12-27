#pragma once

#include "star_windowing/event/RequestSwapChainFromService.hpp"
#include <star_common/EventBus.hpp>
#include <star_common/Handle.hpp>
#include <star_common/HandleTypeRegistry.hpp>
#include <vulkan/vulkan.hpp>

#include <concepts>
template <typename T>
concept ListenerLike = requires(T listener) {
    { listener.getSwapChain() } -> std::same_as<vk::SwapchainKHR>;
};

namespace star::windowing
{
template <typename TListener> class ListenForRequestForSwapChainPolicy
{
  public:
    ListenForRequestForSwapChainPolicy(TListener &me)
        requires ListenerLike<TListener>
        : m_listenerHandle{}, m_me{me}
    {
    }

    void init(common::EventBus &eventBus)
    {
        registerListener(eventBus);
    }

    void cleanup(common::EventBus &eventBus)
    {
        if (m_listenerHandle.isInitialized())
        {
            eventBus.unsubscribe(m_listenerHandle);
        }
    }

  private:
    Handle m_listenerHandle;
    TListener &m_me;

    void registerListener(common::EventBus &eventBus)
    {
        eventBus.subscribe(
            common::HandleTypeRegistry::instance().registerType(event::GetRequestSwapChainFromServiceEventTypeName),
            common::SubscriberCallbackInfo{
                std::bind(&ListenForRequestForSwapChainPolicy<TListener>::eventCallback, this, std::placeholders::_1,
                          std::placeholders::_2),
                std::bind(&ListenForRequestForSwapChainPolicy<TListener>::getHandleForEventBus, this),
                std::bind(&ListenForRequestForSwapChainPolicy<TListener>::notificationFromEventBusOfDeletion, this,
                          std::placeholders::_1)});
    }

    void eventCallback(const common::IEvent &e, bool &keepAlive)
    {
        const auto &event = static_cast<const event::RequestSwapChainFromService &>(e);
        auto swapchain = m_me.getSwapChain();
        *event.getResultSwapChain() = swapchain;

        keepAlive = true;
    }

    Handle *getHandleForEventBus()
    {
        return &m_listenerHandle;
    }

    void notificationFromEventBusOfDeletion(const Handle &noLongerNeededHandle)
    {
        if (m_listenerHandle == noLongerNeededHandle)
        {
            m_listenerHandle = Handle();
        }
    }
};
} // namespace star::windowing