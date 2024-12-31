#include "InspectorPanel.hpp"

#include <icons/IconsMaterialDesignIcons.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Assets/AssetManager.hpp"
#include "Assets/TilemapSerializer.hpp"

#include "Core/Systems/SystemManager.hpp"
#include "Scene/Components.hpp"
#include "Utils/ColorUtils.hpp"

#include "Core/FileSystem.hpp"
#include "EditorLayer.hpp"
#include "EditorTheme.hpp"
#include "Scene/Entity.hpp"
#include "UI/OxUI.hpp"
#include "Utils/FileDialogs.hpp"
#include "Utils/StringUtils.hpp"

#include "Scene/SceneRenderer.hpp"

namespace ox {
static bool s_rename_entity = false;

InspectorPanel::InspectorPanel() : EditorPanel("Inspector", ICON_MDI_INFORMATION, true), context(nullptr) {}

void InspectorPanel::on_imgui_render() {
  selected_entity = EditorLayer::get()->get_selected_entity();
  context = EditorLayer::get()->get_selected_scene().get();

  on_begin();
  if (selected_entity != entt::null) {
    draw_components(selected_entity);
  }

  // OxUI::draw_gradient_shadow_bottom();

  on_end();
}

template <typename T, typename UIFunction>
void draw_component(const char* name, entt::registry& reg, Entity entity, UIFunction ui_function, const bool removable = true) {
  auto component = reg.try_get<T>(entity);
  if (component) {
    static constexpr ImGuiTreeNodeFlags TREE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                     ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                     ImGuiTreeNodeFlags_FramePadding;

    const float line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + line_height * 0.25f);

    const size_t id = entt::type_id<T>().hash();
    OX_ASSERT(EditorTheme::component_icon_map.contains(typeid(T).hash_code()));
    std::string name_str = StringUtils::from_char8_t(EditorTheme::component_icon_map[typeid(T).hash_code()]);
    name_str = name_str.append(name);
    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(id), TREE_FLAGS, "%s", name_str.c_str());

    bool remove_component = false;
    if (removable) {
      ImGui::PushID((int)id);

      const float frame_height = ImGui::GetFrameHeight();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - frame_height * 1.2f);
      if (ui::button(StringUtils::from_char8_t(ICON_MDI_SETTINGS), ImVec2{frame_height * 1.2f, frame_height}))
        ImGui::OpenPopup("ComponentSettings");

      if (ImGui::BeginPopup("ComponentSettings")) {
        if (ImGui::MenuItem("Remove Component"))
          remove_component = true;

        ImGui::EndPopup();
      }

      ImGui::PopID();
    }

    if (open) {
      ui_function(*component, entity);
      ImGui::TreePop();
    }

    if (remove_component)
      reg.remove<T>(entity);
  }
}

void InspectorPanel::draw_sprite_material_properties(Shared<SpriteMaterial>& material) {
  if (ui::button("Reset")) {
    material = create_shared<SpriteMaterial>();
    material->create();
  }
  ImGui::Spacing();
  ui::begin_properties(ui::default_properties_flags);
  if (ui::property("Albedo", material->get_albedo_texture()))
    material->set_albedo_texture(material->get_albedo_texture());
  ui::property_vector("Color", material->parameters.color, true, true);
  ui::draw_vec2_control("UV Size", material->parameters.uv_size, nullptr, 1.0f);
  ui::draw_vec2_control("UV Offset", material->parameters.uv_offset, nullptr, 0.0f);

  ui::end_properties();
}

void InspectorPanel::draw_pbr_material_properties(Shared<PBRMaterial>& material) {
  if (ui::button("Reset")) {
    material = create_shared<PBRMaterial>();
    material->create();
  }
  ui::begin_properties(ui::default_properties_flags);
  const char* alpha_modes[] = {"Opaque", "Blend", "Mask"};
  ui::property("Alpha mode", (int*)&material->parameters.alpha_mode, alpha_modes, 3);
  const char* samplers[] = {"Bilinear", "Trilinear", "Anisotropy"};
  ui::property("Sampler", (int*)&material->parameters.sampling_mode, samplers, 3);
  ui::property("UV Scale", &material->parameters.uv_scale, 0.0f);

  if (ui::property("Albedo", material->get_albedo_texture()))
    material->set_albedo_texture(material->get_albedo_texture());
  ui::property_vector("Color", material->parameters.color, true, true);

  ui::property("Reflectance", &material->parameters.reflectance, 0.0f, 1.0f);
  if (ui::property("Normal", material->get_normal_texture()))
    material->set_normal_texture(material->get_normal_texture());

  if (ui::property("PhysicalMap", material->get_physical_texture()))
    material->set_physical_texture(material->get_physical_texture());
  ui::property("Roughness", &material->parameters.roughness, 0.0f, 1.0f);
  ui::property("Metallic", &material->parameters.metallic, 0.0f, 1.0f);

  if (ui::property("AO", material->get_ao_texture()))
    material->set_ao_texture(material->get_ao_texture());

  if (ui::property("Emissive", material->get_emissive_texture()))
    material->set_emissive_texture(material->get_emissive_texture());
  ui::property_vector("Emissive Color", material->parameters.emissive, true, true);

  ui::end_properties();
}

template <typename T>
static void draw_particle_over_lifetime_module(const std::string_view module_name,
                                               OverLifetimeModule<T>& property_module,
                                               bool color = false,
                                               bool rotation = false) {
  static constexpr ImGuiTreeNodeFlags TREE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                   ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding;

  if (ImGui::TreeNodeEx(module_name.data(), TREE_FLAGS, "%s", module_name.data())) {
    ui::begin_properties();
    ui::property("Enabled", &property_module.enabled);

    if (rotation) {
      T degrees = glm::degrees(property_module.start);
      if (ui::property_vector("Start", degrees))
        property_module.start = glm::radians(degrees);

      degrees = glm::degrees(property_module.end);
      if (ui::property_vector("End", degrees))
        property_module.end = glm::radians(degrees);
    } else {
      ui::property_vector("Start", property_module.start, color);
      ui::property_vector("End", property_module.end, color);
    }

    ui::end_properties();

    ImGui::TreePop();
  }
}

template <typename T>
static void draw_particle_by_speed_module(const std::string_view module_name,
                                          BySpeedModule<T>& property_module,
                                          bool color = false,
                                          bool rotation = false) {
  static constexpr ImGuiTreeNodeFlags TREE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                   ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding;

  if (ImGui::TreeNodeEx(module_name.data(), TREE_FLAGS, "%s", module_name.data())) {
    ui::begin_properties();
    ui::property("Enabled", &property_module.enabled);

    if (rotation) {
      T degrees = glm::degrees(property_module.start);
      if (ui::property_vector("Start", degrees))
        property_module.start = glm::radians(degrees);

      degrees = glm::degrees(property_module.end);
      if (ui::property_vector("End", degrees))
        property_module.end = glm::radians(degrees);
    } else {
      ui::property_vector("Start", property_module.start, color);
      ui::property_vector("End", property_module.end, color);
    }

    ui::property("Min Speed", &property_module.min_speed);
    ui::property("Max Speed", &property_module.max_speed);
    ui::end_properties();
    ImGui::TreePop();
  }
}

template <typename Component>
void InspectorPanel::draw_add_component(entt::registry& reg, Entity entity, const char* name) {
  if (ImGui::MenuItem(name)) {
    if (!reg.all_of<Component>(entity))
      reg.emplace<Component>(entity);
    else
      OX_LOG_ERROR("Entity already has the {}!", typeid(Component).name());
    ImGui::CloseCurrentPopup();
  }
}

void InspectorPanel::draw_components(Entity entity) {
  TagComponent* tag_component = context->registry.try_get<TagComponent>(entity);
  ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.75f);
  if (tag_component) {
    auto& tag = tag_component->tag;

    if (s_rename_entity)
      ImGui::SetKeyboardFocusHere();
    ui::input_text("##Tag", &tag_component->tag, ImGuiInputTextFlags_EnterReturnsTrue);
  }
  ImGui::PopItemWidth();
  ImGui::SameLine();

  if (ui::button(StringUtils::from_char8_t(ICON_MDI_PLUS))) {
    ImGui::OpenPopup("Add Component");
  }
  if (ImGui::BeginPopup("Add Component")) {
    draw_add_component<MeshComponent>(context->registry, selected_entity, "Mesh Renderer");
    draw_add_component<AudioSourceComponent>(context->registry, selected_entity, "Audio Source");
    draw_add_component<AudioListenerComponent>(context->registry, selected_entity, "Audio Listener");
    draw_add_component<LightComponent>(context->registry, selected_entity, "Light");
    draw_add_component<ParticleSystemComponent>(context->registry, selected_entity, "Particle System");
    draw_add_component<CameraComponent>(context->registry, selected_entity, "Camera");
    draw_add_component<PostProcessProbe>(context->registry, selected_entity, "PostProcess Probe");
    draw_add_component<RigidbodyComponent>(context->registry, selected_entity, "Rigidbody");
    draw_add_component<BoxColliderComponent>(context->registry, entity, "Box Collider");
    draw_add_component<SphereColliderComponent>(context->registry, entity, "Sphere Collider");
    draw_add_component<CapsuleColliderComponent>(context->registry, entity, "Capsule Collider");
    draw_add_component<TaperedCapsuleColliderComponent>(context->registry, entity, "Tapered Capsule Collider");
    draw_add_component<CylinderColliderComponent>(context->registry, entity, "Cylinder Collider");
    draw_add_component<MeshColliderComponent>(context->registry, entity, "Mesh Collider");
    draw_add_component<CharacterControllerComponent>(context->registry, entity, "Character Controller");
    draw_add_component<LuaScriptComponent>(context->registry, entity, "Lua Script Component");
    draw_add_component<CPPScriptComponent>(context->registry, entity, "CPP Script Component");
    draw_add_component<SpriteComponent>(context->registry, entity, "Sprite Component");
    draw_add_component<SpriteAnimationComponent>(context->registry, entity, "Sprite Animation Component");
    draw_add_component<TilemapComponent>(context->registry, entity, "Tilemap Component");

    ImGui::EndPopup();
  }

  ImGui::SameLine();

  ui::checkbox(StringUtils::from_char8_t(ICON_MDI_BUG), &debug_mode);

  const auto uuidstr = fmt::format("UUID: {}", (uint64_t)context->registry.get<IDComponent>(entity).uuid);
  ImGui::Text(uuidstr.c_str());

  if (debug_mode) {
    draw_component<RelationshipComponent>("Relationship Component",
                                          context->registry,
                                          entity,
                                          [](const RelationshipComponent& component, entt::entity e) {
      const auto p_fmt = fmt::format("Parent: {}", (uint64_t)component.parent);
      ImGui::Text(p_fmt.c_str());
      ImGui::Text("Childrens:");
      if (ImGui::BeginTable("Children", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        for (const auto& child : component.children) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          const auto c_fmt = fmt::format("UUID: {}", (uint64_t)child);
          ImGui::Text(c_fmt.c_str());
        }
        ImGui::EndTable();
      }
    });
  }

  draw_component<TransformComponent>(" Transform Component", context->registry, entity, [](TransformComponent& component, entt::entity e) {
    ui::begin_properties(ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV);
    ui::draw_vec3_control("Translation", component.position);
    Vec3 rotation = glm::degrees(component.rotation);
    ui::draw_vec3_control("Rotation", rotation);
    component.rotation = glm::radians(rotation);
    ui::draw_vec3_control("Scale", component.scale, nullptr, 1.0f);
    ui::end_properties();
  });

  draw_component<MeshComponent>(" Mesh Component", context->registry, entity, [](MeshComponent& component, entt::entity e) {
    if (!component.mesh_base)
      return;
    ui::begin_properties();
    ui::text("Totoal meshlet count:", fmt::format("{}", component.mesh_base->_meshlets.size()).c_str());
    ui::text("Total material count:", fmt::format("{}", component.materials.size()).c_str());
    ui::text("Mesh asset id:", fmt::format("{}", component.mesh_id).c_str());
    ui::property("Cast shadows", &component.cast_shadows);
    ui::property("Stationary", &component.stationary);
    ui::end_properties();

    ImGui::SeparatorText("Materials");

    const float filter_cursor_pos_x = ImGui::GetCursorPosX();
    ImGuiTextFilter name_filter;

    name_filter.Draw("##material_filter",
                     ImGui::GetContentRegionAvail().x - (ui::get_icon_button_size(ICON_MDI_PLUS, "").x + 2.0f * ImGui::GetStyle().FramePadding.x));

    if (!name_filter.IsActive()) {
      ImGui::SameLine();
      ImGui::SetCursorPosX(filter_cursor_pos_x + ImGui::GetFontSize() * 0.5f);
      ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_MAGNIFY " Search..."));
    }

    for (uint32 i = 0; i < (uint32)component.materials.size(); i++) {
      auto& material = component.materials[i];
      if (name_filter.PassFilter(material->name.c_str())) {
        ImGui::PushID(i);
        constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
        if (ImGui::TreeNodeEx(material->name.c_str(), flags, "%s", material->name.c_str())) {
          draw_pbr_material_properties(material);
          ImGui::TreePop();
        }
        ImGui::PopID();
      }
    }
  });

  draw_component<SpriteComponent>(" Sprite Component", context->registry, entity, [this](SpriteComponent& component, entt::entity e) {
    ui::begin_properties();
    ui::property("Layer", &component.layer);
    ui::property("SortY", &component.sort_y);
    ui::property("FlipX", &component.flip_x);
    ui::end_properties();

    ImGui::SeparatorText("Material");
    draw_sprite_material_properties(component.material);
  });

  draw_component<SpriteAnimationComponent>(" Sprite Animation Component",
                                           context->registry,
                                           entity,
                                           [this](SpriteAnimationComponent& component, entt::entity e) {
    ui::begin_properties();
    if (ui::property("Number of frames", &component.num_frames))
      component.reset();
    if (ui::property("Loop", &component.loop))
      component.reset();
    if (ui::property("Inverted", &component.inverted))
      component.reset();
    if (ui::property("Frames per second", &component.fps))
      component.reset();
    if (ui::property("Columns", &component.columns))
      component.reset();
    if (ui::draw_vec2_control("Frame size", component.frame_size))
      component.reset();
    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();
    ImGui::Spacing();
    if (ui::button("Auto", {x, y})) {
      if (auto* sc = context->registry.try_get<SpriteComponent>(e))
        component.set_frame_size(sc->material->get_albedo_texture().get());
    }
    ui::end_properties();
  });

  draw_component<TilemapComponent>(" Tilemap Component", context->registry, entity, [this](TilemapComponent& component, entt::entity e) {
    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();
    if (ui::button("Load tilemap", {x, y}, "Load exported png and json file from ldtk")) {
      auto path = App::get_system<FileDialogs>()->open_file({{"json file", "json"}});
      component.load(path);
    }
    ImGui::Separator();

    ui::begin_properties();
    std::string layer_name = "empty";
    for (auto& [name, mat] : component.layers)
      layer_name = name;
    ui::text("Layers", layer_name.c_str());
    ui::text("Tilemap size", fmt::format("x: {}, y: {}", component.tilemap_size.x, component.tilemap_size.y).c_str());
    ui::end_properties();
  });

  draw_component<PostProcessProbe>(" PostProcess Probe Component", context->registry, entity, [this](PostProcessProbe& component, entt::entity e) {
    ImGui::Text("Vignette");
    ui::begin_properties();
    ui::property("Enable", &component.vignette_enabled);
    ui::property("Intensity", &component.vignette_intensity);
    ui::end_properties();
    ImGui::Separator();

    ImGui::Text("FilmGrain");
    ui::begin_properties();
    ui::property("Enable", &component.film_grain_enabled);
    ui::property("Intensity", &component.film_grain_intensity);
    ui::end_properties();
    ImGui::Separator();

    ImGui::Text("ChromaticAberration");
    ui::begin_properties();
    ui::property("Enable", &component.chromatic_aberration_enabled);
    ui::property("Intensity", &component.chromatic_aberration_intensity);
    ui::end_properties();
    ImGui::Separator();

    ImGui::Text("Sharpen");
    ui::begin_properties();
    ui::property("Enable", &component.sharpen_enabled);
    ui::property("Intensity", &component.sharpen_intensity);
    ui::end_properties();
    ImGui::Separator();
  });

  draw_component<AudioSourceComponent>(" Audio Source Component",
                                       context->registry,
                                       entity,
                                       [&entity, this](AudioSourceComponent& component, entt::entity e) {
    auto& config = component.config;
    const std::string filepath = component.source ? component.source->get_path()
                                                  : fmt::format("{} Drop an audio file", StringUtils::from_char8_t(ICON_MDI_FILE_UPLOAD));

    auto load_file = [](const std::filesystem::path& path, AudioSourceComponent& comp) {
      if (const std::string ext = path.extension().string(); ext == ".mp3" || ext == ".wav" || ext == ".flac")
        comp.source = create_shared<AudioSource>(App::get_absolute(path.string()).c_str());
    };

    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();
    if (ui::button(filepath.c_str(), {x, y})) {
      const std::string file_path = App::get_system<FileDialogs>()->open_file({{"Audio file", "mp3, wav, flac"}});
      load_file(file_path, component);
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
        const std::filesystem::path path = ui::get_path_from_imgui_payload(payload);
        load_file(path, component);
      }
      ImGui::EndDragDropTarget();
    }
    ImGui::Spacing();

    ui::begin_properties();
    ui::property("Volume Multiplier", &config.volume_multiplier);
    ui::property("Pitch Multiplier", &config.pitch_multiplier);
    ui::property("Play On Awake", &config.play_on_awake);
    ui::property("Looping", &config.looping);
    ui::end_properties();

    ImGui::Spacing();
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_PLAY "Play ")) && component.source)
      component.source->play();
    ImGui::SameLine();
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_PAUSE "Pause ")) && component.source)
      component.source->pause();
    ImGui::SameLine();
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_STOP "Stop ")) && component.source)
      component.source->stop();
    ImGui::Spacing();

    ui::begin_properties();
    ui::property("Spatialization", &config.spatialization);

    if (config.spatialization) {
      ImGui::Indent();
      const char* attenuation_type_strings[] = {"None", "Inverse", "Linear", "Exponential"};
      int attenuation_type = static_cast<int>(config.attenuation_model);
      if (ui::property("Attenuation Model", &attenuation_type, attenuation_type_strings, 4))
        config.attenuation_model = static_cast<AttenuationModelType>(attenuation_type);
      ui::property("Roll Off", &config.roll_off);
      ui::property("Min Gain", &config.min_gain);
      ui::property("Max Gain", &config.max_gain);
      ui::property("Min Distance", &config.min_distance);
      ui::property("Max Distance", &config.max_distance);
      float degrees = glm::degrees(config.cone_inner_angle);
      if (ui::property("Cone Inner Angle", &degrees))
        config.cone_inner_angle = glm::radians(degrees);
      degrees = glm::degrees(config.cone_outer_angle);
      if (ui::property("Cone Outer Angle", &degrees))
        config.cone_outer_angle = glm::radians(degrees);
      ui::property("Cone Outer Gain", &config.cone_outer_gain);
      ui::property("Doppler Factor", &config.doppler_factor);
      ImGui::Unindent();
    }
    ui::end_properties();

    if (component.source) {
      const glm::mat4 inverted = glm::inverse(eutil::get_world_transform(context, entity));
      const Vec3 forward = normalize(Vec3(inverted[2]));
      component.source->set_config(config);
      component.source->set_position(context->registry.get<TransformComponent>(entity).position);
      component.source->set_direction(-forward);
    }
  });

  draw_component<AudioListenerComponent>(" Audio Listener Component",
                                         context->registry,
                                         entity,
                                         [](AudioListenerComponent& component, entt::entity e) {
    auto& config = component.config;
    ui::begin_properties();
    ui::property("Active", &component.active);
    float degrees = glm::degrees(config.cone_inner_angle);
    if (ui::property("Cone Inner Angle", &degrees))
      config.cone_inner_angle = glm::radians(degrees);
    degrees = glm::degrees(config.cone_outer_angle);
    if (ui::property("Cone Outer Angle", &degrees))
      config.cone_outer_angle = glm::radians(degrees);
    ui::property("Cone Outer Gain", &config.cone_outer_gain);
    ui::end_properties();
  });

  draw_component<LightComponent>(" Light Component", context->registry, entity, [](LightComponent& component, entt::entity e) {
    ui::begin_properties();
    const char* light_type_strings[] = {"Directional", "Point", "Spot"};
    int light_type = component.type;
    if (ui::property("Light Type", &light_type, light_type_strings, 3))
      component.type = static_cast<LightComponent::LightType>(light_type);

    if (ui::property("Color Temperature Mode", &component.color_temperature_mode) && component.color_temperature_mode) {
      ColorUtils::TempratureToColor(component.temperature, component.color);
    }

    if (component.color_temperature_mode) {
      if (ui::property<uint32>("Temperature (K)", &component.temperature, 1000, 40000))
        ColorUtils::TempratureToColor(component.temperature, component.color);
    } else {
      ui::property_vector("Color", component.color, true);
    }

    ui::property("Intensity", &component.intensity, 0.0f, 100.0f);

    if (component.type != LightComponent::Directional) {
      ui::property("Range", &component.range, 0.0f, 100.0f);
      ui::property("Radius", &component.radius, 0.0f, 100.0f);
      ui::property("Length", &component.length, 0.0f, 100.0f);
    }

    if (component.type == LightComponent::Spot) {
      ui::property("Outer Cone Angle", &component.outer_cone_angle, 0.0f, 100.0f);
      ui::property("Inner Cone Angle", &component.inner_cone_angle, 0.0f, 100.0f);
    }

    ui::property("Cast Shadows", &component.cast_shadows);

    const ankerl::unordered_dense::map<uint32, int> res_map = {{0, 0}, {512, 1}, {1024, 2}, {2048, 3}};
    const ankerl::unordered_dense::map<int, uint32> id_map = {{0, 0}, {1, 512}, {2, 1024}, {3, 2048}};

    const char* res_strings[] = {"Auto", "512", "1024", "2048"};
    int idx = res_map.at(component.shadow_map_res);
    if (ui::property("Shadow Resolution", &idx, res_strings, 4)) {
      component.shadow_map_res = id_map.at(idx);
    }

    if (component.type == LightComponent::Directional) {
      for (uint32 i = 0; i < (uint32)component.cascade_distances.size(); ++i)
        ui::property(fmt::format("Cascade {}", i).c_str(), &component.cascade_distances[i]);
    }

    ui::end_properties();
  });

  draw_component<RigidbodyComponent>(" Rigidbody Component", context->registry, entity, [this](RigidbodyComponent& component, entt::entity e) {
    ui::begin_properties();

    const char* dofs_strings[] = {
      "None",
      "All",
      "Plane2D",
      "Custom",
    };
    int current_dof_selection = 3;
    switch (component.allowed_dofs) {
      case RigidbodyComponent::AllowedDOFs::None   : current_dof_selection = 0; break;
      case RigidbodyComponent::AllowedDOFs::All    : current_dof_selection = 1; break;
      case RigidbodyComponent::AllowedDOFs::Plane2D: current_dof_selection = 2; break;
      default                                      : current_dof_selection = 3; break;
    }

    if (ui::property("Allowed degree of freedom", &current_dof_selection, dofs_strings, std::size(dofs_strings))) {
      switch (current_dof_selection) {
        case 0: component.allowed_dofs = RigidbodyComponent::AllowedDOFs::None; break;
        case 1: component.allowed_dofs = RigidbodyComponent::AllowedDOFs::All; break;
        case 2: component.allowed_dofs = RigidbodyComponent::AllowedDOFs::Plane2D; break;
      }
    }

    ImGui::Indent();
    ui::begin_property_grid("Allowed positions", nullptr);
    ImGui::CheckboxFlags("x", (uint32*)&component.allowed_dofs, (uint32)RigidbodyComponent::AllowedDOFs::TranslationX);
    ImGui::SameLine();
    ImGui::CheckboxFlags("y", (uint32*)&component.allowed_dofs, (uint32)RigidbodyComponent::AllowedDOFs::TranslationY);
    ImGui::SameLine();
    ImGui::CheckboxFlags("z", (uint32*)&component.allowed_dofs, (uint32)RigidbodyComponent::AllowedDOFs::TranslationZ);
    ui::end_property_grid();

    ui::begin_property_grid("Allowed rotations", nullptr);
    ImGui::CheckboxFlags("x", (uint32*)&component.allowed_dofs, (uint32)RigidbodyComponent::AllowedDOFs::RotationX);
    ImGui::SameLine();
    ImGui::CheckboxFlags("y", (uint32*)&component.allowed_dofs, (uint32)RigidbodyComponent::AllowedDOFs::RotationY);
    ImGui::SameLine();
    ImGui::CheckboxFlags("z", (uint32*)&component.allowed_dofs, (uint32)RigidbodyComponent::AllowedDOFs::RotationZ);
    ui::end_property_grid();
    ImGui::Unindent();

    const char* body_type_strings[] = {"Static", "Kinematic", "Dynamic"};
    int body_type = static_cast<int>(component.type);
    if (ui::property("Body Type", &body_type, body_type_strings, 3))
      component.type = static_cast<RigidbodyComponent::BodyType>(body_type);

    ui::property("Allow Sleep", &component.allow_sleep);
    ui::property("Awake", &component.awake);
    if (component.type == RigidbodyComponent::BodyType::Dynamic) {
      ui::property("Mass", &component.mass, 0.01f, 10000.0f);
      ui::property("Linear Drag", &component.linear_drag);
      ui::property("Angular Drag", &component.angular_drag);
      ui::property("Gravity Scale", &component.gravity_scale);
      ui::property("Continuous", &component.continuous);
      ui::property("Interpolation", &component.interpolation);

      component.linear_drag = glm::max(component.linear_drag, 0.0f);
      component.angular_drag = glm::max(component.angular_drag, 0.0f);
    }

    ui::property("Is Sensor", &component.is_sensor);
    ui::end_properties();
  });

  draw_component<BoxColliderComponent>(" Box Collider", context->registry, entity, [](BoxColliderComponent& component, entt::entity e) {
    ui::begin_properties();
    ui::property_vector("Size", component.size);
    ui::property_vector("Offset", component.offset);
    ui::property("Density", &component.density);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();

    component.density = glm::max(component.density, 0.001f);
  });

  draw_component<SphereColliderComponent>(" Sphere Collider", context->registry, entity, [](SphereColliderComponent& component, entt::entity e) {
    ui::begin_properties();
    ui::property("Radius", &component.radius);
    ui::property_vector("Offset", component.offset);
    ui::property("Density", &component.density);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();

    component.density = glm::max(component.density, 0.001f);
  });

  draw_component<CapsuleColliderComponent>(" Capsule Collider", context->registry, entity, [](CapsuleColliderComponent& component, entt::entity e) {
    ui::begin_properties();
    ui::property("Height", &component.height);
    ui::property("Radius", &component.radius);
    ui::property_vector("Offset", component.offset);
    ui::property("Density", &component.density);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();

    component.density = glm::max(component.density, 0.001f);
  });

  draw_component<TaperedCapsuleColliderComponent>(" Tapered Capsule Collider",
                                                  context->registry,
                                                  entity,
                                                  [](TaperedCapsuleColliderComponent& component, entt::entity e) {
    ui::begin_properties();
    ui::property("Height", &component.height);
    ui::property("Top Radius", &component.top_radius);
    ui::property("Bottom Radius", &component.bottom_radius);
    ui::property_vector("Offset", component.offset);
    ui::property("Density", &component.density);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();

    component.density = glm::max(component.density, 0.001f);
  });

  draw_component<CylinderColliderComponent>(" Cylinder Collider",
                                            context->registry,
                                            entity,
                                            [](CylinderColliderComponent& component, entt::entity e) {
    ui::begin_properties();
    ui::property("Height", &component.height);
    ui::property("Radius", &component.radius);
    ui::property_vector("Offset", component.offset);
    ui::property("Density", &component.density);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();

    component.density = glm::max(component.density, 0.001f);
  });

  draw_component<MeshColliderComponent>(" Mesh Collider", context->registry, entity, [](MeshColliderComponent& component, entt::entity e) {
    ui::begin_properties();
    ui::property_vector("Offset", component.offset);
    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("Restitution", &component.restitution, 0.0f, 1.0f);
    ui::end_properties();
  });

  draw_component<CharacterControllerComponent>(" Character Controller",
                                               context->registry,
                                               entity,
                                               [](CharacterControllerComponent& component, entt::entity e) {
    ui::begin_properties();
    ui::property("CharacterHeightStanding", &component.character_height_standing);
    ui::property("CharacterRadiusStanding", &component.character_radius_standing);
    ui::property("CharacterHeightCrouching", &component.character_height_crouching);
    ui::property("CharacterRadiusCrouching", &component.character_radius_crouching);

    // Movement
    ui::property("ControlMovementDuringJump", &component.control_movement_during_jump);
    ui::property("JumpForce", &component.jump_force);

    ui::property("Friction", &component.friction, 0.0f, 1.0f);
    ui::property("CollisionTolerance", &component.collision_tolerance);
    ui::end_properties();
  });

  draw_component<CameraComponent>("Camera Component", context->registry, entity, [](CameraComponent& component, entt::entity e) {
    const auto is_perspective = component.camera->get_projection() == Camera::Projection::Perspective;
    ui::begin_properties();

    const char* proj_strs[] = {"Perspective", "Orthographic"};
    int proj = static_cast<int>(component.camera->get_projection());
    if (ui::property("Projection", &proj, proj_strs, 2))
      component.camera->set_projection(static_cast<Camera::Projection>(proj));

    if (is_perspective) {
      float fov = component.camera->get_fov();
      if (ui::property("FOV", &fov)) {
        component.camera->set_fov(fov);
      }
      float near_clip = component.camera->get_near();
      if (ui::property("Near Clip", &near_clip)) {
        component.camera->set_near(near_clip);
      }
      float far_clip = component.camera->get_far();
      if (ui::property("Far Clip", &far_clip)) {
        component.camera->set_far(far_clip);
      }
    } else {
      float zoom = component.camera->get_zoom();
      if (ui::property("Zoom", &zoom)) {
        component.camera->set_zoom(zoom);
      }
    }

    ui::end_properties();
  });

  draw_component<LuaScriptComponent>("Lua Script Component", context->registry, entity, [](LuaScriptComponent& component, entt::entity e) {
    const float filter_cursor_pos_x = ImGui::GetCursorPosX();
    ImGuiTextFilter name_filter;

    name_filter.Draw("##scripts_filter",
                     ImGui::GetContentRegionAvail().x - (ui::get_icon_button_size(ICON_MDI_PLUS, "").x + 2.0f * ImGui::GetStyle().FramePadding.x));

    if (!name_filter.IsActive()) {
      ImGui::SameLine();
      ImGui::SetCursorPosX(filter_cursor_pos_x + ImGui::GetFontSize() * 0.5f);
      ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_MAGNIFY " Search..."));
    }

    for (uint32 i = 0; i < (uint32)component.lua_systems.size(); i++) {
      const auto& system = component.lua_systems[i];
      auto name = fs::get_file_name(system->get_path());
      if (name_filter.PassFilter(name.c_str())) {
        ImGui::PushID(i);
        constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
        if (ImGui::TreeNodeEx(name.c_str(), flags, "%s", name.c_str())) {
          auto rld_str = fmt::format("{} Reload", StringUtils::from_char8_t(ICON_MDI_REFRESH));
          if (ui::button(rld_str.c_str()))
            system->reload();
          ImGui::SameLine();
          auto rmv_str = fmt::format("{} Remove", StringUtils::from_char8_t(ICON_MDI_TRASH_CAN));
          if (ui::button(rmv_str.c_str()))
            component.lua_systems.erase(component.lua_systems.begin() + i);
          ImGui::TreePop();
        }
        ImGui::PopID();
      }
    }

    auto load_script = [](const std::string& path, LuaScriptComponent& comp) {
      if (path.empty())
        return;
      const auto ext = fs::get_file_extension(path);
      if (ext == "lua") {
        comp.lua_systems.emplace_back(create_shared<LuaSystem>(path));
      }
    };
    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();
    const auto btn = fmt::format("{} Drop a script file", StringUtils::from_char8_t(ICON_MDI_FILE_UPLOAD));
    if (ui::button(btn.c_str(), {x, y})) {
      const std::string file_path = App::get_system<FileDialogs>()->open_file({{"Lua file", "lua"}});
      load_script(file_path, component);
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
        const auto path = ui::get_path_from_imgui_payload(payload);
        load_script(path, component);
      }
      ImGui::EndDragDropTarget();
    }
  });

  draw_component<CPPScriptComponent>(" CPP Script Component", context->registry, entity, [](CPPScriptComponent& component, entt::entity e) {
    const auto lbl = fmt::format("{} Add system", StringUtils::from_char8_t(ICON_MDI_PLUS_OUTLINE));
    ankerl::unordered_dense::map<size_t, std::string> system_names_strs = {};
    std::vector<const char*> system_names = {};
    system_names.emplace_back(lbl.c_str());

    std::vector<size_t> system_hashes = {};
    system_hashes.emplace_back(0);

    int current_system_selection = 0;

    auto* system_manager = App::get_system<SystemManager>();
    for (auto& [hash, pair] : system_manager->system_registry) {
      auto& [name, system] = pair;
      std::string system_name = name;
      system_name = system_name.erase(0, system_name.find(' ') + 1);
      system_names.emplace_back(system_names_strs.emplace(hash, system_name).first->second.c_str());
      system_hashes.emplace_back(hash);
    }

    constexpr ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("table", 1, flags)) {
      ImGui::TableSetupColumn("  Systems", ImGuiTableColumnFlags_NoHide);
      ImGui::TableHeadersRow();

      int32 delete_index = -1;

      for (uint32 index = 0; auto& sys : component.systems) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(index);
        if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_TRASH_CAN)))
          delete_index = index;
        ImGui::PopID();
        ImGui::SameLine();
        bool open = ImGui::TreeNodeEx(system_names_strs[sys->hash_code].c_str(), ImGuiTreeNodeFlags_SpanAllColumns);
        if (open) {
          ImGui::Indent();
          ImGui::Text("system_hash: %s", std::to_string(sys->hash_code).c_str());
          ImGui::Unindent();
          ImGui::TreePop();
        }

        index += 1;
      }

      if (delete_index > -1)
        component.systems.erase(component.systems.begin() + delete_index);

      ImGui::EndTable();
    }

    const float x = ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth(x);
    if (ui::combo(lbl.c_str(),
                  &current_system_selection,
                  system_names.data(),
                  system_names.size(),
                  std::to_string(system_hashes[current_system_selection]).c_str())) {
      if (auto system = system_manager->get_system(system_hashes[current_system_selection]))
        component.systems.emplace_back(system);
    }
  });

  draw_component<ParticleSystemComponent>("Particle System Component",
                                          context->registry,
                                          entity,
                                          [](const ParticleSystemComponent& component, entt::entity e) {
    auto& props = component.system->get_properties();

    ImGui::Text("Active Particles Count: %u", component.system->get_active_particle_count());
    ImGui::BeginDisabled(props.looping);
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_PLAY)))
      component.system->play();
    ImGui::SameLine();
    if (ui::button(StringUtils::from_char8_t(ICON_MDI_STOP)))
      component.system->stop();
    ImGui::EndDisabled();

    ImGui::Separator();

    ui::begin_properties();
    ui::property("Duration", &props.duration);
    if (ui::property("Looping", &props.looping)) {
      if (props.looping)
        component.system->play();
    }
    ui::property("Start Delay", &props.start_delay);
    ui::property("Start Lifetime", &props.start_lifetime);
    ui::property_vector("Start Velocity", props.start_velocity);
    ui::property_vector("Start Color", props.start_color, true);
    ui::property_vector("Start Size", props.start_size);
    ui::property_vector("Start Rotation", props.start_rotation);
    ui::property("Gravity Modifier", &props.gravity_modifier);
    ui::property("Simulation Speed", &props.simulation_speed);
    ui::property("Play On Awake", &props.play_on_awake);
    ui::property("Max Particles", &props.max_particles);
    ui::end_properties();

    ImGui::Separator();

    ui::begin_properties();
    ui::property("Rate Over Time", &props.rate_over_time);
    ui::property("Rate Over Distance", &props.rate_over_distance);
    ui::property("Burst Count", &props.burst_count);
    ui::property("Burst Time", &props.burst_time);
    ui::property_vector("Position Start", props.position_start);
    ui::property_vector("Position End", props.position_end);
    // OxUI::Property("Texture", props.Texture); //TODO:
    ui::end_properties();

    draw_particle_over_lifetime_module("Velocity Over Lifetime", props.velocity_over_lifetime);
    draw_particle_over_lifetime_module("Force Over Lifetime", props.force_over_lifetime);
    draw_particle_over_lifetime_module("Color Over Lifetime", props.color_over_lifetime, true);
    draw_particle_by_speed_module("Color By Speed", props.color_by_speed, true);
    draw_particle_over_lifetime_module("Size Over Lifetime", props.size_over_lifetime);
    draw_particle_by_speed_module("Size By Speed", props.size_by_speed);
    draw_particle_over_lifetime_module("Rotation Over Lifetime", props.rotation_over_lifetime, false, true);
    draw_particle_by_speed_module("Rotation By Speed", props.rotation_by_speed, false, true);
  });
}
} // namespace ox
