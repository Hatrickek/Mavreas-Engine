#pragma once
#include <mutex>
#include <vuk/Value.hpp>

#include "Event/Event.hpp"

#include "Core/Base.hpp"
#include "Scene/Components.hpp"

namespace vuk {
struct SampledImage;
}

namespace ox {
class VkContext;
class Scene;

class RenderPipeline {
public:
  RenderPipeline(std::string name) : _name(std::move(name)), attach_swapchain(true) {}

  virtual ~RenderPipeline() = default;

  virtual void init(vuk::Allocator& allocator) = 0;
  virtual void shutdown() = 0;

  [[nodiscard]] virtual vuk::Value<vuk::ImageAttachment> on_render(vuk::Allocator& frame_allocator,
                                                                   vuk::Value<vuk::ImageAttachment> target,
                                                                   vuk::Extent3D ext) = 0;

  virtual void on_dispatcher_events(EventDispatcher& dispatcher) {}

  virtual void on_update(Scene* scene) {}
  virtual void on_submit() {}
  virtual void submit_mesh_component(const MeshComponent& render_object) {}
  virtual void submit_light(const LightComponent& light) {}
  virtual void submit_camera(Camera* camera) {}
  virtual void submit_sprite(const SpriteComponent& sprite) {}

  virtual void detach_swapchain(vuk::Extent3D ext, Vec2 offset = {});
  virtual bool is_swapchain_attached() { return attach_swapchain; }

  virtual vuk::Value<vuk::ImageAttachment>* get_final_image() { return final_image; }
  virtual void set_final_image(vuk::Value<vuk::ImageAttachment>* image) { final_image = image; }

  virtual vuk::Allocator* get_frame_allocator() { return _frame_allocator; }
  virtual void set_frame_allocator(vuk::Allocator* allocator) { _frame_allocator = allocator; }

  virtual void set_compiler(vuk::Compiler* compiler) { _compiler = compiler; }
  virtual vuk::Compiler* get_compiler() { return _compiler; }

  virtual const std::string& get_name() { return _name; }
  virtual vuk::Extent3D get_extent() { return _extent; }
  virtual Vec2 get_viewport_offset() { return viewport_offset; }

protected:
  std::string _name = {};
  bool attach_swapchain = false;
  vuk::Extent3D _extent = {};
  Vec2 viewport_offset = {};
  vuk::Value<vuk::ImageAttachment>* final_image = nullptr;
  vuk::Allocator* _frame_allocator;
  vuk::Compiler* _compiler = nullptr;
  std::mutex setup_lock;
};
} // namespace ox
