/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"
#include "BKE_node.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_volume_grid_fwd.hh"

#include "BLI_color_types.hh"
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

template<typename T> struct GeoNodesMultiInput {
  using value_type = T;
  Vector<T> values;
};
template<typename T> constexpr bool is_GeoNodesMultiInput_v = false;
template<typename T> constexpr bool is_GeoNodesMultiInput_v<GeoNodesMultiInput<T>> = true;

/**
 * Executes a multi-function. If all inputs are single values, the results will also be single
 * values. If any input is a field, the outputs will also be fields.
 */
[[nodiscard]] bool execute_multi_function_on_value_variant(
    const mf::MultiFunction &fn,
    const std::shared_ptr<mf::MultiFunction> &owned_fn,
    Span<bke::SocketValueVariant *> input_values,
    Span<bke::SocketValueVariant *> output_values,
    GeoNodesUserData *user_data,
    std::string &r_error_message);

[[nodiscard]] inline bool execute_multi_function_on_value_variant(
    const std::shared_ptr<mf::MultiFunction> &owned_fn,
    const Span<bke::SocketValueVariant *> input_values,
    const Span<bke::SocketValueVariant *> output_values,
    GeoNodesUserData *user_data,
    std::string &r_error_message)
{
  const mf::MultiFunction &fn = *owned_fn;
  return execute_multi_function_on_value_variant(
      fn, std::move(owned_fn), input_values, output_values, user_data, r_error_message);
}

[[nodiscard]] inline bool execute_multi_function_on_value_variant(
    const mf::MultiFunction &fn,
    const Span<bke::SocketValueVariant *> input_values,
    const Span<bke::SocketValueVariant *> output_values,
    GeoNodesUserData *user_data,
    std::string &r_error_message)
{
  return execute_multi_function_on_value_variant(
      fn, {}, input_values, output_values, user_data, r_error_message);
}

/**
 * Performs implicit conversion between socket types. Returns false if the conversion is not
 * possible. In that case, r_to_value is left uninitialized.
 */
[[nodiscard]] std::optional<bke::SocketValueVariant> implicitly_convert_socket_value(
    const bke::bNodeSocketType &from_type,
    const bke::SocketValueVariant &from_value,
    const bke::bNodeSocketType &to_type);

/**
 * Builds a lazy-function that can convert between socket types. Returns null if the conversion is
 * never possible.
 */
const fn::lazy_function::LazyFunction *build_implicit_conversion_lazy_function(
    const bke::bNodeSocketType &from_type,
    const bke::bNodeSocketType &to_type,
    ResourceScope &scope);

}  // namespace blender::nodes
