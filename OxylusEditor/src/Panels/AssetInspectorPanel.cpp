#include "AssetInspectorPanel.hpp"

#include <icons/IconsMaterialDesignIcons.h>

#include "Assets/AssetManager.hpp"
#include "EditorLayer.hpp"

namespace ox {
  AssetInspectorPanel::AssetInspectorPanel() : EditorPanel("AssetInspector", ICON_MDI_INFORMATION) { }

  void AssetInspectorPanel::on_update() { }

  void AssetInspectorPanel::on_imgui_render() {
    if (on_begin()) {
      if (EditorContextType::Asset == EditorLayer::get()->get_context().get_type()) {
        if (EditorLayer::get()->get_context().get_asset_extension() == ".oxmat") {
          DrawMaterialAsset(EditorLayer::get()->get_context().as<std::string>());
        }
      }
      on_end();
    }
  }

  void AssetInspectorPanel::DrawMaterialAsset(const std::string* path) {
    //TODO:
    //m_SelectedMaterial = AssetManager::GetMaterialAsset(*path).Data;
    //InspectorPanel::DrawMaterialProperties(m_SelectedMaterial, true);
  }
}
