#pragma once

#include <shared_mutex>

#include <ankerl/unordered_dense.h>

#include "Log.hpp"

#include "Core/Base.hpp"

namespace ox {
enum class CVarFlags : uint32_t {
  None         = 0,
  Noedit       = 1 << 1,
  EditReadOnly = 1 << 2,
  Advanced     = 1 << 3,
  Dropdown     = 1 << 4,

  EditCheckbox  = 1 << 8,
  EditFloatDrag = 1 << 9,
  EditIntDrag   = 1 << 10,
};

enum class CVarType : char {
  INT,
  FLOAT,
  STRING,
};

class CVarParameter {
public:
  uint32_t array_index;

  CVarType type;
  CVarFlags flags;
  std::string name;
  std::string description;
};

template <typename T>
struct CVarStorage {
  T initial;
  T current;
  CVarParameter* parameter;
};

class CVarSystem {
public:
  constexpr static int MAX_INT_CVARS = 1000;
  constexpr static int MAX_FLOAT_CVARS = 1000;
  constexpr static int MAX_STRING_CVARS = 200;

  std::vector<CVarStorage<int32_t>> int_cvars = {};
  std::vector<CVarStorage<float>> float_cvars = {};
  std::vector<CVarStorage<std::string>> string_cvars = {};

  CVarSystem() = default;

  static CVarSystem* get();
  CVarParameter* get_cvar(uint32_t hash);

  CVarParameter* create_float_cvar(const char* name, const char* description, float default_value, float current_value);
  CVarParameter* create_int_cvar(const char* name, const char* description, int32_t default_value, int32_t current_value);
  CVarParameter* create_string_cvar(const char* name, const char* description, const char* default_value, const char* current_value);

  float* get_float_cvar(uint32_t hash);
  int32_t* get_int_cvar(uint32_t hash);
  std::string* get_string_cvar(uint32_t hash);

  void set_float_cvar(uint32_t hash, float value);
  void set_int_cvar(uint32_t hash, int32_t value);
  void set_string_cvar(uint32_t hash, const char* value);

private:
  std::shared_mutex mutex_;
  ankerl::unordered_dense::map<uint32_t, Unique<CVarParameter>> saved_cvars;
  std::vector<CVarParameter*> cached_edit_parameters;

  CVarParameter* init_cvar(const char* name, const char* description);
};

template <typename T>
struct AutoCVar {
protected:
  uint32_t index;
  using CVarType = T;
};

struct AutoCVar_Float : AutoCVar<float> {
  AutoCVar_Float(const char* name, const char* description, float default_value, CVarFlags flags = CVarFlags::None);

  float get() const;
  float* get_ptr() const;
  void set(float val) const;
};

struct AutoCVar_Int : AutoCVar<int32_t> {
  AutoCVar_Int(const char* name, const char* description, int32_t default_value, CVarFlags flags = CVarFlags::None);
  int32_t get() const;
  int32_t* get_ptr() const;
  void set(int32_t val) const;

  void toggle() const;
};

struct AutoCVar_String : AutoCVar<std::string> {
  AutoCVar_String(const char* name, const char* description, const char* default_value, CVarFlags flags = CVarFlags::None);

  std::string get() const;
  void set(std::string&& val) const;
};
}
