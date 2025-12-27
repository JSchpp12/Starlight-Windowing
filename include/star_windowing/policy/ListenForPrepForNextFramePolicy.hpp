#pragma once

#include <star_common/EventBus.hpp>
#include <star_common/Handle.hpp>
#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/event/PrepForNextFrame.hpp>

namespace star::windowing
{
template <typename T> class ListenForPrepForNextFramePolicy
{
  public:
    ListenForPrepForNextFramePolicy(T &me) : m_listenerHandle{}, m_me{me}
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
    T &m_me;

    void registerListener(common::EventBus &eventBus)
    {
        eventBus.subscribe(common::HandleTypeRegistry::instance().registerType(event::GetPrepForNextFrameEventTypeName),
                           common::SubscriberCallbackInfo{
                               std::bind(&ListenForPrepForNextFramePolicy<T>::eventCallback, this,
                                         std::placeholders::_1, std::placeholders::_2),
                               std::bind(&ListenForPrepForNextFramePolicy<T>::getHandleForEventBus, this),
                               std::bind(&ListenForPrepForNextFramePolicy<T>::notificationFromEventBusOfDeletion, this,
                                         std::placeholders::_1)});
    }

    void eventCallback(const common::IEvent &e, bool &keepAlive)
    {
        const auto &event = static_cast<const event::PrepForNextFrame &>(e);

        m_me.prepForNextFrame(event.getFrameTracker());

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
            m_listenerHandle = Handle{};
        }
    }
};
} // namespace star::windowing