#pragma once
#include "Core/Base.hpp"

namespace sol {
class state;
}

namespace ox::LuaBindings {
void bind_math(const Shared<sol::state>& state);
}
