#pragma once

#include <string>

#include "Base.hpp"

namespace ox {
struct ProjectConfig {
  std::string name = "Untitled";

  std::string start_scene;
  std::string asset_directory;
  std::string module_name;
};

class Project {
public:
  Project() = default;
  Project(const std::string& dir) : project_directory(dir) {}

  ProjectConfig& get_config() { return project_config; }

  static std::string& get_project_directory() { return active_project->project_directory; }
  void set_project_dir(const std::string& dir) { project_directory = dir; }
  static std::string get_asset_directory();
  const std::string& get_project_file_path() const { return project_file_path; }

  void load_module();
  void unload_module() const;
  void check_module();

  static Shared<Project> create_new();
  static Shared<Project> new_project(const std::string& project_dir, const std::string& project_name, const std::string& project_asset_dir);
  static void set_active(const Shared<Project>& project) { active_project = project; }
  static Shared<Project> get_active() { return active_project; }
  static Shared<Project> load(const std::string& path);
  static bool save_active(const std::string& path);

private:
  ProjectConfig project_config;
  std::string project_directory;
  std::string project_file_path;
  inline static Shared<Project> active_project;
};
} // namespace ox
