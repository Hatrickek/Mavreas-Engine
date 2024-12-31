#pragma once
#include <ankerl/unordered_dense.h>

#include "EditorPanel.hpp"

#include "Core/Base.hpp"

namespace vuk {
struct RenderGraph;
}

namespace ox {
class Scene;

class RenderGraphPanel : public EditorPanel {
public:
  RenderGraphPanel();
  ~RenderGraphPanel() override = default;

  void on_imgui_render() override;
  void set_context(const Shared<Scene>& scene) { context = scene; }

private:
  Shared<Scene> context = nullptr;
  ankerl::unordered_dense::map<std::string, Shared<vuk::RenderGraph>> rg_map = {};
};
}
