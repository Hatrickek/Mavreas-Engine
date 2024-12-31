#pragma once

#include <ankerl/unordered_dense.h>
#include <plf_colony.h>

#include "Core/Base.hpp"
#include "Core/ESystem.hpp"
#include "Thread/TaskScheduler.hpp"

namespace ox {
struct TextureLoadInfo;
class Texture;
class Mesh;
class AudioSource;

using AssetID = std::string;

template <typename T>
class AssetTask : public ITaskSet {
public:
  typedef std::function<Shared<T>()> TaskSetFunction;
  typedef std::function<void(const Shared<T>&)> OnCompleteFunction;

  AssetTask(TaskSetFunction func) : _func(std::move(func)) {}

  Shared<T> get_asset() { return _asset; }

  void on_complete(OnCompleteFunction func) { _on_complete = std::move(func); }

  void ExecuteRange(TaskSetPartition range_, uint32_t threadnum_) override {
    _asset = _func();
    _on_complete(_asset);
  }

private:
  Shared<T> _asset = nullptr;
  TaskSetFunction _func = nullptr;
  OnCompleteFunction _on_complete = nullptr;
  EventDispatcher* dispatcher = nullptr;
};

class AssetManager : public ESystem {
public:
  void init() override {};
  void deinit() override {};
  void set_instance();

  static Shared<Texture> get_texture_asset(const TextureLoadInfo& info);
  static Shared<Texture> get_texture_asset(const std::string& name, const TextureLoadInfo& info);
  static AssetTask<Texture>* get_texture_asset_future(const TextureLoadInfo& info);

  static Shared<Mesh> get_mesh_asset(const std::string& path, uint32_t loadingFlags = 0);
  static AssetTask<Mesh>* get_mesh_asset_future(const std::string& path, uint32_t loadingFlags = 0);

  static Shared<AudioSource> get_audio_asset(const std::string& path);

  static void free_unused_assets();

private:
  static AssetManager* _instance;

  struct State {
    std::vector<Unique<AssetTask<Mesh>>> mesh_tasks;
    std::vector<Unique<AssetTask<Texture>>> texture_tasks;
    std::vector<Unique<AssetTask<AudioSource>>> audio_tasks;

    ankerl::unordered_dense::map<AssetID, Shared<Texture>> texture_assets;
    ankerl::unordered_dense::map<AssetID, Shared<Mesh>> mesh_assets;
    ankerl::unordered_dense::map<AssetID, Shared<AudioSource>> audio_assets;
  } _state;

  static Shared<Texture> load_texture_asset(const std::string& path, const TextureLoadInfo& info);
  static Shared<Mesh> load_mesh_asset(const std::string& path, uint32_t loadingFlags);
  static Shared<AudioSource> load_audio_asset(const std::string& path);
};
} // namespace ox
