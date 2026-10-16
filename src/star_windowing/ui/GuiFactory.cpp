#include "star_windowing/ui/GuiFactory.hpp"

#include "star_windowing/GuiDefinition.hpp"
#include "star_windowing/WindowingContext.hpp"
#include "star_windowing/ui/ImGuiFactory.hpp"
#include "star_windowing/ui/InitImGuiVulkanBackend.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/core/Exceptions.hpp>
#include <starlight/core/renderer/RenderPhase.hpp>
#include <starlight/core/waiter/one_shot/CreateDescriptorsOnEventPolicy.hpp>
#include <starlight/event/DescriptorPoolReady.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <vulkan/vulkan.hpp>

namespace star::windowing::GuiFactory
{

static vk::DescriptorPool CreateImGuiDescriptorPool(vk::Device device)
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
    // by the caller (not the engine-wide pool) so this capability stays local
    // to the overlay.
    info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    info.maxSets = kSampledImages + kSamplers;
    info.poolSizeCount = 2;
    info.pPoolSizes = sizes;

    return device.createDescriptorPool(info);
}

void Initialize(star::core::device::StarDevice &device,
                star::core::device::manager::GraphicsContainer &graphicsManagers, star::common::EventBus &eventBus,
                WindowingContext &winContext, vk::SwapchainKHR swapchain,
                const star::core::renderer::RenderPhase *finalizationPhase, const GuiDefinition &config,
                std::string &iniFile, vk::DescriptorPool &descriptorPool, bool &vulkanInitialized)
{
    if (finalizationPhase == nullptr)
        STAR_THROW("GuiFactory requires the main graphics renderer to be known before it is initialized");

    const auto &targets = finalizationPhase->getRenderTargets();
    const auto &colorHandles = targets.colorHandles();
    if (colorHandles.empty() || !targets.colorFormat().has_value())
        STAR_THROW("GuiFactory requires the finalization phase to expose swapchain color targets");

    // ---- ImGui context + GLFW backend ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    iniFile = ResolveIniFile(config.iniFilename);
    io.IniFilename = iniFile.empty() ? nullptr : iniFile.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    if (config.styleScale != 1.0f)
    {
        ImGui::GetStyle().ScaleAllSizes(config.styleScale);
    }

    // install_callbacks=true chains to the GLFW callbacks star installed in
    // InteractivityBus::Init (which runs earlier), so star's events keep flowing
    // while ImGui additionally consumes char/scroll/focus input.
    if (!ImGui_ImplGlfw_InitForVulkan(winContext.window.getGLFWWindow(), /*install_callbacks=*/true))
        STAR_THROW("Failed to initialize the ImGui GLFW backend");

    // ---- deferred Vulkan backend ----
    // The backend allocates its descriptor sets from the caller-owned pool, so
    // defer its Vulkan init until DescriptorPoolReady fires (after the load
    // phase). The context and GLFW backend above do not depend on the pool and
    // stay eager.
    const uint32_t imageCount = static_cast<uint32_t>(device.getVulkanDevice().getSwapchainImagesKHR(swapchain).size());
    descriptorPool = CreateImGuiDescriptorPool(device.getVulkanDevice());

    star::core::waiter::one_shot::CreateDescriptorsOnEventPolicy<InitImGuiVulkanBackend>::Builder(eventBus)
        .setEventType(star::common::HandleTypeRegistry::instance().registerType(
            star::event::DescriptorPoolReady::GetUniqueTypeName()))
        .setPolicy(InitImGuiVulkanBackend{&device, &graphicsManagers, &eventBus, winContext.instance,
                                          targets.colorFormat().value(), imageCount, descriptorPool,
                                          &vulkanInitialized})
        .buildShared();
}

void BeginFrame()
{
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
}

void EndFrame()
{
    ImGui::Render();
}

void CaptureState(bool &keyboard, bool &mouse)
{
    const ImGuiIO &io = ImGui::GetIO();
    keyboard = io.WantCaptureKeyboard;
    mouse = io.WantCaptureMouse;
}

void RenderDrawData(vk::CommandBuffer commandBuffer)
{
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void Shutdown(star::core::device::StarDevice &device, vk::DescriptorPool &descriptorPool, bool &vulkanInitialized)
{
    if (vulkanInitialized)
    {
        ImGui_ImplVulkan_Shutdown();
        vulkanInitialized = false;
    }

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (descriptorPool != VK_NULL_HANDLE)
    {
        device.getVulkanDevice().destroyDescriptorPool(descriptorPool);
        descriptorPool = VK_NULL_HANDLE;
    }
}
} // namespace star::windowing::GuiFactory