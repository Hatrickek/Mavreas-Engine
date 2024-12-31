#pragma once

#include "Core/Base.hpp"
#include "EditorPanel.hpp"

namespace ox {
  class Material;

  class AssetInspectorPanel : public EditorPanel {
  public:
    AssetInspectorPanel();
    void on_update() override;
    void on_imgui_render() override;

  private:
    void DrawMaterialAsset(const std::string* path);

    Shared<Material> m_SelectedMaterial;
  };
}
