#pragma once

#include "Material.hpp"

#include "Core/Types.hpp"
#include "Texture.hpp"

namespace ox {
class SpriteMaterial : public Material {
public:
  struct Parameters {
    float4 color = float4(1.f);

    float2 uv_size = float2(1.f);
    float2 uv_offset = float2(0.f);

    uint32_t albedo_map_id = Asset::INVALID_ID;
  } parameters;

  SpriteMaterial(std::string name = "new_material");

  Shared<Texture>& get_albedo_texture() { return albedo_texture; }

  SpriteMaterial* set_albedo_texture(const Shared<Texture>& texture);

private:
  Shared<Texture> albedo_texture = nullptr;
};
} // namespace ox
