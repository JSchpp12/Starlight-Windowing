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

static vk::Format selectFormat(core::device::DeviceContext &device, const std::vector<vk::Format> &candidates,
                               vk::FormatFeatureFlags features)
{
    vk::Format selected = vk::Format();
    if (!device.getDevice().findSupportedFormat(candidates, vk::ImageTiling::eOptimal, features, selected))
        STAR_THROW("RenderTargets: failed to find a supported format for the requested features");
    return selected;
}

static std::vector<Handle> RegisterTextures(core::device::DeviceContext &context,
                                            core::renderer::RenderingContext &renderingContext,
                                            std::vector<StarTextures::Texture> textures)
{
    std::vector<Handle> handles;
    handles.resize(textures.size());

    for (size_t i = 0; i < textures.size(); i++)
    {
        void *r = nullptr;
        context.getEventBus().emit(core::device::system::event::ManagerRequest{
            star::common::HandleTypeRegistry::instance().getTypeGuaranteedExist(
                core::device::manager::GetImageEventTypeName),
            core::device::manager::ImageRequest{std::move(textures[i])}, handles[i], &r});

        assert(r != nullptr);
        auto *result = static_cast<core::device::manager::ImageRecord *>(r);
        renderingContext.recordDependentImage.manualInsert(handles[i], &result->texture);
    }

    return handles;
}

star::core::renderer::RenderTargets star::windowing::SwapChainRenderPhaseProvider::createRenderTargets(
    star::core::device::DeviceContext &device, star::core::renderer::RenderingContext &renderingContext)
{
    std::vector<StarTextures::Texture> newRenderToImages = std::vector<StarTextures::Texture>();
    const vk::Extent2D winResolution = m_winContext->window.getWindowFramebufferSize();
    const vk::Extent3D resolution =
        vk::Extent3D().setWidth(winResolution.width).setHeight(winResolution.height).setDepth(1);

    vk::Format format = GetColorAttachmentFormat(device, m_winContext);
    // get images in the newly created swapchain
    std::vector<vk::ImageMemoryBarrier2> swapBarriers;
    for (vk::Image &image : device.getDevice().getVulkanDevice().getSwapchainImagesKHR(m_swapChain))
    {
        auto builder =
            star::StarTextures::Texture::Builder(device.getDevice().getVulkanDevice(), image)
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
        newRenderToImages.back().setImageLayout(vk::ImageLayout::ePresentSrcKHR);

        swapBarriers.push_back(vk::ImageMemoryBarrier2()
                                   .setOldLayout(vk::ImageLayout::eUndefined)
                                   .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
                                   .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                                   .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                                   .setImage(newRenderToImages.back().getVulkanImage())
                                   .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                                   .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                                   .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                                   .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                                   .setSubresourceRange(vk::ImageSubresourceRange()
                                                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                                            .setBaseMipLevel(0)
                                                            .setLevelCount(1)
                                                            .setBaseArrayLayer(0)
                                                            .setLayerCount(1)));
    }

    core::helper::command_buffer::SingleTimeCommands(device, star::Queue_Type::Tpresent, [&](vk::CommandBuffer cmd) {
        cmd.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(swapBarriers));
    });

    // create depth images
    vk::Format depthFormat =
        selectFormat(device, {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
                     vk::FormatFeatureFlagBits::eDepthStencilAttachment | vk::FormatFeatureFlagBits::eSampledImage);
    std::vector<StarTextures::Texture> depthTextures;

    depthTextures.reserve(newRenderToImages.size());
    {
        const auto &props = device.getDevice().getPhysicalDevice().getProperties();

        auto builder =
            star::StarTextures::Texture::Builder(device.getDevice().getVulkanDevice(),
                                                 device.getDevice().getAllocator().get())
                .setCreateInfo(
                    Allocator::AllocationBuilder()
                        .setFlags(VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
                        .setUsage(VMA_MEMORY_USAGE_GPU_ONLY)
                        .build(),
                    vk::ImageCreateInfo()
                        .setExtent(resolution)
                        .setArrayLayers(1)
                        .setSharingMode(vk::SharingMode::eExclusive)
                        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
                        .setImageType(vk::ImageType::e2D)
                        .setMipLevels(1)
                        .setTiling(vk::ImageTiling::eOptimal)
                        .setInitialLayout(vk::ImageLayout::eUndefined)
                        .setSamples(vk::SampleCountFlagBits::e1),
                    "OffscreenRenderToImagesDepth")
                .setBaseFormat(depthFormat)
                .addViewInfo(vk::ImageViewCreateInfo()
                                 .setViewType(vk::ImageViewType::e2D)
                                 .setFormat(depthFormat)
                                 .setSubresourceRange(vk::ImageSubresourceRange()
                                                          .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                                                          .setBaseArrayLayer(0)
                                                          .setLayerCount(1)
                                                          .setBaseMipLevel(0)
                                                          .setLevelCount(1)))
                .setSamplerInfo(vk::SamplerCreateInfo()
                                    .setAnisotropyEnable(true)
                                    .setMaxAnisotropy(star::StarTextures::Texture::SelectAnisotropyLevel(props))
                                    .setMagFilter(star::StarTextures::Texture::SelectTextureFiltering(props))
                                    .setMinFilter(star::StarTextures::Texture::SelectTextureFiltering(props))
                                    .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                                    .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                                    .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                                    .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
                                    .setUnnormalizedCoordinates(VK_FALSE)
                                    .setCompareEnable(VK_FALSE)
                                    .setCompareOp(vk::CompareOp::eAlways)
                                    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                                    .setMipLodBias(0.0f)
                                    .setMinLod(0.0f)
                                    .setMaxLod(0.0f));

        std::vector<vk::ImageMemoryBarrier2> depthBarriers{newRenderToImages.size()};
        for (uint8_t i = 0; i < newRenderToImages.size(); i++)
        {
            star::StarTextures::Texture depthTexture = builder.build();

            depthBarriers[i] = vk::ImageMemoryBarrier2()
                                   .setOldLayout(vk::ImageLayout::eUndefined)
                                   .setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                                   .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                                   .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                                   .setImage(depthTexture.getVulkanImage())
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

            depthTextures.emplace_back(std::move(depthTexture));
        }

        core::helper::command_buffer::SingleTimeCommands(
            device, star::Queue_Type::Tpresent, [&](vk::CommandBuffer cmd) {
                cmd.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(depthBarriers));
            });
    }

    auto colorHandles = RegisterTextures(device, renderingContext, std::move(newRenderToImages));
    auto depthHandles = RegisterTextures(device, renderingContext, std::move(depthTextures));

    return star::core::renderer::RenderTargets{std::move(colorHandles), format, std::move(depthHandles), depthFormat};
}

std::unique_ptr<star::core::renderer::RenderPhase> SwapChainRenderPhaseProvider::build(
    star::core::device::DeviceContext &context, star::core::renderer::RenderPhaseRegistry & /*phases*/)
{
    auto phase = std::make_unique<SwapChainRenderPhase>();

    // --- SwapChainRenderer::prepRender equivalent (before DefaultRenderer::prepRender) ---
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