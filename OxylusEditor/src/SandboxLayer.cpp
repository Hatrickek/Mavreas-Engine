#include "SandboxLayer.hpp"

#include "Core/Project.hpp"
#include "EditorLayer.hpp"
#include "EditorTheme.hpp"
#include "Render/RendererConfig.hpp"

namespace ox {
void SandboxLayer::on_attach(EventDispatcher& dispatcher) {
  EditorTheme::init();
  // Window::maximize();
  Project::create_new();

  editor_config.load_config();

  camera.set_position({0, 2, 0});

  editor_scene = create_shared<Scene>();
  EditorLayer::load_default_scene(editor_scene);
}

void SandboxLayer::on_detach() {}

void SandboxLayer::on_update(const Timestep& delta_time) {
  if (_use_editor_camera && !ImGui::GetIO().WantCaptureMouse) {
    const Vec3& position = camera.get_position();
    const Vec2 yaw_pitch = Vec2(camera.get_yaw(), camera.get_pitch());
    Vec3 final_position = position;
    Vec2 final_yaw_pitch = yaw_pitch;

    if (Input::get_mouse_held(MouseCode::Button1)) {
      const Vec2 new_mouse_position = Input::get_mouse_position();

      if (!_using_editor_camera) {
        _using_editor_camera = true;
        _locked_mouse_position = new_mouse_position;
        Input::set_cursor_state(Input::CursorState::Disabled);
      }

      Input::set_mouse_position(_locked_mouse_position.x, _locked_mouse_position.y);

      const Vec2 change = (new_mouse_position - _locked_mouse_position) * EditorCVar::cvar_camera_sens.get();
      final_yaw_pitch.x += change.x;
      final_yaw_pitch.y = glm::clamp(final_yaw_pitch.y - change.y, glm::radians(-89.9f), glm::radians(89.9f));

      const float max_move_speed = EditorCVar::cvar_camera_speed.get() * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : 1.0f);
      if (Input::get_key_held(KeyCode::W))
        final_position += camera.get_forward() * max_move_speed;
      else if (Input::get_key_held(KeyCode::S))
        final_position -= camera.get_forward() * max_move_speed;
      if (Input::get_key_held(KeyCode::D))
        final_position += camera.get_right() * max_move_speed;
      else if (Input::get_key_held(KeyCode::A))
        final_position -= camera.get_right() * max_move_speed;

      if (Input::get_key_held(KeyCode::Q)) {
        final_position.y -= max_move_speed;
      } else if (Input::get_key_held(KeyCode::E)) {
        final_position.y += max_move_speed;
      }
    }
    // Panning
    else if (Input::get_mouse_held(MouseCode::Button2)) {
      const Vec2 new_mouse_position = Input::get_mouse_position();

      if (!_using_editor_camera) {
        _using_editor_camera = true;
        _locked_mouse_position = new_mouse_position;
      }

      Input::set_mouse_position(_locked_mouse_position.x, _locked_mouse_position.y);

      const Vec2 change = (new_mouse_position - _locked_mouse_position) * EditorCVar::cvar_camera_sens.get();

      const float max_move_speed = EditorCVar::cvar_camera_speed.get() * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : 1.0f);
      final_position += camera.get_forward() * change.y * max_move_speed;
      final_position += camera.get_right() * change.x * max_move_speed;
    } else {
      Input::set_cursor_state(Input::CursorState::Normal);
      _using_editor_camera = false;
    }

    const Vec3 damped_position = math::smooth_damp(position,
                                                   final_position,
                                                   _translation_velocity,
                                                   _translation_dampening,
                                                   10000.0f,
                                                   (float)App::get_timestep().get_seconds());
    const Vec2 damped_yaw_pitch = math::smooth_damp(yaw_pitch,
                                                    final_yaw_pitch,
                                                    _rotation_velocity,
                                                    _rotation_dampening,
                                                    1000.0f,
                                                    (float)App::get_timestep().get_seconds());

    camera.set_position(EditorCVar::cvar_camera_smooth.get() ? damped_position : final_position);
    camera.set_yaw(EditorCVar::cvar_camera_smooth.get() ? damped_yaw_pitch.x : final_yaw_pitch.x);
    camera.set_pitch(EditorCVar::cvar_camera_smooth.get() ? damped_yaw_pitch.y : final_yaw_pitch.y);

    camera.update();
  }

  editor_scene->on_editor_update(delta_time, camera);
}

void SandboxLayer::on_imgui_render() {
  if (EditorCVar::cvar_show_style_editor.get())
    ImGui::ShowStyleEditor();
  if (EditorCVar::cvar_show_imgui_demo.get())
    ImGui::ShowDemoWindow();

  runtime_console.on_imgui_render();
}
} // namespace ox
