#include "SceneRenderer.hpp"

#include "Physics/Physics.hpp"
#include "Render/RendererConfig.hpp"
#include "Scene.hpp"

#include "Core/App.hpp"

#include "Jolt/Physics/Body/Body.h"

#include "Entity.hpp"

#include "Render/DebugRenderer.hpp"
#include "Render/DefaultRenderPipeline.hpp"
#include "Render/Renderer.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Scene/Components.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
void SceneRenderer::init(EventDispatcher& dispatcher) {
  OX_SCOPED_ZONE;
  if (!_render_pipeline)
    _render_pipeline = create_shared<DefaultRenderPipeline>("DefaultRenderPipeline");
  Renderer::renderer_context.render_pipeline = _render_pipeline;
  _render_pipeline->init(*App::get_vkcontext().superframe_allocator);
  _render_pipeline->on_dispatcher_events(dispatcher);
}

void SceneRenderer::update(const Timestep& delta_time) const {
  OX_SCOPED_ZONE;

  // Mesh System
  {
    OX_SCOPED_ZONE_N("Mesh System");
    const auto mesh_view = _scene->registry.view<TransformComponent, MeshComponent, TagComponent>();
    for (const auto&& [entity, transform, mesh_component, tag] : mesh_view.each()) {
      if (!tag.enabled)
        continue;

      if (!mesh_component.stationary || mesh_component.dirty) {
        const auto world_transform = eutil::get_world_transform(_scene, entity);
        mesh_component.transform = world_transform;
        for (auto& e : mesh_component.child_entities) {
          mesh_component.child_transforms.emplace_back(eutil::get_world_transform(_scene, e));
        }

        mesh_component.dirty = false;
      }

      _render_pipeline->submit_mesh_component(mesh_component);
    }
  }

  // Sprite animation system
  {
    OX_SCOPED_ZONE_N("Sprite Animation System");
    const auto sprite_view = _scene->registry.view<TransformComponent, SpriteComponent, SpriteAnimationComponent, TagComponent>();
    for (const auto&& [entity, transform, sprite, sprite_animation, tag] : sprite_view.each()) {
      if (!tag.enabled || sprite_animation.num_frames < 1 || sprite_animation.fps < 1 || sprite_animation.columns < 1 ||
          sprite.material->parameters.albedo_map_id == Asset::INVALID_ID)
        continue;

      const auto dt = glm::clamp((float)delta_time.get_seconds(), 0.0f, 0.25f);
      const auto time = sprite_animation.current_time + dt;

      sprite_animation.current_time = time;

      const float duration = float(sprite_animation.num_frames) / sprite_animation.fps;
      uint32 frame = math::flooru32(sprite_animation.num_frames * (time / duration));

      if (time > duration) {
        if (sprite_animation.inverted) {
          sprite_animation.is_inverted = sprite_animation.inverted ? !sprite_animation.is_inverted : false;
          // Remove/add a frame depending on the direction
          const float frame_length = 1.0f / sprite_animation.fps;
          sprite_animation.current_time -= duration - frame_length;
        } else {
          sprite_animation.current_time -= duration;
        }
      }

      if (sprite_animation.loop)
        frame %= sprite_animation.num_frames;
      else
        frame = glm::min(frame, (uint32)sprite_animation.num_frames - 1);

      frame = sprite_animation.is_inverted ? sprite_animation.num_frames - 1 - frame : frame;

      uint32 frame_x = frame % sprite_animation.columns;
      uint32 frame_y = frame / sprite_animation.columns;

      const auto& mat = sprite.material;
      const auto& texture = mat->get_albedo_texture();
      auto& uv_size = mat->parameters.uv_size;
      const auto& uv_offset = mat->parameters.uv_offset;

      auto texture_size = float2(texture->get_extent().width, texture->get_extent().height);
      uv_size = {sprite_animation.frame_size[0] * 1.f / texture_size[0], sprite_animation.frame_size[1] * 1.f / texture_size[1]};
      sprite.current_uv_offset = uv_offset + float2{uv_size.x * frame_x, uv_size.y * frame_y};
    }
  }

  // Sprite System
  {
    OX_SCOPED_ZONE_N("Sprite System");
    const auto sprite_view = _scene->registry.view<TransformComponent, SpriteComponent, TagComponent>();
    for (const auto&& [entity, transform, sprite, tag] : sprite_view.each()) {
      if (!tag.enabled)
        continue;

      const auto world_transform = eutil::get_world_transform(_scene, entity);
      sprite.transform = world_transform;
      sprite.rect = AABB(float3(-0.5, -0.5, -0.5), float3(0.5, 0.5, 0.5));
      sprite.rect = sprite.rect.get_transformed(world_transform);

      _render_pipeline->submit_sprite(sprite);

      if (RendererCVar::cvar_draw_bounding_boxes.get()) {
        DebugRenderer::draw_aabb(sprite.rect, Vec4(1, 1, 1, 1.0f));
      }
    }
  }

  // Tilemaps
  {
    OX_SCOPED_ZONE_N("Tilemap System");
    const auto tilemap_view = _scene->registry.view<TransformComponent, TilemapComponent, TagComponent>();
    for (const auto&& [entity, transform, tilemap, tag] : tilemap_view.each()) {
      if (!tag.enabled)
        continue;

      // TODO: Tilemaps can also be just submitted as sprites into the renderer until we have the layering system.
      SpriteComponent sprite{};
      if (!tilemap.layers.empty())
        sprite.material = tilemap.layers.begin()._Unwrapped()->second;
      sprite.sort_y = false;

      // FIXME: don't care about parents for now
      transform.scale = {tilemap.tilemap_size, 1};
      sprite.transform = transform.get_local_transform();
      sprite.rect = AABB(float3(-0.5, -0.5, -0.5), float3(0.5, 0.5, 0.5));
      sprite.rect = sprite.rect.get_transformed(sprite.transform);

      _render_pipeline->submit_sprite(sprite);
    }
  }

  // Physics debug renderer
  {
    if (RendererCVar::cvar_enable_physics_debug_renderer.get()) {
      auto physics = App::get_system<Physics>();
      physics->debug_draw();
    }
  }

  // Lighting
  {
    OX_SCOPED_ZONE_N("Lighting System");
    const auto lighting_view = _scene->registry.view<TransformComponent, LightComponent>();
    for (auto&& [e, tc, lc] : lighting_view.each()) {
      if (!_scene->registry.get<TagComponent>(e).enabled)
        continue;
      lc.position = tc.position;
      lc.rotation = tc.rotation;
      lc.direction = normalize(math::transform_normal(Vec4(0, 1, 0, 0), toMat4(glm::quat(tc.rotation))));

      _render_pipeline->submit_light(lc);
    }
  }

  // TODO: (very outdated, currently not working)
  // Particle system
  const auto particle_system_view = _scene->registry.view<TransformComponent, ParticleSystemComponent>();
  for (auto&& [e, tc, psc] : particle_system_view.each()) {
    psc.system->on_update((float)App::get_timestep(), tc.position);
    psc.system->on_render();
  }
}
} // namespace ox
