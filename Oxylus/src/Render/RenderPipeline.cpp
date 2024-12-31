#include "RenderPipeline.hpp"

#include "Utils/Log.hpp"

namespace ox {
void RenderPipeline::detach_swapchain(const vuk::Extent3D dim, Vec2 offset) {
  attach_swapchain = false;
  OX_CHECK_GT(dim.width, 0u);
  OX_CHECK_GT(dim.height, 0u);
  _extent = dim;
  viewport_offset = offset;
}
}
