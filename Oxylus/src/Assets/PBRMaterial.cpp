#include "PBRMaterial.hpp"

namespace ox {
PBRMaterial::PBRMaterial(std::string name) : Material(name) {
  parameters = {};
  albedo_texture = nullptr;
  parameters.albedo_map_id = Asset::INVALID_ID;
  normal_texture = nullptr;
  parameters.normal_map_id = Asset::INVALID_ID;
  ao_texture = nullptr;
  parameters.ao_map_id = Asset::INVALID_ID;
  physical_texture = nullptr;
  parameters.physical_map_id = Asset::INVALID_ID;
  emissive_texture = nullptr;
  parameters.emissive_map_id = Asset::INVALID_ID;
}

#define SET_TEXTURE_ID(texture, parameter) \
  if (!texture)                            \
    parameter = Asset::INVALID_ID;         \
  else                                     \
    parameter = texture->get_id();

PBRMaterial* PBRMaterial::set_albedo_texture(const Shared<Texture>& texture) {
  albedo_texture = texture;
  SET_TEXTURE_ID(texture, parameters.albedo_map_id)
  return this;
}

PBRMaterial* PBRMaterial::set_normal_texture(const Shared<Texture>& texture) {
  normal_texture = texture;
  SET_TEXTURE_ID(texture, parameters.normal_map_id)
  return this;
}

PBRMaterial* PBRMaterial::set_physical_texture(const Shared<Texture>& texture) {
  physical_texture = texture;
  SET_TEXTURE_ID(texture, parameters.physical_map_id)
  return this;
}

PBRMaterial* PBRMaterial::set_ao_texture(const Shared<Texture>& texture) {
  ao_texture = texture;
  SET_TEXTURE_ID(texture, parameters.ao_map_id)
  return this;
}

PBRMaterial* PBRMaterial::set_emissive_texture(const Shared<Texture>& texture) {
  emissive_texture = texture;
  SET_TEXTURE_ID(texture, parameters.emissive_map_id)
  return this;
}

PBRMaterial* PBRMaterial::set_color(float4 color) {
  parameters.color = color;
  return this;
}

PBRMaterial* PBRMaterial::set_roughness(float roughness) {
  parameters.roughness = roughness;
  return this;
}

PBRMaterial* PBRMaterial::set_metallic(float metallic) {
  parameters.metallic = metallic;
  return this;
}

PBRMaterial* PBRMaterial::set_reflectance(float reflectance) {
  parameters.reflectance = reflectance;
  return this;
}

PBRMaterial* PBRMaterial::set_emissive(float4 emissive) {
  parameters.emissive = emissive;
  return this;
}

PBRMaterial* PBRMaterial::set_alpha_mode(AlphaMode alpha_mode) {
  parameters.alpha_mode = (uint32_t)alpha_mode;
  return this;
}

PBRMaterial* PBRMaterial::set_alpha_cutoff(float cutoff) {
  parameters.alpha_cutoff = cutoff;
  return this;
}

PBRMaterial* PBRMaterial::set_double_sided(bool double_sided) {
  parameters.double_sided = double_sided;
  return this;
}

PBRMaterial* PBRMaterial::set_sampler(Sampler sampler) {
  parameters.sampling_mode = (uint32)sampler;
  return this;
}

bool PBRMaterial::is_opaque() const { return parameters.alpha_mode == (uint32_t)AlphaMode::Opaque; }

const char* PBRMaterial::alpha_mode_to_string() const {
  switch ((AlphaMode)parameters.alpha_mode) {
    case AlphaMode::Opaque: return "Opaque";
    case AlphaMode::Mask  : return "Mask";
    case AlphaMode::Blend : return "Blend";
    default               : return "Unknown";
  }
}

bool PBRMaterial::operator==(const PBRMaterial& other) const {
  if (parameters.color == other.parameters.color && parameters.emissive == other.parameters.emissive &&
      parameters.roughness == other.parameters.roughness && parameters.metallic == other.parameters.metallic &&
      parameters.reflectance == other.parameters.reflectance && parameters.normal == other.parameters.normal &&
      parameters.ao == other.parameters.ao && parameters.albedo_map_id == other.parameters.albedo_map_id &&
      parameters.physical_map_id == other.parameters.physical_map_id && parameters.normal_map_id == other.parameters.normal_map_id &&
      parameters.ao_map_id == other.parameters.ao_map_id && parameters.emissive_map_id == other.parameters.emissive_map_id &&
      parameters.alpha_cutoff == other.parameters.alpha_cutoff && parameters.double_sided == other.parameters.double_sided &&
      parameters.uv_scale == other.parameters.uv_scale && parameters.alpha_mode == other.parameters.alpha_mode) {
    return true;
  }

  return false;
}

} // namespace ox
