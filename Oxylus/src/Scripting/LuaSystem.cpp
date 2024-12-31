#include "LuaSystem.hpp"

#include <filesystem>

#include <sol/state.hpp>

#include "LuaManager.hpp"

#include "Core/App.hpp"

#include "Scene/Entity.hpp"
#include "Scene/Scene.hpp"

#include "Utils/Log.hpp"

namespace ox {
LuaSystem::LuaSystem(std::string path) : file_path(std::move(path)) {
  init_script(file_path);
}

void LuaSystem::check_result(const sol::protected_function_result& result, const char* func_name) {
  if (!result.valid()) {
    const sol::error err = result;
    OX_LOG_ERROR("Error: {0}", err.what());
  }
}

void LuaSystem::init_script(const std::string& path) {
  OX_SCOPED_ZONE;
  if (!std::filesystem::exists(path)) {
    OX_LOG_ERROR("Couldn't find the script file! {}", path);
    return;
  }

  const auto state = App::get_system<LuaManager>()->get_state();
  environment = create_unique<sol::environment>(*state, sol::create, state->globals());

  const auto load_file_result = state->script_file(file_path, *environment, sol::script_pass_on_error);

  if (!load_file_result.valid()) {
    const sol::error err = load_file_result;
    OX_LOG_ERROR("Failed to Execute Lua script {0}", file_path);
    OX_LOG_ERROR("Error : {0}", err.what());
    std::string error = std::string(err.what());

    const auto linepos = error.find(".lua:");
    std::string error_line = error.substr(linepos + 5); //+4 .lua: + 1
    const auto linepos_end = error_line.find(':');
    error_line = error_line.substr(0, linepos_end);
    const int line = std::stoi(error_line);
    error = error.substr(linepos + error_line.size() + linepos_end + 4); //+4 .lua:

    errors[line] = error;
  }

  for (auto [l, e] : errors) {
    OX_LOG_ERROR("{} {}", l, e);
  }

  on_init_func = create_unique<sol::protected_function>((*environment)["on_init"]);
  if (!on_init_func->valid())
    on_init_func.reset();

  on_update_func = create_unique<sol::protected_function>((*environment)["on_update"]);
  if (!on_update_func->valid())
    on_update_func.reset();

  on_imgui_render_func = create_unique<sol::protected_function>((*environment)["on_imgui_render"]);
  if (!on_imgui_render_func->valid())
    on_imgui_render_func.reset();

  on_release_func = create_unique<sol::protected_function>((*environment)["on_release"]);
  if (!on_release_func->valid())
    on_release_func.reset();

  state->collect_gc();
}

void LuaSystem::on_init(Scene* scene, entt::entity entity) {
  OX_SCOPED_ZONE;
  if (on_init_func) {
    (*environment)["scene"] = scene;
    (*environment)["owner"] = std::ref(scene->registry);
    (*environment)["this"] = entity;
    const auto result = on_init_func->call();
    check_result(result, "on_init");
  }
}

void LuaSystem::on_update(const Timestep& delta_time) {
  OX_SCOPED_ZONE;
  if (on_update_func) {
    const auto result = on_update_func->call(delta_time.get_millis());
    check_result(result, "on_update");
  }
}

void LuaSystem::on_release(Scene* scene, entt::entity entity) {
  OX_SCOPED_ZONE;
  if (on_release_func) {
    const auto result = on_release_func->call();
    (*environment)["scene"] = scene;
    (*environment)["owner"] = std::ref(scene->registry);
    (*environment)["this"] = entity;
    check_result(result, "on_release");
  }

  App::get_system<LuaManager>()->get_state()->collect_gc();
}

void LuaSystem::on_imgui_render(const Timestep& delta_time) {
  OX_SCOPED_ZONE;
  if (on_imgui_render_func) {
    const auto result = on_imgui_render_func->call(delta_time.get_millis());
    check_result(result, "on_imgui_render");
  }
}

void LuaSystem::load(const std::string& path) {
  OX_SCOPED_ZONE;
  init_script(path);
}

void LuaSystem::reload() {
  OX_SCOPED_ZONE;
  if (environment) {
    const sol::protected_function releaseFunc = (*environment)["on_release"];
    if (releaseFunc.valid()) {
      const auto result = releaseFunc.call();
      check_result(result, "on_release");
    }
  }

  init_script(file_path);
}

void LuaSystem::bind_globals(Scene* scene, entt::entity entity, const Timestep& timestep) {
  (*environment)["scene"] = scene;
  (*environment)["owner"] = std::ref(scene->registry);
  (*environment)["this"] = entity;
  (*environment)["current_time"] = timestep.get_elapsed_seconds();
}
}
