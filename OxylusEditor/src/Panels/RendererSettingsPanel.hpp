#pragma once

#include <vector>

#include "EditorPanel.hpp"

namespace ox {
  class RendererSettingsPanel : public EditorPanel {
  public:
    RendererSettingsPanel();
    void on_imgui_render() override;

  private:
    float m_FpsValues[50] = {};
    std::vector<float> m_FrameTimes{};
  };
}
