#include "star_windowing/context/WindowingContext.hpp"

#include <starlight/core/device/managers/DescriptorPool.hpp>

#include <cassert>

namespace star::windowing
{
void WindowingContext::ensureImGuiDescriptorPool(star::core::device::manager::DescriptorPool &poolManager)
{
    if (m_imGuiPool)
        return;

    // createRecord() builds the pool synchronously (no GPU work), so the handle is ready immediately. The manager
    // owns it until device teardown; the context keeps only the raw handle for the ImGui backend.
    m_imGuiPoolHandle = poolManager.submit(star::core::device::manager::DescriptorPoolRequest{
        .requests = {{vk::DescriptorType::eSampledImage, ImGuiSampledImages},
                     {vk::DescriptorType::eSampler, ImGuiSamplers}},
        // The ImGui backend frees its texture descriptor sets individually on shutdown, which requires the
        // free-descriptor-set flag.
        .poolFlags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = ImGuiSampledImages + ImGuiSamplers});

    auto *record = poolManager.get(m_imGuiPoolHandle);
    assert(record != nullptr && record->pool != nullptr && "The ImGui descriptor pool was not created synchronously");
    m_imGuiPool = record->pool->getDescriptorPool();
}

vk::DescriptorPool WindowingContext::imGuiDescriptorPool() const
{
    assert(m_imGuiPool && "ensureImGuiDescriptorPool() must run before imGuiDescriptorPool()");
    return m_imGuiPool;
}

star::Handle WindowingContext::imGuiDescriptorPoolHandle() const
{
    return m_imGuiPoolHandle;
}
} // namespace star::windowing
