/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <concepts>

#include "BLI_fixed_string.hh"
#include "BLI_memory_utils.hh"
#include "BLI_structure_of_array_pointer.hh"
#include "BLI_unroll.hh"

namespace blender::multi_eval {

enum class ParamCategory {
  Input,
  Output,
  Mutable,
};

template<ParamCategory Category, typename T, bool UseSoa = false> struct ParamTag {
  static constexpr ParamCategory category = Category;
  static constexpr bool use_soa = UseSoa;
  using base_type = T;

  static_assert(!UseSoa || SoaType<T>);
};

template<typename T, bool UseSoa = false>
using InputParamTag = ParamTag<ParamCategory::Input, T, UseSoa>;
template<typename T, bool UseSoa = false>
using MutableParamTag = ParamTag<ParamCategory::Mutable, T, UseSoa>;
template<typename T, bool UseSoa = false>
using OutputParamTag = ParamTag<ParamCategory::Output, T, UseSoa>;

/**
 * This provides a more convenient way to build a TypeSequence<ParamTags...> using simple strings.
 * For example, instead of passing TypeSequence<InputParamTag<float>, OutputParamTag<int>>, one can
 * just pass <"io"> as template parameter. The type (float/int) is derived from the passed in
 * arguments.
 *
 * Full string rules:
 * - "i": Add input parameter.
 * - "o": Add output parameter.
 * - "m": Add mutable parameter (it's a input and output).
 * - "#": Enable structure-of-array handling for the previous argument.
 */
template<size_t N> struct FixedParamString : public FixedString<N> {
  constexpr FixedParamString(const char (&str)[N]) : FixedString<N>(str) {}
};

namespace detail {

struct ParamStrTag {
  ParamCategory category = ParamCategory::Input;
  bool use_soa = false;
};

template<size_t ArgsNum> struct ParamStrParseResult {
  bool ok = false;
  std::array<ParamStrTag, ArgsNum> tags{};
};

template<FixedParamString Str, size_t ArgsNum>
constexpr ParamStrParseResult<ArgsNum> parse_param_str()
{
  ParamStrParseResult<ArgsNum> result;
  size_t arg_i = 0;
  size_t str_i = 0;
  constexpr size_t str_size = sizeof(Str.data);
  while (arg_i < ArgsNum) {
    if (str_i >= str_size) {
      return result;
    }
    switch (Str.data[str_i]) {
      case 'i':
        result.tags[arg_i].category = ParamCategory::Input;
        break;
      case 'm':
        result.tags[arg_i].category = ParamCategory::Mutable;
        break;
      case 'o':
        result.tags[arg_i].category = ParamCategory::Output;
        break;
      default:
        return result;
    }
    str_i++;
    if (str_i < str_size && Str.data[str_i] == '#') {
      result.tags[arg_i].use_soa = true;
      str_i++;
    }
    arg_i++;
  }
  if (str_i >= str_size || Str.data[str_i] != '\0') {
    return result;
  }
  result.ok = true;
  return result;
}

template<FixedParamString Str, typename... Args>
constexpr std::array<ParamStrTag, sizeof...(Args)> str_and_args_to_tags_array()
{
  constexpr ParamStrParseResult parsed = parse_param_str<Str, sizeof...(Args)>();
  static_assert(parsed.ok);
  return parsed.tags;
}

template<FixedParamString Str, typename... Args, size_t... I>
constexpr auto param_tags_from_str_and_args_impl(TypeSequence<Args...> /*args*/,
                                                 std::index_sequence<I...> /*indices*/)
{
  constexpr std::array<ParamStrTag, sizeof...(Args)> tags =
      str_and_args_to_tags_array<Str, Args...>();
  return TypeSequence<ParamTag<tags[I].category,
                               std::decay_t<decltype(std::declval<std::decay_t<Args>>()[0])>,
                               tags[I].use_soa || is_SoaReadable_v<std::decay_t<Args>>>...>();
}

template<FixedParamString Str, typename... Args> constexpr auto param_tags_from_str_and_args()
{
  return param_tags_from_str_and_args_impl<Str>(TypeSequence<Args...>(),
                                                std::index_sequence_for<Args...>());
}
}  // namespace detail

/**
 * Depending on the use-case, it may be beneficial to explicitly instruct the compiler to flatten
 * or unroll a loop.
 */
struct MultiEvalCompilerAttributes {
  /**
   * Indicates to the compiler that a function should be flattened as much as possible. This is
   * important for the compiler to recognize that it can perform auto-vectorization.
   */
  bool flatten = false;
  /**
   * Manual loop unrolling factor using #unroll. Values <= 1 disable explicit unrolling.
   */
  int unroll = 0;
};

namespace detail {

template<typename ParamTag> constexpr size_t param_pointer_num()
{
  using T = typename ParamTag::base_type;
  if constexpr (ParamTag::use_soa) {
    return soa_type_sequence_t<T>::size();
  }
  else {
    return 1;
  }
}

template<typename... ParamTags>
constexpr std::array<size_t, sizeof...(ParamTags)> param_pointer_offsets()
{
  constexpr size_t param_num = sizeof...(ParamTags);
  constexpr size_t counts[] = {param_pointer_num<ParamTags>()...};
  std::array<size_t, param_num> offsets;
  size_t offset = 0;
  for (size_t i = 0; i < param_num; i++) {
    offsets[i] = offset;
    offset += counts[i];
  }
  return offsets;
}

template<typename Arg> inline decltype(auto) array_fetch(Arg &&arg, const int64_t i)
{
  using A = std::decay_t<Arg>;
  if constexpr (std::is_same_v<A, Span<bool>>) {
    /* Casting bools to chars for loading allows GCC to auto-vectorize. */
    return bool(reinterpret_cast<const char *>(arg.data())[i]);
  }
  else {
    return std::forward<Arg>(arg)[i];
  }
}

template<size_t Offset, typename T, typename ArgsTuple, size_t... ArgI>
inline T load_soa_from_pointers(const ArgsTuple &args_tuple,
                                const int64_t i,
                                std::index_sequence<ArgI...> /*arg_indices*/)
{
  return struct_from_soa_components<T>(std::get<Offset + ArgI>(args_tuple)[i]...);
}

template<size_t Offset, typename ArgsTuple, typename Components, size_t... ArgI>
inline void store_soa_to_pointers(const ArgsTuple &args_tuple,
                                  const int64_t i,
                                  const Components &soa_components,
                                  std::index_sequence<ArgI...> /*arg_indices*/)
{
  ((std::get<Offset + ArgI>(args_tuple)[i] = std::get<ArgI>(soa_components)), ...);
}

template<int UnrollAmount,
         typename ElementFn,
         size_t... ParamI,
         typename... ParamTags,
         typename... Args>
BLI_INLINE void evaluate_element_wise_impl(const ElementFn &element_fn,
                                           TypeSequence<ParamTags...> /*param_tags*/,
                                           std::index_sequence<ParamI...> /*param_indices*/,
                                           const int64_t size,
                                           Args *__restrict... args)
{
  const std::tuple args_tuple = std::tuple(args...);
  constexpr size_t param_num = sizeof...(ParamTags);
  static_assert(sizeof...(ParamI) == sizeof...(ParamTags));
  static_assert(sizeof...(ParamI) <= sizeof...(Args));
  constexpr std::array<size_t, param_num> param_offsets = param_pointer_offsets<ParamTags...>();

  const auto process_index = [&](const int64_t i) BLI_INLINE_LAMBDA {
    /* Temporary values used when data is stored as structure-of-array but passed to the element_fn
     * as normal struct. */
    std::tuple<TypedBuffer<typename ParamTags::base_type>...> written_values;

#if defined(__clang__)
    /* GCC does not support inlining a function calls with an attribute like clang. However, it's
     * also less necessary because it supports [[gnu::flatten]] recursively which clang doesn't. */
    [[clang::always_inline]]
#endif
    element_fn([&]() -> decltype(auto) {
      using ParamTag = ParamTags;
      using T = typename ParamTag::base_type;
      if constexpr (ParamTag::use_soa) {
        T &value = std::get<ParamI>(written_values).ref();
        if constexpr (ELEM(ParamTag::category, ParamCategory::Input, ParamCategory::Mutable)) {
          value = load_soa_from_pointers<param_offsets[ParamI], T>(
              args_tuple, i, std::make_index_sequence<param_pointer_num<ParamTag>()>());
        }
        return value;
      }
      else {
        return array_fetch(std::get<param_offsets[ParamI]>(args_tuple), i);
      }
    }()...);
    /* Write output to soa buffers. */
    (
        [&] {
          using ParamTag = ParamTags;
          if constexpr (ParamTag::use_soa &&
                        ELEM(ParamTag::category, ParamCategory::Mutable, ParamCategory::Output))
          {
            const auto soa_components = to_soa_components(std::get<ParamI>(written_values).ref());
            store_soa_to_pointers<param_offsets[ParamI]>(
                args_tuple,
                i,
                soa_components,
                std::make_index_sequence<param_pointer_num<ParamTag>()>());
          }
        }(),
        ...);
  };

  int64_t i = 0;
  if constexpr (UnrollAmount > 1) {
    for (; i + UnrollAmount <= size; i += UnrollAmount) {
      unroll<UnrollAmount>([&](const int64_t j) BLI_INLINE_LAMBDA { process_index(i + j); });
    }
  }
  for (; i < size; i++) {
    process_index(i);
  }
}

/**
 * Unifies various different kinds of arguments such that #evaluate_element_wise can deal with many
 * kinds of inputs while #evaluate_element_wise_impl does not need to support them all.
 *
 * Also, this splits the inputs into separate pointers (e.g. a SoaPtr is split into a pointer for
 * each component). This is important such that __restrict can apply to each pointer individually.
 * Just putting __restrict on a SoaPtr does not work.
 */
template<typename ParamTag, typename T> inline auto to_eval_pointer_args(T &&v)
{
  using DecayT = std::decay_t<T>;
  using Base = typename ParamTag::base_type;
  if constexpr (ParamTag::use_soa) {
    static_assert(is_SoaReadable_v<DecayT>);
    /* Create a tuple of the raw pointers. */
    return [&]<size_t... I>(std::index_sequence<I...> /*indices*/) {
      return std::tuple(v.template data<I>()...);
    }(std::make_index_sequence<soa_component_num<Base>>());
  }
  else if constexpr (std::is_pointer_v<DecayT> &&
                     std::is_same_v<std::remove_cv_t<std::remove_pointer_t<DecayT>>, Base>)
  {
    return std::tuple(v);
  }
  else if constexpr (requires {
                       { v.size() } -> std::integral;
                       requires is_same_any_v<decltype(v.data()), Base *, const Base *>;
                     })
  {
    return std::tuple(v.data());
  }
  else {
    BLI_assert_unreachable_static_t(T);
    return std::tuple(static_cast<Base *>(nullptr));
  }
}

/**
 * Tells the compiler to inline every call to detect optimization opportunities like
 * auto-vectorization. #noinline helps GCC to vectorize because the function contains only one
 * obvious loop instead of being embedded into a much larger parent function.
 */
template<int UnrollAmount, typename ElementFn, typename... ParamTags, typename... Args>
[[gnu::flatten, gnu::noinline]] inline void evaluate_element_wise_flattened(
    const ElementFn &element_fn,
    TypeSequence<ParamTags...> param_tags,
    const int64_t size,
    Args *__restrict... args)
{
  evaluate_element_wise_impl<UnrollAmount>(
      element_fn, param_tags, std::index_sequence_for<ParamTags...>(), size, args...);
}

}  // namespace detail

template<MultiEvalCompilerAttributes Attributes = {},
         typename ElementFn,
         typename... ParamTags,
         typename... Args>
inline void evaluate_element_wise(const ElementFn &element_fn,
                                  TypeSequence<ParamTags...> param_tags,
                                  const int64_t size,
                                  Args &&...args)
{
  auto pointers = std::tuple_cat(detail::to_eval_pointer_args<ParamTags>(args)...);
  if constexpr (Attributes.flatten) {
    std::apply(
        [&](auto *...ptrs) {
          detail::evaluate_element_wise_flattened<Attributes.unroll>(
              element_fn, param_tags, size, ptrs...);
        },
        pointers);
  }
  else {
    std::apply(
        [&](auto *...ptrs) {
          detail::evaluate_element_wise_impl<Attributes.unroll>(
              element_fn, param_tags, std::index_sequence_for<ParamTags...>(), size, ptrs...);
        },
        pointers);
  }
}

template<FixedParamString ParamStr,
         MultiEvalCompilerAttributes Attributes = {},
         typename ElementFn,
         typename... Args>
inline void evaluate_element_wise(const ElementFn &element_fn, const int64_t size, Args &&...args)
{
  using ParamTagsSeq = decltype(detail::param_tags_from_str_and_args<ParamStr, Args...>());
  [&]<typename... ParamTags>(TypeSequence<ParamTags...> param_tags) {
    evaluate_element_wise<Attributes>(element_fn, param_tags, size, std::forward<Args>(args)...);
  }(ParamTagsSeq());
}

}  // namespace blender::multi_eval
