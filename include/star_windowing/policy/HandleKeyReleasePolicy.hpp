#pragma once

#include <star_windowing/event/KeyRelease.hpp>
#include <star_common/HandleTypeRegistry.hpp>
#include <star_common/EventBus.hpp>
#include <star_common/Handle.hpp>
namespace star::windowing
{
template <typename T> class HandleKeyReleasePolicy
{
  public:
    HandleKeyReleasePolicy(T &me) : me(me)
    {
    }
    virtual ~HandleKeyReleasePolicy() = default;

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
            common::HandleTypeRegistry::instance().registerType(event::GetKeyReleaseEventTypeName),
            common::SubscriberCallbackInfo{std::bind(&HandleKeyReleasePolicy<T>::eventCallback, this,
                                                     std::placeholders::_1, std::placeholders::_2),
                                           std::bind(&HandleKeyReleasePolicy<T>::getHandleForEventBus, this),
                                           std::bind(&HandleKeyReleasePolicy<T>::notificationFromEventBusOfDeletion,
                                                     this, std::placeholders::_1)});
    }

    void eventCallback(const common::IEvent &e, bool &keepAlive)
    {
        const auto &event = static_cast<const event::KeyRelease &>(e);

        me.onKeyRelease(event.getKey(), event.getScancode(), event.getMods());

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
} // namespace star::windowing