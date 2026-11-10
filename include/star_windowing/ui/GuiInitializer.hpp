#pragma once

#include <star_common/Handle.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/core/renderer/IRenderPhaseProvider.hpp>
#include <starlight/virtual/StarScene.hpp>

#include <cassert>
#include <memory>
#include <utility>

namespace star::windowing
{
/// @brief The GUI overlay's provider plus the already-built main graphics phase it attaches to.
/// @details Mirrors the skybox initializer: the overlay is created while the scene is being set up, but its phase
/// is added only after the finalization phase exists. submitMe() is called with the finalization handle.
class GuiInitializer
{
  public:
    GuiInitializer(star::StarScene &scene, star::Handle finalizationPhase,
                   std::unique_ptr<star::core::renderer::IRenderPhaseProvider> provider)
        : m_scene(scene), m_finalizationPhase(std::move(finalizationPhase)), m_provider(std::move(provider))
    {
    }

    /// @brief Adds the overlay phase now that the finalization phase is built.
    star::Handle submitMe(star::core::device::DeviceContext &context)
    {
        assert(m_scene.getPhase(m_finalizationPhase) != nullptr &&
               "The finalization phase must be built before submitting the GUI");
        return m_scene.addProvider(std::move(m_provider), context);
    }

  private:
    star::StarScene &m_scene;
    star::Handle m_finalizationPhase;
    std::unique_ptr<star::core::renderer::IRenderPhaseProvider> m_provider;
};
} // namespace star::windowing
