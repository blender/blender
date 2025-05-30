/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_compute_context.hh"
#include "BLI_function_ref.hh"
#include "BLI_multi_value_map.hh"

#include "NOD_inverse_eval_path.hh"
#include "NOD_inverse_eval_run.hh"

#include "BKE_compute_context_cache_fwd.hh"

struct Object;
struct NodesModifierData;
struct wmWindowManager;

namespace blender::nodes::gizmos {

namespace ie = inverse_eval;

/**
 * Cached on node groups after each update to make looking up and evaluating gizmos more efficient.
 */
struct TreeGizmoPropagation {
  Vector<const bNode *> gizmo_nodes;
  /**
   * Sockets that are special from a gizmo perspective because their value is controlled by a
   * gizmo or because they are a group input that has a gizmo.
   */
  Set<const bNodeSocket *> gizmo_endpoint_sockets;

  /** Supports quickly finding the gizmo sockets that are controlled by certain gizmo targets. */
  MultiValueMap<ie::ValueNodeElem, ie::SocketElem> gizmo_inputs_by_value_nodes;
  MultiValueMap<ie::SocketElem, ie::SocketElem> gizmo_inputs_by_node_inputs;
  MultiValueMap<ie::GroupInputElem, ie::SocketElem> gizmo_inputs_by_group_inputs;

  BLI_STRUCT_EQUALITY_OPERATORS_4(TreeGizmoPropagation,
                                  gizmo_nodes,
                                  gizmo_inputs_by_value_nodes,
                                  gizmo_inputs_by_node_inputs,
                                  gizmo_inputs_by_group_inputs)
};

/**
 * Updates the #TreeGizmoPropagation cached on the node-tree.
 *
 * \return False, if the propagation changed.
 */
bool update_tree_gizmo_propagation(bNodeTree &tree);

bool is_builtin_gizmo_node(const bNode &node);

using ForeachGizmoFn = FunctionRef<void(const Object &object,
                                        const NodesModifierData &nmd,
                                        const ComputeContext &compute_context,
                                        const bNode &gizmo_node,
                                        const bNodeSocket &gizmo_socket)>;

/**
 * Calls the given function for each gizmo that is active. It scans open node editors for selected
 * or pinned gizmos and also finds the gizmos for the active object.
 */
void foreach_active_gizmo(const bContext &C,
                          bke::ComputeContextCache &compute_context_cache,
                          ForeachGizmoFn fn);

using ForeachGizmoInModifierFn = FunctionRef<void(const ComputeContext &compute_context,
                                                  const bNode &gizmo_node,
                                                  const bNodeSocket &gizmo_socket)>;

/**
 * Similar to #foreach_active_gizmo but filters the list of gizmos to those that are relevant for a
 * specific modifier evaluation.
 */
void foreach_active_gizmo_in_modifier(const Object &object,
                                      const NodesModifierData &nmd,
                                      const wmWindowManager &wm,
                                      bke::ComputeContextCache &compute_context_cache,
                                      ForeachGizmoInModifierFn fn);

/**
 * Iterates over all compute contexts that are touched by a specific gizmo back-propagation path.
 * This is used to make sure that all sockets on the path are logged.
 */
void foreach_compute_context_on_gizmo_path(const ComputeContext &gizmo_context,
                                           const bNode &gizmo_node,
                                           const bNodeSocket &gizmo_socket,
                                           FunctionRef<void(const ComputeContext &context)> fn);

/**
 * Iterates over all sockets that propagate values modified by gizmos backwards. This is used to
 * draw the links between those sockets in a special way.
 */
void foreach_socket_on_gizmo_path(const ComputeContext &gizmo_context,
                                  const bNode &gizmo_node,
                                  const bNodeSocket &gizmo_socket,
                                  FunctionRef<void(const ComputeContext &context,
                                                   const bNodeSocket &socket,
                                                   const ie::ElemVariant &elem)> fn);

/**
 * Get the value element of a gizmo socket that can be affected by a gizmo. E.g. for the Transform
 * Gizmo node this may report that only the rotation component can be controlled with the gizmo.
 */
ie::ElemVariant get_editable_gizmo_elem(const ComputeContext &gizmo_context,
                                        const bNode &gizmo_node,
                                        const bNodeSocket &gizmo_socket);

/**
 * Should be called when a gizmo is moved and the change should be propagated back to the right
 * place.
 *
 * \param C: The context the gizmo is changed in. Required for propagating updates correctly.
 * \param object: The object with the modifier that the gizmo belongs to.
 * \param nmd: The modifier that the gizmo belongs to.
 * \param eval_log: The logged evaluation data that should be used to compute the backpropagated
 *   value. This may not be the most recent logged data, because generally the same logged data is
 *   used while interacting with a gizmo.
 * \param gizmo_context: The compute context of the gizmo node whose gizmo is modified.
 * \param gizmo_socket: The gizmo socket whose gizmo is modified.
 * \param apply_on_gizmo_value_fn: Applies the change done to the gizmo to the value in the gizmo
 *   node. For example, if an arrow gizmo is moved, the distance it's moved is added to the socket
 *   value. This is a callback because gizmo sockets are multi-inputs and thus multiple values
 *   need to change.
 *
 */
void apply_gizmo_change(bContext &C,
                        Object &object,
                        NodesModifierData &nmd,
                        geo_eval_log::GeoNodesLog &eval_log,
                        const ComputeContext &gizmo_context,
                        const bNodeSocket &gizmo_socket,
                        FunctionRef<void(bke::SocketValueVariant &value)> apply_on_gizmo_value_fn);

/**
 * Returns true if the value if the given node is controlled by a gizmo.
 */
bool value_node_has_gizmo(const bNodeTree &tree, const bNode &node);

}  // namespace blender::nodes::gizmos
