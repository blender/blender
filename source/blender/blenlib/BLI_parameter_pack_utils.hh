/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * C++ has a feature called "parameter packs" which allow building variadic templates.
 * This file has some utilities to work with such parameter packs.
 */

#include <tuple>
#include <type_traits>

#include "BLI_utildefines.h"

namespace blender {

/**
 * A type that encodes a specific value.
 */
template<typename T, T Element> struct TypeForValue {
  static constexpr T value = Element;
};

/**
 * A struct that allows passing in a type as a function parameter.
 */
template<typename T> struct TypeTag {
  using type = T;
};

/**
 * A type that encodes a list of values of the same type.
 * This is similar to #std::integer_sequence, but a bit more general. It's main purpose it to also
 * support enums instead of just ints.
 */
template<typename T, T... Elements> struct ValueSequence {
  /**
   * Get the number of elements in the sequence.
   */
  static constexpr size_t size() noexcept
  {
    return sizeof...(Elements);
  }

  /**
   * Get the element at a specific index.
   */
  template<size_t I> static constexpr T at_index()
  {
    static_assert(I < sizeof...(Elements));
    return std::tuple_element_t<I, std::tuple<TypeForValue<T, Elements>...>>::value;
  }

  /**
   * Return true if the element is in the sequence.
   */
  template<T Element> static constexpr bool contains()
  {
    return ((Element == Elements) || ...);
  }
};

/**
 * A type that encodes a list of types.
 * #std::tuple can also encode a list of types, but has a much more complex implementation.
 */
template<typename... T> struct TypeSequence {
  /**
   * Get the number of types in the sequence.
   */
  static constexpr size_t size() noexcept
  {
    return sizeof...(T);
  }

  /**
   * Get the type at a specific index.
   */
  template<size_t I> using at_index = std::tuple_element_t<I, std::tuple<T...>>;
};

namespace detail {

template<typename T, T Value, size_t... I>
inline ValueSequence<T, ((I == 0) ? Value : Value)...> make_value_sequence_impl(
    std::index_sequence<I...> /* indices */)
{
  return {};
}

template<typename T, T Value1, T Value2, size_t... Value1Indices, size_t... I>
inline ValueSequence<T,
                     (ValueSequence<size_t, Value1Indices...>::template contains<I>() ? Value1 :
                                                                                        Value2)...>
    make_two_value_sequence_impl(ValueSequence<size_t, Value1Indices...> /* value1_indices */,
                                 std::index_sequence<I...> /* indices */)
{
  return {};
};

}  // namespace detail

/**
 * Utility to create a #ValueSequence that has the same value at every index.
 */
template<typename T, T Value, size_t Size>
using make_value_sequence = decltype(detail::make_value_sequence_impl<T, Value>(
    std::make_index_sequence<Size>()));

/**
 * Utility to create a #ValueSequence that contains two different values. The indices of where the
 * first value should be used are passed in.
 */
template<typename T, T Value1, T Value2, size_t Size, size_t... Value1Indices>
using make_two_value_sequence = decltype(detail::make_two_value_sequence_impl<T, Value1, Value2>(
    ValueSequence<size_t, Value1Indices...>(), std::make_index_sequence<Size>()));

namespace parameter_pack_utils_static_tests {
enum class MyEnum { A, B };
static_assert(std::is_same_v<make_value_sequence<MyEnum, MyEnum::A, 3>,
                             ValueSequence<MyEnum, MyEnum::A, MyEnum::A, MyEnum::A>>);
static_assert(
    std::is_same_v<make_two_value_sequence<MyEnum, MyEnum::A, MyEnum::B, 5, 1, 2>,
                   ValueSequence<MyEnum, MyEnum::B, MyEnum::A, MyEnum::A, MyEnum::B, MyEnum::B>>);
}  // namespace parameter_pack_utils_static_tests

}  // namespace blender
