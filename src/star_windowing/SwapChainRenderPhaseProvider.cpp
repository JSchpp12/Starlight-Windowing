#include "star_windowing/SwapChainRenderPhaseProvider.hpp"

#include "star_windowing/SwapChainRenderPhase.hpp"
#include "star_windowing/WindowingContext.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/core/Exceptions.hpp>
#include <starlight/core/device/managers/Semaphore.hpp>
#include <starlight/core/device/system/event/ManagerRequest.hpp>
#include <starlight/core/helper/command_buffer/CommandBufferHelpers.hpp>
#include <starlight/core/helper/queue/QueueHelpers.hpp>
#include <starlight/core/renderer/DefaultRenderPhase.hpp>
#include <starlight/core/renderer/RenderingContext.hpp>

#include <cassert>
#include <vector>

namespace star::windowing
{
namespace
{
std::vector<star::Handle> CreateSemaphores(star::core::device::DeviceContext &context, const size_t &numToCreate,
                                           bool isTimeline)
{
    auto semaphores = std::vector<star::Handle>(numToCreate);

    for (size_t i{0}; i < numToCreate; i++)
    {
        auto request = isTimeline ? star::core::device::manager::SemaphoreRequest(uint64_t{0})
                                  : star::core::device::manager::SemaphoreRequest();

        context.getEventBus().emit(star::core::device::system::event::ManagerRequest(
            star::common::HandleTypeRegistry::instance()
                .getType(star::core::device::manager::GetSemaphoreEventTypeName)
                .value(),
            std::move(request), semaphores[i]));

        if (!semaphores[i].isInitialized())
        {
            STAR_THROW("failed to create semaphores for a frame");
        }
    }

    return semaphores;
}
} // namespace

SwapChainRenderPhaseProvider::SwapChainRenderPhaseProvider(WindowingContext *winContext, vk::SwapchainKHR swapchain,
                                                           star::core::device::DeviceContext &context,
                                                           std::vector<std::shared_ptr<star::StarObject>> objects,
                                                           std::shared_ptr<std::vector<star::Light>> lights,
                                                           std::shared_ptr<star::StarCamera> camera)
    : star::core::renderer::DefaultRenderPhaseProvider(context, std::move(lights), camera, std::move(objects))
{
    m_winContext = winContext;
    m_swapChain = swapchain;
    // presentation must wait on all prior commands before presenting
    m_config.waitStage = vk::PipelineStageFlagBits::eAllCommands;
}

SwapChainRenderPhaseProvider::SwapChainRenderPhaseProvider(WindowingContext *winContext, vk::SwapchainKHR swapchain,
                                                           star::core::device::DeviceContext &context,
                                                           std::vector<std::shared_ptr<star::StarObject>> objects,
                                                           std::shared_ptr<star::core::renderer::FrameData> frameData)
    : star::core::renderer::DefaultRenderPhaseProvider(context, std::move(objects), std::move(frameData))
{
    m_winContext = winContext;
    m_swapChain = swapchain;
    // presentation must wait on all prior commands before presenting
    m_config.waitStage = vk::PipelineStageFlagBits::eAllCommands;
}

static vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) noexcept
{
    for (const auto &availableFormat : availableFormats)
    {
        // check if a format allows 8 bits for R,G,B, and alpha channel
        // use SRGB color space

        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }

    // if nothing matches what we are looking for, just take what is available
    return availableFormats[0];
}

static vk::Format GetColorAttachmentFormat(star::core::device::DeviceContext &device,
                                           const star::windowing::WindowingContext *winContext) noexcept
{
    core::SwapChainSupportDetails swapChainSupport =
        device.getDevice().getSwapchainSupport(winContext->surface.getVulkanSurface());

    vk::SurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    return surfaceFormat.format;
}

star::core::renderer::RenderTargets star::windowing::SwapChainRenderPhaseProvider::createRenderTargets(
    star::core::device::DeviceContext &device, star::core::renderer::RenderingContext &renderingContext)
{
    std::vector<StarTextures::Texture> newRenderToImages = std::vector<StarTextures::Texture>();
    const vk::Extent2D winResolution = m_winContext->window.getWindowFramebufferSize();
    const vk::Extent3D resolution =
        vk::Extent3D().setWidth(winResolution.width).setHeight(winResolution.height).setDepth(1);

    vk::Format format = GetColorAttachmentFormat(device, m_winContext);
    // Only create views for presentable images here. A swapchain image cannot be recorded or transitioned until it has
    // been acquired; the render phase transitions the acquired image each frame after acquisition.
    for (vk::Image &image : device.getDevice().getVulkanDevice().getSwapchainImagesKHR(m_swapChain))
    {
        auto builder =
            star::StarTextures::Texture::Builder(device.getDevice(), image)
                .setSizeInfo(star::StarTextures::Texture::CalculateSize(format, resolution, 1, vk::ImageType::e2D, 1),
                             resolution)
                .setBaseFormat(format)
                .addViewInfo(vk::ImageViewCreateInfo()
                                 .setViewType(vk::ImageViewType::e2D)
                                 .setFormat(format)
                                 .setSubresourceRange(vk::ImageSubresourceRange()
                                                          .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                                          .setBaseArrayLayer(0)
                                                          .setLayerCount(1)
                                                          .setBaseMipLevel(0)
                                                          .setLevelCount(1)));
        newRenderToImages.emplace_back(builder.build());
    }

    auto depthTextures = star::core::renderer::RenderTargets::createDefaultDepthAttachments(
        device, newRenderToImages.size(), static_cast<int>(resolution.width), static_cast<int>(resolution.height));
    if (depthTextures.empty())
        STAR_THROW("Failed to create depth attachments for presentation");
    const auto depthFormat = depthTextures.front().getBaseFormat();

    std::vector<vk::ImageMemoryBarrier2> depthBarriers{depthTextures.size()};
    for (size_t i = 0; i < depthTextures.size(); i++)
    {
        depthBarriers[i] = vk::ImageMemoryBarrier2()
                               .setOldLayout(vk::ImageLayout::eUndefined)
                               .setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                               .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                               .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                               .setImage(depthTextures[i].getVulkanImage())
                               .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                               .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
                               .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                                                 vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
                               .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests)
                               .setSubresourceRange(vk::ImageSubresourceRange()
                                                        .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                                                        .setBaseMipLevel(0)
                                                        .setLevelCount(1)
                                                        .setBaseArrayLayer(0)
                                                        .setLayerCount(1));
    }

    core::helper::command_buffer::SingleTimeCommands(device, star::Queue_Type::Tpresent, [&](vk::CommandBuffer cmd) {
        cmd.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(depthBarriers));
    });

    auto colorHandles =
        star::core::renderer::RenderTargets::registerTextures(device, renderingContext, std::move(newRenderToImages));
    auto depthHandles =
        star::core::renderer::RenderTargets::registerTextures(device, renderingContext, std::move(depthTextures));

    return star::core::renderer::RenderTargets{std::move(colorHandles), format, std::move(depthHandles), depthFormat};
}

std::unique_ptr<star::core::renderer::RenderPhase> SwapChainRenderPhaseProvider::build(
    star::core::device::DeviceContext &context, star::core::renderer::RenderPhaseRegistry & /*phases*/)
{
    auto phase = std::make_unique<SwapChainRenderPhase>();

    const size_t numSwapChainImages = context.getDevice().getVulkanDevice().getSwapchainImagesKHR(m_swapChain).size();

    const auto binaryDoneSemaphores = CreateSemaphores(context, numSwapChainImages, false);
    phase->m_timelineSemaphores =
        CreateSemaphores(context, context.frameTracker().getSetup().getNumFramesInFlight(), true);
    phase->rawBinaryRenderDoneSemaphores.resize(binaryDoneSemaphores.size());
    for (size_t i = 0; i < binaryDoneSemaphores.size(); i++)
    {
        phase->rawBinaryRenderDoneSemaphores[i] =
            context.getGraphicsManagers().semaphoreManager->get(binaryDoneSemaphores[i])->semaphore;
    }

    phase->m_presentationQueueToUse = star::core::helper::GetEngineDefaultQueue(
        context.getEventBus(), context.getGraphicsManagers().queueManager, star::Queue_Type::Tpresent);
    if (phase->m_presentationQueueToUse == nullptr)
        STAR_THROW("Failed to acquire a presentation queue from engine");

    phase->m_swapChain = m_swapChain;
    phase->m_winContext = m_winContext;
    phase->device = &context;
    phase->m_presentationCommands.init(&phase->m_presentationSharedDeps, &phase->m_swapChain,
                                       phase->m_presentationQueueToUse);
    phase->m_presentationCommands.prepRender(context);
    phase->m_cmdBus = &context.getCmdBus();

    star::core::renderer::DefaultRenderPhase::Builder(context)
        .setObjects(std::move(m_objects))
        .setFrameData(m_frameData)
        .setOwnsFrameData(m_createdFrameData)
        .setConfig(m_config)
        .setRenderTargetsFactory([&context, this](star::core::renderer::RenderingContext &renderingContext) {
            return this->createRenderTargets(context, renderingContext);
        })
        .buildInto(*phase);

    return phase;
}
} // namespace star::windowing
