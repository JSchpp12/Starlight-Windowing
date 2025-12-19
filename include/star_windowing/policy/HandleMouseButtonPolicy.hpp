#pragma once

#include <star_windowing/event/MouseButton.hpp>

namespace star::windowing
{
template <typename T> class HandleMouseButtonPolicy
{
  public:
    HandleMouseButtonPolicy(T &me) : me(me)
    {
    }
    virtual ~HandleMouseButtonPolicy() = default;

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
            common::HandleTypeRegistry::instance().registerType(event::GetMouseButtonEventTypeName),
            common::SubscriberCallbackInfo{std::bind(&HandleMouseButtonPolicy<T>::eventCallback, this,
                                                     std::placeholders::_1, std::placeholders::_2),
                                           std::bind(&HandleMouseButtonPolicy<T>::getHandleForEventBus, this),
                                           std::bind(&HandleMouseButtonPolicy<T>::notificationFromEventBusOfDeletion,
                                                     this, std::placeholders::_1)});
    }

    void eventCallback(const common::IEvent &e, bool &keepAlive)
    {
        const auto &event = static_cast<const event::MouseButton &>(e);

        me.onMouseButtonAction(event.getButton(), event.getAction(), event.getMods());

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