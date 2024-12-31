#pragma once
#include <ankerl/unordered_dense.h>

namespace ox {
class EditorTheme {
public:
  static ankerl::unordered_dense::map<size_t, const char8_t*> component_icon_map;

  static void init();
};
}
