﻿#pragma once
#include <entt/entt.hpp>

#include <sol/sol.hpp>

#include "Utils/Log.h"

// --- Registry ---

template <typename Component>
auto is_valid(const entt::registry* registry, entt::entity entity) {
  OX_CORE_ASSERT(registry);
  return registry->valid(entity);
}

template <typename Component>
auto emplace_component(entt::registry* registry,
                       entt::entity entity,
                       const sol::table& instance,
                       sol::this_state s) {
  OX_CORE_ASSERT(registry);
  auto& comp = registry->emplace_or_replace<Component>(entity, instance.valid() ? instance.as<Component>() : Component{});
  return sol::make_reference(s, std::ref(comp));
}

template <typename Component>
auto get_component(entt::registry* registry,
                   entt::entity entity,
                   sol::this_state s) {
  OX_CORE_ASSERT(registry);
  auto& comp = registry->get_or_emplace<Component>(entity);
  return sol::make_reference(s, std::ref(comp));
}

template <typename Component>
bool has_component(entt::registry* registry, entt::entity entity) {
  OX_CORE_ASSERT(registry);
  return registry->any_of<Component>(entity);
}

template <typename Component>
auto remove_component(entt::registry* registry, entt::entity entity) {
  OX_CORE_ASSERT(registry);
  return registry->remove<Component>(entity);
}

template <typename Component>
void clear_component(entt::registry* registry) {
  OX_CORE_ASSERT(registry);
  registry->clear<Component>();
}

template <typename Component>
void register_meta_component() {
  using namespace entt::literals;

  entt::meta<Component>()
   .template func<&is_valid<Component>>("valid"_hs)
   .template func<&emplace_component<Component>>("emplace"_hs)
   .template func<&get_component<Component>>("get"_hs)
   .template func<&has_component<Component>>("has"_hs)
   .template func<&clear_component<Component>>("clear"_hs)
   .template func<&remove_component<Component>>("remove"_hs);
}

// --- Dispacter ---

template <typename Event>
auto connect_listener(entt::dispatcher* dispatcher, const sol::function& f) {
  OX_CORE_ASSERT(dispatcher && f.valid());

  struct script_listener {
    script_listener(entt::dispatcher& dispatcher, sol::function f)
      : callback{std::move(f)} {
      connection = dispatcher.sink<Event>().template connect<&script_listener::receive>(*this);
    }

    script_listener(const script_listener&) = delete;
    script_listener(script_listener&&) noexcept = default;

    ~script_listener() {
      connection.release();
      callback.abandon();
    }

    script_listener&operator=(const script_listener&) = delete;
    script_listener&operator=(script_listener&&) noexcept = default;

    void receive(const Event& evt) {
      if (connection && callback.valid())
        callback(evt);
    }

    sol::function callback;
    entt::connection connection;
  };

  return std::make_unique<script_listener>(*dispatcher, f);
}

template <typename Event>
void trigger_event(entt::dispatcher* dispatcher, const sol::table& evt) {
  OX_CORE_ASSERT(dispatcher && evt.valid());
  dispatcher->trigger(evt.as<Event>());
}

template <typename Event>
void enqueue_event(entt::dispatcher* dispatcher, const sol::table& evt) {
  OX_CORE_ASSERT(dispatcher && evt.valid());
  dispatcher->enqueue(evt.as<Event>());
}

template <typename Event>
void clear_event(entt::dispatcher* dispatcher) {
  OX_CORE_ASSERT(dispatcher);
  dispatcher->clear<Event>();
}

template <typename Event>
void update_event(entt::dispatcher* dispatcher) {
  OX_CORE_ASSERT(dispatcher);
  dispatcher->update<Event>();
}

template <typename Event>
void register_meta_event() {
  using namespace entt::literals;

  entt::meta<Event>()
   .template func<&connect_listener<Event>>("connect_listener"_hs)
   .template func<&trigger_event<Event>>("trigger_event"_hs)
   .template func<&enqueue_event<Event>>("enqueue_event"_hs)
   .template func<&clear_event<Event>>("clear_event"_hs)
   .template func<&update_event<Event>>("update_event"_hs);
}

#define SET_COMPONENT_TYPE_ID(var, type) var.set("type_id", &entt::type_hash<type>::value)
#define SET_TYPE_FIELD(var, type, field) var[#field] = &type::field
#define SET_TYPE_FUNCTION(var, type, function) var.set_function(#function, &type::function)
#define NEW_TYPE_WITH_CTOR(state, type, factory_func) state->new_usertype<type>(#type, sol::call_constructor, sol::factories(factory_func));
