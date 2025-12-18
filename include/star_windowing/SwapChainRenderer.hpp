#pragma once

#include "star_windowing/PresentationCommands.hpp"
#include "star_windowing/StarWindow.hpp"
#include "star_windowing/WindowingContext.hpp"

#include <memory>
#include <starlight/core/renderer/DefaultRenderer.hpp>
#include <vulkan/vulkan.hpp>

namespace star::windowing
{
class SwapChainRenderer : public star::core::renderer::DefaultRenderer
{
  public:
    SwapChainRenderer() = default;
    SwapChainRenderer(WindowingContext *winContext, core::device::DeviceContext &context,
                      const uint8_t &numFramesInFlight, std::vector<std::shared_ptr<StarObject>> objects,
                      std::shared_ptr<std::vector<Light>> lights, std::shared_ptr<StarCamera> camera);

    SwapChainRenderer(WindowingContext *winContext, core::device::DeviceContext &context,
                      const uint8_t &numFramesInFlight, std::vector<std::shared_ptr<StarObject>> objects,
                      std::shared_ptr<ManagerController::RenderResource::Buffer> lightData,
                      std::shared_ptr<ManagerController::RenderResource::Buffer> lightListData,
                      std::shared_ptr<ManagerController::RenderResource::Buffer> cameraData);

    virtual ~SwapChainRenderer() = default;
    SwapChainRenderer(const SwapChainRenderer &) = delete;
    SwapChainRenderer &operator=(const SwapChainRenderer &) = delete;
    SwapChainRenderer(SwapChainRenderer &&);
    SwapChainRenderer &operator=(SwapChainRenderer &&);

    virtual void prepRender(common::IDeviceContext &device, const uint8_t &numFramesInFlight) override;

    virtual void cleanupRender(common::IDeviceContext &device) override;

    virtual void frameUpdate(common::IDeviceContext &context, const uint8_t &frameInFlightIndex) override;

    void submitPresentation(const int &frameIndexToBeDrawn, const vk::Semaphore *mainGraphicsDoneSemaphore);
    
    std::vector<Handle> &getDoneSemaphores()
    {
        return imageAvailableSemaphores;
    }
    std::vector<Handle> getDoneSemaphores() const
    {
        return imageAvailableSemaphores;
    }

  protected:
    WindowingContext *m_winContext = nullptr;
    core::device::DeviceContext *device = nullptr;
    vk::SwapchainKHR swapChain;
    PresentationCommands::RecordDependencies m_presentationSharedDeps; 
    PresentationCommands m_presentationCommands;

    // tracker for which frame is being processed of the available permitted frames
    uint8_t previousFrame = 0, numFramesInFlight = 0;

    bool frameBufferResized =
        false; // explicit declaration of resize, used if driver does not trigger VK_ERROR_OUT_OF_DATE

    // Sync obj storage
    std::vector<Handle> inFlightFences, imagesInFlight;
    std::vector<Handle> imageAvailableSemaphores, imageAcquireSemaphores;
    std::vector<Handle> graphicsDoneSemaphoresExternalUse; /// These are guaranteed to match with the current frame in
                                                           /// flight for other command buffers to reference

    virtual star::core::device::manager::ManagerCommandBuffer::Request getCommandBufferRequest() override;

    // std::unique_ptr<ScreenshotBuffer> screenshotCommandBuffer = nullptr;
    std::unique_ptr<std::string> screenshotPath = nullptr;

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) const;

    bool doesSwapChainSupportTransferOperations(core::device::DeviceContext &context) const;

    vk::Format getColorAttachmentFormat(core::device::DeviceContext &context) const override;

    // Look through givent present modes and pick the "best" one
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);

    void prepareForSubmission(const int &frameIndexToBeDrawn);

    vk::Semaphore submitBuffer(StarCommandBuffer &buffer, const int &frameIndexToBeDrawn,
                               std::vector<vk::Semaphore> *previousCommandBufferSemaphores,
                               std::vector<vk::Semaphore> dataSemaphores,
                               std::vector<vk::PipelineStageFlags> dataWaitPoints,
                               std::vector<std::optional<uint64_t>> previousSignaledValues);

    virtual std::vector<StarTextures::Texture> createRenderToImages(core::device::DeviceContext &device,
                                                                    const uint8_t &numFramesInFlight) override;

    virtual vk::RenderingAttachmentInfo prepareDynamicRenderingInfoColorAttachment(
        const uint8_t &frameInFlightIndex) override;

    virtual void recordCommandBuffer(vk::CommandBuffer &buffer, const uint8_t &frameIndexToBeDrawn,
                                     const uint64_t &frameIndex) override;

    /// <summary>
    /// If the swapchain is no longer compatible, it must be recreated.
    /// </summary>
    virtual void recreateSwapChain();

    void cleanupSwapChain(core::device::DeviceContext &context);

    /// <summary>
    /// Create semaphores that are going to be used to sync rendering and presentation queues
    /// </summary>
    static std::vector<Handle> CreateSemaphores(core::device::DeviceContext &context, const uint8_t &numToCreate,
                                                const bool &isTimeline);

    /// <summary>
    /// Fences are needed for CPU-GPU sync. Creates these required objects
    /// </summary>
    void createFences(core::device::DeviceContext &context);

    /// <summary>
    /// Create tracking information in order to link fences with the swap chain images using
    /// </summary>
    void createFenceImageTracking();

    /// <summary>
    /// Create a swap chain that will be used in rendering images
    /// </summary>
    void createSwapChain(core::device::DeviceContext &context);

    void prepareRenderingContext(core::device::DeviceContext &context);

    void addSemaphoresToRenderingContext(core::device::DeviceContext &context);

    void addFencesToRenderingContext(core::device::DeviceContext &context);

    std::vector<vk::ImageMemoryBarrier2> getImageBarriersForThisFrame();
};
} // namespace star::windowing