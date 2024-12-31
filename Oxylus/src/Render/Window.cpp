#include "Window.hpp"

#include "Core/EmbeddedLogo.hpp"
#include "Utils/Log.hpp"


#include "stb_image.h"

#include "Core/ApplicationEvents.hpp"

#include "GLFW/glfw3.h"

#include "Utils/Profiler.hpp"

namespace ox {
Window::WindowData Window::s_window_data;
GLFWwindow* Window::s_window_handle;

void Window::init_window(const AppSpec& spec) {
  OX_SCOPED_ZONE;
  glfwInit();

  const auto monitor_size = get_monitor_size();

  auto window_width = (float)monitor_size.x * 0.8f;
  auto window_height = (float)monitor_size.y * 0.8f;

  if (spec.default_window_size.x != 0 && spec.default_window_size.y != 0) { 
    window_width = spec.default_window_size.x;
    window_height = spec.default_window_size.y;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  s_window_handle = glfwCreateWindow((int)window_width, (int)window_height, spec.name.c_str(), nullptr, nullptr);

  // center window
  const auto center = get_center_pos((int)window_width, (int)window_height);
  glfwSetWindowPos(s_window_handle, center.x, center.y);

  // Load file icon
  {
    int width, height, channels;
    const auto image_data = stbi_load_from_memory(EngineLogo, (int)EngineLogoLen, &width, &height, &channels, 4);
    const GLFWimage window_icon{
      .width = 40,
      .height = 40,
      .pixels = image_data,
    };
    glfwSetWindowIcon(s_window_handle, 1, &window_icon);
    stbi_image_free(image_data);
  }
  if (s_window_handle == nullptr) {
    OX_LOG_ERROR("Failed to create GLFW WindowHandle");
    glfwTerminate();
  }

  glfwSetWindowCloseCallback(s_window_handle, close_window);
}

void Window::poll_events() {
  OX_SCOPED_ZONE;
  glfwPollEvents();
}

void Window::close_window(GLFWwindow*) {
  App::get()->close();
  glfwTerminate();
}

void Window::set_window_user_data(void* data) { glfwSetWindowUserPointer(get_glfw_window(), data); }

GLFWwindow* Window::get_glfw_window() {
  if (s_window_handle == nullptr) {
    OX_LOG_ERROR("Glfw WindowHandle is nullptr. Did you call InitWindow() ?");
  }
  return s_window_handle;
}

uint32_t Window::get_width() {
  int width, height;
  glfwGetWindowSize(s_window_handle, &width, &height);
  return (uint32_t)width;
}

uint32_t Window::get_height() {
  int width, height;
  glfwGetWindowSize(s_window_handle, &width, &height);
  return (uint32_t)height;
}

Vec2 Window::get_content_scale(GLFWmonitor* monitor) {
  float xscale, yscale;
  glfwGetMonitorContentScale(monitor == nullptr ? glfwGetPrimaryMonitor() : monitor, &xscale, &yscale);
  return {xscale, yscale};
}

IVec2 Window::get_monitor_size(GLFWmonitor* monitor) {
  int width, height;
  glfwGetMonitorWorkarea(monitor == nullptr ? glfwGetPrimaryMonitor() : monitor, nullptr, nullptr, &width, &height);
  return {width, height};
}

IVec2 Window::get_center_pos(const int width, const int height) {
  int32_t monitor_width = 0, monitor_height = 0;
  int32_t monitor_posx = 0, monitor_posy = 0;
  glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &monitor_posx, &monitor_posy, &monitor_width, &monitor_height);
  const auto video_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

  return {monitor_posx + (video_mode->width - width) / 2, monitor_posy + (video_mode->height - height) / 2};
}

bool Window::is_focused() { return glfwGetWindowAttrib(get_glfw_window(), GLFW_FOCUSED); }

bool Window::is_minimized() { return glfwGetWindowAttrib(get_glfw_window(), GLFW_ICONIFIED); }

void Window::minimize() { glfwIconifyWindow(s_window_handle); }

void Window::maximize() { glfwMaximizeWindow(s_window_handle); }

bool Window::is_maximized() { return glfwGetWindowAttrib(s_window_handle, GLFW_MAXIMIZED); }

void Window::restore() { glfwRestoreWindow(s_window_handle); }

bool Window::is_decorated() { return (bool)glfwGetWindowAttrib(s_window_handle, GLFW_DECORATED); }

void Window::set_undecorated() { glfwSetWindowAttrib(s_window_handle, GLFW_DECORATED, false); }

void Window::set_decorated() { glfwSetWindowAttrib(s_window_handle, GLFW_DECORATED, true); }

bool Window::is_floating() { return (bool)glfwGetWindowAttrib(s_window_handle, GLFW_FLOATING); }

void Window::set_floating() { glfwSetWindowAttrib(s_window_handle, GLFW_FLOATING, true); }

void Window::set_not_floating() { glfwSetWindowAttrib(s_window_handle, GLFW_FLOATING, false); }

bool Window::is_fullscreen_borderless() { return s_window_data.is_fullscreen_borderless; }

void Window::set_fullscreen_borderless() {
  auto* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  glfwSetWindowMonitor(s_window_handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
  s_window_data.is_fullscreen_borderless = true;
}

void Window::set_windowed() {
  auto* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  const auto center = get_center_pos(1600, 900); // TODO: why is this harcoded lol
  glfwSetWindowMonitor(s_window_handle, nullptr, center.x, center.y, 1600, 900, mode->refreshRate);
  s_window_data.is_fullscreen_borderless = false;
}

void Window::wait_for_events() {
  OX_SCOPED_ZONE;
  glfwWaitEvents();
}
} // namespace ox
