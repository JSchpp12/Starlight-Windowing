#pragma once

#include <star_common/IServiceCommand.hpp>

#include <functional>
#include <string_view>
#include <utility>

namespace star
{
class StarScene;
} // namespace star

namespace star::windowing
{
namespace create_gui
{
inline constexpr const char *GetTypeName()
{
    return "swGuiCreate";
}
} // namespace create_gui

/// @brief Asks the gui controller service to attach (or update) the ImGui
/// overlay for the given scene.
struct CreateGui : public star::common::IServiceCommand
{
    static constexpr std::string_view GetUniqueTypeName()
    {
        return create_gui::GetTypeName();
    }

    CreateGui(star::StarScene &scene, std::function<void()> draw) : scene(scene), draw(std::move(draw))
    {
    }

    star::StarScene &scene;
    std::function<void()> draw;
};
} // namespace star::windowing
