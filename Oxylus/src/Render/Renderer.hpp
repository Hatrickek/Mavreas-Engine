#pragma once

#include "Scene/Components.hpp"

#include "Render/RenderPipeline.hpp"
#include "Render/RendererConfig.hpp"

namespace ox {
class DefaultRenderPipeline;
class LayerStack;
class ImGuiLayer;
class VkContext;

class Renderer {
public:
  static struct RendererContext {
    // TODO:
    // need to get rid of this and instead use the render pipeline from scenes directly.
    // currently this gets filled at scene creation
    Shared<RenderPipeline> render_pipeline = nullptr;
    Shared<vuk::Compiler> compiler = nullptr;
    vuk::Extent3D viewport_size = {1600, 900, 1};
    Vec2 viewport_offset = {};
  } renderer_context;

  static void init();
  static void deinit();

  static void draw(VkContext* vkctx, ImGuiLayer* imgui_layer, LayerStack& layer_stack);

  static UVec2 get_viewport_size() { return {renderer_context.viewport_size.width, renderer_context.viewport_size.height}; }
  static Vec2 get_viewport_offset() { return renderer_context.viewport_offset; }
  static unsigned get_viewport_width() { return renderer_context.viewport_size.width; }
  static unsigned get_viewport_height() { return renderer_context.viewport_size.height; }

private:
  // Config
  static RendererConfig renderer_config;
};
} // namespace ox
