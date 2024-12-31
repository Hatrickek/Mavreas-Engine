#include "HotReloadableScenes.hpp"
#include "Utils/Log.hpp"

#include <chrono>
#include <fstream>

namespace ox {
void HotReloadableScenes::init() {
  if (!std::filesystem::exists(scene_path)) {
    OX_LOG_ERROR("System HotReloadableScene: Scene path doesn't exist: {0}", scene_path);
    return;
  }
  m_last_write_time = std::filesystem::last_write_time(scene_path);
}

void HotReloadableScenes::deinit() {}

void HotReloadableScenes::update() {
  using namespace std::filesystem;
  if (last_write_time(scene_path).time_since_epoch().count()
      != m_last_write_time.time_since_epoch().count()) {
    //File changed event
    m_dispatcher->trigger<ReloadSceneEvent>();
    m_last_write_time = last_write_time(scene_path);
  }
}

void HotReloadableScenes::set_scene_path(const std::string& path) {
  scene_path = path;
}
}
