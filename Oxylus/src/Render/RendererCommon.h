﻿#pragma once
#include <utility>

#include <vuk/Future.hpp>

#include "Core/Base.hpp"

namespace ox {
class Texture;
class Mesh;

class RendererCommon {
public:
  /// Apply gaussian blur in a single pass
  static void apply_blur(const vuk::Value<vuk::ImageAttachment>& src_attachment, const vuk::Value<vuk::ImageAttachment>& dst_attachment);

  static vuk::Value<vuk::ImageAttachment> generate_cubemap_from_equirectangular(const vuk::ImageAttachment& cubemap);

  static Shared<Mesh> generate_quad();
  static Shared<Mesh> generate_cube();
  static Shared<Mesh> generate_sphere();

private:
  static struct MeshLib {
    Shared<Mesh> quad = {};
    Shared<Mesh> cube = {};
    Shared<Mesh> sphere = {};
  } mesh_lib;
};
}
