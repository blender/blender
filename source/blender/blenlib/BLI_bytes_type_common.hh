/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Utilities to generate code only for the bytes types (see #BLI_bytes_type.hh) of a few common
 * layouts, and to dispatch to that code at run time.
 *
 * Only generating code for the common layouts limits the binary size of the generated code, but
 * means that other types need a separate fallback, which makes sense when the fallback has a small
 * binary size cost, like a loop calling #CPPType functions. Otherwise generating code for every
 * type is often better.
 */

#include <utility>

#include "BLI_bytes_type.hh"
#include "BLI_generic_span.hh"

namespace blender::bytes_type {

constexpr Layout common_layouts[] = {
    {1, 1},   /* bool, int8_t */
    {4, 1},   /* ColorGeometry4b */
    {4, 2},   /* short2 */
    {4, 4},   /* float, int */
    {8, 4},   /* float2, int2 */
    {12, 4},  /* float3 */
    {16, 4},  /* float4, ColorGeometry4f, math::Quaternion */
    {64, 16}, /* float4x4 */
};

/** Same as #Layout::of for a type known at runtime. */
inline Layout layout_of(const CPPType &type)
{
  return {type.size, type.alignment};
}

constexpr bool is_common(const Layout layout)
{
  for (const Layout &common_layout : common_layouts) {
    if (common_layout == layout) {
      return true;
    }
  }
  return false;
}

/** True if T is trivial and its layout is one of the #common_layouts. */
template<typename T>
constexpr bool is_common_v = std::is_trivial_v<T> && is_common(Layout::of<T>());

/**
 * Call the templated function with the bytes type that has the given layout.
 * \return False if the layout is not one of the #common_layouts.
 */
template<typename Fn> inline bool layout_to_common_bytes_type_try(const Layout layout, Fn &&fn)
{
  return [&]<size_t... I>(std::index_sequence<I...> /*indices*/) {
    return ((layout == common_layouts[I] &&
             (fn.template
              operator()<typename detail::BytesTypeImpl<common_layouts[I].size,
                                                        common_layouts[I].alignment>::type>(),
              true)) ||
            ...);
  }(std::make_index_sequence<std::size(common_layouts)>());
}

/**
 * Similar to #CPPType::to_static_type_try, but calls the templated function with the type's bytes
 * type instead, if it is trivial and its layout is in #common_layouts.
 *
 * \tparam Types: Types that the function is called with directly if their layout is not common,
 * so that they are supported as well.
 * \return False if the type is not trivial with a common layout and not one of Types.
 */
template<typename... Types, typename Fn>
inline bool to_common_bytes_type_try(const CPPType &type, Fn &&fn)
{
  if (type.is_trivial && layout_to_common_bytes_type_try(layout_of(type), fn)) {
    return true;
  }
  [[maybe_unused]] const auto try_type = [&]<typename T>() {
    if constexpr (is_common_v<T>) {
      /* Handled by the layout dispatch above. */
      return false;
    }
    else {
      if (type.is<T>()) {
        fn.template operator()<T>();
        return true;
      }
      return false;
    }
  };
  return (try_type.template operator()<Types>() || ...);
}

/** Same as #to_common_bytes_type_try, but asserts if the type is not supported. */
template<typename... Types, typename Fn>
inline void to_common_bytes_type(const CPPType &type, Fn &&fn)
{
  if (!to_common_bytes_type_try<Types...>(type, fn)) {
    BLI_assert_unreachable();
  }
}

/** True if T is the type that #to_common_bytes_type calls the function with for the type. */
template<typename T> inline bool is_common_bytes_type_for(const CPPType &type)
{
  if constexpr (is_common_v<T>) {
    return type.is_trivial && layout_of(type) == Layout::of<T>();
  }
  else {
    return type.is<T>();
  }
}

/** Get a span of the type that #to_common_bytes_type called the function with. */
template<typename T> inline Span<T> typed(const GSpan span)
{
  BLI_assert(span.is_empty() || is_common_bytes_type_for<T>(span.type()));
  return Span<T>(static_cast<const T *>(span.data()), span.size());
}

/** Get a span of the type that #to_common_bytes_type called the function with. */
template<typename T> inline MutableSpan<T> typed(const GMutableSpan span)
{
  BLI_assert(span.is_empty() || is_common_bytes_type_for<T>(span.type()));
  return MutableSpan<T>(static_cast<T *>(span.data()), span.size());
}

}  // namespace blender::bytes_type
