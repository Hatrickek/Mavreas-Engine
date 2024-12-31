#pragma once
#include "Base.hpp"

#include "Event/Event.hpp"

namespace ox {
/// Engine systems interface
class ESystem {
public:
  ESystem() = default;
  virtual ~ESystem() = default;
  DELETE_DEFAULT_CONSTRUCTORS(ESystem)

  virtual void init() = 0;
  virtual void deinit() = 0;

  virtual void update() {}
  virtual void imgui_update() {}

  void set_dispatcher(EventDispatcher* dispatcher) { m_dispatcher = dispatcher; }

protected:
  EventDispatcher* m_dispatcher = nullptr;
};
}
