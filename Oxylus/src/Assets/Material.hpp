#pragma once

#include <string>
#include "Asset.hpp"

namespace ox {
class Material : public Asset {
public:
  std::string name = "Material";
  std::string path{};

  explicit Material(const std::string& material_name);
  ~Material() = default;

  void create(const std::string& material_name = "Material");

  const std::string& get_name() const { return name; }

protected:
  static uint32_t material_id_counter;
};
} // namespace ox
