#include "SceneSerializer.hpp"

#include "Utils/Profiler.hpp"
#include "Assets/AssetManager.hpp"
#include "Core/Project.hpp"
#include "EntitySerializer.hpp"

#include <fstream>

#include "Core/App.hpp"
#include "Core/FileSystem.hpp"

namespace ox {
SceneSerializer::SceneSerializer(const Shared<Scene>& scene) : m_scene(scene) {}

void SceneSerializer::serialize(const std::string& filePath) const {
  auto tbl = toml::table{{{"entities", toml::array{}}}};
  auto entities = tbl.find("entities")->second.as_array();

  tbl.emplace("name", m_scene->scene_name);

  for (const auto [e] : m_scene->registry.storage<entt::entity>().each()) {
    toml::array entity_array = {};
    EntitySerializer::serialize_entity(&entity_array, m_scene.get(), e);
    entities->emplace_back(toml::table{{"entity", entity_array}});
  }

  std::stringstream ss;
  ss << "# Oxylus scene file \n";
  ss << toml::default_formatter{tbl, toml::default_formatter::default_flags & ~toml::format_flags::indent_sub_tables};
  std::ofstream filestream(filePath);
  filestream << ss.str();

  OX_LOG_INFO("Saved scene {0}.", m_scene->scene_name);
}

bool SceneSerializer::deserialize(const std::string& filePath) const {
  const auto content = fs::read_file(filePath);
  if (content.empty()) {
    OX_ASSERT(!content.empty(), fmt::format("Couldn't read scene file: {0}", filePath).c_str());
  }

  toml::table table = toml::parse(content);

  if (table.empty()) {
    OX_LOG_ERROR("Scene was unable to load from TOML file {0}", filePath);
    return false;
  }

  m_scene->scene_name = table["name"].as_string()->get();

  auto entities = table["entities"].as_array();

  for (auto& entity : *entities) {
    auto entity_arr = entity.as_table()->get("entity")->as_array();
    EntitySerializer::deserialize_entity(entity_arr, m_scene.get(), true);
  }

  OX_LOG_INFO("Scene loaded : {0}", fs::get_file_name(m_scene->scene_name));
  return true;
}
}
