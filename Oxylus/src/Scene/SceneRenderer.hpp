#pragma once
#include "Components.hpp"

namespace ox {
class RenderPipeline;
class Scene;

class SceneRenderer {
public:
  SceneRenderer(Scene* scene) : _scene(scene) {}
  ~SceneRenderer() = default;

  void init(EventDispatcher& dispatcher);
  void update(const Timestep& delta_time) const;

  Shared<RenderPipeline> get_render_pipeline() const { return _render_pipeline; }
  void set_render_pipeline(const Shared<RenderPipeline>& render_pipeline) { _render_pipeline = render_pipeline;}

private:
  Scene* _scene;
  Shared<RenderPipeline> _render_pipeline = nullptr;

  friend class Scene;
};
}
