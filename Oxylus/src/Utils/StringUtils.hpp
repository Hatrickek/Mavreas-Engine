#pragma once

#include <string>

namespace ox {
namespace StringUtils {
static void replace_string(std::string& subject, std::string_view search, std::string_view replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
}

static const char* from_char8_t(const char8_t* c) {
  return reinterpret_cast<const char*>(c);
}
};
}
