#include "Project.hpp"

#include <entt/locator/locator.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/node.hpp>
#include <filesystem>

#include "Assets/AssetManager.hpp"
#include "Base.hpp"
#include "Core/App.hpp"
#include "Core/FileSystem.hpp"
#include "FileSystem.hpp"
#include "Modules/ModuleRegistry.hpp"
#include "ProjectSerializer.hpp"

#include "Modules/ModuleUtil.hpp"

#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
static std::filesystem::file_time_type last_module_write_time = {};

std::string Project::get_asset_directory() { return fs::append_paths(get_project_directory(), active_project->project_config.asset_directory); }

void Project::load_module() {
  if (get_config().module_name.empty())
    return;

  const auto module_path = fs::append_paths(get_project_directory(), project_config.module_name);
  ModuleUtil::load_module(project_config.module_name, module_path);
  auto* module_registry = App::get_system<ModuleRegistry>();
  if (auto* module = module_registry->get_lib(project_config.module_name))
    last_module_write_time = std::filesystem::last_write_time(module->path + dylib::filename_components::suffix);
}

void Project::unload_module() const {
  if (!project_config.module_name.empty())
    ModuleUtil::unload_module(project_config.module_name);
}

void Project::check_module() {
  if (get_config().module_name.empty())
    return;

  auto* module_registry = App::get_system<ModuleRegistry>();
  if (auto* module = module_registry->get_lib(project_config.module_name)) {
    const auto& module_path = module->path + dylib::filename_components::suffix;
    if (std::filesystem::last_write_time(module_path).time_since_epoch().count() != last_module_write_time.time_since_epoch().count()) {
      ModuleUtil::unload_module(project_config.module_name);
      ModuleUtil::load_module(project_config.module_name, module->path);
      last_module_write_time = std::filesystem::last_write_time(module_path);
      OX_LOG_INFO("Reloaded {} module.", project_config.module_name);
    }
  }
}

Shared<Project> Project::create_new() {
  OX_SCOPED_ZONE;
  active_project = create_shared<Project>();
  return active_project;
}

Shared<Project> Project::new_project(const std::string& project_dir, const std::string& project_name, const std::string& project_asset_dir) {
  auto project = create_shared<Project>();
  project->get_config().name = project_name;
  project->get_config().asset_directory = project_asset_dir;

  project->set_project_dir(project_dir);

  if (project_dir.empty())
    return nullptr;

  std::filesystem::create_directory(project_dir);

  const auto asset_folder_dir = fs::append_paths(project_dir, project_asset_dir);
  std::filesystem::create_directory(asset_folder_dir);

  const ProjectSerializer serializer(project);
  serializer.serialize(fs::append_paths(project_dir, project_name + ".oxproj"));

  set_active(project);

  return project;
}

Shared<Project> Project::load(const std::string& path) {
  const Shared<Project> project = create_shared<Project>();

  const ProjectSerializer serializer(project);
  if (serializer.deserialize(path)) {
    project->set_project_dir(std::filesystem::path(path).parent_path().string());
    project->project_file_path = std::filesystem::absolute(path).string();

    const auto asset_dir_path = fs::append_paths(fs::get_directory(project->project_file_path), project->project_config.asset_directory);
    App::get_system<VFS>()->mount_dir("Assets/", asset_dir_path);

    active_project = project;
    active_project->load_module();

    OX_LOG_INFO("Project loaded: {0}", project->get_config().name);
    return active_project;
  }

  return nullptr;
}

bool Project::save_active(const std::string& path) {
  const ProjectSerializer serializer(active_project);
  if (serializer.serialize(path)) {
    active_project->set_project_dir(std::filesystem::path(path).parent_path().string());
    return true;
  }
  return false;
}
} // namespace ox
