#include "App.hpp"

#include <filesystem>

#include "Assets/AssetManager.hpp"
#include "FileSystem.hpp"
#include "Layer.hpp"
#include "LayerStack.hpp"
#include "Physics/Physics.hpp"
#include "Project.hpp"

#include "Audio/AudioEngine.hpp"

#include "Modules/ModuleRegistry.hpp"

#include "Render/Renderer.hpp"
#include "Render/Window.hpp"

#include "Scripting/LuaManager.hpp"

#include "Systems/SystemManager.hpp"
#include "Thread/TaskScheduler.hpp"
#include "Thread/ThreadManager.hpp"

#include "UI/ImGuiLayer.hpp"

#include "Utils/FileDialogs.hpp"
#include "Utils/Profiler.hpp"
#include "Utils/Random.hpp"

namespace ox {
App* App::_instance = nullptr;

App::App(AppSpec spec) : app_spec(std::move(spec)) {
  OX_SCOPED_ZONE;
  if (_instance) {
    OX_LOG_ERROR("Application already exists!");
    return;
  }

  _instance = this;

  layer_stack = create_shared<LayerStack>();
  thread_manager = create_shared<ThreadManager>();

  if (app_spec.working_directory.empty())
    app_spec.working_directory = std::filesystem::current_path().string();
  else
    std::filesystem::current_path(app_spec.working_directory);

  register_system<VFS>();
  register_system<Random>();
  register_system<TaskScheduler>();
  register_system<FileDialogs>();
  register_system<AudioEngine>();
  register_system<LuaManager>();
  register_system<ModuleRegistry>();
  register_system<RendererConfig>();
  register_system<SystemManager>();
  register_system<AssetManager>();
  register_system<Physics>();

  Window::init_window(app_spec);
  Window::set_dispatcher(&dispatcher);
  register_system<Input>();

  // Shortcut for commonly used Systems
  get_system<Input>()->set_instance();
  get_system<AssetManager>()->set_instance();
  get_system<Physics>()->set_instance();

  for (auto& [_, system] : system_registry) {
    system->set_dispatcher(&dispatcher);
    system->init();
  }

  vk_context.create_context(app_spec);
  Renderer::init();

  imgui_layer = new ImGuiLayer();
  push_overlay(imgui_layer);
}

App::~App() { close(); }

void App::set_instance(App* instance) {
  _instance = instance;
  get_system<Input>()->set_instance();
  get_system<AssetManager>()->set_instance();
  get_system<Physics>()->set_instance();
}

App& App::push_layer(Layer* layer) {
  layer_stack->push_layer(layer);
  layer->on_attach(dispatcher);

  return *this;
}

App& App::push_overlay(Layer* layer) {
  layer_stack->push_overlay(layer);
  layer->on_attach(dispatcher);

  return *this;
}

void App::run() {
  auto input_sys = get_system<Input>();

  while (is_running) {
    update_timestep();

    update_layers(timestep);

    for (auto& [_, system] : system_registry)
      system->update();

    update_renderer();

    input_sys->reset_pressed();

    Window::poll_events();
    while (App::get_vkcontext().suspend) {
      Window::wait_for_events();
    }
  }

  layer_stack.reset();

  if (Project::get_active())
    Project::get_active()->unload_module();

  for (auto& [_, system] : system_registry)
    system->deinit();

  system_registry.clear();
  Renderer::deinit();

  ThreadManager::get()->wait_all_threads();
  Window::close_window(Window::get_glfw_window());
}

void App::update_layers(const Timestep& ts) {
  OX_SCOPED_ZONE_N("Update Layers");
  for (Layer* layer : *layer_stack.get())
    layer->on_update(ts);
}

void App::update_renderer() { Renderer::draw(&vk_context, imgui_layer, *layer_stack.get()); }

void App::update_timestep() {
  timestep.on_update();

  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = (float)timestep.get_seconds();
}

void App::close() { is_running = false; }

bool App::asset_directory_exists() { return std::filesystem::exists(get_asset_directory()); }

std::string App::get_asset_directory() {
  if (Project::get_active() && !Project::get_active()->get_config().asset_directory.empty())
    return Project::get_asset_directory();
  return _instance->app_spec.assets_path;
}

std::string App::get_asset_directory(const std::string_view asset_path) { return fs::append_paths(get_asset_directory(), asset_path); }

std::string App::get_asset_directory_absolute() {
  if (Project::get_active()) {
    const auto p = std::filesystem::absolute(Project::get_asset_directory());
    return p.string();
  }
  const auto p = absolute(std::filesystem::path(_instance->app_spec.assets_path));
  return p.string();
}

std::string App::get_relative(const std::string& path) { return fs::preferred_path(std::filesystem::relative(path, get_asset_directory()).string()); }

std::string App::get_absolute(const std::string& path) { return fs::append_paths(fs::preferred_path(get_asset_directory_absolute()), path); }
} // namespace ox
