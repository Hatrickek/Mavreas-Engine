#pragma once
#include "Scene/Components.hpp"
#include "Core/Base.hpp"

namespace ox {
struct SkyboxLoadEvent {
  Shared<Texture> cube_map = nullptr;
};
}
