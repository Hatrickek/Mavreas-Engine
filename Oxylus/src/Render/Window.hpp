#pragma once
#include "Core/App.hpp"
#include "Core/Input.hpp"
#include "Event/Event.hpp"

typedef struct GLFWmonitor GLFWmonitor;

namespace ox {
class Window {
public:
  static void init_window(const AppSpec& spec);
  static void poll_events();
  static void close_window(GLFWwindow* window);

  static void set_window_user_data(void* data);

  static GLFWwindow* get_glfw_window();
  static uint32_t get_width();
  static uint32_t get_height();

  // ratio between the current DPI and the platform's default DPI
  static Vec2 get_content_scale(GLFWmonitor* monitor = nullptr);
  static IVec2 get_monitor_size(GLFWmonitor* monitor = nullptr);

  static IVec2 get_center_pos(int width, int height);

  static bool is_focused();
  static bool is_minimized();
  static void minimize();
  static void maximize();
  static bool is_maximized();
  static void restore();

  static bool is_decorated();
  static void set_undecorated();
  static void set_decorated();

  static bool is_floating();
  static void set_floating();
  static void set_not_floating();

  static bool is_fullscreen_borderless();
  static void set_fullscreen_borderless();

  static void set_windowed();

  static void wait_for_events();

  static void set_dispatcher(EventDispatcher* dispatcher) { s_window_data.dispatcher = dispatcher; }
  static EventDispatcher* get_dispatcher() { return s_window_data.dispatcher; }

private:
  static struct WindowData {
    EventDispatcher* dispatcher;
    bool is_fullscreen_borderless = false;
  } s_window_data;

  static GLFWwindow* s_window_handle;
};
} // namespace ox
