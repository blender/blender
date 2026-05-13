/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_compute_context.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_resource_scope.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_idprop.hh"

#include "NOD_socket_value_inference.hh"

namespace blender {

struct bNodeTree;
struct bNodeTreeInterfaceSocket;
struct PointerRNA;
namespace bke {
struct GeometrySet;
}
struct IDProperty;
namespace nodes {
struct GeoNodesCallData;
namespace eval_log {
class NodesEvalLog;
}  // namespace eval_log
}  // namespace nodes

namespace nodes {

constexpr StringRef input_use_attribute_suffix = "_use_attribute";
constexpr StringRef input_attribute_name_suffix = "_attribute_name";

struct IDPropNameGetter {
  StringRef operator()(const IDProperty *value) const
  {
    return StringRef(value->name);
  }
};

/**
 * \return Whether using an attribute to input values of this type is supported.
 */
bool socket_type_has_attribute_toggle(eNodeSocketDatatype type);

/**
 * \return Whether using an attribute to input values of this type is supported, and the node
 * group's input for this socket accepts a field rather than just single values.
 */
bool input_has_attribute_toggle(const bNodeTree &node_tree, const int socket_index);

bke::GeometrySet execute_geometry_nodes_on_geometry(const bNodeTree &btree,
                                                    const PointerRNA &properties_ptr,
                                                    const ComputeContext &base_compute_context,
                                                    GeoNodesCallData &call_data,
                                                    bke::GeometrySet input_geometry);

/**
 * Get input values for the node tree for static value/usage inferencing. Inferencing does not
 * fully evaluate the node tree (would be way to slow), and does not support all socket types. So
 * this function may return #InferenceValue::Unknown for some sockets.
 */
Vector<InferenceValue> get_geometry_nodes_input_inference_values(const bNodeTree &btree,
                                                                 const PointerRNA &properties_ptr,
                                                                 ResourceScope &scope);

}  // namespace nodes
}  // namespace blender
