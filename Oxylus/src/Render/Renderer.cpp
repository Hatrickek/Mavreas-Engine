#include "Renderer.hpp"

#include <future>
#include <vuk/RenderGraph.hpp>
#include <vuk/vsl/Core.hpp>

#include "Assets/AssetManager.hpp"
#include "Core/LayerStack.hpp"
#include "Render/DebugRenderer.hpp"
#include "Render/DefaultRenderPipeline.hpp"
#include "Render/Mesh.hpp"
#include "Render/Window.hpp"
#include "Thread/TaskScheduler.hpp"
#include "UI/ImGuiLayer.hpp"
#include "Utils/Profiler.hpp"
#include "Vulkan/VkContext.hpp"

namespace ox {
Renderer::RendererContext Renderer::renderer_context;
RendererConfig Renderer::renderer_config;

void Renderer::init() {
  OX_SCOPED_ZONE;

  renderer_context.compiler = create_shared<vuk::Compiler>();

  Texture::create_white_texture();
  DebugRenderer::init();
}

void Renderer::deinit() {
  OX_SCOPED_ZONE;
  DebugRenderer::release();
}

void Renderer::draw(VkContext* vkctx, ImGuiLayer* imgui_layer, LayerStack& layer_stack) {
  OX_SCOPED_ZONE;

  const auto rp = renderer_context.render_pipeline;
  auto extent = rp->is_swapchain_attached() ? vkctx->swapchain->images[vkctx->current_frame].extent : rp->get_extent();

  // consume renderer related cvars
  if (RendererCVar::cvar_reload_render_pipeline.get()) {
    rp->init(*App::get_vkcontext().superframe_allocator);
    RendererCVar::cvar_reload_render_pipeline.toggle();
  }

  if (vkctx->is_vsync() != (bool)RendererCVar::cvar_vsync.get()) {
    vkctx->set_vsync((bool)RendererCVar::cvar_vsync.get());
    vkctx->handle_resize(extent.width, extent.height);
    return;
  }

  auto& frame_resource = vkctx->superframe_resource->get_next_frame();
  vkctx->runtime->next_frame();

  vuk::Allocator frame_allocator(frame_resource);

  auto imported_swapchain = vuk::declare_swapchain(*vkctx->swapchain);
  auto swapchain_image = vuk::acquire_next_image("swp_img", std::move(imported_swapchain));

  vuk::ProfilingCallbacks cbs = vkctx->tracy_profiler->setup_vuk_callback();

  vuk::Value<vuk::ImageAttachment> cleared_image = vuk::clear_image(std::move(swapchain_image), vuk::Black<float>);

  renderer_context.viewport_size = extent;
  renderer_context.viewport_offset = rp->get_viewport_offset();

  rp->set_frame_allocator(&frame_allocator);
  rp->set_compiler(renderer_context.compiler.get());

  if (rp->is_swapchain_attached()) {
    vuk::Value<vuk::ImageAttachment> result = rp->on_render(frame_allocator, cleared_image, extent);

    imgui_layer->begin();
    for (const auto& layer : layer_stack)
      layer->on_imgui_render();
    for (auto& [_, system] : App::get_system_registry())
      system->imgui_update();
    imgui_layer->end();

    auto imgui_output = imgui_layer->render_draw_data(frame_allocator, *renderer_context.compiler, result);

    auto entire_thing = vuk::enqueue_presentation(std::move(imgui_output));
    entire_thing.submit(frame_allocator, *renderer_context.compiler, {.callbacks = cbs});
  } else {
    auto att = vuk::ImageAttachment::from_preset(Preset::eRTT2DUnmipped,
                                                 vkctx->swapchain->images[vkctx->current_frame].format,
                                                 extent,
                                                 vuk::Samples::e1);
    auto viewport_img = vuk::clear_image(vuk::declare_ia("viewport_img", att), vuk::Black<float>);

    vuk::Value<vuk::ImageAttachment> viewport_result = rp->on_render(frame_allocator, viewport_img, extent);
    rp->set_final_image(&viewport_result);

    imgui_layer->begin();
    for (const auto& layer : layer_stack)
      layer->on_imgui_render();
    for (auto& [_, system] : App::get_system_registry())
      system->imgui_update();
    imgui_layer->end();

    vuk::Value<vuk::ImageAttachment> result = imgui_layer->render_draw_data(frame_allocator, *renderer_context.compiler, cleared_image);

    auto entire_thing = vuk::enqueue_presentation(std::move(result));
    entire_thing.submit(frame_allocator, *renderer_context.compiler, {.callbacks = cbs});
  }

  rp->on_submit();

  vkctx->current_frame = (vkctx->current_frame + 1) % vkctx->num_inflight_frames;
  vkctx->num_frames = vkctx->runtime->get_frame_count();
}
} // namespace ox
