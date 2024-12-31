#pragma once

#include "ESystem.hpp"
#include "Keycodes.hpp"

#include "ApplicationEvents.hpp"
#include "Types.hpp"

typedef struct GLFWcursor GLFWcursor;
typedef struct GLFWwindow GLFWwindow;

namespace ox {
class Input : public ESystem {
public:
  enum class CursorState { Disabled = 0x00034003, Normal = 0x00034001, Hidden = 0x00034002 };

  void init() override;
  void deinit() override;

  void set_instance();

  void reset_pressed();
  void reset();

  static void on_key_pressed_event(const KeyPressedEvent& event);
  static void on_key_released_event(const KeyReleasedEvent& event);
  static void on_mouse_pressed_event(const MouseButtonPressedEvent& event);
  static void on_mouse_button_released_event(const MouseButtonReleasedEvent& event);

  /// Keyboard
  static bool get_key_pressed(const KeyCode key) { return _instance->input_data.key_pressed[int(key)]; }
  static bool get_key_released(const KeyCode key) { return _instance->input_data.key_released[int(key)]; }
  static bool get_key_held(const KeyCode key) { return _instance->input_data.key_held[int(key)]; }
  static bool get_mouse_clicked(const MouseCode key) { return _instance->input_data.mouse_clicked[int(key)]; }
  static bool get_mouse_held(const MouseCode key) { return _instance->input_data.mouse_held[int(key)]; }

  /// Gamepad
  static bool get_gamepad_button_pressed(const GamepadButtonCode button) { return false; }
  static bool get_gamepad_button_released(const GamepadButtonCode button) { return false; }
  /// @return -1.0, 1.0 (inclusive)
  static float get_gamepad_axis(const GamepadAxisCode axis) { return 0.0f; }
  static const char* get_gamepad_name(const int32 joystick_id);
  static bool is_joystick_gamepad(const int32 joystick_id);

  /// Mouse
  static Vec2 get_mouse_position();
  static void set_mouse_position(float x, float y);
  static float get_mouse_offset_x();
  static float get_mouse_offset_y();
  static float get_mouse_scroll_offset_y();

  /// Cursor
  static CursorState get_cursor_state() { return _instance->cursor_state; }
  static void set_cursor_state(CursorState state);
  static GLFWcursor* load_cursor_icon(const char* image_path);
  /// @param cursor https://www.glfw.org/docs/3.4/group__shapes.html
  static GLFWcursor* load_cursor_icon_standard(int cursor);
  static void set_cursor_icon(GLFWcursor* cursor);
  static void set_cursor_icon_default();
  static void destroy_cursor(GLFWcursor* cursor);

private:
#define MAX_KEYS 512
#define MAX_BUTTONS 32

  static Input* _instance;

  struct InputData {
    bool key_pressed[MAX_KEYS] = {};
    bool key_released[MAX_KEYS] = {};
    bool key_held[MAX_KEYS] = {};
    bool mouse_held[MAX_BUTTONS] = {};
    bool mouse_clicked[MAX_BUTTONS] = {};

    float mouse_offset_x;
    float mouse_offset_y;
    float scroll_offset_y;
    Vec2 mouse_pos;
  } input_data = {};

  CursorState cursor_state = CursorState::Normal;

  void set_key_pressed(const KeyCode key, const bool a) { input_data.key_pressed[int(key)] = a; }
  void set_key_released(const KeyCode key, const bool a) { input_data.key_released[int(key)] = a; }
  void set_key_held(const KeyCode key, const bool a) { input_data.key_held[int(key)] = a; }
  void set_mouse_clicked(const MouseCode key, const bool a) { input_data.mouse_clicked[int(key)] = a; }
  void set_mouse_held(const MouseCode key, const bool a) { input_data.mouse_held[int(key)] = a; }

  static void cursor_pos_callback(GLFWwindow* window, double xpos_in, double ypos_in);
  static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
};
} // namespace ox
