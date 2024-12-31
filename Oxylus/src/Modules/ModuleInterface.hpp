#pragma once

#define OX_BUILD_DLL
#include "Linker.hpp"

#include "Core/App.hpp"
#include "Core/Systems/SystemManager.hpp"

#include <entt/locator/locator.hpp>
#include <entt/meta/context.hpp>
#include <imgui_internal.h>
#include <sol/state.hpp>

namespace ox {
class OX_SHARED ModuleInterface {
public:
  virtual ~ModuleInterface() = default;

  virtual void init(App* app_instance, ImGuiContext* imgui_context) = 0;

  virtual void register_components(sol::state* state, const entt::locator<entt::meta_ctx>::node_type& ctx) = 0;
  virtual void unregister_components(sol::state* state, const entt::locator<entt::meta_ctx>::node_type& ctx) = 0;

  virtual void register_cpp_systems(SystemManager* system_manager) = 0;
  virtual void unregister_cpp_systems(SystemManager* system_manager) = 0;
};

// use this function return a heap allocated ModuleInterface
#define CREATE_MODULE_FUNC extern "C" OX_SHARED ModuleInterface* create_module()
} // namespace ox
