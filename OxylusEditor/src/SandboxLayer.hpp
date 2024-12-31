#pragma once
#include "Core/Layer.hpp"
#include "Render/Camera.hpp"
#include "UI/RuntimeConsole.hpp"
#include "Utils/EditorConfig.hpp"

namespace ox {
class Scene;
class SandboxLayer : public Layer {
public:
  SandboxLayer() : Layer("SandboxLayer") {}
  void on_attach(EventDispatcher& dispatcher) override;
  void on_detach() override;
  void on_update(const Timestep& delta_time) override;
  void on_imgui_render() override;

private:
  EditorConfig editor_config = {};
  RuntimeConsole runtime_console = {};
  Shared<Scene> editor_scene;

  Camera camera;
  float _translation_dampening = 0.6f;
  float _rotation_dampening = 0.3f;
  bool _use_editor_camera = true;
  bool _using_editor_camera = false;
  Vec2 _locked_mouse_position = Vec2(0.0f);
  Vec3 _translation_velocity = Vec3(0);
  Vec2 _rotation_velocity = Vec2(0);
};
} // namespace ox
