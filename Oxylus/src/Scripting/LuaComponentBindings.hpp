#pragma once
#include "Core/Base.hpp"

namespace sol {
class state;
}

namespace ox::LuaBindings {
void bind_components(const Shared<sol::state>& state);

void bind_light_component(const Shared<sol::state>& state);
void bind_mesh_component(const Shared<sol::state>& state);
void bind_camera_component(const Shared<sol::state>& state);
}
