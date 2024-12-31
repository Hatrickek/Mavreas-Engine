#pragma once
#include <cstdint>
#include <string>

namespace ox {
class AssetManager;

class Asset {
public:
  static constexpr uint32_t INVALID_ID = UINT32_MAX;

  Asset() = default;
  explicit Asset(const uint32_t id) : asset_id(id) {}

  uint32_t get_id() const { return asset_id; }
  void set_id(uint32_t id) { asset_id = id; }
  bool is_valid_id() const { return asset_id != INVALID_ID; }

  const std::string& get_path() const { return asset_path; }

private:
  uint32_t asset_id = UINT32_MAX;
  std::string asset_path = {};

  friend AssetManager;
};
} // namespace ox
