#include "star_windowing/KeyStates.hpp"

namespace star::windowing
{
std::map<KEY, bool> KeyStates::states =
    std::map<KEY, bool>{{KEY::A, false}, {KEY::B, false}, {KEY::C, false}, {KEY::D, false}, {KEY::E, false},
                        {KEY::F, false}, {KEY::G, false}, {KEY::H, false}, {KEY::I, false}, {KEY::J, false},
                        {KEY::L, false}, {KEY::M, false}, {KEY::N, false}, {KEY::O, false}, {KEY::P, false},
                        {KEY::Q, false}, {KEY::R, false}, {KEY::S, false}, {KEY::T, false}, {KEY::U, false},
                        {KEY::V, false}, {KEY::W, false}, {KEY::X, false}, {KEY::Y, false}, {KEY::Z, false}};

}