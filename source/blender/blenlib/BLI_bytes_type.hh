/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Code that only moves values around without interpreting them (copying, gathering, filling,
 * etc.) does not depend on the type of values, only on their size and alignment. This code can be
 * generated once per "bytes type" instead of once per type, reducing binary size.
 *
 * A "bytes type" is a trivial type with the same size and alignment as the original type. Scalars
 * and arrays of scalars are used, because they generally result in the same generated code as the
 * originals (avoiding byte arrays, because GCC doesn't seem to vectorize some loops with them).
 *
 * \note This relies on Blender being compiled without strict aliasing.
 *
 * See #BLI_bytes_type_common.hh for generating code only for common layouts.
 */

#include <cstdint>
#include <type_traits>

#include "BLI_utildefines.hh"

namespace blender::bytes_type {

namespace detail {

template<int64_t Size> struct UIntForSize;
template<> struct UIntForSize<1> {
  using type = uint8_t;
};
template<> struct UIntForSize<2> {
  using type = uint16_t;
};
template<> struct UIntForSize<4> {
  using type = uint32_t;
};
template<> struct UIntForSize<8> {
  using type = uint64_t;
};

/** An array of the largest unsigned integers that the alignment allows. */
template<int64_t Size, int64_t Alignment> struct alignas(Alignment) BytesTypeArray {
  static constexpr int64_t ValueSize = Alignment < 8 ? Alignment : 8;
  typename UIntForSize<ValueSize>::type values[size_t(Size / ValueSize)];
};

template<int64_t Size, int64_t Alignment> struct BytesTypeImpl {
  using type = BytesTypeArray<Size, Alignment>;
};
template<int64_t Size>
  requires(ELEM(Size, 1, 2, 4, 8))
struct BytesTypeImpl<Size, Size> {
  using type = typename UIntForSize<Size>::type;
};

}  // namespace detail

/** The size and alignment of a type, which is all that matters for its bytes type. */
struct Layout {
  int64_t size;
  int64_t alignment;

  template<typename T> static constexpr Layout of()
  {
    return {int64_t(sizeof(T)), int64_t(alignof(T))};
  }

  friend constexpr bool operator==(const Layout &a, const Layout &b) = default;
};

/** The bytes type for a trivially copyable type T. */
template<typename T>
using BytesType = typename detail::BytesTypeImpl<int64_t(sizeof(T)), int64_t(alignof(T))>::type;

static_assert(std::is_same_v<BytesType<float>, uint32_t>);
static_assert(std::is_same_v<BytesType<float[3]>, detail::BytesTypeArray<12, 4>>);

/**
 * The type used to copy values of T: its bytes type if it is trivially copyable, otherwise T
 * itself. Code that default constructs values also has to check
 * #std::is_trivially_default_constructible, because the default constructor of T may still do
 * something.
 */
template<typename T>
using to_copy_type = std::conditional_t<std::is_trivially_copyable_v<T>, BytesType<T>, T>;

}  // namespace blender::bytes_type
