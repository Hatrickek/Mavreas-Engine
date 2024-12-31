#include "LayerStack.hpp"

namespace ox {
LayerStack::~LayerStack() {
  for (Layer* layer : m_layers) {
    layer->on_detach();
    delete layer;
  }
}

void LayerStack::push_layer(Layer* layer) {
  m_layers.emplace(m_layers.begin() + m_layer_insert_index, layer);
  m_layer_insert_index++;
}

void LayerStack::push_overlay(Layer* overlay) {
  m_layers.emplace_back(overlay);
}

void LayerStack::pop_layer(Layer* layer) {
  auto it = std::find(m_layers.begin(), m_layers.begin() + m_layer_insert_index, layer);
  if (it != m_layers.begin() + m_layer_insert_index) {
    layer->on_detach();
    m_layers.erase(it);
    m_layer_insert_index--;
  }
}

void LayerStack::pop_overlay(Layer* overlay) {
  auto it = std::find(m_layers.begin() + m_layer_insert_index, m_layers.end(), overlay);
  if (it != m_layers.end()) {
    overlay->on_detach();
    m_layers.erase(it);
  }
}
}
