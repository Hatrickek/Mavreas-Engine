#include "RectPacker.hpp"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

namespace ox {
void RectPacker::State::clear() {
  rects.clear();
}

void RectPacker::State::add_rect(const Rect& rect) {
  rects.push_back(rect);
  width = std::max(width, rect.w);
  height = std::max(height, rect.h);
}

bool RectPacker::State::pack(const int max_width) {
  while (width <= max_width || height <= max_width) {
    if ((int)nodes.size() < width) {
      nodes.resize(width);
    }
    stbrp_init_target(&context, width, height, nodes.data(), int(nodes.size()));
    if (stbrp_pack_rects(&context, rects.data(), int(rects.size())))
      return true;
    if (height < width) {
      height *= 2;
    }
    else {
      width *= 2;
    }
  }
  width = 0;
  height = 0;
  return false;
}
}
