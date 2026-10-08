#include "star_windowing/ui/ImGuiRenderPhaseProvider.hpp"

#include "star_windowing/WindowingContext.hpp"
#include "star_windowing/ui/ImGuiFactory.hpp"
#include "star_windowing/ui/ImGuiRenderPhase.hpp"

#include <starlight/core/Exceptions.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/core/helper/queue/QueueHelpers.hpp>
#include <starlight/core/renderer/RenderPhaseRegistry.hpp>
#include <starlight/core/renderer/RenderingTargetInfo.hpp>
#include <starlight/wrappers/graphics/StarTextures/Texture.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace star::windowing
{
ImGuiRenderPhaseProvider::ImGuiRenderPhaseProvider(WindowingContext *winContext, vk::SwapchainKHR swapchain,
                                                   const star::core::renderer::RenderPhase *finalizationPhase,
                                                   GuiDefinition config)
    : m_winContext(winContext), m_swapChain(swapchain), m_finalizationPhase(finalizationPhase),
      m_config(std::move(config))
{
}

void ImGuiRenderPhaseProvider::addFrameCallback(std::function<void()> callback)
{
    m_frameCallbacks.emplace_back(std::move(callback));
}

void ImGuiRenderPhaseProvider::setCaptureStateCallback(std::function<void(bool, bool)> callback)
{
    m_captureStateCallback = std::move(callback);
}

std::unique_ptr<star::core::renderer::RenderPhase> ImGuiRenderPhaseProvider::build(
    star::core::device::DeviceContext &context, star::core::renderer::RenderPhaseRegistry & /*phases*/)
{
    if (m_winContext == nullptr)
        STAR_THROW("ImGuiRenderPhaseProvider requires a valid WindowingContext");

    const auto *finalizationPhase = m_finalizationPhase;
    if (finalizationPhase == nullptr)
        STAR_THROW("ImGui overlay requires the main graphics renderer to be known before it is built");

    const auto &targets = finalizationPhase->getRenderTargets();
    const auto &colorHandles = targets.colorHandles();
    if (colorHandles.empty() || !targets.colorFormat().has_value())
        STAR_THROW("ImGui overlay requires the finalization phase to expose swapchain color targets");

    auto &imageManager = context.getImageManager();
    std::vector<const star::StarTextures::Texture *> swapchainTextures;
    swapchainTextures.reserve(colorHandles.size());
    for (const auto &handle : colorHandles)
    {
        auto *image = imageManager.get(handle);
        if (image == nullptr)
            STAR_THROW("ImGui overlay failed to resolve a swapchain image handle");

        swapchainTextures.push_back(&image->texture);
    }

    // The swapchain handle is authoritative for the image count used by the
    // ImGui backend's per-image render buffers.
    const size_t swapchainImageCount =
        static_cast<size_t>(context.getDevice().getVulkanDevice().getSwapchainImagesKHR(m_swapChain).size());
    if (swapchainImageCount != swapchainTextures.size())
        STAR_THROW("ImGui overlay swapchain image count does not match the finalization phase's color targets");

    auto *presentQueue = star::core::helper::GetEngineDefaultQueue(
        context.getEventBus(), context.getGraphicsManagers().queueManager, star::Queue_Type::Tpresent);
    if (presentQueue == nullptr)
        STAR_THROW("ImGui overlay could not acquire the engine presentation queue");

    const vk::Format colorFormat = targets.colorFormat().value();
    star::core::renderer::RenderingTargetInfo renderingTargetInfo;
    renderingTargetInfo.colorAttachmentFormats = {colorFormat};

    return ImGuiRenderPhase::Builder(context)
        .setWindowingContext(m_winContext)
        .setRenderingTargetInfo(std::move(renderingTargetInfo))
        .setSwapchainTextures(std::move(swapchainTextures))
        .setPresentQueue(presentQueue)
        .setIniFilename(ResolveIniFile(m_config.iniFilename))
        .setEnabled(m_config.enabled)
        .setStyleScale(m_config.styleScale)
        .setFrameCallbacks(m_frameCallbacks)
        .setCaptureStateCallback(m_captureStateCallback)
        .buildUnique();
}
} // namespace star::windowing
