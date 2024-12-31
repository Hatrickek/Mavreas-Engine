#include "EditorSettingsPanel.hpp"

#include "icons/IconsMaterialDesignIcons.h"
#include "imgui.h"

#include "UI/ImGuiLayer.hpp"

namespace ox {
  EditorSettingsPanel::EditorSettingsPanel() : EditorPanel("Editor Settings", ICON_MDI_COGS, false) { }

  void EditorSettingsPanel::on_imgui_render() {
    constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
    if (on_begin(window_flags)) {
      //Theme
      const char* themes[] = {"Dark", "White"};
      int themeIndex = 0;
      if (ImGui::Combo("Theme", &themeIndex, themes, std::size(themes))) {
        ImGuiLayer::apply_theme(!(bool)themeIndex);
      }
      on_end();
    }
  }
}
