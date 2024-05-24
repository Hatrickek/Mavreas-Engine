#pragma once

#include <string>

#include "Event/Event.hpp"
#include "Keycodes.hpp"
#include "Utils/Timestep.hpp"

namespace ox {
class Layer {
public:
  Layer(const std::string& name = "Layer");
  virtual ~Layer() = default;

  virtual void on_attach(EventDispatcher& dispatcher) {}
  virtual void on_detach() {}
  virtual void on_update(const Timestep& delta_time) {}
  virtual void on_imgui_render() {}

  const std::string& get_name() const { return debug_name; }

protected:
  std::string debug_name;
};
} // namespace ox
