#include "star_windowing/PresentationCommands.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/event/RenderReadyForFinalization.hpp>

namespace star::windowing
{
void PresentationCommands::init(RecordDependencies *recordDeps, vk::SwapchainKHR *swapchain)
{
    m_recordDeps = recordDeps;
    m_swapchain = swapchain;
}

void PresentationCommands::prepRender(core::device::DeviceContext &context)
{
    m_deviceEventBus = &context.getEventBus();

    registerListener(context.getEventBus());
}

// Handle PresentationCommands::registerWithManager(core::device::DeviceContext &context)
// {
//     return context.getManagerCommandBuffer().submit(
//         {.recordBufferCallback = std::bind(&PresentationCommands::recordCommandBuffer, this, std::placeholders::_1,
//                                            std::placeholders::_2, std::placeholders::_3),
//          .order = Command_Buffer_Order::presentation,
//          .orderIndex = Command_Buffer_Order_Index::first,
//          .type = Queue_Type::Tpresent,
//          .waitStage = {},
//          .willBeSubmittedEachFrame = true,
//          .recordOnce = false,
//          .beforeBufferSubmissionCallback =
//              std::bind(&PresentationCommands::beforeRecordCallback, this, std::placeholders::_1)},
//         context.getCurrentFrameIndex());
// }

void PresentationCommands::registerListener(common::EventBus &eventBus)
{
    if (!common::HandleTypeRegistry::instance().contains(event::GetRenderReadyForFinalizationTypeName))
    {
        common::HandleTypeRegistry::instance().registerType(event::GetRenderReadyForFinalizationTypeName);
    }

    eventBus.subscribe(
        common::HandleTypeRegistry::instance().getTypeGuaranteedExist(event::GetRenderReadyForFinalizationTypeName),
        {[this](const common::IEvent &e, bool &keepAlive) { this->eventCallback(e, keepAlive); },
         [this]() -> Handle * { return this->getHandleForUpdate(); },
         [this](const Handle &noLongerNeededHandle) {
             this->notificationFromEventBusHandleDelete(noLongerNeededHandle);
         }});
}

void PresentationCommands::eventCallback(const star::common::IEvent &e, bool &keepAlive)
{
    const auto &event = static_cast<const event::RenderReadyForFinalization &>(e);
    submitPresentation(event.getDevice());

    keepAlive = true;
}

void PresentationCommands::submitPresentation(core::device::StarDevice &device)
{
    assert(m_recordDeps != nullptr);

    auto presentInfo = vk::PresentInfoKHR()
                           .setWaitSemaphoreCount(1)
                           .setPWaitSemaphores(&m_recordDeps->mainGraphicsDoneSemaphore)
                           .setPImageIndices(&m_recordDeps->acquiredSwapChainImageIndex)
                           .setSwapchainCount(1)
                           .setPSwapchains(m_swapchain);

    const auto presentResult = device.getDefaultQueue(star::Queue_Type::Tpresent).getVulkanQueue().presentKHR(presentInfo);

    if (presentResult != vk::Result::eSuccess){
        throw std::runtime_error("failed"); 
    }
}

void PresentationCommands::notificationFromEventBusHandleDelete(const Handle &noLongerNeededSubscriberHandle)
{
    if (m_listener == noLongerNeededSubscriberHandle)
    {
        m_listener = Handle();
    }
}
} // namespace star::windowing