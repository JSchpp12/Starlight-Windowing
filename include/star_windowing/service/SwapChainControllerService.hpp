#pragma once
#include <starlight/core/WorkerPool.hpp>
#include <starlight/policy/command/ListenForGetFrameTracker.hpp>
#include <starlight/policy/event/ListenForFrameComplete.hpp>
#include <starlight/policy/event/ListenForPrepForNextFrame.hpp>
#include <starlight/service/InitParameters.hpp>

#include <star_windowing/Swapchain.hpp>
#include <star_windowing/WindowingContext.hpp>
#include <star_windowing/policy/ListenForRequestForSwapChainPolicy.hpp>

namespace star::windowing
{
class SwapChainControllerService : private ListenForRequestForSwapChainPolicy<SwapChainControllerService>
{
  public:
    SwapChainControllerService()
        : ListenForRequestForSwapChainPolicy<SwapChainControllerService>{*this}, m_frameTracker{}, m_listenerHandle{},
          m_listenGetFrameTracker{*this}, m_listenFrameComplete{*this}, m_listenPrepNextFrame{*this} {};

    explicit SwapChainControllerService(WindowingContext &winContext)
        : ListenForRequestForSwapChainPolicy<SwapChainControllerService>{*this}, m_frameTracker{}, m_swapChain{},
          m_listenerHandle{}, m_listenGetFrameTracker{*this}, m_listenFrameComplete{*this},
          m_listenPrepNextFrame{*this}, m_winContext{&winContext}, m_eventBus{nullptr} {};

    SwapChainControllerService(const SwapChainControllerService &) = delete;
    SwapChainControllerService &operator=(const SwapChainControllerService &) = delete;
    SwapChainControllerService(SwapChainControllerService &&);
    SwapChainControllerService &operator=(SwapChainControllerService &&);
    ~SwapChainControllerService() = default;

    void init();

    void negotiateWorkers(core::WorkerPool &pool, job::TaskManager &tm)
    {
        (void)pool;
        (void)tm;
    }

    void onGetFrameTracker(star::frames::GetFrameTracker &cmd);

    void onFrameComplete(const star::event::FrameComplete &event, bool &keepAlive);

    void setInitParameters(star::service::InitParameters &prams);

    void onStartOfNextFrame(const star::event::StartOfNextFrame &event, bool &keepAlive);

    void shutdown();

    void cleanup(common::EventBus &eventBus, core::CommandBus &cmdBus);

    vk::SwapchainKHR getSwapChain() const
    {
        return m_swapChain.getVulkanSwapchain();
    }

  private:
    star::common::FrameTracker m_frameTracker;
    SwapChain m_swapChain;
    Handle m_listenerHandle;
    star::policy::command::ListenForGetFrameTracker<SwapChainControllerService> m_listenGetFrameTracker;
    star::policy::event::ListenForFrameComplete<SwapChainControllerService> m_listenFrameComplete;
    star::policy::event::ListenForStartOfNextFrame<SwapChainControllerService> m_listenPrepNextFrame;
    WindowingContext *m_winContext = nullptr;
    common::EventBus *m_eventBus = nullptr;
    star::core::CommandBus *m_cmdBus = nullptr;
    core::device::StarDevice *m_device = nullptr;

    void initListeners(common::EventBus &eventBus);

    void initListeners(star::core::CommandBus &cmdBus);

    void cleanupListeners(common::EventBus &eventBus);

    void cleanupListeners(star::core::CommandBus &cmdBus);

    uint8_t incrementNextFrameInFlight(const common::FrameTracker &frameTracker) const noexcept;

    uint8_t incrementNextSwapChainImage(const common::FrameTracker &frameTracker);
};
} // namespace star::windowing