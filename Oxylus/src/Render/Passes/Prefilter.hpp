#pragma once
#include <vuk/Value.hpp>

#include "Assets/Texture.hpp"

namespace ox {
class Mesh;

class Prefilter {
public:
  static vuk::Value<vuk::ImageAttachment> generate_brdflut();
  static vuk::Value<vuk::ImageAttachment> generate_irradiance_cube(const Shared<Mesh>& skybox, const Shared<Texture>& cubemap);
  static vuk::Value<vuk::ImageAttachment> generate_prefiltered_cube(const Shared<Mesh>& skybox, const Shared<Texture>& cubemap);
};
}
