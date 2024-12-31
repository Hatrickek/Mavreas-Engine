#include "LuaUIBindings.hpp"

#include <sol/state.hpp>

#include "LuaHelpers.hpp"
#include "LuaImGuiBindings.hpp"

#include "UI/OxUI.hpp"

namespace ox::LuaBindings {
void bind_ui(const Shared<sol::state>& state) {
  LuaImGuiBindings::init(state.get());

  auto uitbl = state->create_table("UI");
  uitbl.set_function("begin_properties",[] {return ui::begin_properties(ui::default_properties_flags);});
  uitbl.set_function("begin_properties_flags", [](const ImGuiTableFlags flags) { return ui::begin_properties(flags); });
  uitbl.set_function("end_properties", &ui::end_properties);
  uitbl.set_function("text", [](const char* text1, const char* text2) { ui::text(text1, text2); });
  uitbl.set_function("text_tooltip", &ui::text);
  uitbl.set_function("property_bool", [](const char* label, bool* flag) { ui::property(label, flag); });
  uitbl.set_function("property_bool_tooltip", [](const char* label, bool* flag, const char* tooltip) { ui::property(label, flag, tooltip); });
  uitbl.set_function("property_input_field", [](const char* label, std::string* text, ImGuiInputFlags flags) { ui::property(label, text, flags); });
  uitbl.set_function("property_input_field_tooltip", [](const char* label, std::string* text, ImGuiInputFlags flags, const char* tooltip) { ui::property(label, text, flags, tooltip); });

  // TODO: the rest of the api
}
}
