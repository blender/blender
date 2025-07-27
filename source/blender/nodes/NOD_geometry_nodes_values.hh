/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.hh"
#include "BKE_volume_grid_fwd.hh"

#include "BLI_color.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_memory_utils.hh"

#include "FN_field.hh"
#include "FN_lazy_function.hh"
#include "FN_multi_function.hh"

#include "NOD_geometry_nodes_bundle_fwd.hh"
#include "NOD_geometry_nodes_closure_fwd.hh"
#include "NOD_geometry_nodes_list_fwd.hh"

namespace blender {
namespace bke {
class SocketValueVariant;
}
namespace nodes {
struct GeoNodesUserData;
}
}  // namespace blender

namespace blender::nodes {

/** True if a static type can also exist as field in Geometry Nodes. */
template<typename T>
static constexpr bool geo_nodes_is_field_base_type_v = is_same_any_v<T,
                                                                     float,
                                                                     int,
                                                                     bool,
                                                                     ColorGeometry4f,
                                                                     float3,
                                                                     std::string,
                                                                     math::Quaternion,
                                                                     float4x4>;

/** True if Geometry Nodes sockets can store values of the given type and the type is stored
 * embedded in a #SocketValueVariant. */
template<typename T>
static constexpr bool geo_nodes_type_stored_as_SocketValueVariant_v =
    std::is_enum_v<T> || geo_nodes_is_field_base_type_v<T> || fn::is_field_v<T> ||
    bke::is_VolumeGrid_v<T> ||
    is_same_any_v<T,
                  fn::GField,
                  bke::GVolumeGrid,
                  nodes::BundlePtr,
                  nodes::ClosurePtr,
                  nodes::ListPtr>;

[[nodiscard]] bool execute_multi_function_on_value_variant(
    const mf::MultiFunction &fn,
    const std::shared_ptr<mf::MultiFunction> &owned_fn,
    const Span<bke::SocketValueVariant *> input_values,
    const Span<bke::SocketValueVariant *> output_values,
    GeoNodesUserData *user_data,
    std::string &r_error_message);

/**
 * Performs implicit conversion between socket types. Returns false if the conversion is not
 * possible. In that case, r_to_value is left uninitialized.
 */
[[nodiscard]] bool implicitly_convert_socket_value(const bke::bNodeSocketType &from_type,
                                                   const void *from_value,
                                                   const bke::bNodeSocketType &to_type,
                                                   void *r_to_value);

/**
 * Builds a lazy-function that can convert between socket types. Returns null if the conversion is
 * never possible.
 */
const fn::lazy_function::LazyFunction *build_implicit_conversion_lazy_function(
    const bke::bNodeSocketType &from_type,
    const bke::bNodeSocketType &to_type,
    ResourceScope &scope);

}  // namespace blender::nodes
