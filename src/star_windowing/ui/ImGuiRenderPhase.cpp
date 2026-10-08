#include "star_windowing/ui/ImGuiRenderPhase.hpp"

#include "star_windowing/WindowingContext.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/core/Exceptions.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/core/device/managers/Fence.hpp>
#include <starlight/core/device/managers/Semaphore.hpp>
#include <starlight/core/device/system/event/ManagerRequest.hpp>
#include <starlight/core/helper/queue/QueueHelpers.hpp>
#include <starlight/core/waiter/one_shot/CreateDescriptorsOnEventPolicy.hpp>
#include <starlight/event/DescriptorPoolReady.hpp>
#include <starlight/wrappers/graphics/StarCommandBuffer.hpp>
#include <starlight/wrappers/graphics/StarTextures/Texture.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace star::windowing
{
namespace
{
/// One binary semaphore per swapchain image: the overlay signals the one
/// dedicated to the acquired image index, and it becomes the present-wait.
/// This satisfies the swapchain semaphore reuse contract -- the semaphore is
/// only re-signaled after the same image has been re-acquired, which implies
/// the presentation engine is done with it.
void CreatePerImageRenderDoneSemaphores(star::core::device::DeviceContext &context,
                                        std::vector<PerFifRenderResource> &resources)
{
    for (auto &resource : resources)
    {
        star::Handle semaphoreHandle;
        context.getEventBus().emit(star::core::device::system::event::ManagerRequest(
            star::common::HandleTypeRegistry::instance()
                .getType(star::core::device::manager::GetSemaphoreEventTypeName)
                .value(),
            star::core::device::manager::SemaphoreRequest(), semaphoreHandle));

        if (!semaphoreHandle.isInitialized())
        {
            STAR_THROW("Failed to create per-image render-done semaphores for the ImGui overlay");
        }

        resource.renderDoneSemaphore =
            context.getGraphicsManagers().semaphoreManager->get(semaphoreHandle)->semaphore;
    }
}

/// One signaled fence per frame in flight. The manager disables
/// StarCommandBuffer's internal ready fences when a submission override is
/// used, so the phase owns the equivalent guard for host-side record safety.
std::vector<vk::Fence> CreateSubmitFences(star::core::device::DeviceContext &context, const size_t &numToCreate)
{
    auto fences = std::vector<vk::Fence>(numToCreate);
    for (size_t i{0}; i < numToCreate; i++)
    {
        void *r = nullptr;
        star::Handle recordHandle;
        context.getEventBus().emit(star::core::device::system::event::ManagerRequest(
            star::common::HandleTypeRegistry::instance().getTypeGuaranteedExist(
                star::core::device::manager::GetFenceEventName),
            star::core::device::manager::FenceRequest{true}, recordHandle, &r));

        if (r == nullptr)
        {
            STAR_THROW("Failed to create submit fences for the ImGui overlay");
        }

        fences[i] = static_cast<star::core::device::manager::FenceRecord *>(r)->fence;
    }

    return fences;
}

vk::DescriptorPool CreateImGuiDescriptorPool(vk::Device device)
{
    constexpr uint32_t kSampledImages = 64;
    constexpr uint32_t kSamplers = 2;

    vk::DescriptorPoolSize sizes[2] = {
        {vk::DescriptorType::eSampledImage, kSampledImages},
        {vk::DescriptorType::eSampler, kSamplers},
    };

    vk::DescriptorPoolCreateInfo info{};
    // The ImGui backend frees its texture descriptor sets individually on
    // shutdown, which requires the free-descriptor-set flag. The pool is owned
    // by the phase (not the engine-wide pool) so this capability stays local to
    // the overlay.
    info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    info.maxSets = kSampledImages + kSamplers;
    info.poolSizeCount = 2;
    info.pPoolSizes = sizes;

    return device.createDescriptorPool(info);
}

/// One-shot initializer for the ImGui Vulkan backend. It is invoked by the
/// engine's DescriptorPoolReady event through the same waiter DescriptorRecipe
/// uses, so the backend's descriptor sets are created once the engine signals
/// that its descriptor pools are allocated. The pool itself is owned by the
/// phase because the ImGui backend frees individual texture sets on shutdown.
struct InitImGuiVulkanBackend
{
    star::core::device::DeviceContext *context{nullptr};
    vk::Instance instance{};
    vk::Format swapchainFormat{};
    uint32_t imageCount{0};
    vk::DescriptorPool descriptorPool{};

    int operator()()
    {
        assert(context != nullptr);

        auto *presentQueue = star::core::helper::GetEngineDefaultQueue(
            context->getEventBus(), context->getGraphicsManagers().queueManager, star::Queue_Type::Tpresent);
        if (presentQueue == nullptr)
            STAR_THROW("ImGui overlay could not acquire the engine presentation queue");

        auto &device = context->getDevice();
        VkFormat format = static_cast<VkFormat>(swapchainFormat);
        VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{};
        pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &format;

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = device.getPhysicalDevice().getProperties().apiVersion;
        initInfo.Instance = static_cast<VkInstance>(instance);
        initInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(device.getPhysicalDevice());
        initInfo.Device = static_cast<VkDevice>(device.getVulkanDevice());
        initInfo.QueueFamily = presentQueue->getParentQueueFamilyIndex();
        initInfo.Queue = static_cast<VkQueue>(presentQueue->getVulkanQueue());
        initInfo.DescriptorPool = static_cast<VkDescriptorPool>(descriptorPool);
        initInfo.DescriptorPoolSize = 0;
        initInfo.MinImageCount = std::max<uint32_t>(imageCount, 2);
        initInfo.ImageCount = imageCount;
        initInfo.UseDynamicRendering = true;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;

        if (!ImGui_ImplVulkan_Init(&initInfo))
            STAR_THROW("Failed to initialize the ImGui Vulkan backend");

        return 0;
    }
};
} // namespace

static vk::ImageMemoryBarrier2 MakeLayoutBarrier(const vk::Image &image, const vk::ImageLayout &oldLayout,
                                                 const vk::ImageLayout &newLayout, const vk::AccessFlags2 &srcAccess,
                                                 const vk::AccessFlags2 &dstAccess,
                                                 const vk::PipelineStageFlags2 &srcStage,
                                                 const vk::PipelineStageFlags2 &dstStage) noexcept
{
    return vk::ImageMemoryBarrier2()
        .setOldLayout(oldLayout)
        .setNewLayout(newLayout)
        .setSubresourceRange(vk::ImageSubresourceRange()
                                 .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                 .setBaseMipLevel(0)
                                 .setLevelCount(1)
                                 .setBaseArrayLayer(0)
                                 .setLayerCount(1))
        .setImage(image)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setSrcStageMask(srcStage)
        .setSrcAccessMask(srcAccess)
        .setDstStageMask(dstStage)
        .setDstAccessMask(dstAccess);
}

ImGuiRenderPhase::ImGuiRenderPhase(star::core::renderer::RenderingTargetInfo renderingTargetInfo)
    : m_renderingTargetInfo(std::move(renderingTargetInfo))
{
}

ImGuiRenderPhase::Builder::Builder(star::core::device::DeviceContext &context) : m_context(context)
{
}

ImGuiRenderPhase::Builder &ImGuiRenderPhase::Builder::setWindowingContext(WindowingContext *winContext)
{
    m_winContext = winContext;
    return *this;
}

ImGuiRenderPhase::Builder &ImGuiRenderPhase::Builder::setRenderingTargetInfo(
    star::core::renderer::RenderingTargetInfo renderingTargetInfo)
{
    m_renderingTargetInfo = std::move(renderingTargetInfo);
    return *this;
}

ImGuiRenderPhase::Builder &ImGuiRenderPhase::Builder::setSwapchainTextures(
    std::vector<const star::StarTextures::Texture *> textures)
{
    m_swapchainTextures = std::move(textures);
    return *this;
}

ImGuiRenderPhase::Builder &ImGuiRenderPhase::Builder::setPresentQueue(StarQueue *presentQueue)
{
    m_presentQueue = presentQueue;
    return *this;
}

ImGuiRenderPhase::Builder &ImGuiRenderPhase::Builder::setIniFilename(std::string iniFilename)
{
    m_iniFilename = std::move(iniFilename);
    return *this;
}

ImGuiRenderPhase::Builder &ImGuiRenderPhase::Builder::setEnabled(bool enabled)
{
    m_enabled = enabled;
    return *this;
}

ImGuiRenderPhase::Builder &ImGuiRenderPhase::Builder::setStyleScale(float styleScale)
{
    m_styleScale = styleScale;
    return *this;
}

ImGuiRenderPhase::Builder &ImGuiRenderPhase::Builder::setFrameCallbacks(std::vector<FrameCallback> callbacks)
{
    m_frameCallbacks = std::move(callbacks);
    return *this;
}

ImGuiRenderPhase::Builder &ImGuiRenderPhase::Builder::setCaptureStateCallback(CaptureStateCallback callback)
{
    m_captureStateCallback = std::move(callback);
    return *this;
}

std::unique_ptr<ImGuiRenderPhase> ImGuiRenderPhase::Builder::buildUnique()
{
    assert(m_winContext != nullptr && "ImGuiRenderPhase::Builder requires a WindowingContext");

    auto phase = std::make_unique<ImGuiRenderPhase>(m_renderingTargetInfo);
    ImGuiRenderPhase &target = *phase;

    target.m_perFifResources.reserve(m_swapchainTextures.size());
    for (const auto *texture : m_swapchainTextures)
    {
        target.m_perFifResources.push_back(PerFifRenderResource{
            .image = texture->getVulkanImage(),
            .imageView = texture->getImageView(),
        });
        const auto baseExtent = texture->getBaseExtent();
        target.m_extent = vk::Extent2D{baseExtent.width, baseExtent.height};
    }
    target.m_iniFilename = std::move(m_iniFilename);
    target.m_enabled = m_enabled;
    target.m_frameCallbacks = std::move(m_frameCallbacks);
    target.m_captureStateCallback = std::move(m_captureStateCallback);
    target.m_presentQueue = m_presentQueue;
    target.m_device = m_context.getDevice().getVulkanDevice();

    const size_t imageCount = target.m_perFifResources.size();

    // ---- ImGui context + backends ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = target.m_iniFilename.empty() ? nullptr : target.m_iniFilename.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    if (m_styleScale != 1.0f)
    {
        ImGui::GetStyle().ScaleAllSizes(m_styleScale);
    }

    // install_callbacks=true chains to the GLFW callbacks star installed in
    // InteractivityBus::Init (which runs earlier), so star's events keep flowing
    // while ImGui additionally consumes char/scroll/focus input.
    if (!ImGui_ImplGlfw_InitForVulkan(m_winContext->window.getGLFWWindow(), /*install_callbacks=*/true))
        STAR_THROW("Failed to initialize the ImGui GLFW backend");

    target.m_descriptorPool = CreateImGuiDescriptorPool(m_context.getDevice().getVulkanDevice());

    // The backend allocates its descriptor sets from the phase-owned pool, so
    // defer its Vulkan init until DescriptorPoolReady fires (after the load
    // phase). The context and GLFW backend above do not depend on the pool and
    // stay eager.
    star::core::waiter::one_shot::CreateDescriptorsOnEventPolicy<InitImGuiVulkanBackend>::Builder(
        m_context.getEventBus())
        .setEventType(star::common::HandleTypeRegistry::instance().registerType(
            star::event::DescriptorPoolReady::GetUniqueTypeName()))
        .setPolicy(InitImGuiVulkanBackend{&m_context, m_winContext->instance,
                                          target.m_renderingTargetInfo.colorAttachmentFormats.front(),
                                          static_cast<uint32_t>(imageCount), target.m_descriptorPool})
        .buildShared();

    CreatePerImageRenderDoneSemaphores(m_context, target.m_perFifResources);
    target.m_submitFences = CreateSubmitFences(m_context, m_context.frameTracker().getSetup().getNumFramesInFlight());
    // The command-buffer manager reserves 8 scratch one-time waits; the extra
    // slot is the finalization phase's render-done semaphore. Reserving here
    // keeps submitBuffer allocation-free in the steady state.
    target.m_waitInfos.reserve(1 + 8);

    // ---- command buffer ----
    // end_of_frame, on the presentation queue (same family the finalization
    // phase renders/submits on, so no ownership transfers are needed for the
    // layout transitions). A submission override is used so the present-wait
    // semaphore is dedicated to the acquired swapchain image: the override
    // waits on the finalization phase's per-image render-done semaphore, then
    // signals this phase's per-image render-done semaphore which the manager
    // propagates to the present operation. Using the default (non-override)
    // submission would make the manager return its per-frame-in-flight
    // completion semaphore as the present wait, which violates the swapchain
    // semaphore reuse contract (binary semaphores used with
    // vkQueuePresentKHR may only be re-signaled after their image was
    // re-acquired -- VUID-vkQueueSubmit-pSignalSemaphores-00067). Index
    // 'fourth' keeps clear of the screen-capture copy ('first') and the
    // image-metric copy ('fifth').
    namespace ph = std::placeholders;
    target.m_commandBuffer = m_context.getManagerCommandBuffer().submit(
        star::core::device::manager::ManagerCommandBuffer::Request{
            .recordBufferCallback = std::bind(&ImGuiRenderPhase::recordCommandBuffer, &target, ph::_1, ph::_2, ph::_3),
            .order = star::Command_Buffer_Order::end_of_frame,
            .orderIndex = star::Command_Buffer_Order_Index::fourth,
            .type = star::Queue_Type::Tpresent,
            .waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .willBeSubmittedEachFrame = true,
            .recordOnce = false,
            .beforeBufferSubmissionCallback = std::bind(&ImGuiRenderPhase::waitForSubmitFence, &target, ph::_1),
            .overrideBufferSubmissionCallback =
                std::bind(&ImGuiRenderPhase::submitBuffer, &target, ph::_1, ph::_2, ph::_3, ph::_4, ph::_5, ph::_6)},
        m_context.frameTracker().getCurrent().getGlobalFrameCounter());

    return phase;
}

void ImGuiRenderPhase::addFrameCallback(FrameCallback callback)
{
    m_frameCallbacks.emplace_back(std::move(callback));
}

void ImGuiRenderPhase::clearFrameCallbacks()
{
    m_frameCallbacks.clear();
}

void ImGuiRenderPhase::setCaptureStateCallback(CaptureStateCallback callback)
{
    m_captureStateCallback = std::move(callback);
}

void ImGuiRenderPhase::frameUpdate(star::common::IDeviceContext & /*context*/)
{
    // Run the platform + renderer backends and build this frame's draw data.
    // The base RenderPhase::frameUpdate is intentionally not called: it would
    // try to update render groups and query the command-order DAG, neither of
    // which this phase participates in.
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();

    if (m_enabled)
    {
        for (auto &callback : m_frameCallbacks)
        {
            callback();
        }
    }

    ImGui::Render();

    // Publish capture state to the gui controller service, which owns the one
    // authoritative capture decision used to gate star input events.
    if (m_captureStateCallback)
    {
        const ImGuiIO &io = ImGui::GetIO();
        m_captureStateCallback(io.WantCaptureKeyboard, io.WantCaptureMouse);
    }
}

void ImGuiRenderPhase::recordCommandBuffer(star::StarCommandBuffer &commandBuffer, const star::common::FrameTracker &ft,
                                           const uint64_t & /*frameIndex*/)
{
    const uint8_t frameInFlight = ft.getCurrent().getFrameInFlightIndex();
    const size_t imageIndex = static_cast<size_t>(ft.getCurrent().getFinalTargetImageIndex());
    assert(imageIndex < m_perFifResources.size() && "ImGui overlay image index out of range");
    const PerFifRenderResource &resource = m_perFifResources[imageIndex];

    commandBuffer.begin(frameInFlight);
    vk::CommandBuffer cb = commandBuffer.buffer(frameInFlight);

    // The finalization phase left the acquired image in PRESENT_SRC. Re-open it
    // for attachment writes; this buffer's submission override waits on the
    // finalization phase's per-image render-done semaphore before executing.
    const vk::ImageMemoryBarrier2 prep = MakeLayoutBarrier(
        resource.image, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    cb.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(prep));

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView = resource.imageView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    // eLoad is the entire point of an overlay: keep the scene rendered beneath.
    colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea = vk::Rect2D().setOffset({0, 0}).setExtent(m_extent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    cb.beginRendering(renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);
    cb.endRendering();

    const vk::ImageMemoryBarrier2 post = MakeLayoutBarrier(
        resource.image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eNone);
    cb.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post));

    cb.end();
}

void ImGuiRenderPhase::waitForSubmitFence(const int &frameInFlightIndex)
{
    assert(m_device != VK_NULL_HANDLE && "Device must be assigned by the builder before use");

    const size_t index = static_cast<size_t>(frameInFlightIndex);
    assert(index < m_submitFences.size() && "Fence index out of range");
    if (m_device == VK_NULL_HANDLE || index >= m_submitFences.size())
        return;

    const vk::Result waitResult = m_device.waitForFences(1, &m_submitFences[index], VK_TRUE, UINT64_MAX);
    if (waitResult != vk::Result::eSuccess)
    {
        STAR_THROW("Failed to wait for ImGui overlay submit fence");
    }

    const vk::Result resetResult = m_device.resetFences(1, &m_submitFences[index]);
    if (resetResult != vk::Result::eSuccess)
    {
        STAR_THROW("Failed to reset ImGui overlay submit fence");
    }
}

vk::Semaphore ImGuiRenderPhase::submitBuffer(StarCommandBuffer &buffer, const star::common::FrameTracker &frameTracker,
                                             std::vector<vk::Semaphore> *previousCommandBufferSemaphores,
                                             std::vector<vk::Semaphore> &dataSemaphores,
                                             std::vector<vk::PipelineStageFlags> &dataWaitPoints,
                                             std::vector<std::optional<uint64_t>> &previousSignaledValues)
{
    assert(m_presentQueue != nullptr && "Presentation queue must be assigned by the builder");

    const size_t frameInFlight = static_cast<size_t>(frameTracker.getCurrent().getFrameInFlightIndex());
    const size_t imageIndex = static_cast<size_t>(frameTracker.getCurrent().getFinalTargetImageIndex());
    assert(frameInFlight < m_submitFences.size() && "Submit fence index out of range");
    assert(imageIndex < m_perFifResources.size() &&
           "Render-done semaphore index out of range for the acquired swapchain image");

    m_waitInfos.clear();

    // Wait on the finalization phase's per-image render-done semaphore before
    // the overlay writes to the same swapchain image.
    if (previousCommandBufferSemaphores != nullptr && !previousCommandBufferSemaphores->empty() &&
        previousCommandBufferSemaphores->front() != VK_NULL_HANDLE)
    {
        m_waitInfos.push_back(vk::SemaphoreSubmitInfo()
                                  .setSemaphore(previousCommandBufferSemaphores->front())
                                  .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                                  .setValue(0));
    }

    // honor any one-time waits handed to us by the command buffer manager
    assert(dataSemaphores.size() == dataWaitPoints.size() &&
           "One-time wait semaphores and wait points are out of alignment");
    for (size_t i = 0; i < dataSemaphores.size(); i++)
    {
        uint64_t value = 0;
        if (i < previousSignaledValues.size() && previousSignaledValues[i].has_value())
            value = previousSignaledValues[i].value();

        m_waitInfos.push_back(
            vk::SemaphoreSubmitInfo()
                .setSemaphore(dataSemaphores[i])
                .setValue(value)
                .setStageMask(static_cast<vk::PipelineStageFlags2>(static_cast<uint32_t>(dataWaitPoints[i]))));
    }

    const vk::CommandBufferSubmitInfo cbInfo =
        vk::CommandBufferSubmitInfo().setCommandBuffer(buffer.buffer(frameInFlight));

    const vk::SemaphoreSubmitInfo signalInfo = vk::SemaphoreSubmitInfo()
                                                   .setSemaphore(m_perFifResources[imageIndex].renderDoneSemaphore)
                                                   .setValue(0)
                                                   .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

    const vk::SubmitInfo2 submitInfo =
        vk::SubmitInfo2().setWaitSemaphoreInfos(m_waitInfos).setCommandBufferInfos(cbInfo).setSignalSemaphoreInfos(
            signalInfo);

    const vk::Result result = m_presentQueue->getVulkanQueue().submit2(1, &submitInfo, m_submitFences[frameInFlight]);
    if (result != vk::Result::eSuccess)
    {
        STAR_THROW("Failed to submit ImGui overlay command buffer");
    }

    return m_perFifResources[imageIndex].renderDoneSemaphore;
}

void ImGuiRenderPhase::cleanupRender(star::common::IDeviceContext &context)
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    auto &deviceContext = static_cast<star::core::device::DeviceContext &>(context);
    if (m_descriptorPool != VK_NULL_HANDLE)
    {
        deviceContext.getDevice().getVulkanDevice().destroyDescriptorPool(m_descriptorPool);
        m_descriptorPool = VK_NULL_HANDLE;
    }
}

star::core::renderer::RenderingTargetInfo ImGuiRenderPhase::getRenderTargetInfo() const
{
    return m_renderingTargetInfo;
}
} // namespace star::windowing
