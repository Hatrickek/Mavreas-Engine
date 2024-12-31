#pragma once

#include "EditorPanel.hpp"

namespace ox {
class ProjectPanel : public EditorPanel {
public:
  ProjectPanel();
  ~ProjectPanel() override = default;

  void on_update() override;
  void on_imgui_render() override;

  void load_project_for_editor(const std::string& filepath);

private:
  static void new_project(const std::string& project_dir, const std::string& project_name, const std::string& project_asset_dir);
};
}
