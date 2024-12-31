#include "ContentPanel.hpp"

#include <icons/IconsMaterialDesignIcons.h>

#include "Assets/AssetManager.hpp"
#include "Core/App.hpp"
#include "Core/FileSystem.hpp"
#include "Core/Project.hpp"
#include "EditorContext.hpp"
#include "EditorLayer.hpp"

#include "Thread/ThreadManager.hpp"

#include "UI/ImGuiLayer.hpp"
#include "UI/OxUI.hpp"
#include "Utils/FileWatch.hpp"
#include "Utils/Profiler.hpp"
#include "Utils/StringUtils.hpp"
#include "Utils/Timestep.hpp"

namespace ox {
static const ankerl::unordered_dense::map<FileType, const char*> FILE_TYPES_TO_STRING = {
  {FileType::Unknown, "Unknown"},

  {FileType::Scene, "Scene"},
  {FileType::Prefab, "Prefab"},
  {FileType::Shader, "Shader"},
  {FileType::Texture, "Texture"},
  {FileType::Cubemap, "Cubemap"},
  {FileType::Model, "Model"},
  {FileType::Script, "Script"},
  {FileType::Audio, "Audio"},
};

static const ankerl::unordered_dense::map<std::string, FileType> FILE_TYPES = {
  {".oxscene", FileType::Scene}, {".oxprefab", FileType::Prefab}, {".hlsl", FileType::Shader},    {".hlsli", FileType::Shader},
  {".glsl", FileType::Shader},   {".frag", FileType::Shader},     {".vert", FileType::Shader},

  {".png", FileType::Texture},   {".jpg", FileType::Texture},     {".jpeg", FileType::Texture},   {".bmp", FileType::Texture},
  {".gif", FileType::Texture},   {".ktx", FileType::Texture},     {".ktx2", FileType::Texture},   {".tiff", FileType::Texture},

  {".hdr", FileType::Cubemap},   {".tga", FileType::Cubemap},

  {".gltf", FileType::Model},    {".glb", FileType::Model},       {".oxmat", FileType::Material},

  {".mp3", FileType::Audio},     {".m4a", FileType::Audio},       {".wav", FileType::Audio},      {".ogg", FileType::Audio},

  {".lua", FileType::Script},
};

static const ankerl::unordered_dense::map<FileType, ImVec4> TYPE_COLORS = {
  {FileType::Scene, {0.75f, 0.35f, 0.20f, 1.00f}},
  {FileType::Prefab, {0.10f, 0.50f, 0.80f, 1.00f}},
  {FileType::Shader, {0.10f, 0.50f, 0.80f, 1.00f}},
  {FileType::Material, {0.80f, 0.20f, 0.30f, 1.00f}},
  {FileType::Texture, {0.80f, 0.20f, 0.30f, 1.00f}},
  {FileType::Cubemap, {0.80f, 0.20f, 0.30f, 1.00f}},
  {FileType::Model, {0.20f, 0.80f, 0.75f, 1.00f}},
  {FileType::Audio, {0.20f, 0.80f, 0.50f, 1.00f}},
  {FileType::Script, {0.0f, 16.0f, 121.0f, 1.00f}},
};

static const ankerl::unordered_dense::map<FileType, const char8_t*> FILE_TYPES_TO_ICON = {
  {FileType::Unknown, ICON_MDI_FILE},

  {FileType::Scene, ICON_MDI_FILE},
  {FileType::Prefab, ICON_MDI_FILE},
  {FileType::Shader, ICON_MDI_IMAGE_FILTER_BLACK_WHITE},

  {FileType::Material, ICON_MDI_SPRAY_BOTTLE},
  {FileType::Texture, ICON_MDI_FILE_IMAGE},
  {FileType::Cubemap, ICON_MDI_IMAGE_FILTER_HDR},
  {FileType::Model, ICON_MDI_VECTOR_POLYGON},

  {FileType::Audio, ICON_MDI_MICROPHONE},
};

static bool drag_drop_target(const std::filesystem::path& drop_path) {
  if (ImGui::BeginDragDropTarget()) {
    const ImGuiPayload* payload1 = ImGui::AcceptDragDropPayload("Registry");
    const ImGuiPayload* payload2 = ImGui::AcceptDragDropPayload("Entity");
    if (payload1 && payload2) {
      const auto* registry = static_cast<entt::registry*>(payload1->Data);
      const auto* entity = static_cast<Entity*>(payload1->Data);
      auto entity_name = registry->get<TagComponent>(*entity).tag;
      const std::filesystem::path path = drop_path / entity_name.append(".oxprefab");
      EntitySerializer::serialize_entity_as_prefab(path.string().c_str(), *entity);
      return true;
    }

    ImGui::EndDragDropTarget();
  }

  return false;
}

static void drag_drop_from(const std::filesystem::path& filepath) {
  if (ImGui::BeginDragDropSource()) {
    const std::string path_str = filepath.string();
    ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", path_str.c_str(), path_str.length() + 1);
    ImGui::TextUnformatted(filepath.filename().string().c_str());
    ImGui::EndDragDropSource();
  }
}

static void open_file(const std::filesystem::path& path) {
  const std::string filepath_string = path.string();
  const char* filepath = filepath_string.c_str();
  const std::string ext = path.extension().string();
  const auto& file_type_it = FILE_TYPES.find(ext);
  if (file_type_it != FILE_TYPES.end()) {
    const FileType file_type = file_type_it->second;
    switch (file_type) {
      case FileType::Scene: {
        EditorLayer::get()->open_scene(filepath);
        break;
      }
      case FileType::Unknown : break;
      case FileType::Prefab  : break;
      case FileType::Texture : break;
      case FileType::Cubemap : break;
      case FileType::Model   : break;
      case FileType::Material: {
        EditorLayer::get()->set_context_as_asset_with_path(path.string());
        break;
      }
      case FileType::Audio : [[fallthrough]];
      case FileType::Shader: [[fallthrough]];
      case FileType::Script: {
        fs::open_file_externally(filepath);
        break;
      }
    }
  } else {
    fs::open_file_externally(filepath);
  }
}

std::pair<bool, uint32_t> ContentPanel::directory_tree_view_recursive(const std::filesystem::path& path,
                                                                      uint32_t* count,
                                                                      int* selectionMask,
                                                                      ImGuiTreeNodeFlags flags) {
  OX_SCOPED_ZONE;
  bool anyNodeClicked = false;
  uint32_t nodeClicked = 0;

  if (path.empty())
    return {};

  for (const auto& entry : std::filesystem::directory_iterator(path)) {
    ImGuiTreeNodeFlags nodeFlags = flags;

    auto& entryPath = entry.path();

    const bool entryIsFile = !std::filesystem::is_directory(entryPath);
    if (entryIsFile)
      nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    const bool selected = (*selectionMask & BIT(*count)) != 0;
    if (selected) {
      nodeFlags |= ImGuiTreeNodeFlags_Selected;
      ImGui::PushStyleColor(ImGuiCol_Header, ImGuiLayer::header_selected_color);
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGuiLayer::header_selected_color);
    } else {
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGuiLayer::header_hovered_color);
    }

    const uint64_t id = *count;
    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(id), nodeFlags, "");
    ImGui::PopStyleColor(selected ? 2 : 1);

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      if (!entryIsFile)
        update_directory_entries(entryPath);

      nodeClicked = *count;
      anyNodeClicked = true;
    }

    const std::string filepath = entryPath.string();

    if (!entryIsFile)
      drag_drop_target(entryPath);
    drag_drop_from(entryPath);

    auto name = fs::get_name_with_extension(filepath);

    const char8_t* folderIcon = ICON_MDI_FILE;
    if (entryIsFile) {
      auto fileType = FileType::Unknown;
      const auto& fileTypeIt = FILE_TYPES.find(entryPath.extension().string());
      if (fileTypeIt != FILE_TYPES.end())
        fileType = fileTypeIt->second;

      const auto& fileTypeIconIt = FILE_TYPES_TO_ICON.find(fileType);
      if (fileTypeIconIt != FILE_TYPES_TO_ICON.end())
        folderIcon = fileTypeIconIt->second;
    } else {
      folderIcon = open ? ICON_MDI_FOLDER_OPEN : ICON_MDI_FOLDER;
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiLayer::asset_icon_color);
    ImGui::TextUnformatted(StringUtils::from_char8_t(folderIcon));
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextUnformatted(name.data());
    m_currently_visible_items_tree_view++;

    (*count)--;

    if (!entryIsFile) {
      if (open) {
        const auto [isClicked, clickedNode] = directory_tree_view_recursive(entryPath, count, selectionMask, flags);

        if (!anyNodeClicked) {
          anyNodeClicked = isClicked;
          nodeClicked = clickedNode;
        }

        ImGui::TreePop();
      } else {
        /*for ([[maybe_unused]] const auto& e : std::filesystem::recursive_directory_iterator(entryPath))
          (*count)--;*/
      }
    }
  }

  return {anyNodeClicked, nodeClicked};
}

ContentPanel::ContentPanel() : EditorPanel("Contents", ICON_MDI_FOLDER_STAR, true) {
  const auto scale = Window::get_content_scale();
  thumbnail_max_limit *= scale.x;
  thumbnail_size_grid_limit *= scale.x;

  _white_texture = Texture::get_white_texture();

  auto file_icon = create_shared<Texture>(TextureLoadInfo{
    .path = App::get_asset_directory("Icons/FileIcon.png"),
    .preset = Preset::eRTT2DUnmipped,
  });
  thumbnail_cache.emplace("file_icon", file_icon);

  auto directory_icon = create_shared<Texture>(TextureLoadInfo{
    .path = App::get_asset_directory("Icons/FolderIcon.png"),
    .preset = Preset::eRTT2DUnmipped,
  });
  thumbnail_cache.emplace("folder_icon", directory_icon);

  auto mesh_icon = create_shared<Texture>(TextureLoadInfo{
    .path = App::get_asset_directory("Icons/MeshFileIcon.png"),
    .preset = Preset::eRTT2DUnmipped,
  });
  thumbnail_cache.emplace("mesh_icon", mesh_icon);
}

void ContentPanel::init() {
  m_assets_directory = Project::get_asset_directory();
  m_current_directory = m_assets_directory;
  refresh();

  [[maybe_unused]] static filewatch::FileWatch<std::string> watch(m_assets_directory.string(), [this](const auto&, const filewatch::Event) {
    ThreadManager::get()->asset_thread.queue_job([this] { refresh(); });
  });
}

void ContentPanel::on_update() { m_elapsed_time += (float)App::get_timestep(); }

void ContentPanel::on_imgui_render() {
  constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;

  constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody;

  if (!Project::get_asset_directory().empty() && m_assets_directory.empty()) {
    init();
  }

  on_begin(windowFlags);
  {
    render_header();
    ImGui::Separator();
    const ImVec2 availableRegion = ImGui::GetContentRegionAvail();
    if (ImGui::BeginTable("MainViewTable", 2, tableFlags, availableRegion)) {
      ImGui::TableSetupColumn("##side_view", ImGuiTableColumnFlags_WidthFixed, 150);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      render_side_view();
      ImGui::TableNextColumn();
      render_body(EditorCVar::cvar_file_thumbnail_size.get() >= thumbnail_size_grid_limit);

      ImGui::EndTable();
    }
  }
  on_end();
}

void ContentPanel::invalidate() {
  m_assets_directory = Project::get_asset_directory();
  m_current_directory = m_assets_directory;
  refresh();
}

void ContentPanel::render_header() {
  if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_COGS)))
    ImGui::OpenPopup("SettingsPopup");
  if (ImGui::BeginPopup("SettingsPopup")) {
    ui::begin_properties(ImGuiTableFlags_SizingStretchSame);
    ui::property("Thumbnail Size",
                   EditorCVar::cvar_file_thumbnail_size.get_ptr(),
                   thumbnail_size_grid_limit - 0.1f,
                   thumbnail_max_limit,
                   nullptr,
                   0.1f,
                   "");
    ui::property("Show file thumbnails", (bool*)EditorCVar::cvar_file_thumbnails.get_ptr());
    ui::end_properties();
    ImGui::EndPopup();
  }

  ImGui::SameLine();
  const float cursorPosX = ImGui::GetCursorPosX();
  m_filter.Draw("###ConsoleFilter", ImGui::GetContentRegionAvail().x);
  if (!m_filter.IsActive()) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(cursorPosX + ImGui::GetFontSize() * 0.5f);
    ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_MAGNIFY " Search..."));
  }

  ImGui::Spacing();
  ImGui::Spacing();

  // Back button
  {
    bool disabledBackButton = false;
    if (m_current_directory == m_assets_directory)
      disabledBackButton = true;

    if (disabledBackButton) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_ARROW_LEFT_CIRCLE_OUTLINE))) {
      m_back_stack.push(m_current_directory);
      update_directory_entries(m_current_directory.parent_path());
    }

    if (disabledBackButton) {
      ImGui::PopStyleVar();
      ImGui::PopItemFlag();
    }
  }

  ImGui::SameLine();

  // Front button
  {
    bool disabledFrontButton = false;
    if (m_back_stack.empty())
      disabledFrontButton = true;

    if (disabledFrontButton) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_ARROW_RIGHT_CIRCLE_OUTLINE))) {
      const auto& top = m_back_stack.top();
      update_directory_entries(top);
      m_back_stack.pop();
    }

    if (disabledFrontButton) {
      ImGui::PopStyleVar();
      ImGui::PopItemFlag();
    }
  }

  ImGui::SameLine();

  ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_FOLDER));

  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_Button, {0.0f, 0.0f, 0.0f, 0.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.0f, 0.0f, 0.0f, 0.0f});
  std::filesystem::path current = m_assets_directory.parent_path();
  std::filesystem::path directoryToOpen;
  const std::filesystem::path currentDirectory = relative(m_current_directory, current);
  for (auto& path : currentDirectory) {
    current /= path;
    ImGui::SameLine();
    if (ImGui::Button(path.filename().string().c_str()))
      directoryToOpen = current;

    if (m_current_directory != current) {
      ImGui::SameLine();
      ImGui::TextUnformatted("/");
    }
  }
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();

  if (!directoryToOpen.empty())
    update_directory_entries(directoryToOpen);
}

void ContentPanel::render_side_view() {
  OX_SCOPED_ZONE;
  static int selectionMask = 0;

  constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX |
                                         ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_ScrollY;

  constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanFullWidth;

  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {0, 0});
  if (ImGui::BeginTable("SideViewTable", 1, tableFlags)) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGuiTreeNodeFlags nodeFlags = treeNodeFlags;
    const bool selected = m_current_directory == m_assets_directory && selectionMask == 0;
    if (selected) {
      nodeFlags |= ImGuiTreeNodeFlags_Selected;
      ImGui::PushStyleColor(ImGuiCol_Header, ImGuiLayer::header_selected_color);
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGuiLayer::header_selected_color);
    } else {
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGuiLayer::header_hovered_color);
    }

    const bool opened = ImGui::TreeNodeEx(m_assets_directory.string().c_str(), nodeFlags, "");
    // bool clickedTree = false;
    // if (ImGui::IsItemClicked())
    // clickedTree = true;
    ImGui::PopStyleColor(selected ? 2 : 1);

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      update_directory_entries(m_assets_directory);
      selectionMask = 0;
    }
    const char8_t* folderIcon = opened ? ICON_MDI_FOLDER_OPEN : ICON_MDI_FOLDER;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiLayer::asset_icon_color);
    ImGui::TextUnformatted(StringUtils::from_char8_t(folderIcon));
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextUnformatted("Assets");

    if (opened) {
      uint32_t count = 0;
      // for ([[maybe_unused]] const auto& entry : std::filesystem::recursive_directory_iterator(m_AssetsDirectory))
      //   count++;

      const auto [isClicked, clickedNode] = directory_tree_view_recursive(m_assets_directory, &count, &selectionMask, treeNodeFlags);

      if (isClicked) {
        // (process outside of tree loop to avoid visual inconsistencies during the clicking frame)
        if (ImGui::GetIO().KeyCtrl)
          selectionMask ^= BIT(clickedNode); // CTRL+click to toggle
        else
          selectionMask = BIT(clickedNode);  // Click to single-select
      }

      ImGui::TreePop();
    }
    ImGui::EndTable();
    if (ImGui::IsItemClicked())
      EditorLayer::get()->reset_context();
  }

  ImGui::PopStyleVar();
}

void ContentPanel::render_body(bool grid) {
  std::filesystem::path directory_to_open;

  constexpr float padding = 2.0f;
  const float scaled_thumbnail_size = EditorCVar::cvar_file_thumbnail_size.get() * ImGui::GetIO().FontGlobalScale;
  const float scaled_thumbnail_size_x = scaled_thumbnail_size * 0.55f;
  const float cell_size = scaled_thumbnail_size_x + 2 * padding + scaled_thumbnail_size_x * 0.1f;

  constexpr float overlay_padding_y = 6.0f * padding;
  constexpr float thumbnail_padding = overlay_padding_y * 0.5f;
  const float thumb_image_size = scaled_thumbnail_size_x - thumbnail_padding;

  const ImVec2 background_thumbnail_size = {scaled_thumbnail_size_x + padding * 2, scaled_thumbnail_size};

  const float panel_width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize;
  int column_count = static_cast<int>(panel_width / cell_size);
  if (column_count < 1)
    column_count = 1;

  float line_height = ImGui::GetTextLineHeight();
  int flags = ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_ScrollY;

  if (!grid) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {0, 0});
    column_count = 1;
    flags |= ImGuiTableFlags_RowBg | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_SizingStretchSame;
  } else {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {scaled_thumbnail_size_x * 0.05f, scaled_thumbnail_size_x * 0.05f});
    flags |= ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingFixedFit;
  }

  ImVec2 cursor_pos = ImGui::GetCursorPos();
  const ImVec2 region = ImGui::GetContentRegionAvail();
  ImGui::InvisibleButton("##DragDropTargetAssetPanelBody", region);

  ImGui::SetItemAllowOverlap();
  ImGui::SetCursorPos(cursor_pos);

  if (ImGui::BeginTable("BodyTable", column_count, flags)) {
    bool any_item_hovered = false;

    int i = 0;

    for (auto& file : m_directory_entries) {
      if (!m_filter.PassFilter(file.name.c_str()))
        continue;

      ImGui::PushID(i);

      const bool is_dir = file.is_directory;
      const char* filename = file.name.c_str();

      std::string texture_name = "folder_icon";
      if (!is_dir) {
        if ((file.type == FileType::Texture || file.type == FileType::Cubemap) && EditorCVar::cvar_file_thumbnails.get()) {
          if (thumbnail_cache.contains(file.file_path)) {
            texture_name = file.file_path;
          } else {
            const auto& file_path = file.file_path;
            // make sure this runs only if it's not already queued
            if (ThreadManager::get()->asset_thread.get_queue_size() == 0) {
              ThreadManager::get()->asset_thread.queue_job([this, file_path] {
                auto thumbnail_texture = create_shared<Texture>();
                thumbnail_texture->load(TextureLoadInfo{.path = file_path, .preset = Preset::eRTT2DUnmipped});
                thumbnail_cache.emplace(file_path, thumbnail_texture);
              });
            }
            texture_name = file.file_path;
          }
        } else if (file.type == FileType::Model) {
          texture_name = "mesh_icon";
        } else {
          texture_name = "file_icon";
        }
      }

      ImGui::TableNextColumn();

      const auto& path = file.directory_entry.path();
      std::string strPath = path.string();

      if (grid) {
        cursor_pos = ImGui::GetCursorPos();

        bool highlight = false;
        const EditorContext& context = EditorLayer::get()->get_context();
        if (context.is_valid(EditorContextType::File)) {
          highlight = file.file_path == context.as<char>();
        }

        // Background button
        static std::string id = "###";
        id[2] = static_cast<char>(i);
        const bool clicked = ui::toggle_button(id.c_str(), highlight, background_thumbnail_size, 0.1f);
        if (m_elapsed_time > 0.25f && clicked) {
          EditorLayer::get()->set_context_as_file_with_path(strPath);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGuiLayer::popup_item_spacing);
        if (ImGui::BeginPopupContextItem()) {
          if (ImGui::MenuItem("Delete")) {
            m_directory_to_delete = path;
            ImGui::CloseCurrentPopup();
          }
          if (ImGui::MenuItem("Rename")) {
            ImGui::CloseCurrentPopup();
          }

          ImGui::Separator();

          draw_context_menu_items(path, is_dir);
          ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        if (is_dir)
          drag_drop_target(file.file_path);

        drag_drop_from(file.file_path);

        if (ImGui::IsItemHovered())
          any_item_hovered = true;

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          if (is_dir) {
            directory_to_open = path;
            m_filter.Clear();
          } else
            open_file(path);
          EditorLayer::get()->reset_context();
        }

        // Foreground Image
        ImGui::SetCursorPos({cursor_pos.x + padding, cursor_pos.y + padding});
        ImGui::SetItemAllowOverlap();
        ui::image(*_white_texture->get_view(),
                    {background_thumbnail_size.x - padding * 2.0f, background_thumbnail_size.y - padding * 2.0f},
                    {},
                    {},
                    ImGuiLayer::window_bg_alternative_color);

        // Thumbnail Image
        ImGui::SetCursorPos({cursor_pos.x + thumbnail_padding * 0.75f, cursor_pos.y + thumbnail_padding});
        ImGui::SetItemAllowOverlap();
        if (thumbnail_cache.contains(texture_name))
          ui::image(*thumbnail_cache[texture_name]->get_view(), {thumb_image_size, thumb_image_size});

        // Type Color frame
        const ImVec2 type_color_frame_size = {scaled_thumbnail_size_x, scaled_thumbnail_size_x * 0.03f};
        ImGui::SetCursorPosX(cursor_pos.x + padding);
        ui::image(*_white_texture->get_view(),
                    type_color_frame_size,
                    {0, 0},
                    {1, 1},
                    is_dir ? ImVec4(0.0f, 0.0f, 0.0f, 0.0f) : file.file_type_indicator_color);

        const ImVec2 rect_min = ImGui::GetItemRectMin();
        const ImVec2 rect_size = ImGui::GetItemRectSize();
        const ImRect clip_rect = ImRect({rect_min.x + padding * 1.0f, rect_min.y + padding * 2.0f},
                                        {rect_min.x + rect_size.x, rect_min.y + scaled_thumbnail_size_x - ImGuiLayer::regular_font->FontSize * 2.0f});
        ui::clipped_text(clip_rect.Min, clip_rect.Max, filename, nullptr, nullptr, {0, 0}, nullptr, clip_rect.GetSize().x);

        if (!is_dir) {
          constexpr auto y_pos_pad = 10.f;
          ImGui::SetCursorPos({cursor_pos.x + padding * 2.0f,
                               cursor_pos.y + background_thumbnail_size.y - ImGuiLayer::small_font->FontSize * 2.0f + y_pos_pad});
          ImGui::BeginDisabled();
          ImGui::PushFont(ImGuiLayer::small_font);
          ImGui::TextUnformatted(file.file_type_string.data());
          ImGui::PopFont();
          ImGui::EndDisabled();
        }
      } else {
        constexpr ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf;

        const bool opened = ImGui::TreeNodeEx(file.name.c_str(), tree_node_flags, "");

        if (ImGui::IsItemHovered())
          any_item_hovered = true;

        if (is_dir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          directory_to_open = path;
          m_filter.Clear();
        }

        drag_drop_from(file.file_path.c_str());

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - line_height);
        if (thumbnail_cache.contains(texture_name))
          ui::image(*thumbnail_cache[texture_name]->get_view(), {line_height, line_height});
        ImGui::SameLine();
        ImGui::TextUnformatted(filename);

        if (opened)
          ImGui::TreePop();
      }

      ImGui::PopID();
      ++i;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGuiLayer::popup_item_spacing);
    if (ImGui::BeginPopupContextWindow("AssetPanelHierarchyContextWindow", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
      EditorLayer::get()->reset_context();
      draw_context_menu_items(m_current_directory, true);
      ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    ImGui::EndTable();

    if (!any_item_hovered && ImGui::IsItemClicked())
      EditorLayer::get()->reset_context();
  }

  ImGui::PopStyleVar();

  if (!m_directory_to_delete.empty()) {
    if (!ImGui::IsPopupOpen("Delete?"))
      ImGui::OpenPopup("Delete?");
  }

  if (ImGui::BeginPopupModal("Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("%s will be deleted. \nAre you sure? This operation cannot be undone!\n\n", m_directory_to_delete.string().c_str());
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      std::filesystem::remove_all(m_directory_to_delete);
      m_directory_to_delete.clear();
      ThreadManager::get()->asset_thread.queue_job([this] { refresh(); });
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
      m_directory_to_delete.clear();
    }
    EditorLayer::get()->reset_context();
    ImGui::EndPopup();
  }

  if (!directory_to_open.empty())
    update_directory_entries(directory_to_open);
}

void ContentPanel::update_directory_entries(const std::filesystem::path& directory) {
  OX_SCOPED_ZONE;
  std::lock_guard lock(m_directory_mutex);
  m_current_directory = directory;
  m_directory_entries.clear();

  if (directory.empty())
    return;

  const auto directory_it = std::filesystem::directory_iterator(directory);
  for (auto& directory_entry : directory_it) {
    const auto& path = directory_entry.path();
    const auto relative_path = relative(path, m_assets_directory);
    const std::string filename = relative_path.filename().string();
    const std::string extension = relative_path.extension().string();

    auto file_type = FileType::Unknown;
    const auto& file_type_it = FILE_TYPES.find(extension);
    if (file_type_it != FILE_TYPES.end())
      file_type = file_type_it->second;

    std::string_view file_type_string = FILE_TYPES_TO_STRING.at(FileType::Unknown);
    const auto& file_string_type_it = FILE_TYPES_TO_STRING.find(file_type);
    if (file_string_type_it != FILE_TYPES_TO_STRING.end())
      file_type_string = file_string_type_it->second;

    ImVec4 file_type_color = {1.0f, 1.0f, 1.0f, 1.0f};
    const auto& file_type_color_it = TYPE_COLORS.find(file_type);
    if (file_type_color_it != TYPE_COLORS.end())
      file_type_color = file_type_color_it->second;

    File entry =
      {filename, path.string(), extension, directory_entry, nullptr, directory_entry.is_directory(), file_type, file_type_string, file_type_color};

    m_directory_entries.push_back(entry);
  }

  m_elapsed_time = 0.0f;
}

void ContentPanel::draw_context_menu_items(const std::filesystem::path& context, bool isDir) {
  if (isDir) {
    if (ImGui::BeginMenu("Create")) {
      if (ImGui::MenuItem("Folder")) {
        int i = 0;
        bool created = false;
        std::string newFolderPath;
        while (!created) {
          std::string folderName = "New Folder" + (i == 0 ? "" : fmt::format(" ({})", i));
          newFolderPath = (context / folderName).string();
          created = std::filesystem::create_directory(newFolderPath);
          ++i;
        }
        EditorLayer::get()->set_context_as_file_with_path(newFolderPath);
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndMenu();
    }
  }
  if (ImGui::MenuItem("Show in Explorer")) {
    fs::open_folder_select_file(context.string().c_str());
    ImGui::CloseCurrentPopup();
  }
  if (ImGui::MenuItem("Open")) {
    fs::open_file_externally(context.string().c_str());
    ImGui::CloseCurrentPopup();
  }
  if (ImGui::MenuItem("Copy Path")) {
    ImGui::SetClipboardText(context.string().c_str());
    ImGui::CloseCurrentPopup();
  }

  if (isDir) {
    if (ImGui::MenuItem("Refresh")) {
      refresh();
      ImGui::CloseCurrentPopup();
    }
  }
}
} // namespace ox
