#pragma once

#include <filesystem>
#include <stack>
#include <vector>

#include <imgui.h>
#include <mutex>

#include <ankerl/unordered_dense.h>

#include "EditorPanel.hpp"

#include "Core/Base.hpp"

namespace ox {
class Texture;

enum class FileType {
  Unknown = 0,
  Scene,
  Prefab,
  Shader,
  Texture,
  Cubemap,
  Model,
  Audio,
  Material,
  Script
};

class ContentPanel : public EditorPanel {
public:
  ContentPanel();

  ~ContentPanel() override = default;

  ContentPanel(const ContentPanel& other) = delete;
  ContentPanel(ContentPanel&& other) = delete;
  ContentPanel& operator=(const ContentPanel& other) = delete;
  ContentPanel& operator=(ContentPanel&& other) = delete;

  void init();
  void on_update() override;
  void on_imgui_render() override;

  void invalidate();

private:
  std::pair<bool, uint32_t> directory_tree_view_recursive(const std::filesystem::path& path, uint32_t* count, int* selectionMask, ImGuiTreeNodeFlags flags);
  void render_header();
  void render_side_view();
  void render_body(bool grid);
  void update_directory_entries(const std::filesystem::path& directory);
  void refresh() { update_directory_entries(m_current_directory); }

  void draw_context_menu_items(const std::filesystem::path& context, bool isDir);

private:
  struct File {
    std::string name;
    std::string file_path;
    std::string extension;
    std::filesystem::directory_entry directory_entry;
    Shared<Texture> thumbnail = nullptr;
    bool is_directory = false;

    FileType type;
    std::string_view file_type_string;
    ImVec4 file_type_indicator_color;
  };

  std::filesystem::path m_assets_directory;
  std::filesystem::path m_current_directory;
  std::stack<std::filesystem::path> m_back_stack;
  std::vector<File> m_directory_entries;
  std::mutex m_directory_mutex;
  uint32_t m_currently_visible_items_tree_view = 0;
  float thumbnail_max_limit = 256.0f;
  float thumbnail_size_grid_limit = 96.0f; // lower values than this will switch to grid view
  ImGuiTextFilter m_filter;
  float m_elapsed_time = 0.0f;

  ankerl::unordered_dense::map<std::string, Shared<Texture>> thumbnail_cache;

  Shared<Texture> _white_texture;
  std::filesystem::path m_directory_to_delete;
};
}
