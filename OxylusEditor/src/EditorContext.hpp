#pragma once
#include <cstring>
#include <filesystem>

namespace ox {
  enum class EditorContextType {
    None = 0,
    Entity,
    File,
    Asset
  };

  struct EditorContext {
    void set(EditorContextType type, const char* data, size_t size) {
      delete[] m_data;

      if (data && size != 0) {
        m_type = type;
        m_size = size;
        m_data = new char[m_size];
        memcpy(m_data, data, m_size);
      }
      else {
        m_type = EditorContextType::None;
        m_data = nullptr;
      }
    }

    void reset() {
      set(EditorContextType::None, nullptr, 0);
    }

    EditorContext() = default;

    ~EditorContext() {
      delete[] m_data;
    }

    EditorContext(const EditorContext& other) {
      set(other.m_type, other.m_data, other.m_size);
    }

    EditorContext& operator=(const EditorContext& other) {
      set(other.m_type, other.m_data, other.m_size);
      return *this;
    }

    EditorContext(EditorContext&& other) = delete;
    EditorContext operator=(EditorContext&& other) = delete;

    [[nodiscard]] EditorContextType get_type() const { return m_type; }

    template <typename T>
    [[nodiscard]] const T* as() const { return reinterpret_cast<T*>(m_data); }

    std::string get_asset_extension() const {
      const auto path = reinterpret_cast<std::string*>(m_data);
      if (!path)
        return {};
      return std::filesystem::path(*path).extension().string();
    }

    [[nodiscard]] bool is_valid(EditorContextType type) const { return type == m_type && m_data != nullptr; }
    operator bool() const { return m_type != EditorContextType::None && m_data != nullptr; }
    bool operator==(const EditorContext& other) const { return m_type == other.m_type && m_data == other.m_data; }

  private:
    EditorContextType m_type = EditorContextType::None;
    char* m_data = nullptr;
    size_t m_size = 0;
  };
}
