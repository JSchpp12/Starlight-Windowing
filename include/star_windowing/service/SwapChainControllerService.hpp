#pragma once

#include <star_common/HandleTypeRegistry.hpp>
#include <star_windowing/Swapchain.hpp>
#include <star_windowing/WindowingContext.hpp>
#include <starlight/policy/ListenForPrepForNextFramePolicy.hpp>
#include <star_windowing/policy/ListenForRequestForSwapChainPolicy.hpp>
#include <starlight/service/InitParameters.hpp>

namespace star::windowing
{
class SwapChainControllerService : private ListenForRequestForSwapChainPolicy<SwapChainControllerService>,
                                   private star::policy::ListenForPrepForNextFramePolicy<SwapChainControllerService>
{
  public:
    SwapChainControllerService()
        : ListenForRequestForSwapChainPolicy<SwapChainControllerService>{*this},
          star::policy::ListenForPrepForNextFramePolicy<SwapChainControllerService>{*this} {};

    explicit SwapChainControllerService(WindowingContext &winContext)
        : ListenForRequestForSwapChainPolicy<SwapChainControllerService>{*this},
          star::policy::ListenForPrepForNextFramePolicy<SwapChainControllerService>{*this}, m_swapChain{}, m_listenerHandle{},
          m_winContext{&winContext}, m_deviceEventBus{nullptr} {};

    SwapChainControllerService(const SwapChainControllerService &) = delete;
    SwapChainControllerService &operator=(const SwapChainControllerService &) = delete;
    SwapChainControllerService(SwapChainControllerService &&);
    SwapChainControllerService &operator=(SwapChainControllerService &&);
    ~SwapChainControllerService() = default;

    void init(const uint8_t &numFramesInFlight);

    void setInitParameters(star::service::InitParameters &prams);

    void shutdown();

    void cleanup(common::EventBus &eventBus);

    vk::SwapchainKHR getSwapChain()
    {
        return m_swapChain.getVulkanSwapchain();
    }
  protected:
    void prepForNextFrame(common::FrameTracker *frameTracker);

  private:
    friend class star::policy::ListenForPrepForNextFramePolicy<SwapChainControllerService>;

    SwapChain m_swapChain;
    Handle m_listenerHandle;
    WindowingContext *m_winContext = nullptr;
    common::EventBus *m_deviceEventBus = nullptr;
    common::FrameTracker *m_deviceFrameTracker = nullptr;
    core::device::StarDevice *m_device = nullptr;

    void initListeners(common::EventBus &eventBus); 

    uint8_t incrementNextFrameInFlight(const common::FrameTracker &frameTracker) const noexcept;

    uint8_t incrementNextSwapChainImage(const common::FrameTracker &frameTracker); 
};
} // namespace star::windowing