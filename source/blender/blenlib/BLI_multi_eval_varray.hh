/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Using virtual arrays (#VArray) is very useful to represent values which can be an array but are
 * often just a single value. The downside of using the #VArray indirection is that element access
 * requires a virtual function call which has significant overhead in hot loops and prevents the
 * compiler from doing optimizations such as auto-vectorization.
 *
 * This file provides the ability to run element-wise functions efficiently when all the inputs are
 * virtual arrays. It does so by giving explicit control over the trade-off between run-time
 * performance, compile time and binary size. For example, the most efficient code could be
 * achieved when the hot loop is generated for all possible variations of inputs
 * (#AllSpanOrSingle). However, that can result in an exponentially increasing amount of code with
 * the number of inputs. Instead, other execution presets like #SomeSpanOrSingle or #Materialized
 * can be used which can offer close to the same performance in most cases without many of the
 * downsides.
 *
 * The main entry point in the API is the #multi_eval::execute function and its corresponding
 * utilities for specific parameter counts.
 */

#include "BLI_devirtualize_parameters.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_parameter_pack_utils.hh"

namespace blender::multi_eval {

enum class ParamCategory {
  Input,
  Output,
  Mutable,
};

template<ParamCategory Category, typename T> struct ParamTag {
  static constexpr ParamCategory category = Category;
  using base_type = T;
};

/** Method to execute a function in case devirtualization was not possible. */
enum class FallbackMode {
  /** Access all elements in virtual arrays through virtual function calls. */
  Simple,
  /** Process elements in chunks to reduce virtual function call overhead. */
  Materialized,
};

/**
 * These presets determine what code is generated for evaluation. Different presets make different
 * trade-offs between run-time performance and compile-time/binary size.
 */
namespace exec_presets {

/**
 * The "naive" method. Every element is processed separately and input values are retrieved from
 * the virtual arrays one by one. This generates the least amount of code, but is also the slowest
 * method.
 */
struct Simple {
  static constexpr bool use_devirtualization = false;
  static constexpr FallbackMode fallback_mode = FallbackMode::Simple;
};

/**
 * This is an improvement over the #Simple method. It still generates a relatively small amount of
 * code, because the function is only instantiated once. It's generally faster than #Simple,
 * because inputs are retrieved from the virtual arrays in chunks, reducing virtual method call
 * overhead.
 */
struct Materialized {
  static constexpr bool use_devirtualization = false;
  static constexpr FallbackMode fallback_mode = FallbackMode::Materialized;
};

/**
 * The most efficient preset, but also potentially generates a lot of code (exponential in the
 * number of inputs of the function). It generates separate optimized loops for all combinations of
 * inputs. This should be used for small functions of which all inputs are likely to be single
 * values or spans, and the number of inputs is relatively small.
 */
struct AllSpanOrSingle {
  static constexpr bool use_devirtualization = true;
  static constexpr FallbackMode fallback_mode = FallbackMode::Materialized;

  template<typename... ParamTags, typename... LoadedParams, size_t... I>
  auto create_devirtualizers(TypeSequence<ParamTags...> /*param_tags*/,
                             std::index_sequence<I...> /*indices*/,
                             const std::tuple<LoadedParams...> &loaded_params) const
  {
    return std::make_tuple([&]() {
      using ParamTag = ParamTags;
      using T = typename ParamTag::base_type;
      if constexpr (ParamTag::category == ParamCategory::Input) {
        const auto &varray_impl = *std::get<I>(loaded_params);
        return VArrayDevirtualizer<T, true, true>{varray_impl.common_info(), varray_impl.size()};
      }
      else if constexpr (ELEM(ParamTag::category, ParamCategory::Output, ParamCategory::Mutable)) {
        T *ptr = std::get<I>(loaded_params);
        return BasicDevirtualizer<T *>{ptr};
      }
    }()...);
  }
};

/**
 * A slightly weaker variant of #AllSpanOrSingle. It generates less code, because it assumes that
 * some of the inputs are most likely single values. It should be used for small functions which
 * have too many inputs to make #AllSpanOrSingle a reasonable choice.
 */
template<size_t... Indices> struct SomeSpanOrSingle {
  static constexpr bool use_devirtualization = true;
  static constexpr FallbackMode fallback_mode = FallbackMode::Materialized;

  template<typename... ParamTags, typename... LoadedParams, size_t... I>
  auto create_devirtualizers(TypeSequence<ParamTags...> /*param_tags*/,
                             std::index_sequence<I...> /*indices*/,
                             const std::tuple<LoadedParams...> &loaded_params) const
  {
    return std::make_tuple([&]() {
      using ParamTag = ParamTags;
      using T = typename ParamTag::base_type;

      if constexpr (ParamTag::category == ParamCategory::Input) {
        constexpr bool UseSpan = ValueSequence<size_t, Indices...>::template contains<I>();
        const auto &varray_impl = *std::get<I>(loaded_params);
        return VArrayDevirtualizer<T, true, UseSpan>{varray_impl.common_info(),
                                                     varray_impl.size()};
      }
      else if constexpr (ELEM(ParamTag::category, ParamCategory::Output, ParamCategory::Mutable)) {
        T *ptr = std::get<I>(loaded_params);
        return BasicDevirtualizer<T *>{ptr};
      }
    }()...);
  }
};

}  // namespace exec_presets

template<typename T>
concept ExecPresetC = requires {
  { T::use_devirtualization } -> std::convertible_to<bool>;
  { T::fallback_mode } -> std::convertible_to<FallbackMode>;

  requires(!T::use_devirtualization ||
           requires(const T preset, const std::tuple<const GVArrayImpl *> &loaded_params) {
             preset.create_devirtualizers(TypeSequence<ParamTag<ParamCategory::Input, int>>(),
                                          std::index_sequence<0>(),
                                          loaded_params);
           });
};

template<typename T>
concept ParamTagC = requires {
  typename T::base_type;
  { T::category } -> std::convertible_to<ParamCategory>;
};

namespace detail {

enum class MaterializeArgMode {
  Unknown,
  Single,
  Span,
  Materialized,
};

template<typename ParamTag> struct MaterializeArgInfo {
  MaterializeArgMode mode = MaterializeArgMode::Unknown;
  const typename ParamTag::base_type *internal_span_data;
};

template<typename... ParamTags, typename ElementFn, typename... Chunks>
#if (defined(__GNUC__) && !defined(__clang__))
[[gnu::optimize("-funroll-loops")]] [[gnu::optimize("O3")]]
#endif
inline void execute_materialized_hot_loop(TypeSequence<ParamTags...> /*param_tags*/,
                                          const ElementFn &element_fn,
                                          const int64_t size,
                                          Chunks &&__restrict... chunks)
{
  for (int64_t i = 0; i < size; i++) {
    element_fn(chunks[i]...);
  }
}

template<int64_t ChunkSize,
         ParamTagC... ParamTags,
         size_t... I,
         typename ElementFn,
         typename... Params>
inline void execute_materialized(TypeSequence<ParamTags...> /*param_tags*/,
                                 std::index_sequence<I...> /*indices*/,
                                 ElementFn &&element_fn,
                                 const IndexMaskSegment mask,
                                 const std::tuple<Params...> &params)
{
  const int64_t mask_size = mask.size();
  const int64_t tmp_buffer_size = std::min(mask_size, ChunkSize);

  /* Local buffers that are used to temporarily store values for processing. */
  std::tuple<TypedBuffer<typename ParamTags::base_type, ChunkSize>...> temporary_buffers;

  /* Information about every parameter. */
  std::tuple<MaterializeArgInfo<ParamTags>...> args_info;

  (
      /* Setup information for all parameters. */
      [&] {
        /* Use `typedef` instead of `using` to work around a compiler bug. */
        using ParamTag = ParamTags;
        using T = typename ParamTag::base_type;
        [[maybe_unused]] MaterializeArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
        if constexpr (ParamTag::category == ParamCategory::Input) {
          const auto &varray_impl = *std::get<I>(params);
          const CommonVArrayInfo common_info = varray_impl.common_info();
          if (common_info.type == CommonVArrayInfo::Type::Single) {
            /* If an input #VArray is a single value, we have to fill the buffer with that value
             * only once. The same unchanged buffer can then be reused in every chunk. */
            const T &in_single = *static_cast<const T *>(common_info.data);
            T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
            uninitialized_fill_n(tmp_buffer, tmp_buffer_size, in_single);
            arg_info.mode = MaterializeArgMode::Single;
          }
          else if (common_info.type == CommonVArrayInfo::Type::Span) {
            /* Remember the span so that it doesn't have to be retrieved in every iteration. */
            arg_info.internal_span_data = static_cast<const T *>(common_info.data);
          }
          else {
            arg_info.internal_span_data = nullptr;
          }
        }
      }(),
      ...);

  IndexMaskFromSegment index_mask_from_segment;
  const int64_t segment_offset = mask.offset();

  /* Outer loop over all chunks. */
  for (int64_t chunk_start = 0; chunk_start < mask_size; chunk_start += ChunkSize) {
    const int64_t chunk_end = std::min<int64_t>(chunk_start + ChunkSize, mask_size);
    const int64_t chunk_size = chunk_end - chunk_start;
    const IndexMaskSegment sliced_mask = mask.slice(chunk_start, chunk_size);
    const int64_t mask_start = sliced_mask[0];
    const bool sliced_mask_is_range = unique_sorted_indices::non_empty_is_range(
        sliced_mask.base_span());

    /* Move mutable data into temporary array. */
    if (!sliced_mask_is_range) {
      (
          [&] {
            /* Use `typedef` instead of `using` to work around a compiler bug. */
            using ParamTag = ParamTags;
            using T = typename ParamTag::base_type;
            if constexpr (ParamTag::category == ParamCategory::Mutable) {
              T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
              T *param_buffer = std::get<I>(params);
              for (int64_t i = 0; i < chunk_size; i++) {
                new (tmp_buffer + i) T(std::move(param_buffer[sliced_mask[i]]));
              }
            }
          }(),
          ...);
    }

    const IndexMask *current_segment_mask = nullptr;

    execute_materialized_hot_loop(
        TypeSequence<ParamTags...>(),
        element_fn,
        chunk_size,
        /* Prepare every parameter for this chunk. */
        [&] {
          using ParamTag = ParamTags;
          using T = typename ParamTag::base_type;
          [[maybe_unused]] MaterializeArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
          T *tmp_buffer = std::get<I>(temporary_buffers);
          if constexpr (ParamTag::category == ParamCategory::Input) {
            if (arg_info.mode == MaterializeArgMode::Single) {
              /* The single value has been filled into a buffer already reused for every chunk. */
              return const_cast<const T *>(tmp_buffer);
            }
            if (sliced_mask_is_range && arg_info.internal_span_data != nullptr) {
              /* In this case we can just use an existing span instead of "compressing" it into
               * a new temporary buffer. */
              arg_info.mode = MaterializeArgMode::Span;
              return arg_info.internal_span_data + mask_start;
            }
            const auto &varray_impl = *std::get<I>(params);
            if (current_segment_mask == nullptr) {
              current_segment_mask = &index_mask_from_segment.update(
                  {segment_offset, sliced_mask.base_span()});
            }
            /* As a fallback, do a virtual function call to retrieve all elements in the current
             * chunk. The elements are stored in a temporary buffer reused for every chunk. */
            varray_impl.materialize_compressed(*current_segment_mask, tmp_buffer, true);
            /* Remember that this parameter has been materialized, so that the values are
             * destructed properly when the chunk is done. */
            arg_info.mode = MaterializeArgMode::Materialized;
            return const_cast<const T *>(tmp_buffer);
          }
          else if constexpr (ELEM(ParamTag::category,
                                  ParamCategory::Output,
                                  ParamCategory::Mutable)) {
            /* For outputs, just pass a pointer. This is important so that `__restrict` works. */
            if (sliced_mask_is_range) {
              /* Can write into the caller-provided buffer directly. */
              T *param_buffer = std::get<I>(params);
              return param_buffer + mask_start;
            }
            /* Use the temporary buffer. The values will have to be copied out of that
             * buffer into the caller-provided buffer afterwards. */
            return tmp_buffer;
          }
        }()...);

    /* Relocate outputs from temporary buffers to buffers provided by caller. */
    if (!sliced_mask_is_range) {
      (
          [&] {
            /* Use `typedef` instead of `using` to work around a compiler bug. */
            using ParamTag = ParamTags;
            using T = typename ParamTag::base_type;
            if constexpr (ParamTag::category == ParamCategory::Output) {
              T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
              T *param_buffer = std::get<I>(params);
              for (int64_t i = 0; i < chunk_size; i++) {
                /* Use placement-new since the buffer is assumed to be uninitialized. */
                new (param_buffer + sliced_mask[i]) T(std::move(tmp_buffer[i]));
                std::destroy_at(tmp_buffer + i);
              }
            }
            else if constexpr (ParamTag::category == ParamCategory::Mutable) {
              T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
              T *param_buffer = std::get<I>(params);
              for (int64_t i = 0; i < chunk_size; i++) {
                /* Move back to the buffer since this is assumed to be initialized already. */
                param_buffer[sliced_mask[i]] = std::move(tmp_buffer[i]);
                std::destroy_at(tmp_buffer + i);
              }
            }
          }(),
          ...);
    }

    (
        /* Destruct values that have been materialized before. */
        [&] {
          /* Use `typedef` instead of `using` to work around a compiler bug. */
          using ParamTag = ParamTags;
          using T = typename ParamTag::base_type;
          [[maybe_unused]] MaterializeArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
          if constexpr (ParamTag::category == ParamCategory::Input) {
            if (arg_info.mode == MaterializeArgMode::Materialized) {
              T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
              destruct_n(tmp_buffer, chunk_size);
            }
          }
        }(),
        ...);
  }

  (
      /* Destruct buffers for single value inputs. */
      [&] {
        /* Use `typedef` instead of `using` to work around a compiler bug. */
        using ParamTag = ParamTags;
        using T = typename ParamTag::base_type;
        [[maybe_unused]] MaterializeArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
        if constexpr (ParamTag::category == ParamCategory::Input) {
          if (arg_info.mode == MaterializeArgMode::Single) {
            T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
            destruct_n(tmp_buffer, tmp_buffer_size);
          }
        }
      }(),
      ...);
}

template<typename Arg> inline decltype(auto) execute_array_fetch(Arg &&arg, const int64_t i)
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

/**
 * Executes #element_fn for all indices in the mask. The passed in #args contain the input as well
 * as output parameters. Usually types in #args are devirtualized (e.g. a `Span<int>` is passed in
 * instead of a `VArray<int>`).
 */
template<typename MaskT, typename... Args, typename ElementFn>
/* Perform additional optimizations on this loop because it is a very hot loop. For example, the
 * math node in geometry nodes is processed here. */
#if (defined(__GNUC__) && !defined(__clang__))
[[gnu::optimize("-funroll-loops")]] [[gnu::optimize("O3")]]
#endif
inline void execute_array(const ElementFn &element_fn,
                          MaskT mask,
                          /* Use restrict to tell the compiler that pointer inputs do not alias
                           * each other. This is important for some compiler optimizations. */
                          Args &&__restrict... args)
{
  if constexpr (std::is_same_v<std::decay_t<MaskT>, IndexRange>) {
    /* Having this explicit loop is necessary for MSVC to be able to vectorize this. */
    const int64_t start = mask.start();
    const int64_t end = mask.one_after_last();
    for (int64_t i = start; i < end; i++) {
      element_fn(execute_array_fetch(args, i)...);
    }
  }
  else {
    for (const int64_t i : mask) {
      element_fn(execute_array_fetch(args, i)...);
    }
  }
}

template<typename ElementFn,
         ExecPresetC ExecPreset,
         typename... ParamTags,
         size_t... I,
         typename... Params>
inline void execute_element_fn_with_params(ElementFn &&element_fn,
                                           const ExecPreset &exec_preset,
                                           const IndexMask &mask,
                                           TypeSequence<ParamTags...> /*param_tags*/,
                                           std::index_sequence<I...> /*indices*/,
                                           Params &&...params)
{
  const auto loaded_params = std::make_tuple([&]() {
    using ParamTag = ParamTags;
    using T = typename ParamTag::base_type;
    using Param = std::decay_t<Params>;
    auto &param = params;
    constexpr ParamCategory category = ParamTag::category;

    if constexpr (category == ParamCategory::Input) {
      static_assert(is_VArray_v<Param> || is_VMutableArray_v<Param> ||
                    is_same_any_v<Param, GVArray, GVMutableArray>);
      assert_value_type<T>(param);
      return param.get_implementation();
    }
    else if constexpr (category == ParamCategory::Output) {
      assert_value_type<T>(param);
      return static_cast<T *>(param.data());
    }
    else if constexpr (category == ParamCategory::Mutable) {
      assert_value_type<T>(param);
      return static_cast<T *>(param.data());
    }
    else {
      BLI_assert_unreachable_static_t(ParamTag);
    }
  }()...);

  bool executed_devirtualized = false;
  if constexpr (ExecPreset::use_devirtualization) {
    /* Get segments before devirtualization to avoid generating this code multiple times. */
    const Vector<std::variant<IndexRange, IndexMaskSegment>, 16> mask_segments =
        mask.to_spans_and_ranges<16>();
    const auto devirtualizers = exec_preset.create_devirtualizers(
        TypeSequence<ParamTags...>(), std::index_sequence<I...>(), loaded_params);
    executed_devirtualized = call_with_devirtualized_parameters(
        devirtualizers, [&](auto &&...args) {
          for (const std::variant<IndexRange, IndexMaskSegment> &segment : mask_segments) {
            if (std::holds_alternative<IndexRange>(segment)) {
              const auto segment_range = std::get<IndexRange>(segment);
              execute_array(std::forward<ElementFn>(element_fn),
                            segment_range,
                            std::forward<decltype(args)>(args)...);
            }
            else {
              const auto segment_indices = std::get<IndexMaskSegment>(segment);
              execute_array(std::forward<ElementFn>(element_fn),
                            segment_indices,
                            std::forward<decltype(args)>(args)...);
            }
          }
        });
  }
  else {
    UNUSED_VARS(exec_preset);
  }

  /* If devirtualized execution was disabled or not possible, use a fallback method which is
   * slower but always works. */
  if (!executed_devirtualized) {
    /* The materialized method is most common because it avoids most virtual function overhead but
     * still instantiates the function only once. */
    if constexpr (ExecPreset::fallback_mode == multi_eval::FallbackMode::Materialized) {
      mask.foreach_segment([&](const IndexMaskSegment segment) {
        execute_materialized<64>(TypeSequence<ParamTags...>(),
                                 std::index_sequence<I...>(),
                                 element_fn,
                                 segment,
                                 loaded_params);
      });
    }
    else {
      /* This fallback is slower because it uses virtual method calls for every element. */
      mask.foreach_segment([&](const IndexMaskSegment segment) {
        execute_array(element_fn, segment, [&]() {
          /* Use `typedef` instead of `using` to work around a compiler bug. */
          using ParamTag = ParamTags;
          using T = typename ParamTag::base_type;
          if constexpr (ParamTag::category == ParamCategory::Input) {
            const auto &varray_impl = *std::get<I>(loaded_params);
            using ImplType = std::decay_t<decltype(varray_impl)>;
            if constexpr (std::is_base_of_v<GVArrayImpl, ImplType>) {
              return GVArray(&varray_impl).typed<T>();
            }
            else {
              return VArray<T>(&varray_impl);
            }
          }
          else if constexpr (ELEM(ParamTag::category,
                                  ParamCategory::Output,
                                  ParamCategory::Mutable))
          {
            T *ptr = std::get<I>(loaded_params);
            return ptr;
          }
        }()...);
      });
    }
  }
}

}  // namespace detail

template<typename ElementFn, ExecPresetC ExecPreset, typename... ParamTags, typename... Params>
inline void execute(ElementFn &&element_fn,
                    const ExecPreset &exec_preset,
                    const IndexMask &mask,
                    TypeSequence<ParamTags...> param_tags,
                    Params &&...params)
{
  detail::execute_element_fn_with_params(std::forward<ElementFn>(element_fn),
                                         exec_preset,
                                         mask,
                                         param_tags,
                                         std::index_sequence_for<ParamTags...>(),
                                         std::forward<Params>(params)...);
}

template<typename ElementFn, ExecPresetC ExecPreset, typename Out, typename... In>
inline void execute_multi_in_1out_uninitialized(const IndexMask &mask,
                                                const ExecPreset &exec_preset,
                                                const VArray<In> &...in_params,
                                                MutableSpan<Out> out_param,
                                                ElementFn &&element_fn)
{
  constexpr auto param_tags =
      TypeSequence<ParamTag<ParamCategory::Input, In>..., ParamTag<ParamCategory::Output, Out>>();
  execute([&](const In &...in, Out &out) { new (&out) Out(element_fn(in...)); },
          exec_preset,
          mask,
          param_tags,
          in_params...,
          out_param);
}

template<typename In1,
         typename Out1,
         typename ElementFn,
         ExecPresetC ExecPreset = exec_presets::Materialized>
inline void execute_1in_1out_uninitialized(
    const IndexMask &mask,
    const VArray<In1> &in1,
    MutableSpan<Out1> out1,
    ElementFn &&element_fn,
    const ExecPreset &exec_preset = exec_presets::Materialized())
{
  execute_multi_in_1out_uninitialized<ElementFn, ExecPreset, Out1, In1>(
      mask, exec_preset, in1, out1, std::forward<ElementFn>(element_fn));
}

template<typename In1,
         typename In2,
         typename Out1,
         typename ElementFn,
         ExecPresetC ExecPreset = exec_presets::Materialized>
inline void execute_2in_1out_uninitialized(
    const IndexMask &mask,
    const VArray<In1> &in1,
    const VArray<In2> &in2,
    MutableSpan<Out1> out1,
    ElementFn &&element_fn,
    const ExecPreset &exec_preset = exec_presets::Materialized())
{
  execute_multi_in_1out_uninitialized<ElementFn, ExecPreset, Out1, In1, In2>(
      mask, exec_preset, in1, in2, out1, std::forward<ElementFn>(element_fn));
}

template<typename In1,
         typename In2,
         typename In3,
         typename Out1,
         typename ElementFn,
         ExecPresetC ExecPreset = exec_presets::Materialized>
inline void execute_3in_1out_uninitialized(
    const IndexMask &mask,
    const VArray<In1> &in1,
    const VArray<In2> &in2,
    const VArray<In3> &in3,
    MutableSpan<Out1> out1,
    ElementFn &&element_fn,
    const ExecPreset &exec_preset = exec_presets::Materialized())
{
  execute_multi_in_1out_uninitialized<ElementFn, ExecPreset, Out1, In1, In2, In3>(
      mask, exec_preset, in1, in2, in3, out1, std::forward<ElementFn>(element_fn));
}

template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename Out1,
         typename ElementFn,
         ExecPresetC ExecPreset = exec_presets::Materialized>
inline void execute_4in_1out_uninitialized(
    const IndexMask &mask,
    const VArray<In1> &in1,
    const VArray<In2> &in2,
    const VArray<In3> &in3,
    const VArray<In4> &in4,
    MutableSpan<Out1> out1,
    ElementFn &&element_fn,
    const ExecPreset &exec_preset = exec_presets::Materialized())
{
  execute_multi_in_1out_uninitialized<ElementFn, ExecPreset, Out1, In1, In2, In3, In4>(
      mask, exec_preset, in1, in2, in3, in4, out1, std::forward<ElementFn>(element_fn));
}

template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename In5,
         typename Out1,
         typename ElementFn,
         ExecPresetC ExecPreset = exec_presets::Materialized>
inline void execute_5in_1out_uninitialized(
    const IndexMask &mask,
    const VArray<In1> &in1,
    const VArray<In2> &in2,
    const VArray<In3> &in3,
    const VArray<In4> &in4,
    const VArray<In5> &in5,
    MutableSpan<Out1> out1,
    ElementFn &&element_fn,
    const ExecPreset &exec_preset = exec_presets::Materialized())
{
  execute_multi_in_1out_uninitialized<ElementFn, ExecPreset, Out1, In1, In2, In3, In4, In5>(
      mask, exec_preset, in1, in2, in3, in4, in5, out1, std::forward<ElementFn>(element_fn));
}

}  // namespace blender::multi_eval
