#pragma once

#include <filesystem>

#include "Core/ESystem.hpp"

namespace ox {
struct ReloadSceneEvent {
  ReloadSceneEvent() = default;
};

class HotReloadableScenes : public ESystem {
public:
  float update_interval = 1.0f;

  HotReloadableScenes(std::string scene_path) : scene_path(std::move(scene_path)) {}

  void init() override;
  void deinit() override;
  void update() override;

  void set_scene_path(const std::string& path);

private:
  std::string scene_path;
  std::filesystem::file_time_type m_last_write_time;
};
}
