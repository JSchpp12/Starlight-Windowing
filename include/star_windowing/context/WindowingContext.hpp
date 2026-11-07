#pragma once

#include "star_windowing/RenderingSurface.hpp"
#include "star_windowing/StarWindow.hpp"

#include <star_common/Handle.hpp>

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <string>

namespace star::core::device::manager
{
class DescriptorPool;
}

namespace star::windowing
{
struct WindowingContext
{
    struct CurrentFrameSyncInfo
    {
        vk::Semaphore *swapChainAcquireSemaphore = nullptr;
        vk::Fence *imageAvailableFence = nullptr;
    };

    /// Maximum concurrently-live sampled images the ImGui backend may register. The backend allocates one set per
    /// texture, so this is a concurrency/data bound, not a leak ceiling: sets are freed individually on texture
    /// destruction and the freed slots recycle.
    static constexpr uint32_t ImGuiSampledImages = 64;
    /// Sampler descriptors requested alongside the sampled images (one per distinct sampler the backend registers).
    static constexpr uint32_t ImGuiSamplers = 2;

    /// @brief Registers the ImGui descriptor pool with the engine's descriptor pool manager. Idempotent.
    /// @details Mirrors the skybox conversion context: the manager builds the pool synchronously (no GPU work) and
    /// owns the record until device teardown, so the context only caches the raw handle for the ImGui backend.
    ///
    /// @warning Threading: must be called on the thread that owns the descriptor pool manager (the main/render
    /// thread). The pool is never destroyed explicitly; the manager frees it during device teardown, which runs after
    /// the GUI service has shut the backend down.
    void ensureImGuiDescriptorPool(star::core::device::manager::DescriptorPool &poolManager);

    /// @brief Raw handle of the shared ImGui pool. Valid after ensureImGuiDescriptorPool().
    vk::DescriptorPool imGuiDescriptorPool() const;

    /// @brief Manager registration handle of the ImGui pool. Valid after ensureImGuiDescriptorPool().
    star::Handle imGuiDescriptorPoolHandle() const;

    /// Vulkan instance the surface/device were created from. Cached here so
    /// presentation-coupled phases (e.g. the ImGui overlay) can reach it
    /// without threading the instance through the engine's device context.
    vk::Instance instance{VK_NULL_HANDLE};
    RenderingSurface surface;
    StarWindow window;
    CurrentFrameSyncInfo syncInfo;

    /// Resolved ImGui ini path. Owned here because ImGui keeps a pointer to it for the lifetime of its context, which
    /// outlives the maker that initialized it.
    std::string imGuiIniFile;
    /// Set once the deferred ImGui Vulkan backend init has run.
    bool imGuiVulkanInitialized = false;

  private:
    /// Manager registration of the ImGui pool; the record is freed by the manager at device teardown.
    star::Handle m_imGuiPoolHandle{};
    /// Raw handle cached for the ImGui Vulkan backend.
    vk::DescriptorPool m_imGuiPool{};
};
} // namespace star::windowing
