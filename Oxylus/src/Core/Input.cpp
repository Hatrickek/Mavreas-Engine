#include "Input.hpp"
#include "ApplicationEvents.hpp"
#include "Render/Window.hpp"
#include "Types.hpp"
#include "stb_image.h"

#include "GLFW/glfw3.h"

namespace ox {
Input* Input::_instance = nullptr;

void Input::init() {
  glfwSetCursorPosCallback(Window::get_glfw_window(), cursor_pos_callback);
  glfwSetScrollCallback(Window::get_glfw_window(), scroll_callback);

  glfwSetKeyCallback(Window::get_glfw_window(), [](GLFWwindow*, const int key, int, const int action, int) {
    switch (action) {
      case GLFW_PRESS: {
        Window::get_dispatcher()->trigger(KeyPressedEvent((KeyCode)key, 0));
        break;
      }
      case GLFW_RELEASE: {
        Window::get_dispatcher()->trigger(KeyReleasedEvent((KeyCode)key));
        break;
      }
      case GLFW_REPEAT: {
        Window::get_dispatcher()->trigger(KeyPressedEvent((KeyCode)key, 1));
        break;
      }
    }
  });

  glfwSetMouseButtonCallback(Window::get_glfw_window(), [](GLFWwindow*, int button, int action, int) {
    switch (action) {
      case GLFW_PRESS: {
        Window::get_dispatcher()->trigger(MouseButtonPressedEvent((MouseCode)button));
        break;
      }
      case GLFW_RELEASE: {
        Window::get_dispatcher()->trigger(MouseButtonReleasedEvent((MouseCode)button));
        break;
      }
    }
  });

  glfwSetJoystickCallback([](int jid, int event) {
    Window::get_dispatcher()->trigger(JoystickConfigCallback{
      .event = (JoystickConfigCallback::Event)event,
      .joystick_id = jid,
    });
  });

  Window::get_dispatcher()->sink<KeyPressedEvent>().connect<&Input::on_key_pressed_event>();
  Window::get_dispatcher()->sink<KeyReleasedEvent>().connect<&Input::on_key_released_event>();
  Window::get_dispatcher()->sink<MouseButtonPressedEvent>().connect<&Input::on_mouse_pressed_event>();
  Window::get_dispatcher()->sink<MouseButtonReleasedEvent>().connect<&Input::on_mouse_button_released_event>();
}

void Input::deinit() {}

void Input::set_instance() {
  if (_instance == nullptr)
    _instance = App::get_system<Input>();
}

void Input::reset_pressed() {
  memset(input_data.key_pressed, 0, MAX_KEYS);
  memset(input_data.mouse_clicked, 0, MAX_BUTTONS);
  input_data.scroll_offset_y = 0;
}

void Input::reset() {
  memset(input_data.key_held, 0, MAX_KEYS);
  memset(input_data.key_pressed, 0, MAX_KEYS);
  memset(input_data.mouse_clicked, 0, MAX_BUTTONS);
  memset(input_data.mouse_held, 0, MAX_BUTTONS);

  input_data.scroll_offset_y = 0;
}

void Input::on_key_pressed_event(const KeyPressedEvent& event) {
  _instance->set_key_pressed(event.get_key_code(), event.get_repeat_count() < 1);
  _instance->set_key_released(event.get_key_code(), false);
  _instance->set_key_held(event.get_key_code(), true);
}

void Input::on_key_released_event(const KeyReleasedEvent& event) {
  _instance->set_key_pressed(event.get_key_code(), false);
  _instance->set_key_released(event.get_key_code(), true);
  _instance->set_key_held(event.get_key_code(), false);
}

void Input::on_mouse_pressed_event(const MouseButtonPressedEvent& event) {
  _instance->set_mouse_clicked(event.get_mouse_button(), true);
  _instance->set_mouse_held(event.get_mouse_button(), true);
}

void Input::on_mouse_button_released_event(const MouseButtonReleasedEvent& event) {
  _instance->set_mouse_clicked(event.get_mouse_button(), false);
  _instance->set_mouse_held(event.get_mouse_button(), false);
}

const char* Input::get_gamepad_name(int32 joystick_id) { return glfwGetGamepadName(joystick_id); }

bool Input::is_joystick_gamepad(int32 joystick_id) { return glfwJoystickIsGamepad(joystick_id); }

Vec2 Input::get_mouse_position() { return _instance->input_data.mouse_pos; }

float Input::get_mouse_offset_x() { return _instance->input_data.mouse_offset_x; }

float Input::get_mouse_offset_y() { return _instance->input_data.mouse_offset_y; }

float Input::get_mouse_scroll_offset_y() { return _instance->input_data.scroll_offset_y; }

void Input::set_mouse_position(const float x, const float y) {
  glfwSetCursorPos(Window::get_glfw_window(), x, y);
  _instance->input_data.mouse_pos.x = x;
  _instance->input_data.mouse_pos.y = y;
}

GLFWcursor* Input::load_cursor_icon(const char* image_path) {
  int width, height, channels = 4;
  const auto image_data = stbi_load(image_path, &width, &height, &channels, STBI_rgb_alpha);
  const GLFWimage* cursor_image = new GLFWimage{.width = width, .height = height, .pixels = image_data};
  const auto cursor = glfwCreateCursor(cursor_image, 0, 0);
  stbi_image_free(image_data);

  return cursor;
}

GLFWcursor* Input::load_cursor_icon_standard(const int cursor) { return glfwCreateStandardCursor(cursor); }

void Input::set_cursor_icon(GLFWcursor* cursor) { glfwSetCursor(Window::get_glfw_window(), cursor); }

void Input::set_cursor_icon_default() { glfwSetCursor(Window::get_glfw_window(), nullptr); }

void Input::set_cursor_state(const CursorState state) {
  auto window = Window::get_glfw_window();
  switch (state) {
    case CursorState::Disabled:
      _instance->cursor_state = CursorState::Disabled;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      break;
    case CursorState::Normal:
      _instance->cursor_state = CursorState::Normal;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      break;
    case CursorState::Hidden:
      _instance->cursor_state = CursorState::Hidden;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
      break;
  }
}

void Input::destroy_cursor(GLFWcursor* cursor) { glfwDestroyCursor(cursor); }

void Input::cursor_pos_callback(GLFWwindow* window, const double xpos_in, const double ypos_in) {
  _instance->input_data.mouse_offset_x = _instance->input_data.mouse_pos.x - static_cast<float>(xpos_in);
  _instance->input_data.mouse_offset_y = _instance->input_data.mouse_pos.y - static_cast<float>(ypos_in);
  _instance->input_data.mouse_pos = glm::vec2{static_cast<float>(xpos_in), static_cast<float>(ypos_in)};
}

void Input::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) { _instance->input_data.scroll_offset_y = (float)yoffset; }
} // namespace ox
