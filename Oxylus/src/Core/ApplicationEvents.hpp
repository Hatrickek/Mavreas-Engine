#pragma once
#include "Keycodes.hpp"

namespace ox {
class KeyEvent {
public:
  KeyCode get_key_code() const { return key_code; }

protected:
  KeyEvent(const KeyCode keycode) : key_code(keycode) {}

  KeyCode key_code;
};

struct KeyPressedEvent : KeyEvent {
public:
  KeyPressedEvent(const KeyCode keycode, const int repeat_count) : KeyEvent(keycode), repeat_count(repeat_count) {}

  int get_repeat_count() const { return repeat_count; }

  int repeat_count;
};

struct KeyReleasedEvent : KeyEvent {
  KeyReleasedEvent(const KeyCode keycode) : KeyEvent(keycode) {}
};

class MouseButtonEvent {
public:
  MouseCode get_mouse_button() const { return m_button; }

protected:
  MouseButtonEvent(const MouseCode button) : m_button(button) {}

  MouseCode m_button;
};

struct MouseButtonPressedEvent : MouseButtonEvent {
  MouseButtonPressedEvent(const MouseCode button) : MouseButtonEvent(button) {}
};

struct MouseButtonReleasedEvent : MouseButtonEvent {
  MouseButtonReleasedEvent(const MouseCode button) : MouseButtonEvent(button) {}
};

struct JoystickConfigCallback {
  enum class Event { Disconnected = 0x00040002, Connected = 0x00040001 } event;

  int joystick_id = 0;
};
} // namespace ox
