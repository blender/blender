/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_array.hh"
#include "BLI_generic_pointer.hh"

#include "BKE_node.hh"

#include "NOD_geometry_nodes_execute.hh"
#include "NOD_socket_usage_inference_fwd.hh"
#include "NOD_socket_value_inference.hh"

struct bNodeTree;
struct bNodeSocket;
struct IDProperty;

namespace blender::nodes::socket_usage_inference {

class SocketUsageInferencerImpl;

/**
 * Can detect which sockets are used or disabled.
 */
class SocketUsageInferencer {
 private:
  SocketUsageInferencerImpl &impl_;

  friend class SocketUsageParams;

 public:
  SocketUsageInferencer(const bNodeTree &tree,
                        ResourceScope &scope,
                        SocketValueInferencer &value_inferencer,
                        bke::ComputeContextCache &compute_context_cache,
                        bool ignore_top_level_node_muting = false);

  bool is_socket_used(const SocketInContext &socket);
  bool is_group_input_used(int input_i);

  bool is_disabled_output(const SocketInContext &socket);
  bool is_disabled_group_output(int output_i);

  /** This can be used when detecting the usage of all input sockets in a node tree, instead of
   * just the inputs of the group as a whole.
   */
  void mark_top_level_node_outputs_as_used();
};

class SocketUsageParams {
 private:
  SocketUsageInferencer &inferencer_;
  const ComputeContext *compute_context_ = nullptr;

 public:
  const bNodeTree &tree;
  const bNode &node;
  const bNodeSocket &socket;

  SocketUsageParams(SocketUsageInferencer &inferencer,
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
   * Returns true if any output is known to be used or false if no output is used. std::nullopt is
   * returned if it's not known yet if any output is used. In this case, the caller should return
   * early, it will be checked again once new information about output usages becomes available.
   */
  std::optional<bool> any_output_is_used() const;

  /**
   * Utility for the case when the socket depends on a specific menu input to have a certain value.
   */
  bool menu_input_may_be(StringRef identifier, int enum_value) const;
};

/**
 * Determine which sockets in the tree are currently used and thus which should be grayed out or
 * made invisible.
 */
Array<SocketUsage> infer_all_sockets_usage(const bNodeTree &tree);

/**
 * Get usage of the inputs and outputs of the node group given the set of input values. The result
 * can be used to e.g. gray out or hide individual inputs that are unused.
 *
 * \param group: The node group that is called.
 * \param group_input_values: An optional input value for each node group input. The type is
 *   expected to be `bNodeSocketType::base_cpp_type`. If the input value for a socket is not known
 *   or can't be represented as base type, null has to be passed instead.
 * \param r_input_usages: The destination array where the inferred input usages are written.
 * \param r_output_usages: The destination array where the inferred output usages are written.
 */
void infer_group_interface_usage(
    const bNodeTree &group,
    Span<InferenceValue> group_input_values,
    MutableSpan<SocketUsage> r_input_usages,
    std::optional<MutableSpan<SocketUsage>> r_output_usages = std::nullopt);

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
void infer_group_interface_usage(
    const bNodeTree &group,
    const IDProperty *properties,
    MutableSpan<SocketUsage> r_input_usages,
    std::optional<MutableSpan<SocketUsage>> r_output_usages = std::nullopt);

}  // namespace blender::nodes::socket_usage_inference
