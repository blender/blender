/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * High performance CPU code often makes use of SIMD instructions to process multiple elements at
 * once on the same thread. To use SIMD, it's often beneficial when data is laid out as
 * "Structure-of-Arrays" (SoA) instead of "Array-of-Structures" (AoS):
 * - SoA: Store each component in a separate array. For example, to store 3D positions, there are
 *   separate arrays for all the x values, y values and z values.
 * - AoS: The more common way of storing data like positions. All vector components are stored
 *   interleaved: xyzxyzxyz. This is the case when just storing Vector<float3>.
 *
 * This and related headers provide common tooling to work with structure-of-array data more
 * easily. It provides ways to pass related buffers around and to convert from one format to the
 * other.
 */

#include "BLI_parameter_pack_utils.hh"

namespace blender {

/**
 * Number of structure-of-array components for a specific type. This needs to be specialized for
 * other types. For example, for float3 this is 3.
 */
template<typename T> constexpr size_t soa_component_num = 0;

/** Support using structure-of-array types for primitive values. */
template<typename T>
  requires std::is_arithmetic_v<T>
constexpr size_t soa_component_num<T> = 1;
template<size_t I, typename T>
  requires std::is_arithmetic_v<T>
constexpr const T &fetch_soa_component(const T &value)
{
  return value;  // NOLINT
}

/**
 * For a type to support structure-of-array processing it:
 * 1. Needs to be trivial.
 * 2. Set the soa_component_num to the correct value.
 * 3. Implement fetch_soa_component.
 */
template<typename T>
concept SoaType = std::is_trivial_v<T> && soa_component_num<T> > 0 &&
                  requires(const T &value) { fetch_soa_component<0>(value); };

/**
 * Construct a struct from its structure-of-array components. By default, it just does
 * brace-initialization but this can be overridden.
 */
template<typename T> struct StructFromSoaComponents {
  template<typename... Args> static T from(Args &&...args)
  {
    return T{args...};
  }
};

/** Create the struct from its components. */
template<typename T, typename... Args> inline T struct_from_soa_components(Args &&...args)
{
  return StructFromSoaComponents<T>::from(args...);
}

/**
 * Retrieve a tuple where each of the components (e.g. the x, y and z value of a vector) is a
 * separate item.
 */
template<typename T> inline auto to_soa_components(const T &value)
{
  return [&]<size_t... I>(std::index_sequence<I...> /*indices*/) {
    return std::tuple(fetch_soa_component<I>(value)...);
  }(std::make_index_sequence<soa_component_num<T>>());
}

namespace detail {
template<typename T> inline auto to_soa_type_sequence()
{
  return []<size_t... I>(std::index_sequence<I...> /*indices*/) {
    return TypeSequence<std::decay_t<decltype(fetch_soa_component<I>(std::declval<T>()))>...>();
  }(std::make_index_sequence<soa_component_num<T>>());
}
}  // namespace detail

/** Creates e.g. a TypeSequence<float, float, float> for float3. */
template<typename T> using soa_type_sequence_t = decltype(detail::to_soa_type_sequence<T>());

/** Get the type of a specific component. E.g. soa_component_t<float3, 2> == float. */
template<typename T, size_t I>
using soa_component_t = soa_type_sequence_t<T>::template at_index<I>;

namespace detail {
template<typename T, size_t... I>
constexpr bool soa_is_homogeneous_impl(std::index_sequence<I...> /*indices*/)
{
  return (std::is_same_v<soa_component_t<T, 0>, soa_component_t<T, I>> && ...);
}
}  // namespace detail

/** True when every SoA component has the same type. */
template<typename T>
constexpr bool soa_is_homogeneous_v = detail::soa_is_homogeneous_impl<T>(
    std::make_index_sequence<soa_component_num<T>>());

}  // namespace blender
