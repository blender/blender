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
 * The main entry point in the API is #evaluate_element_wise_varray.
 */

#include "BLI_devirtualize_parameters.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_multi_eval.hh"
#include "BLI_parameter_pack_utils.hh"
#include "BLI_structure_of_array_fixed.hh"

namespace blender::multi_eval {

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
template<bool Flatten = false, int UnrollAmount = 0> struct MaterializedT {
  static constexpr bool use_devirtualization = false;
  static constexpr FallbackMode fallback_mode = FallbackMode::Materialized;
  static constexpr MultiEvalCompilerAttributes materialize_attributes = {.flatten = Flatten,
                                                                         .unroll = UnrollAmount};
};
using Materialized = MaterializedT<>;
template<int UnrollAmount> using MaterializedUnroll = MaterializedT<false, UnrollAmount>;
using MaterializeFlatten = MaterializedT<true>;
template<int UnrollAmount> using MaterializeFlattenUnroll = MaterializedT<true, UnrollAmount>;

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

template<ExecPresetC ExecPreset>
consteval MultiEvalCompilerAttributes get_materialize_compiler_attributes()
{
  if constexpr (requires { ExecPreset::materialize_attributes; }) {
    return ExecPreset::materialize_attributes;
  }
  return {};
}

struct DummySoaArrays {};

template<bool UseSoa, typename T, int64_t MaxSize> struct TempSoaStorage {
  using type = DummySoaArrays;
};
template<typename T, int64_t MaxSize> struct TempSoaStorage<true, T, MaxSize> {
  using type = FixedSoaArray<T, MaxSize>;
};

template<int64_t MaxChunkSize,
         ParamTagC... ParamTags,
         size_t... I,
         typename ChunkFn,
         typename... Params>
inline void evaluate_chunked_varray_impl(TypeSequence<ParamTags...> /*param_tags*/,
                                         std::index_sequence<I...> /*indices*/,
                                         ChunkFn &&chunk_fn,
                                         const IndexMaskSegment mask,
                                         Params &&...params)
{
  const int64_t mask_size = mask.size();
  const int64_t tmp_buffer_size = std::min(mask_size, MaxChunkSize);

  /* Local buffers that are used to temporarily store values for processing. */
  std::tuple<TypedBuffer<typename ParamTags::base_type, MaxChunkSize>...> temporary_buffers;
  std::tuple<typename TempSoaStorage<ParamTags::use_soa,
                                     typename ParamTags::base_type,
                                     MaxChunkSize>::type...>
      soa_buffers;

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
          const auto &varray_impl = *params;
          const CommonVArrayInfo common_info = varray_impl.common_info();
          if (common_info.type == CommonVArrayInfo::Type::Single) {
            /* If an input #VArray is a single value, we have to fill the buffer with that value
             * only once. The same unchanged buffer can then be reused in every chunk. */
            const T &in_single = *static_cast<const T *>(common_info.data);
            arg_info.mode = MaterializeArgMode::Single;
            if constexpr (ParamTag::use_soa) {
              std::get<I>(soa_buffers).fill(in_single, tmp_buffer_size);
            }
            else {
              T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
              uninitialized_fill_n(tmp_buffer, tmp_buffer_size, in_single);
            }
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
  for (int64_t chunk_start = 0; chunk_start < mask_size; chunk_start += MaxChunkSize) {
    const int64_t chunk_end = std::min<int64_t>(chunk_start + MaxChunkSize, mask_size);
    const int64_t chunk_size = chunk_end - chunk_start;
    const IndexMaskSegment sliced_mask = mask.slice(chunk_start, chunk_size);
    const int64_t mask_start = sliced_mask[0];
    const bool sliced_mask_is_range = unique_sorted_indices::non_empty_is_range(
        sliced_mask.base_span());

    const IndexMask *current_segment_mask = nullptr;

    chunk_fn(chunk_size,
             /* Prepare every parameter for this chunk. */
             [&]() -> decltype(auto) {
               using ParamTag = ParamTags;
               using T = typename ParamTag::base_type;
               [[maybe_unused]] MaterializeArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
               T *tmp_buffer = std::get<I>(temporary_buffers);
               auto &soa_buffer = std::get<I>(soa_buffers);
               if constexpr (ParamTag::category == ParamCategory::Input) {
                 if (arg_info.mode == MaterializeArgMode::Single) {
                   /* The single value has been filled into a buffer already reused for every
                    * chunk. */
                   if constexpr (ParamTag::use_soa) {
                     return soa_buffer;
                   }
                   else {
                     return const_cast<const T *>(tmp_buffer);
                   }
                 }
                 if (sliced_mask_is_range && arg_info.internal_span_data != nullptr) {
                   /* In this case we can just use an existing span instead of "compressing" it
                    * into a new temporary buffer. */
                   arg_info.mode = MaterializeArgMode::Span;
                   const T *data_chunk_start = arg_info.internal_span_data + mask_start;
                   if constexpr (ParamTag::use_soa) {
                     soa_buffer.set_multiple(data_chunk_start, chunk_size);
                     return soa_buffer;
                   }
                   else {
                     return data_chunk_start;
                   }
                 }
                 const auto &varray_impl = *params;
                 if (current_segment_mask == nullptr) {
                   current_segment_mask = &index_mask_from_segment.update(
                       {segment_offset, sliced_mask.base_span()});
                 }
                 /* As a fallback, do a virtual function call to retrieve all elements in the
                  * current chunk. The elements are stored in a temporary buffer reused for every
                  * chunk. */
                 varray_impl.materialize_compressed(*current_segment_mask, tmp_buffer, true);
                 /* Remember that this parameter has been materialized, so that the values are
                  * destructed properly when the chunk is done. */
                 arg_info.mode = MaterializeArgMode::Materialized;
                 if constexpr (ParamTag::use_soa) {
                   soa_buffer.set_multiple(tmp_buffer, chunk_size);
                   return soa_buffer;
                 }
                 else {
                   return const_cast<const T *>(tmp_buffer);
                 }
               }
               else if constexpr (ParamTag::category == ParamCategory::Mutable) {
                 T *param_buffer = params;
                 if constexpr (ParamTag::use_soa) {
                   if (sliced_mask_is_range) {
                     soa_buffer.set_multiple(param_buffer + mask_start, chunk_size);
                   }
                   else {
                     for (int64_t i = 0; i < chunk_size; i++) {
                       soa_buffer.set(i, param_buffer[sliced_mask[i]]);
                     }
                   }
                   return soa_buffer;
                 }
                 else {
                   if (sliced_mask_is_range) {
                     return param_buffer + mask_start;
                   }
                   for (int64_t i = 0; i < chunk_size; i++) {
                     new (tmp_buffer + i) T(std::move(param_buffer[sliced_mask[i]]));
                   }
                   return tmp_buffer;
                 }
               }
               else if constexpr (ParamTag::category == ParamCategory::Output) {
                 if constexpr (ParamTag::use_soa) {
                   return soa_buffer;
                 }
                 else {
                   if (sliced_mask_is_range) {
                     /* Can write into the caller-provided buffer directly. */
                     T *param_buffer = params;
                     return param_buffer + mask_start;
                   }
                   /* Use the temporary buffer. The values will have to be copied out of that
                    * buffer into the caller-provided buffer afterwards. */
                   return tmp_buffer;
                 }
               }
             }()...);

    (
        [&] {
          using ParamTag = ParamTags;
          using T = typename ParamTag::base_type;
          [[maybe_unused]] MaterializeArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
          T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
          auto &soa_buffer = std::get<I>(soa_buffers);
          if constexpr (ParamTag::category == ParamCategory::Input) {
            if (arg_info.mode == MaterializeArgMode::Materialized) {
              destruct_n(tmp_buffer, chunk_size);
            }
          }
          else if constexpr (ParamTag::category == ParamCategory::Mutable) {
            T *param_buffer = params;
            if constexpr (ParamTag::use_soa) {
              if (sliced_mask_is_range) {
                soa_buffer.get_multiple(param_buffer + mask_start, chunk_size);
              }
              else {
                for (int64_t i = 0; i < chunk_size; i++) {
                  param_buffer[sliced_mask[i]] = soa_buffer[i];
                }
              }
            }
            else {
              if (!sliced_mask_is_range) {
                for (int64_t i = 0; i < chunk_size; i++) {
                  param_buffer[sliced_mask[i]] = std::move(tmp_buffer[i]);
                  std::destroy_at(tmp_buffer + i);
                }
              }
            }
          }
          else if constexpr (ParamTag::category == ParamCategory::Output) {
            T *param_buffer = params;
            if constexpr (ParamTag::use_soa) {
              if (sliced_mask_is_range) {
                soa_buffer.get_multiple(param_buffer + mask_start, chunk_size);
              }
              else {
                for (int64_t i = 0; i < chunk_size; i++) {
                  param_buffer[sliced_mask[i]] = soa_buffer[i];
                }
              }
            }
            else {
              if (!sliced_mask_is_range) {
                for (int64_t i = 0; i < chunk_size; i++) {
                  new (param_buffer + sliced_mask[i]) T(std::move(tmp_buffer[i]));
                  std::destroy_at(tmp_buffer + i);
                }
              }
            }
          }
        }(),
        ...);
  }

  (
      /* Destruct buffers for single value inputs. */
      [&] {
        using ParamTag = ParamTags;
        using T = typename ParamTag::base_type;
        [[maybe_unused]] MaterializeArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
        if constexpr (ParamTag::category == ParamCategory::Input) {
          if constexpr (!ParamTag::use_soa) {
            if (arg_info.mode == MaterializeArgMode::Single) {
              T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
              destruct_n(tmp_buffer, tmp_buffer_size);
            }
          }
        }
      }(),
      ...);
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
[[gnu::optimize("-funroll-loops")]]
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
      element_fn(array_fetch(args, i)...);
    }
  }
  else {
    for (const int64_t i : mask) {
      element_fn(array_fetch(args, i)...);
    }
  }
}

/**
 * This unifies various different types of arguments passed to #evaluate_element_wise_varray such
 * that #evaluate_chunked_varray_impl only has to deal with simpler types: VArrayImpl, GVArrayImpl
 * and raw pointers.
 */
template<typename ParamTag, typename T> inline auto to_eval_chunked_arg(T &&v)
{
  using DecayT = std::decay_t<T>;
  using Base = typename ParamTag::base_type;
  if constexpr (ParamTag::category == ParamCategory::Input) {
    /* Any kind of virtual array is a valid input. */
    if constexpr (is_VArray_v<DecayT> || is_VMutableArray_v<DecayT> ||
                  is_same_any_v<DecayT, GVArray, GVMutableArray>)
    {
      assert_value_type<Base>(v);
      if constexpr (is_same_any_v<DecayT, GVArray, GVMutableArray>) {
        return static_cast<const GVArrayImpl *>(v.get_implementation());
      }
      else {
        return static_cast<const VArrayImpl<Base> *>(v.get_implementation());
      }
    }
    else {
      BLI_assert_unreachable_static_t(T);
    }
  }
  else if constexpr (ELEM(ParamTag::category, ParamCategory::Mutable, ParamCategory::Output)) {
    /* Outputs need to be span-like. */
    if constexpr (requires {
                    { v.size() } -> std::integral;
                    { v.data() } -> std::convertible_to<void *>;
                  })
    {
      assert_value_type<Base>(v);
      return static_cast<Base *>(v.data());
    }
    else {
      BLI_assert_unreachable_static_t(T);
    }
  }
}

/**
 * Evaluates the given function for each element using one of multiple possible strategies. The
 * strategy is chosen based on the #ExecPreset. In order of performance:

 * If supported and enabled, it attempts to run a fully devirtualized optimized code path. For
 * example, for simple float addition, there are optimized code paths for adding two varying
 * floats, or a float and a single value.
 *
 * If the inputs don't match any fully devirtualized code path, a generic code path is used.
 * That can be either the materialized/chunked evaluation where the input data is extracted from
 * the virtual arrays in chunks, which reduces virtual method call overhead significantly and the
 * inner loop can still be optimized well by the compiler.
 *
 * Alternatively, a very basic fallback loop is used that does virtual method calls for each input
 * for each index. This is slower but produces the least amount of code. It's mainly used for
 testing
 * currently.
 */
template<typename ElementFn,
         ExecPresetC ExecPreset,
         typename... ParamTags,
         size_t... I,
         typename... Params>
inline void evaluate_element_fn_with_params(ElementFn &&element_fn,
                                            const ExecPreset &exec_preset,
                                            const IndexMask &mask,
                                            TypeSequence<ParamTags...> /*param_tags*/,
                                            std::index_sequence<I...> /*indices*/,
                                            Params &&...params)
{
  const auto loaded_params = std::make_tuple(to_eval_chunked_arg<ParamTags>(params)...);

  /* Attempt the fully devirtualized code path. */
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
      constexpr int64_t MAX_CHUNK_SIZE = 64;
      mask.foreach_segment([&](const IndexMaskSegment segment) {
        evaluate_chunked_varray_impl<MAX_CHUNK_SIZE>(
            TypeSequence<ParamTags...>(),
            std::index_sequence<I...>(),
            [&]<typename... Args>(const int64_t size, Args &&...chunk_args) {
              evaluate_element_wise<get_materialize_compiler_attributes<ExecPreset>()>(
                  element_fn,
                  TypeSequence<ParamTags...>(),
                  size,
                  std::forward<Args>(chunk_args)...);
            },
            segment,
            std::get<I>(loaded_params)...);
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

/**
 * Evaluate the given function for each index in the mask. It automatically handles efficiently
 * retrieving the data from multiple virtual arrays without a lot of virtual method call overhead.
 */
template<typename ElementFn, ExecPresetC ExecPreset, typename... ParamTags, typename... Params>
inline void evaluate_element_wise_varray(ElementFn &&element_fn,
                                         const ExecPreset &exec_preset,
                                         const IndexMask &mask,
                                         TypeSequence<ParamTags...> param_tags,
                                         Params &&...params)
{
  detail::evaluate_element_fn_with_params(std::forward<ElementFn>(element_fn),
                                          exec_preset,
                                          mask,
                                          param_tags,
                                          std::index_sequence_for<ParamTags...>(),
                                          std::forward<Params>(params)...);
}

/** Same as above, but often more convenient with the string template argument. */
template<FixedParamString ParamStr, typename ElementFn, ExecPresetC ExecPreset, typename... Params>
inline void evaluate_element_wise_varray(ElementFn &&element_fn,
                                         const ExecPreset &exec_preset,
                                         const IndexMask &mask,
                                         Params &&...params)
{
  using ParamTagsSeq = decltype(detail::param_tags_from_str_and_args<ParamStr, Params...>());
  [&]<typename... ParamTags>(TypeSequence<ParamTags...> param_tags) {
    evaluate_element_wise_varray(std::forward<ElementFn>(element_fn),
                                 exec_preset,
                                 mask,
                                 param_tags,
                                 std::forward<Params>(params)...);
  }(ParamTagsSeq());
}

/**
 * Utility to process all the indices in the mask in chunks. It takes an arbitrary number of
 * heterogeneous virtual arrays as input and calls the given function multiple times for each
 * chunk.
 */
template<int64_t MaxChunkSize, typename ChunkFn, typename... ParamTags, typename... Params>
inline void evaluate_chunked_varray(ChunkFn &&chunk_fn,
                                    const IndexMask &mask,
                                    TypeSequence<ParamTags...> param_tags,
                                    Params &&...params)
{
  mask.foreach_segment([&](const IndexMaskSegment segment) {
    detail::evaluate_chunked_varray_impl<MaxChunkSize>(
        param_tags,
        std::index_sequence_for<ParamTags...>(),
        chunk_fn,
        segment,
        detail::to_eval_chunked_arg<ParamTags>(params)...);
  });
}

/** Same as above but often more convenient with the template parameter string. */
template<FixedParamString ParamStr, int64_t MaxChunkSize, typename ChunkFn, typename... Params>
inline void evaluate_chunked_varray(ChunkFn &&chunk_fn, const IndexMask &mask, Params &&...params)
{
  using ParamTagsSeq = decltype(detail::param_tags_from_str_and_args<ParamStr, Params...>());
  [&]<typename... ParamTags>(TypeSequence<ParamTags...> param_tags) {
    evaluate_chunked_varray<MaxChunkSize>(
        std::forward<ChunkFn>(chunk_fn), mask, param_tags, std::forward<Params>(params)...);
  }(ParamTagsSeq());
}

}  // namespace blender::multi_eval
