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

struct bNodeTree;
struct bNodeTreeInterfaceSocket;
namespace blender::bke {
struct GeometrySet;
}
struct IDProperty;
namespace blender::nodes {
struct GeoNodesCallData;
namespace geo_eval_log {
class GeoNodesLog;
}  // namespace geo_eval_log
}  // namespace blender::nodes

namespace blender::nodes {

constexpr StringRef input_use_attribute_suffix = "_use_attribute";
constexpr StringRef input_attribute_name_suffix = "_attribute_name";

struct IDPropNameGetter {
  StringRef operator()(const IDProperty *value) const
  {
    return StringRef(value->name);
  }
};

std::optional<StringRef> input_attribute_name_get(const IDProperty *properties,
                                                  const bNodeTreeInterfaceSocket &io_input);

/**
 * \return Whether using an attribute to input values of this type is supported.
 */
bool socket_type_has_attribute_toggle(eNodeSocketDatatype type);

/**
 * \return Whether using an attribute to input values of this type is supported, and the node
 * group's input for this socket accepts a field rather than just single values.
 */
bool input_has_attribute_toggle(const bNodeTree &node_tree, const int socket_index);

bool id_property_type_matches_socket(const bNodeTreeInterfaceSocket &socket,
                                     const IDProperty &property,
                                     bool use_name_for_ids = false);

std::unique_ptr<IDProperty, bke::idprop::IDPropertyDeleter> id_property_create_from_socket(
    const bNodeTreeInterfaceSocket &socket,
    nodes::StructureType structure_type,
    bool use_name_for_ids);

bke::GeometrySet execute_geometry_nodes_on_geometry(const bNodeTree &btree,
                                                    const IDProperty *properties,
                                                    const ComputeContext &base_compute_context,
                                                    GeoNodesCallData &call_data,
                                                    bke::GeometrySet input_geometry);

void update_input_properties_from_node_tree(const bNodeTree &tree,
                                            const IDProperty *old_properties,
                                            IDProperty &properties,
                                            bool use_name_for_ids = false);

void update_output_properties_from_node_tree(const bNodeTree &tree,
                                             const IDProperty *old_properties,
                                             IDProperty &properties);

/**
 * Get input values for the node tree for static value/usage inferencing. Inferencing does not
 * fully evaluate the node tree (would be way to slow), and does not support all socket types. So
 * this function may return #InferenceValue::Unknown for some sockets.
 */
Vector<InferenceValue> get_geometry_nodes_input_inference_values(const bNodeTree &btree,
                                                                 const IDProperty *properties,
                                                                 ResourceScope &scope);

}  // namespace blender::nodes
