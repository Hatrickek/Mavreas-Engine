#include "ViewportPanel.hpp"

#include <icons/IconsMaterialDesignIcons.h>

#include "Core/FileSystem.hpp"
#include "EditorLayer.hpp"
#include "ImGuizmo.h"
#include "Render/Camera.hpp"
#include "Scene/Components.hpp"
#include "UI/ImGuiLayer.hpp"
#include "Utils/EditorConfig.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "Render/DebugRenderer.hpp"
#include "Render/RenderPipeline.hpp"
#include "Render/RendererConfig.hpp"
#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"

#include "Scene/SceneRenderer.hpp"

#include "Thread/TaskScheduler.hpp"

#include "UI/OxUI.hpp"

#include "Utils/OxMath.hpp"
#include "Utils/StringUtils.hpp"
#include "Utils/Timestep.hpp"
#include "imgui.h"

namespace ox {
ViewportPanel::ViewportPanel() : EditorPanel("Viewport", ICON_MDI_TERRAIN, true) {
  OX_SCOPED_ZONE;
  gizmo_image_map[typeid(LightComponent).hash_code()] = create_shared<Texture>(TextureLoadInfo{
    .path = "Resources/Icons/PointLightIcon.png",
    .preset = Preset::eRTT2DUnmipped,
  });
  gizmo_image_map[typeid(CameraComponent).hash_code()] = create_shared<Texture>(TextureLoadInfo{
    .path = "Resources/Icons/CameraIcon.png",
    .preset = Preset::eRTT2DUnmipped,
  });
}

void ViewportPanel::on_imgui_render() {
  draw_performance_overlay();

  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});

  if (on_begin(flags)) {
    bool viewport_settings_popup = false;
    ImVec2 start_cursor_pos = ImGui::GetCursorPos();

    const auto popup_item_spacing = ImGuiLayer::popup_item_spacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, popup_item_spacing);
    if (ImGui::BeginPopupContextItem("RightClick")) {
      if (ImGui::MenuItem("Fullscreen"))
        fullscreen_viewport = !fullscreen_viewport;
      ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    if (ImGui::BeginMenuBar()) {
      if (ImGui::MenuItem(StringUtils::from_char8_t(ICON_MDI_COGS))) {
        viewport_settings_popup = true;
      }
      ImGui::EndMenuBar();
    }

    if (viewport_settings_popup)
      ImGui::OpenPopup("ViewportSettings");

    ImGui::SetNextWindowSize({300.f, 0.f});
    if (ImGui::BeginPopup("ViewportSettings")) {
      ui::begin_properties();
      ui::property("VSync", (bool*)RendererCVar::cvar_vsync.get_ptr());
      ui::property<float>("Camera sensitivity", EditorCVar::cvar_camera_sens.get_ptr(), 0.1f, 20.0f);
      ui::property<float>("Movement speed", EditorCVar::cvar_camera_speed.get_ptr(), 5, 100.0f);
      ui::property("Smooth camera", (bool*)EditorCVar::cvar_camera_smooth.get_ptr());
      ui::property("Camera zoom", EditorCVar::cvar_camera_zoom.get_ptr(), 1, 100);
      ui::property<float>("Grid distance", RendererCVar::cvar_draw_grid_distance.get_ptr(), 10.f, 100.0f);
      ui::end_properties();
      ImGui::EndPopup();
    }

    const auto viewport_min_region = ImGui::GetWindowContentRegionMin();
    const auto viewport_max_region = ImGui::GetWindowContentRegionMax();
    viewport_position = Vec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);
    viewport_bounds[0] = {viewport_min_region.x + viewport_position.x, viewport_min_region.y + viewport_position.y};
    viewport_bounds[1] = {viewport_max_region.x + viewport_position.x, viewport_max_region.y + viewport_position.y};

    is_viewport_focused = ImGui::IsWindowFocused();
    is_viewport_hovered = ImGui::IsWindowHovered();

    viewport_panel_size = Vec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    if ((int)m_viewport_size.x != (int)viewport_panel_size.x || (int)m_viewport_size.y != (int)viewport_panel_size.y) {
      m_viewport_size = {viewport_panel_size.x, viewport_panel_size.y};
    }

    constexpr auto sixteen_nine_ar = 1.7777777f;
    const auto fixed_width = m_viewport_size.y * sixteen_nine_ar;
    ImGui::SetCursorPosX((viewport_panel_size.x - fixed_width) * 0.5f);

    const auto extent = vuk::Extent3D((uint32_t)fixed_width, (uint32_t)viewport_panel_size.y, 1);
    const auto rp = context->get_renderer()->get_render_pipeline();

    const auto off = (viewport_panel_size.x - fixed_width) * 0.5f; // add offset since we render image with fixed aspect ratio
    viewport_offset = {viewport_bounds[0].x + off * 0.5f, viewport_bounds[0].y};

    rp->detach_swapchain(extent, viewport_offset);
    vuk::Value<vuk::ImageAttachment>* final_image = rp->get_final_image();

    if (final_image) {
      ui::image(*final_image, ImVec2{fixed_width, viewport_panel_size.y});
    } else {
      const auto text_width = ImGui::CalcTextSize("No render target!").x;
      ImGui::SetCursorPosX((m_viewport_size.x - text_width) * 0.5f);
      ImGui::SetCursorPosY(m_viewport_size.y * 0.5f);
      ImGui::Text("No render target!");
    }

    if (m_scene_hierarchy_panel)
      m_scene_hierarchy_panel->drag_drop_target();

    if (!context->is_running()) {
      auto projection = m_camera.get_projection_matrix();
      projection[1][1] *= -1;
      Mat4 view_proj = projection * m_camera.get_view_matrix();
      const Frustum& frustum = m_camera.get_frustum();

      show_component_gizmo<LightComponent>(fixed_width, viewport_panel_size.y, 0, 0, view_proj, frustum, context.get());
      show_component_gizmo<AudioSourceComponent>(fixed_width, viewport_panel_size.y, 0, 0, view_proj, frustum, context.get());
      show_component_gizmo<AudioListenerComponent>(fixed_width, viewport_panel_size.y, 0, 0, view_proj, frustum, context.get());
      show_component_gizmo<CameraComponent>(fixed_width, viewport_panel_size.y, 0, 0, view_proj, frustum, context.get());

      draw_gizmos();
    }

    {
      // Transform Gizmos Button Group
      const float frame_height = 1.3f * ImGui::GetFrameHeight();
      const ImVec2 frame_padding = ImGui::GetStyle().FramePadding;
      const ImVec2 button_size = {frame_height, frame_height};
      constexpr float button_count = 8.0f;
      const ImVec2 gizmo_position = {viewport_bounds[0].x + m_gizmo_position.x, viewport_bounds[0].y + m_gizmo_position.y};
      const ImRect bb(gizmo_position.x,
                      gizmo_position.y,
                      gizmo_position.x + button_size.x + 8,
                      gizmo_position.y + (button_size.y + 2) * (button_count + 0.5f));
      ImVec4 frame_color = ImGui::GetStyleColorVec4(ImGuiCol_Tab);
      frame_color.w = 0.5f;
      ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(frame_color), false, ImGui::GetStyle().FrameRounding);
      const Vec2 temp_gizmo_position = m_gizmo_position;

      ImGui::SetCursorPos({start_cursor_pos.x + temp_gizmo_position.x + frame_padding.x, start_cursor_pos.y + temp_gizmo_position.y});
      ImGui::BeginGroup();
      {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {1, 1});

        const ImVec2 dragger_cursor_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPosX(dragger_cursor_pos.x + frame_padding.x);
        ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_DOTS_HORIZONTAL));
        ImVec2 dragger_size = ImGui::CalcTextSize(StringUtils::from_char8_t(ICON_MDI_DOTS_HORIZONTAL));
        dragger_size.x *= 2.0f;
        ImGui::SetCursorPos(dragger_cursor_pos);
        ImGui::InvisibleButton("GizmoDragger", dragger_size);
        static ImVec2 last_mouse_position = ImGui::GetMousePos();
        const ImVec2 mouse_pos = ImGui::GetMousePos();
        if (ImGui::IsItemActive()) {
          m_gizmo_position.x += mouse_pos.x - last_mouse_position.x;
          m_gizmo_position.y += mouse_pos.y - last_mouse_position.y;
        }
        last_mouse_position = mouse_pos;

        constexpr float alpha = 0.6f;
        if (ui::toggle_button(StringUtils::from_char8_t(ICON_MDI_AXIS_ARROW), m_gizmo_type == ImGuizmo::TRANSLATE, button_size, alpha, alpha))
          m_gizmo_type = ImGuizmo::TRANSLATE;
        if (ui::toggle_button(StringUtils::from_char8_t(ICON_MDI_ROTATE_3D), m_gizmo_type == ImGuizmo::ROTATE, button_size, alpha, alpha))
          m_gizmo_type = ImGuizmo::ROTATE;
        if (ui::toggle_button(StringUtils::from_char8_t(ICON_MDI_ARROW_EXPAND), m_gizmo_type == ImGuizmo::SCALE, button_size, alpha, alpha))
          m_gizmo_type = ImGuizmo::SCALE;
        if (ui::toggle_button(StringUtils::from_char8_t(ICON_MDI_VECTOR_SQUARE), m_gizmo_type == ImGuizmo::BOUNDS, button_size, alpha, alpha))
          m_gizmo_type = ImGuizmo::BOUNDS;
        if (ui::toggle_button(StringUtils::from_char8_t(ICON_MDI_ARROW_EXPAND_ALL), m_gizmo_type == ImGuizmo::UNIVERSAL, button_size, alpha, alpha))
          m_gizmo_type = ImGuizmo::UNIVERSAL;
        if (ui::toggle_button(m_gizmo_mode == ImGuizmo::WORLD ? StringUtils::from_char8_t(ICON_MDI_EARTH)
                                                              : StringUtils::from_char8_t(ICON_MDI_EARTH_OFF),
                              m_gizmo_mode == ImGuizmo::WORLD,
                              button_size,
                              alpha,
                              alpha))
          m_gizmo_mode = m_gizmo_mode == ImGuizmo::LOCAL ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        if (ui::toggle_button(StringUtils::from_char8_t(ICON_MDI_GRID), RendererCVar::cvar_draw_grid.get(), button_size, alpha, alpha))
          RendererCVar::cvar_draw_grid.toggle();
        if (ui::toggle_button(StringUtils::from_char8_t(ICON_MDI_CAMERA),
                              m_camera.get_projection() == Camera::Projection::Orthographic,
                              button_size,
                              alpha,
                              alpha))
          m_camera.set_projection(m_camera.get_projection() == Camera::Projection::Orthographic ? Camera::Projection::Perspective
                                                                                                : Camera::Projection::Orthographic);

        ImGui::PopStyleVar(2);
      }
      ImGui::EndGroup();
    }

    {
      // Scene Button Group
      constexpr float button_count = 3.0f;
      constexpr float y_pad = 3.0f;
      const ImVec2 button_size = {35.f, 25.f};
      const ImVec2 group_size = {button_size.x * button_count, button_size.y + y_pad};

      ImGui::SetCursorPos({m_viewport_size.x * 0.5f - (group_size.x * 0.5f), start_cursor_pos.y + y_pad});
      ImGui::BeginGroup();
      {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {1, 1});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.0f);

        const bool highlight = EditorLayer::get()->scene_state == EditorLayer::SceneState::Play;
        const char8_t* icon = EditorLayer::get()->scene_state == EditorLayer::SceneState::Edit ? ICON_MDI_PLAY : ICON_MDI_STOP;
        if (ui::toggle_button(StringUtils::from_char8_t(icon), highlight, button_size)) {
          if (EditorLayer::get()->scene_state == EditorLayer::SceneState::Edit)
            EditorLayer::get()->on_scene_play();
          else if (EditorLayer::get()->scene_state == EditorLayer::SceneState::Play)
            EditorLayer::get()->on_scene_stop();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.4f));
        if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_PAUSE), button_size)) {
          if (EditorLayer::get()->scene_state == EditorLayer::SceneState::Play)
            EditorLayer::get()->on_scene_stop();
        }
        ImGui::SameLine();
        if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_STEP_FORWARD), button_size)) {
          EditorLayer::get()->on_scene_simulate();
        }
        ImGui::PopStyleColor();

        ImGui::PopStyleVar(3);
      }
      ImGui::EndGroup();
    }

    ImGui::PopStyleVar();
    on_end();
  }
}

void ViewportPanel::set_context(const Shared<Scene>& scene, SceneHierarchyPanel& scene_hierarchy_panel) {
  m_scene_hierarchy_panel = &scene_hierarchy_panel;
  context = scene;
}

void ViewportPanel::on_update() {
  if (is_viewport_hovered && !context->is_running() && m_use_editor_camera) {
    const Vec3& position = m_camera.get_position();
    const Vec2 yaw_pitch = Vec2(m_camera.get_yaw(), m_camera.get_pitch());
    Vec3 final_position = position;
    Vec2 final_yaw_pitch = yaw_pitch;

    const auto is_ortho = m_camera.get_projection() == Camera::Projection::Orthographic;
    if (is_ortho) {
      final_position = {0.0f, 0.0f, 0.0f};
      final_yaw_pitch = {glm::radians(-90.f), 0.f};
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && !is_ortho) {
      const Vec2 new_mouse_position = Input::get_mouse_position();

      if (!m_using_editor_camera) {
        m_using_editor_camera = true;
        m_locked_mouse_position = new_mouse_position;
        Input::set_cursor_state(Input::CursorState::Disabled);
      }

      Input::set_mouse_position(m_locked_mouse_position.x, m_locked_mouse_position.y);
      // Input::SetCursorIcon(EditorLayer::Get()->m_CrosshairCursor);

      const Vec2 change = (new_mouse_position - m_locked_mouse_position) * EditorCVar::cvar_camera_sens.get();
      final_yaw_pitch.x += change.x;
      final_yaw_pitch.y = glm::clamp(final_yaw_pitch.y - change.y, glm::radians(-89.9f), glm::radians(89.9f));

      const float max_move_speed = EditorCVar::cvar_camera_speed.get() * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : 1.0f);
      if (ImGui::IsKeyDown(ImGuiKey_W))
        final_position += m_camera.get_forward() * max_move_speed;
      else if (ImGui::IsKeyDown(ImGuiKey_S))
        final_position -= m_camera.get_forward() * max_move_speed;
      if (ImGui::IsKeyDown(ImGuiKey_D))
        final_position += m_camera.get_right() * max_move_speed;
      else if (ImGui::IsKeyDown(ImGuiKey_A))
        final_position -= m_camera.get_right() * max_move_speed;

      if (ImGui::IsKeyDown(ImGuiKey_Q)) {
        final_position.y -= max_move_speed;
      } else if (ImGui::IsKeyDown(ImGuiKey_E)) {
        final_position.y += max_move_speed;
      }
    }
    // Panning
    else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
      const Vec2 new_mouse_position = Input::get_mouse_position();

      if (!m_using_editor_camera) {
        m_using_editor_camera = true;
        m_locked_mouse_position = new_mouse_position;
      }

      Input::set_mouse_position(m_locked_mouse_position.x, m_locked_mouse_position.y);
      // Input::SetCursorIcon(EditorLayer::Get()->m_CrosshairCursor);

      const Vec2 change = (new_mouse_position - m_locked_mouse_position) * EditorCVar::cvar_camera_sens.get();

      const float max_move_speed = EditorCVar::cvar_camera_speed.get() * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : 1.0f);
      final_position += m_camera.get_forward() * change.y * max_move_speed;
      final_position += m_camera.get_right() * change.x * max_move_speed;
    } else {
      // Input::SetCursorIconDefault();
      Input::set_cursor_state(Input::CursorState::Normal);
      m_using_editor_camera = false;
    }

    const Vec3 damped_position = math::smooth_damp(position,
                                                   final_position,
                                                   m_translation_velocity,
                                                   m_translation_dampening,
                                                   10000.0f,
                                                   (float)App::get_timestep().get_seconds());
    const Vec2 damped_yaw_pitch = math::smooth_damp(yaw_pitch,
                                                    final_yaw_pitch,
                                                    m_rotation_velocity,
                                                    m_rotation_dampening,
                                                    1000.0f,
                                                    (float)App::get_timestep().get_seconds());

    m_camera.set_position(EditorCVar::cvar_camera_smooth.get() ? damped_position : final_position);
    m_camera.set_yaw(EditorCVar::cvar_camera_smooth.get() ? damped_yaw_pitch.x : final_yaw_pitch.x);
    m_camera.set_pitch(EditorCVar::cvar_camera_smooth.get() ? damped_yaw_pitch.y : final_yaw_pitch.y);
    m_camera.set_zoom(EditorCVar::cvar_camera_zoom.get());

    m_camera.update();
  }
}

void ViewportPanel::draw_performance_overlay() {
  if (!performance_overlay_visible)
    return;
  ui::draw_framerate_overlay(ImVec2(viewport_position.x, viewport_position.y),
                             ImVec2(viewport_panel_size.x, viewport_panel_size.y),
                             {15, 55},
                             &performance_overlay_visible);
}

void ViewportPanel::draw_gizmos() {
  const Entity selected_entity = m_scene_hierarchy_panel->get_selected_entity();
  auto tc = context->registry.try_get<TransformComponent>(selected_entity);
  if (selected_entity != entt::null && m_gizmo_type != -1 && tc) {
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewport_bounds[0].x,
                      viewport_bounds[0].y,
                      viewport_bounds[1].x - viewport_bounds[0].x,
                      viewport_bounds[1].y - viewport_bounds[0].y);

    auto camera_projection = m_camera.get_projection_matrix();
    camera_projection[1][1] *= -1;

    const Mat4& camera_view = m_camera.get_view_matrix();

    Mat4 transform = eutil::get_world_transform(context.get(), selected_entity);

    // Snapping
    const bool snap = Input::get_key_held(KeyCode::LeftControl);
    float snap_value = 0.5f; // Snap to 0.5m for translation/scale
    // Snap to 45 degrees for rotation
    if (m_gizmo_type == ImGuizmo::OPERATION::ROTATE)
      snap_value = 45.0f;

    const float snap_values[3] = {snap_value, snap_value, snap_value};

    const auto is_ortho = m_camera.get_projection() == Camera::Projection::Orthographic;
    ImGuizmo::SetOrthographic(is_ortho);
    if (m_gizmo_mode == ImGuizmo::OPERATION::TRANSLATE && is_ortho)
      m_gizmo_mode = ImGuizmo::OPERATION::TRANSLATE_X | ImGuizmo::OPERATION::TRANSLATE_Y;

    ImGuizmo::Manipulate(value_ptr(camera_view),
                         value_ptr(camera_projection),
                         static_cast<ImGuizmo::OPERATION>(m_gizmo_type),
                         static_cast<ImGuizmo::MODE>(m_gizmo_mode),
                         value_ptr(transform),
                         nullptr,
                         snap ? snap_values : nullptr);

    if (ImGuizmo::IsUsing()) {
      const Entity parent = eutil::get_parent(context.get(), selected_entity);
      const Mat4& parent_world_transform = parent != entt::null ? eutil::get_world_transform(context.get(), parent) : Mat4(1.0f);
      Vec3 translation, rotation, scale;
      if (math::decompose_transform(inverse(parent_world_transform) * transform, translation, rotation, scale)) {
        tc->position = translation;
        const Vec3 delta_rotation = rotation - tc->rotation;
        tc->rotation += delta_rotation;
        tc->scale = scale;
      }
    }
  }
  if (Input::get_key_held(KeyCode::LeftControl)) {
    if (Input::get_key_pressed(KeyCode::Q)) {
      if (!ImGuizmo::IsUsing())
        m_gizmo_type = -1;
    }
    if (Input::get_key_pressed(KeyCode::W)) {
      if (!ImGuizmo::IsUsing())
        m_gizmo_type = ImGuizmo::OPERATION::TRANSLATE;
    }
    if (Input::get_key_pressed(KeyCode::E)) {
      if (!ImGuizmo::IsUsing())
        m_gizmo_type = ImGuizmo::OPERATION::ROTATE;
    }
    if (Input::get_key_pressed(KeyCode::R)) {
      if (!ImGuizmo::IsUsing())
        m_gizmo_type = ImGuizmo::OPERATION::SCALE;
    }
  }
}
} // namespace ox
