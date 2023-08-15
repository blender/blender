/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_compute_context.hh"
#include "BLI_function_ref.hh"
#include "BLI_multi_value_map.hh"

#include "BKE_idprop.hh"

struct bNodeTree;
struct bNodeSocket;
struct Depsgraph;
namespace blender::bke {
struct GeometrySet;
}
struct IDProperty;
struct Object;
namespace blender::nodes {
struct GeoNodesLFUserData;
namespace geo_eval_log {
class GeoModifierLog;
}  // namespace geo_eval_log
}  // namespace blender::nodes

namespace blender::nodes {

StringRef input_use_attribute_suffix();
StringRef input_attribute_name_suffix();

/**
 * \return Whether using an attribute to input values of this type is supported.
 */
bool socket_type_has_attribute_toggle(const bNodeSocket &socket);

/**
 * \return Whether using an attribute to input values of this type is supported, and the node
 * group's input for this socket accepts a field rather than just single values.
 */
bool input_has_attribute_toggle(const bNodeTree &node_tree, const int socket_index);

bool id_property_type_matches_socket(const bNodeSocket &socket, const IDProperty &property);

std::unique_ptr<IDProperty, bke::idprop::IDPropertyDeleter> id_property_create_from_socket(
    const bNodeSocket &socket);

bke::GeometrySet execute_geometry_nodes_on_geometry(
    const bNodeTree &btree,
    const IDProperty *properties,
    const ComputeContext &base_compute_context,
    bke::GeometrySet input_geometry,
    FunctionRef<void(nodes::GeoNodesLFUserData &)> fill_user_data);

void update_input_properties_from_node_tree(const bNodeTree &tree,
                                            const IDProperty *old_properties,
                                            bool use_bool_for_use_attribute,
                                            IDProperty &properties);

void update_output_properties_from_node_tree(const bNodeTree &tree,
                                             const IDProperty *old_properties,
                                             IDProperty &properties);

}  // namespace blender::nodes
