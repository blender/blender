/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"
#include "BLI_generic_pointer.hh"

#include "BKE_node.hh"

#include "NOD_geometry_nodes_execute.hh"
#include "NOD_socket_usage_inference_fwd.hh"

struct bNodeTree;
struct bNodeSocket;
struct IDProperty;

namespace blender::nodes::socket_usage_inference {

struct SocketUsageInferencer;

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

class InputSocketUsageParams {
 private:
  SocketUsageInferencer &inferencer_;
  const ComputeContext *compute_context_ = nullptr;

 public:
  const bNodeTree &tree;
  const bNode &node;
  const bNodeSocket &socket;

  InputSocketUsageParams(SocketUsageInferencer &inferencer,
                         const ComputeContext *compute_context,
                         const bNodeTree &tree,
                         const bNode &node,
                         const bNodeSocket &socket);

  /**
   * Get an the statically known input value for the given socket identifier. The value may be
   * unknown, in which case null is returned.
   */
  InferenceValue get_input(StringRef identifier) const;

  /**
   * Utility for the case when the socket depends on a specific menu input to have a certain value.
   */
  bool menu_input_may_be(StringRef identifier, int enum_value) const;
};

/**
 * Get a boolean value for each input socket in the given tree that indicates whether that input is
 * used. It is assumed that all output sockets in the tree are used.
 */
Array<SocketUsage> infer_all_input_sockets_usage(const bNodeTree &tree);

/**
 * Get a boolean value for each node group input that indicates whether that input is used by the
 * outputs. The result can be used to e.g. gray out or hide individual inputs that are unused.
 *
 * \param group: The node group that is called.
 * \param group_input_values: An optional input value for each node group input. The type is
 *   expected to be `bNodeSocketType::base_cpp_type`. If the input value for a socket is not known
 *   or can't be represented as base type, null has to be passed instead.
 * \param r_input_usages: The destination array where the inferred usages are written.
 */
void infer_group_interface_inputs_usage(const bNodeTree &group,
                                        Span<GPointer> group_input_values,
                                        MutableSpan<SocketUsage> r_input_usages);

/**
 * Same as above, but automatically retrieves the input values from the given sockets..
 * This is used for group nodes.
 */
void infer_group_interface_inputs_usage(const bNodeTree &group,
                                        Span<const bNodeSocket *> input_sockets,
                                        MutableSpan<SocketUsage> r_input_usages);

/**
 * Same as above, but automatically retrieves the input values from the given properties.
 * This is used with the geometry nodes modifier and node tools.
 */
void infer_group_interface_inputs_usage(const bNodeTree &group,
                                        const PropertiesVectorSet &properties,
                                        MutableSpan<SocketUsage> r_input_usages);

}  // namespace blender::nodes::socket_usage_inference
