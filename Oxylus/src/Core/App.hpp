#pragma once
#include <exception>
#include <optional>
#include <string>

#include <ankerl/unordered_dense.h>

#include "Base.hpp"
#include "Core/Types.hpp"
#include "ESystem.hpp"

#include "Render/Vulkan/VkContext.hpp"
#include "Utils/Log.hpp"
#include "Utils/Timestep.hpp"
#include "VFS.hpp"

int main(int argc, char** argv);

namespace ox {
class Layer;
class LayerStack;
class ImGuiLayer;
class ThreadManager;

struct AppCommandLineArgs {
  struct Arg {
    std::string arg_str;
    uint32 arg_index;
  };

  std::vector<Arg> args = {};

  AppCommandLineArgs() = default;
  AppCommandLineArgs(const int argc, char** argv) {
    for (int i = 0; i < argc; i++)
      args.emplace_back(Arg{.arg_str = argv[i], .arg_index = (uint32)i});
  }

  bool contains(const std::string_view arg) const {
    for (const auto& a : args) {
      if (a.arg_str == arg) {
        return true;
      }
    }
    return false;
  }

  std::optional<Arg> get(const uint32 index) const {
    try {
      return args.at(index);
    } catch (std::exception& exception) {
      return std::nullopt;
    }
  }

  std::optional<uint32> get_index(const std::string_view arg) const {
    for (const auto& a : args) {
      if (a.arg_str == arg) {
        return a.arg_index;
      }
    }
    return std::nullopt;
  }
};

struct AppSpec {
  std::string name = "Oxylus App";
  std::string working_directory = {};
  std::string assets_path = "Resources";
  uint32_t device_index = 0;
  AppCommandLineArgs command_line_args;
  int2 default_window_size = {0, 0};
};

using SystemRegistry = ankerl::unordered_dense::map<size_t, Shared<ESystem>>;

class App {
public:
  App(AppSpec spec);
  virtual ~App();

  App& push_layer(Layer* layer);
  App& push_overlay(Layer* layer);

  void close();

  const AppSpec& get_specification() const { return app_spec; }
  const AppCommandLineArgs& get_command_line_args() const { return app_spec.command_line_args; }
  ImGuiLayer* get_imgui_layer() const { return imgui_layer; }
  const Shared<LayerStack>& get_layer_stack() const { return layer_stack; }
  static App* get() { return _instance; }
  static void set_instance(App* instance);
  static const Timestep& get_timestep() { return _instance->timestep; }
  static VkContext& get_vkcontext() { return _instance->vk_context; }

  static bool asset_directory_exists();
  static std::string get_asset_directory();
  static std::string get_asset_directory(std::string_view asset_path); // appends the asset_path at the end
  static std::string get_asset_directory_absolute();
  static std::string get_relative(const std::string& path);
  static std::string get_absolute(const std::string& path);

  static SystemRegistry& get_system_registry() { return _instance->system_registry; }

  template <typename T, typename... Args>
  static void register_system(Args&&... args) {
    auto typeName = typeid(T).hash_code();
    OX_ASSERT(!_instance->system_registry.contains(typeName), "Registering system more than once.");

    Shared<T> system = create_shared<T>(std::forward<Args>(args)...);
    _instance->system_registry.emplace(typeName, std::move(system));
  }

  template <typename T>
  static void unregister_system() {
    const auto typeName = typeid(T).hash_code();

    if (_instance->system_registry.contains(typeName)) {
      _instance->system_registry.erase(typeName);
    }
  }

  template <typename T>
  static T* get_system() {
    const auto typeName = typeid(T).hash_code();

    if (_instance->system_registry.contains(typeName)) {
      return dynamic_cast<T*>(_instance->system_registry[typeName].get());
    }

    return nullptr;
  }

  template <typename T>
  static bool has_system() {
    const auto typeName = typeid(T).hash_code();
    return _instance->system_registry.contains(typeName);
  }

private:
  static App* _instance;
  AppSpec app_spec;
  ImGuiLayer* imgui_layer;
  Shared<LayerStack> layer_stack;
  VkContext vk_context;

  SystemRegistry system_registry = {};
  EventDispatcher dispatcher;

  Shared<ThreadManager> thread_manager;
  Timestep timestep;

  bool is_running = true;
  float last_frame_time = 0.0f;

  void run();
  void update_layers(const Timestep& ts);
  void update_renderer();
  void update_timestep();

  friend int ::main(int argc, char** argv);
};

App* create_application(const AppCommandLineArgs& args);
} // namespace ox
