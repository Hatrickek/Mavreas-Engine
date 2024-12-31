#include "Material.hpp"

#include "Utils/Profiler.hpp"

namespace ox {
uint32_t Material::material_id_counter = 0;

Material::Material(const std::string& material_name) { create(material_name); }

void Material::create(const std::string& material_name) {
  OX_SCOPED_ZONE;

  set_id(material_id_counter);
  material_id_counter += 1;

  name = material_name;
}
} // namespace ox
