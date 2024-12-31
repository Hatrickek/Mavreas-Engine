#include "LuaApplicationBindings.hpp"

#include <sol/state.hpp>

#include "LuaHelpers.hpp"

#include "Core/App.hpp"

namespace ox::LuaBindings {
void bind_application(const Shared<sol::state>& state) {
  auto app_table = state->create_table("App");
  app_table.set_function("get_relative", App::get_asset_directory_absolute);
  app_table.set_function("get_relative", App::get_relative);
  app_table.set_function("get_absolute", App::get_absolute);
}
}
