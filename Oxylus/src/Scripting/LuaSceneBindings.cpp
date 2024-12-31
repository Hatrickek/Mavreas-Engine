#include "LuaSceneBindings.hpp"

#include <entt/signal/dispatcher.hpp>
#include <sol/state.hpp>
#include <sol/variadic_args.hpp>

#include "LuaHelpers.hpp"

#include "Scene/Entity.hpp"
#include "Scene/Scene.hpp"

namespace ox::LuaBindings {
[[nodiscard]] entt::id_type get_type_id(const sol::table& obj) {
  const auto f = obj["type_id"].get<sol::function>();
  OX_ASSERT(f.valid() && "type_id not exposed to lua!");
  return f.valid() ? f().get<entt::id_type>() : -1;
}

template <typename T>
[[nodiscard]] entt::id_type deduce_type(T&& obj) {
  switch (obj.get_type()) {
    case sol::type::number: return obj.template as<entt::id_type>();
    case sol::type::table: return get_type_id(obj);
  }
  OX_ASSERT(false, "probably not registered the component");
  return -1;
}

auto collect_types(const sol::variadic_args& va) {
  std::set<entt::id_type> types;
  std::transform(va.cbegin(),
                 va.cend(),
                 std::inserter(types, types.begin()),
                 [](const auto& obj) { return deduce_type(obj); });
  return types;
}

// @see
// https://github.com/skypjack/entt/wiki/Crash-Course:-runtime-reflection-system

template <typename... Args>
inline auto invoke_meta_func(entt::meta_type meta_type,
                             entt::id_type function_id,
                             Args&&... args) {
  if (!meta_type) {
    OX_LOG_WARN("Not a meta_type!");
  }
  else {
    if (auto&& meta_function = meta_type.func(function_id); meta_function)
      return meta_function.invoke({}, std::forward<Args>(args)...);
  }
  return entt::meta_any{};
}

template <typename... Args>
inline auto invoke_meta_func(entt::id_type type_id,
                             entt::id_type function_id,
                             Args&&... args) {
  return invoke_meta_func(entt::resolve(type_id),
                          function_id,
                          std::forward<Args>(args)...);
}

void bind_registry(sol::table& entt_module) {
  using namespace entt::literals;

  entt_module.new_usertype<entt::runtime_view>(
    "runtime_view",
    sol::no_constructor,

    "size_hint",
    &entt::runtime_view::size_hint,
    "contains",
    &entt::runtime_view::contains,
    "each",
    [](const entt::runtime_view& self, const sol::function& callback) {
      if (callback.valid()) {
        for (auto entity : self)
          callback(entity);
      }
    },
    "front",
    [](const entt::runtime_view& self) -> entt::entity {
      if (self.size_hint() == 0) {
        OX_LOG_ERROR("front(): view was empty!");
        return {};
      }
      return *self.begin();
    },
    "exclude",
    [](entt::runtime_view& self, entt::registry& reg, const sol::variadic_args& va) {
      const auto types = collect_types(va);

      for (auto&& [componentId, storage] : reg.storage()) {
        if (types.contains(componentId)) {
          return self.exclude(storage);
        }
      }

      return self;
    }
  );

  entt_module.new_usertype<entt::registry>(
    "registry",
    sol::meta_function::construct,
    sol::factories([] { return entt::registry{}; }),

    "size",
    [](const entt::registry& self) {
      return self.storage<entt::entity>()->size();
    },
    "alive",
    [](const entt::registry& self) {
      return self.storage<entt::entity>()->in_use();
    },

    "valid",
    &entt::registry::valid,
    "current",
    &entt::registry::current,

    "create",
    [](entt::registry& self) { return self.create(); },
    "destroy",
    [](entt::registry& self, entt::entity entity) {
      if (!self.valid(entity))
        return (uint16_t)0;
      return self.destroy(entity);
    },

    "emplace",
    [](entt::registry& self, entt::entity entity, const sol::table& comp, sol::this_state s) -> sol::object {
      if (!comp.valid())
        return sol::lua_nil_t{};
      const auto maybe_any = invoke_meta_func(get_type_id(comp), "emplace"_hs, &self, entity, comp, s);
      return maybe_any ? maybe_any.cast<sol::reference>() : sol::lua_nil_t{};
    },
    "remove",
    [](entt::registry& self, entt::entity entity, const sol::object& type_or_id) {
      const auto maybe_any =
        invoke_meta_func(deduce_type(type_or_id), "remove"_hs, &self, entity);
      return maybe_any ? maybe_any.cast<size_t>() : 0;
    },
    "has",
    [](entt::registry& self, entt::entity entity, const sol::object& type_or_id) {
      const auto maybe_any =
        invoke_meta_func(deduce_type(type_or_id), "has"_hs, &self, entity);
      return maybe_any ? maybe_any.cast<bool>() : false;
    },
    "any_of",
    [](const sol::table& self, entt::entity entity, const sol::variadic_args& va) {
      const auto types = collect_types(va);
      const auto has = self["has"].get<sol::function>();
      return std::any_of(types.cbegin(),
                         types.cend(),
                         [&](auto type_id) { return has(self, entity, type_id).template get<bool>(); }
      );
    },
    "get",
    [](entt::registry& self,
       entt::entity entity,
       const sol::object& type_or_id,
       sol::this_state s) {
      const auto maybe_any = invoke_meta_func(deduce_type(type_or_id), "get"_hs, &self, entity, s);
      return maybe_any ? maybe_any.cast<sol::reference>() : sol::lua_nil_t{};
    },
    "clear",
    sol::overload(
      &entt::registry::clear<>,
      [](entt::registry& self, sol::object type_or_id) {
        invoke_meta_func(deduce_type(type_or_id), "clear"_hs, &self);
      }
    ),

    "orphan",
    &entt::registry::orphan,

    "runtime_view",
    [](entt::registry& self, const sol::variadic_args& va) {
      const auto types = collect_types(va);

      auto view = entt::runtime_view{};
      for (auto&& [componentId, storage] : self.storage()) {
        if (types.contains(componentId)) {
          view.iterate(storage);
        }
      }
      return view;
    }
  );
}

void bind_dispatcher(const Shared<sol::state>& state, sol::basic_table_core<false, sol::basic_reference<>> entt_module) {
  struct BaseScriptEvent {
    sol::table self;
  };

  state->new_usertype<BaseScriptEvent>("BaseScriptEvent", "type_id", [] { return entt::type_hash<BaseScriptEvent>::value(); });

  struct ScriptedEventListener {
    ScriptedEventListener(entt::dispatcher& dispatcher, const sol::table& type, const sol::function& f)
      : key{get_key(type)}, callback{f} {
      connection = dispatcher.sink<BaseScriptEvent>()
                             .connect<&ScriptedEventListener::receive>(*this);
    }

    ScriptedEventListener(const ScriptedEventListener&) = delete;
    ScriptedEventListener(ScriptedEventListener&&) noexcept = default;
    ~ScriptedEventListener() { connection.release(); }

    ScriptedEventListener&
    operator=(const ScriptedEventListener&) = delete;
    ScriptedEventListener&
    operator=(ScriptedEventListener&&) noexcept = default;

    static uintptr_t get_key(const sol::table& t) {
      return reinterpret_cast<uintptr_t>(
        t["__index"].get<sol::table>().pointer());
    }

    void receive(const BaseScriptEvent& evt) const {
      OX_ASSERT(connection && callback.valid());
      if (auto& [self] = evt; get_key(self) == key)
        callback(self);
    }

    const uintptr_t key;
    const sol::function callback;
    entt::connection connection;
  };

  using namespace entt::literals;

  entt_module.new_usertype<entt::dispatcher>(
    "dispatcher",
    sol::meta_function::construct,
    sol::factories([] { return entt::dispatcher{}; }),

    "trigger",
    [](entt::dispatcher& self, const sol::table& evt) {
      if (const auto event_id = get_type_id(evt);
        event_id == entt::type_hash<BaseScriptEvent>::value()) {
        self.trigger(BaseScriptEvent{evt});
      }
      else {
        invoke_meta_func(event_id, "trigger_event"_hs, &self, evt);
      }
    },
    "enqueue",
    [](entt::dispatcher& self, const sol::table& evt) {
      if (const auto event_id = get_type_id(evt);
        event_id == entt::type_hash<BaseScriptEvent>::value()) {
        self.enqueue(BaseScriptEvent{evt});
      }
      else {
        invoke_meta_func(event_id, "enqueue_event"_hs, &self, evt);
      }
    },
    "clear",
    sol::overload(
      [](entt::dispatcher& self) { self.clear(); },
      [](entt::dispatcher& self, const sol::object& type_or_id) { invoke_meta_func(deduce_type(type_or_id), "clear_event"_hs, &self); }
    ),
    "update",
    sol::overload(
      [](entt::dispatcher& self) { self.update(); },
      [](entt::dispatcher& self, const sol::object& type_or_id) { invoke_meta_func(deduce_type(type_or_id), "update_event"_hs, &self); }
    ),
    "connect",
    [](entt::dispatcher& self,
       const sol::object& type_or_id,
       const sol::function& listener,
       sol::this_state s) {
      if (!listener.valid()) {
        OX_LOG_ERROR("Connected listener is not valid!");
        return entt::meta_any{};
      }
      if (const auto event_id = deduce_type(type_or_id);
        event_id == entt::type_hash<BaseScriptEvent>::value()) {
        return entt::meta_any{
          std::make_unique<ScriptedEventListener>(
            self,
            type_or_id,
            listener)
        };
      }
      else {
        return invoke_meta_func(event_id,
                                "connect_listener"_hs,
                                &self,
                                listener);
      }
    },
    "disconnect",
    [](const sol::table& connection) {
      connection.as<entt::meta_any>().reset();
    }
  );
}

void bind_scene(const Shared<sol::state>& state) {
  OX_SCOPED_ZONE;
  sol::usertype<Scene> scene_type = state->new_usertype<Scene>("Scene");
  scene_type.set_function("get_registry", &Scene::get_registry);
  scene_type.set_function("create_entity", [](Scene& self, const std::string& name) { return self.create_entity(name); });
  scene_type.set_function("load_mesh", &Scene::load_mesh);

  auto entt_module = (*state)["entt"].get_or_create<sol::table>();

  bind_registry(entt_module);
  bind_dispatcher(state, entt_module);

  // TODO: bind scheduler maybe?
}
}
