#include "LuaRendererBindings.hpp"

#include <sol/state.hpp>

#include "LuaHelpers.hpp"

#include "Core/Types.hpp"

#include "Render/Renderer.hpp"
#include "Render/Window.hpp"

namespace ox::LuaBindings {
void bind_renderer(const Shared<sol::state>& state) {
  auto window_table = state->create_table("Window");
  SET_TYPE_FUNCTION(window_table, Window, get_width);
  SET_TYPE_FUNCTION(window_table, Window, get_height);
  SET_TYPE_FUNCTION(window_table, Window, minimize);
  SET_TYPE_FUNCTION(window_table, Window, is_minimized);
  SET_TYPE_FUNCTION(window_table, Window, maximize);
  SET_TYPE_FUNCTION(window_table, Window, is_maximized);
  SET_TYPE_FUNCTION(window_table, Window, restore);
  SET_TYPE_FUNCTION(window_table, Window, is_fullscreen_borderless);
  SET_TYPE_FUNCTION(window_table, Window, set_fullscreen_borderless);
  SET_TYPE_FUNCTION(window_table, Window, set_windowed);

  auto renderer_table = state->create_table("Renderer");
  SET_TYPE_FUNCTION(renderer_table, Renderer, get_viewport_width);
  SET_TYPE_FUNCTION(renderer_table, Renderer, get_viewport_height);
  SET_TYPE_FUNCTION(renderer_table, Renderer, get_viewport_size);
  SET_TYPE_FUNCTION(renderer_table, Renderer, get_viewport_offset);
}
}
