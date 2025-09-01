/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_generic_pointer.hh"
#include "BLI_resource_scope.hh"

#include "BKE_compute_context_cache_fwd.hh"

#include "DNA_material_types.h"
#include "NOD_node_in_compute_context.hh"

struct bNodeTree;

namespace blender::nodes {

/**
 * During socket usage inferencing, some socket values are computed. This class represents such a
 * computed value. Not all possible values can be presented here, only "basic" once (like int, but
 * not int-field). A value can also be unknown if it can't be determined statically.
 */
class InferenceValue {
 private:
  /**
   * Non-owning pointer to a value of type #bNodeSocketType.base_cpp_type of the corresponding
   * socket. If this is null, the value is assumed to be unknown (aka, it can't be determined
   * statically).
   */
  const void *value_ = nullptr;

 public:
  explicit InferenceValue(const void *value) : value_(value) {}

  static InferenceValue Unknown()
  {
    return InferenceValue(nullptr);
  }

  bool is_unknown() const
  {
    return value_ == nullptr;
  }

  const void *data() const
  {
    return value_;
  }

  template<typename T> T get_known() const
  {
    BLI_assert(!this->is_unknown());
    return *static_cast<const T *>(this->value_);
  }

  template<typename T> std::optional<T> get() const
  {
    if (this->is_unknown()) {
      return std::nullopt;
    }
    return this->get_known<T>();
  }
};

class SocketValueInferencerImpl;

class SocketValueInferencer {
 private:
  SocketValueInferencerImpl &impl_;

 public:
  SocketValueInferencer(const bNodeTree &tree,
                        ResourceScope &scope,
                        bke::ComputeContextCache &compute_context_cache,
                        const std::optional<Span<GPointer>> tree_input_values,
                        const std::optional<Span<bool>> top_level_ignored_inputs);

  InferenceValue get_socket_value(const SocketInContext &socket);
};

namespace switch_node_inference_utils {

bool is_socket_selected__switch(const SocketInContext &socket, const InferenceValue &condition);
bool is_socket_selected__index_switch(const SocketInContext &socket,
                                      const InferenceValue &condition);
bool is_socket_selected__menu_switch(const SocketInContext &socket,
                                     const InferenceValue &condition);
bool is_socket_selected__mix_node(const SocketInContext &socket, const InferenceValue &condition);
bool is_socket_selected__shader_mix_node(const SocketInContext &socket,
                                         const InferenceValue &condition);

}  // namespace switch_node_inference_utils

}  // namespace blender::nodes
