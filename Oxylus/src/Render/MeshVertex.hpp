#pragma once
#include <vuk/runtime/CommandBuffer.hpp>

#include "Core/Types.hpp"

namespace ox {
struct Vertex {
  float3 position;
  uint32 normal;
  float2 uv;
};

inline auto vertex_pack = vuk::Packed{
  vuk::Format::eR32Sfloat, // 4 postition x
  vuk::Format::eR32Sfloat, // 4 postition y
  vuk::Format::eR32Sfloat, // 4 postition z
  vuk::Format::eR32Uint, // 4 normal
  vuk::Format::eR32Sfloat, // 4 uv x
  vuk::Format::eR32Sfloat, // 4 uv y
};
}
