#include "SpriteMaterial.hpp"
#include "Assets/Material.hpp"

namespace ox {
SpriteMaterial::SpriteMaterial(std::string name) : Material(name) {
  parameters = {};
  albedo_texture = nullptr;
  parameters.albedo_map_id = Asset::INVALID_ID;
}

#define SET_TEXTURE_ID(texture, parameter) \
  if (!texture)                            \
    parameter = Asset::INVALID_ID;         \
  else                                     \
    parameter = texture->get_id();

SpriteMaterial* SpriteMaterial::set_albedo_texture(const Shared<Texture>& texture) {
  albedo_texture = texture;
  SET_TEXTURE_ID(texture, parameters.albedo_map_id)
  return this;
}

} // namespace ox
