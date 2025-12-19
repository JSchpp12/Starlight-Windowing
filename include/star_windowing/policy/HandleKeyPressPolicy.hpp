#pragma once

#include <star_windowing/event/KeyPress.hpp>

#include <star_common/Handle.hpp>
#include <star_common/IEvent.hpp>
#include <star_common/EventBus.hpp>

namespace star::windowing{
template <typename T> class HandleKeyPressPolicy
{
  public:
    HandleKeyPressPolicy(T &me) : me(me)
    {
    }
    virtual ~HandleKeyPressPolicy() = default;

    void init(common::EventBus &eventBus)
    {
        registerListener(eventBus); 
    }

  private:
    T &me;
    Handle m_callbackRegistration;

    void registerListener(common::EventBus &eventBus)
    {
        eventBus.subscribe(
            common::HandleTypeRegistry::instance().registerType(event::GetKeyPressedEventTypeName),
            common::SubscriberCallbackInfo{std::bind(&HandleKeyPressPolicy<T>::eventCallback, this,
                                                     std::placeholders::_1, std::placeholders::_2),
                                           std::bind(&HandleKeyPressPolicy<T>::getHandleForEventBus, this),
                                           std::bind(&HandleKeyPressPolicy<T>::notificationFromEventBusOfDeletion,
                                                     this, std::placeholders::_1)});
    }

    void eventCallback(const common::IEvent &e, bool &keepAlive)
    {
        const auto &event = static_cast<const event::KeyPress &>(e);

        me.onKeyPress(event.getKey(), event.getScancode(), event.getMods());

        keepAlive = true;
    }

    Handle *getHandleForEventBus()
    {
        return &m_callbackRegistration;
    }

    void notificationFromEventBusOfDeletion(const Handle &noLongerNeededHandle)
    {
        if (m_callbackRegistration == noLongerNeededHandle)
        {
            m_callbackRegistration = Handle();
        }
    }
};
}