#include "ProjectSerializer.hpp"

#include "Utils/Log.hpp"
#include "Utils/Toml.hpp"
#include "FileSystem.hpp"

namespace ox {
ProjectSerializer::ProjectSerializer(Shared<Project> project) : project(std::move(project)) {}

bool ProjectSerializer::serialize(const std::string& file_path) const {
  const auto& config = project->get_config();

  const auto root = toml::table{{
    "project",
    toml::table{
      {"name", config.name},
      {"asset_directory", config.asset_directory},
      {"start_scene", config.start_scene},
      {"module_name", config.module_name},
    },
  }};

  fs::write_file(file_path, root, "# Oxylus project file"); // TODO: check for result

  return true;
}

bool ProjectSerializer::deserialize(const std::string& file_path) const {
  auto& [name, start_scene, asset_directory, module_name] = project->get_config();
  const auto content = fs::read_file(file_path);
  if (content.empty()) {
    OX_ASSERT(!content.empty(), "Couldn't load project file: {0}", file_path);
    return false;
  }

  try {
    toml::table toml = toml::parse(content);

    const auto project_node = toml["project"];
    name = project_node["name"].as_string()->get();
    asset_directory = project_node["asset_directory"].as_string()->get();
    start_scene = project_node["start_scene"].as_string()->get();
    module_name = project_node["module_name"].as_string()->get();
  } catch (const std::exception& exc) {
    OX_LOG_ERROR("{}", exc.what());
  }

  return true;
}
} // namespace ox
