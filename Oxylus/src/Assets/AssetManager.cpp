#include "AssetManager.hpp"

#include "Assets/Texture.hpp"
#include "Audio/AudioSource.hpp"
#include "Core/App.hpp"
#include "Render/Mesh.hpp"

#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
AssetManager* AssetManager::_instance = nullptr;

void AssetManager::set_instance() {
  if (_instance == nullptr)
    _instance = App::get_system<AssetManager>();
}

Shared<Texture> AssetManager::get_texture_asset(const TextureLoadInfo& info) {
  if (_instance->_state.texture_assets.contains(info.path)) {
    return _instance->_state.texture_assets[info.path];
  }

  return load_texture_asset(info.path, info);
}

// TODO: Doesn't respect virtual dirs
Shared<Texture> AssetManager::get_texture_asset(const std::string& name, const TextureLoadInfo& info) {
  if (_instance->_state.texture_assets.contains(name)) {
    return _instance->_state.texture_assets[name];
  }

  return load_texture_asset(name, info);
}

// TODO: Doesn't respect virtual dirs
AssetTask<Texture>* AssetManager::get_texture_asset_future(const TextureLoadInfo& info) {
  const auto* t = &_instance->_state.texture_tasks.emplace_back(create_unique<AssetTask<Texture>>([info] {
    if (_instance->_state.texture_assets.contains(info.path)) {
      return _instance->_state.texture_assets[info.path];
    }

    return load_texture_asset(info.path, info);
  }));

  return t->get();
}

Shared<Mesh> AssetManager::get_mesh_asset(const std::string& path, const uint32_t loadingFlags) {
  OX_SCOPED_ZONE;
  if (_instance->_state.mesh_assets.contains(path)) {
    return _instance->_state.mesh_assets[path];
  }

  return load_mesh_asset(path, loadingFlags);
}

// TODO: Doesn't respect virtual dirs
AssetTask<Mesh>* AssetManager::get_mesh_asset_future(const std::string& path, uint32_t loadingFlags) {
  const auto* t = &_instance->_state.mesh_tasks.emplace_back(create_unique<AssetTask<Mesh>>([path, loadingFlags] {
    if (_instance->_state.mesh_assets.contains(path)) {
      return _instance->_state.mesh_assets[path];
    }

    return load_mesh_asset(path, loadingFlags);
  }));

  return t->get();
}

Shared<AudioSource> AssetManager::get_audio_asset(const std::string& path) {
  OX_SCOPED_ZONE;
  if (_instance->_state.audio_assets.contains(path)) {
    return _instance->_state.audio_assets[path];
  }

  return load_audio_asset(path);
}

Shared<Texture> AssetManager::load_texture_asset(const std::string& path, const TextureLoadInfo& info) {
  OX_SCOPED_ZONE;

  const auto resolved_path = App::get_system<VFS>()->resolve_physical_dir(info.path);
  TextureLoadInfo new_info = info;
  new_info.path = resolved_path;

  Shared<Texture> texture = create_shared<Texture>(new_info);
  texture->asset_id = (uint32_t)_instance->_state.texture_assets.size();
  texture->asset_path = path;
  return _instance->_state.texture_assets.emplace(path, texture).first->second;
}

Shared<Mesh> AssetManager::load_mesh_asset(const std::string& path, uint32_t loadingFlags) {
  OX_SCOPED_ZONE;
  const auto resolved_path = App::get_system<VFS>()->resolve_physical_dir(path);
  Shared<Mesh> asset = create_shared<Mesh>(resolved_path);
  asset->asset_id = (uint32_t)_instance->_state.mesh_assets.size();
  asset->asset_path = path;
  return _instance->_state.mesh_assets.emplace(path, asset).first->second;
}

Shared<AudioSource> AssetManager::load_audio_asset(const std::string& path) {
  OX_SCOPED_ZONE;
  const auto resolved_path = App::get_system<VFS>()->resolve_physical_dir(path);
  Shared<AudioSource> source = create_shared<AudioSource>(resolved_path);
  source->asset_path = path;
  return _instance->_state.audio_assets.emplace(path, source).first->second;
}

void AssetManager::free_unused_assets() {
  OX_SCOPED_ZONE;
  const auto m_count = std::erase_if(_instance->_state.mesh_assets,
                                     [](const std::pair<std::string, Shared<Mesh>>& pair) { return pair.second.use_count() <= 1; });

  if (m_count > 0)
    OX_LOG_INFO("Cleaned up {} mesh assets.", m_count);

  const auto t_count = std::erase_if(_instance->_state.texture_assets,
                                     [](const std::pair<std::string, Shared<Texture>>& pair) { return pair.second.use_count() <= 1; });

  if (t_count > 0)
    OX_LOG_INFO("Cleaned up {} texture assets.", t_count);
}
} // namespace ox
