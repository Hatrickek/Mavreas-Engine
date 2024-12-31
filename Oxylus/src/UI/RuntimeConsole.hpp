#pragma once

#include <imgui.h>
#include <functional>
#include <optional>
#include <charconv>

#include <ankerl/unordered_dense.h>

#include "Utils/Log.hpp"

namespace ox {
class RuntimeConsole {
public:
  struct ParsedCommandValue {
    std::string str_value = {};

    ParsedCommandValue(std::string str) : str_value(std::move(str)) {}

    template <typename T = int32_t>
    std::optional<T> as() const {
      T value = {};
      if (std::from_chars(str_value.c_str(), str_value.data() + str_value.size(), value).ec == std::errc{})
        return (T)value;
      return {};
    }
  };

  bool set_focus_to_keyboard_always = false;
  const char* panel_name = "RuntimeConsole";
  bool visible = false;
  std::string id = {};

  RuntimeConsole();
  ~RuntimeConsole();

  void register_command(const std::string& command, const std::string& on_succes_log, const std::function<void()>& action);
  void register_command(const std::string& command, const std::string& on_succes_log, int32_t* value);
  void register_command(const std::string& command, const std::string& on_succes_log, std::string* value);
  void register_command(const std::string& command, const std::string& on_succes_log, bool* value);

  void add_log(const char* fmt, loguru::Verbosity verb);
  void clear_log();

  void on_imgui_render();

private:
  struct ConsoleText {
    std::string text = {};
    loguru::Verbosity verbosity = {};
  };

  void render_console_text(const std::string& text, int32_t id, loguru::Verbosity verb);

  struct ConsoleCommand {
    int32_t* int_value = nullptr;
    std::string* str_value = nullptr;
    bool* bool_value = nullptr;
    std::function<void()> action = nullptr;
    std::string on_succes_log = {};
  };

  // Commands
  ankerl::unordered_dense::map<std::string, ConsoleCommand> command_map;
  void process_command(const std::string& command);

  void help_command();
  std::vector<std::string> get_available_commands();

  ParsedCommandValue parse_value(const std::string& command);
  std::string parse_command(const std::string& command);

  // Input field
  static constexpr uint32_t MAX_TEXT_BUFFER_SIZE = 32;
  int32_t history_position = -1;
  std::vector<ConsoleText> text_buffer = {};
  std::vector<std::string> input_log = {};
  bool request_scroll_to_bottom = true;
  bool request_keyboard_focus = true;
  bool auto_scroll = true;
  int input_text_callback(ImGuiInputTextCallbackData* data);

  loguru::Verbosity text_filter = loguru::Verbosity_OFF;
  float animation_counter = 0.0f;
};
}
