/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * This file contains several utilities to create multi-functions with less redundant code.
 */

#include <functional>

#include "BLI_devirtualize_parameters.hh"

#include "FN_multi_function.hh"

namespace blender::fn {

namespace devi = devirtualize_parameters;

/**
 * These presets determine what code is generated for a #CustomMF. Different presets make different
 * trade-offs between run-time performance and compile-time/binary size.
 */
namespace CustomMF_presets {

/** Method to execute a function in case devirtualization was not possible. */
enum class FallbackMode {
  /** Access all elements in virtual arrays through virtual function calls. */
  Simple,
  /** Process elements in chunks to reduce virtual function call overhead. */
  Materialized,
};

/**
 * The "naive" method for executing a #CustomMF. Every element is processed separately and input
 * values are retrieved from the virtual arrays one by one. This generates the least amount of
 * code, but is also the slowest method.
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

  template<typename Fn, typename... ParamTypes>
  void try_devirtualize(devi::Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    using devi::DeviMode;
    devirtualizer.try_execute_devirtualized(
        make_value_sequence<DeviMode,
                            DeviMode::Span | DeviMode::Single | DeviMode::Range,
                            sizeof...(ParamTypes)>());
  }
};

/**
 * A slightly weaker variant of #AllSpanOrSingle. It generates less code, because it assumes that
 * some of the inputs are most likely single values. It should be used for small functions which
 * have too many inputs to make #AllSingleOrSpan a reasonable choice.
 */
template<size_t... Indices> struct SomeSpanOrSingle {
  static constexpr bool use_devirtualization = true;
  static constexpr FallbackMode fallback_mode = FallbackMode::Materialized;

  template<typename Fn, typename... ParamTypes>
  void try_devirtualize(devi::Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    using devi::DeviMode;
    devirtualizer.try_execute_devirtualized(
        make_two_value_sequence<DeviMode,
                                DeviMode::Span | DeviMode::Single | DeviMode::Range,
                                DeviMode::Single,
                                sizeof...(ParamTypes),
                                0,
                                (Indices + 1)...>());
  }
};

}  // namespace CustomMF_presets

namespace detail {

/**
 * Executes #element_fn for all indices in the mask. The passed in #args contain the input as well
 * as output parameters. Usually types in #args are devirtualized (e.g. a `Span<int>` is passed in
 * instead of a `VArray<int>`).
 */
template<typename MaskT, typename... Args, typename... ParamTags, size_t... I, typename ElementFn>
void execute_array(TypeSequence<ParamTags...> /* param_tags */,
                   std::index_sequence<I...> /* indices */,
                   ElementFn element_fn,
                   MaskT mask,
                   /* Use restrict to tell the compiler that pointer inputs do not alias each
                    * other. This is important for some compiler optimizations. */
                   Args &&__restrict... args)
{
  for (const int64_t i : mask) {
    element_fn([&]() -> decltype(auto) {
      using ParamTag = typename TypeSequence<ParamTags...>::template at_index<I>;
      if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
        /* For inputs, pass the value (or a reference to it) to the function. */
        return args[i];
      }
      else if constexpr (ParamTag::category == MFParamCategory::SingleOutput) {
        /* For outputs, pass a pointer to the function. This is done instead of passing a
         * reference, because the pointer points to uninitialized memory. */
        return &args[i];
      }
    }()...);
  }
}

}  // namespace detail

namespace materialize_detail {

enum class ArgMode {
  Unknown,
  Single,
  Span,
  Materialized,
};

template<typename ParamTag> struct ArgInfo {
  ArgMode mode = ArgMode::Unknown;
  Span<typename ParamTag::base_type> internal_span;
};

/**
 * Similar to #execute_array but accepts two mask inputs, one for inputs and one for outputs.
 */
template<typename... ParamTags, typename ElementFn, typename... Chunks>
void execute_materialized_impl(TypeSequence<ParamTags...> /* param_tags */,
                               const ElementFn element_fn,
                               const IndexRange in_mask,
                               const IndexMask out_mask,
                               Chunks &&__restrict... chunks)
{
  BLI_assert(in_mask.size() == out_mask.size());
  for (const int64_t i : IndexRange(in_mask.size())) {
    const int64_t in_i = in_mask[i];
    const int64_t out_i = out_mask[i];
    element_fn([&]() -> decltype(auto) {
      using ParamTag = ParamTags;
      if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
        return chunks[in_i];
      }
      else if constexpr (ParamTag::category == MFParamCategory::SingleOutput) {
        /* For outputs, a pointer is passed, because the memory is uninitialized. */
        return &chunks[out_i];
      }
    }()...);
  }
}

/**
 * Executes #element_fn for all indices in #mask. However, instead of processing every element
 * separately, processing happens in chunks. This allows retrieving from input virtual arrays in
 * chunks, which reduces virtual function call overhead.
 */
template<typename... ParamTags, size_t... I, typename ElementFn, typename... Args>
void execute_materialized(TypeSequence<ParamTags...> /* param_tags */,
                          std::index_sequence<I...> /* indices */,
                          const ElementFn element_fn,
                          const IndexMask mask,
                          Args &&...args)
{

  /* In theory, all elements could be processed in one chunk. However, that has the disadvantage
   * that large temporary arrays are needed. Using small chunks allows using small arrays, which
   * are reused multiple times, which improves cache efficiency. The chunk size also shouldn't be
   * too small, because then overhead of the outer loop over chunks becomes significant again. */
  static constexpr int64_t MaxChunkSize = 32;
  const int64_t mask_size = mask.size();
  const int64_t buffer_size = std::min(mask_size, MaxChunkSize);

  /* Local buffers that are used to temporarily store values retrieved from virtual arrays. */
  std::tuple<TypedBuffer<typename ParamTags::base_type, MaxChunkSize>...> buffers_owner;

  /* A span for each parameter which is either empty or points to memory in #buffers_owner. */
  std::tuple<MutableSpan<typename ParamTags::base_type>...> buffers;

  /* Information about every parameter. */
  std::tuple<ArgInfo<ParamTags>...> args_info;

  (
      /* Setup information for all parameters. */
      [&] {
        /* Use `typedef` instead of `using` to work around a compiler bug. */
        typedef ParamTags ParamTag;
        typedef typename ParamTag::base_type T;
        [[maybe_unused]] ArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
        if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
          VArray<T> &varray = *args;
          if (varray.is_single()) {
            /* If an input #VArray is a single value, we have to fill the buffer with that value
             * only once. The same unchanged buffer can then be reused in every chunk. */
            MutableSpan<T> in_chunk{std::get<I>(buffers_owner).ptr(), buffer_size};
            const T in_single = varray.get_internal_single();
            uninitialized_fill_n(in_chunk.data(), in_chunk.size(), in_single);
            std::get<I>(buffers) = in_chunk;
            arg_info.mode = ArgMode::Single;
          }
          else if (varray.is_span()) {
            /* Remember the span so that it doesn't have to be retrieved in every iteration. */
            arg_info.internal_span = varray.get_internal_span();
          }
        }
      }(),
      ...);

  /* Outer loop over all chunks. */
  for (int64_t chunk_start = 0; chunk_start < mask_size; chunk_start += MaxChunkSize) {
    const IndexMask sliced_mask = mask.slice(chunk_start, MaxChunkSize);
    const int64_t chunk_size = sliced_mask.size();
    const bool sliced_mask_is_range = sliced_mask.is_range();

    execute_materialized_impl(
        TypeSequence<ParamTags...>(),
        element_fn,
        /* Inputs are "compressed" into contiguous arrays without gaps. */
        IndexRange(chunk_size),
        /* Outputs are written directly into the correct place in the output arrays. */
        sliced_mask,
        /* Prepare every parameter for this chunk. */
        [&] {
          using ParamTag = ParamTags;
          using T = typename ParamTag::base_type;
          [[maybe_unused]] ArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
          if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
            if (arg_info.mode == ArgMode::Single) {
              /* The single value has been filled into a buffer already reused for every chunk. */
              return Span<T>(std::get<I>(buffers));
            }
            else {
              const VArray<T> &varray = *args;
              if (sliced_mask_is_range) {
                if (!arg_info.internal_span.is_empty()) {
                  /* In this case we can just use an existing span instead of "compressing" it into
                   * a new temporary buffer. */
                  const IndexRange sliced_mask_range = sliced_mask.as_range();
                  arg_info.mode = ArgMode::Span;
                  return arg_info.internal_span.slice(sliced_mask_range);
                }
              }
              /* As a fallback, do a virtual function call to retrieve all elements in the current
               * chunk. The elements are stored in a temporary buffer reused for every chunk. */
              MutableSpan<T> in_chunk{std::get<I>(buffers_owner).ptr(), chunk_size};
              varray.materialize_compressed_to_uninitialized(sliced_mask, in_chunk);
              /* Remember that this parameter has been materialized, so that the values are
               * destructed properly when the chunk is done. */
              arg_info.mode = ArgMode::Materialized;
              return Span<T>(in_chunk);
            }
          }
          else if constexpr (ParamTag::category == MFParamCategory::SingleOutput) {
            /* For outputs, just pass a pointer. This is important so that `__restrict` works. */
            return args->data();
          }
        }()...);

    (
        /* Destruct values that have been materialized before. */
        [&] {
          /* Use `typedef` instead of `using` to work around a compiler bug. */
          typedef ParamTags ParamTag;
          typedef typename ParamTag::base_type T;
          [[maybe_unused]] ArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
          if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
            if (arg_info.mode == ArgMode::Materialized) {
              T *in_chunk = std::get<I>(buffers_owner).ptr();
              destruct_n(in_chunk, chunk_size);
            }
          }
        }(),
        ...);
  }

  (
      /* Destruct buffers for single value inputs. */
      [&] {
        /* Use `typedef` instead of `using` to work around a compiler bug. */
        typedef ParamTags ParamTag;
        typedef typename ParamTag::base_type T;
        [[maybe_unused]] ArgInfo<ParamTags> &arg_info = std::get<I>(args_info);
        if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
          if (arg_info.mode == ArgMode::Single) {
            MutableSpan<T> in_chunk = std::get<I>(buffers);
            destruct_n(in_chunk.data(), in_chunk.size());
          }
        }
      }(),
      ...);
}
}  // namespace materialize_detail

template<typename... ParamTags> class CustomMF : public MultiFunction {
 private:
  std::function<void(IndexMask mask, MFParams params)> fn_;
  MFSignature signature_;

  using TagsSequence = TypeSequence<ParamTags...>;

 public:
  template<typename ElementFn, typename ExecPreset = CustomMF_presets::Materialized>
  CustomMF(const char *name,
           ElementFn element_fn,
           ExecPreset exec_preset = CustomMF_presets::Materialized())
  {
    MFSignatureBuilder signature{name};
    add_signature_parameters(signature, std::make_index_sequence<TagsSequence::size()>());
    signature_ = signature.build();
    this->set_signature(&signature_);

    fn_ = [element_fn, exec_preset](IndexMask mask, MFParams params) {
      execute(
          element_fn, exec_preset, mask, params, std::make_index_sequence<TagsSequence::size()>());
    };
  }

  template<typename ElementFn, typename ExecPreset, size_t... I>
  static void execute(ElementFn element_fn,
                      ExecPreset exec_preset,
                      IndexMask mask,
                      MFParams params,
                      std::index_sequence<I...> /* indices */)
  {
    std::tuple<typename ParamTags::array_type...> retrieved_params;
    (
        /* Get all parameters from #params and store them in #retrieved_params. */
        [&]() {
          /* Use `typedef` instead of `using` to work around a compiler bug. */
          typedef typename TagsSequence::template at_index<I> ParamTag;
          typedef typename ParamTag::base_type T;

          if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
            std::get<I>(retrieved_params) = params.readonly_single_input<T>(I);
          }
          if constexpr (ParamTag::category == MFParamCategory::SingleOutput) {
            std::get<I>(retrieved_params) = params.uninitialized_single_output<T>(I);
          }
        }(),
        ...);

    auto array_executor = [&](auto &&...args) {
      detail::execute_array(TagsSequence(),
                            std::make_index_sequence<TagsSequence::size()>(),
                            element_fn,
                            std::forward<decltype(args)>(args)...);
    };

    /* First try devirtualized execution, since this is the most efficient. */
    bool executed_devirtualized = false;
    if constexpr (ExecPreset::use_devirtualization) {
      devi::Devirtualizer<decltype(array_executor), IndexMask, typename ParamTags::array_type...>
          devirtualizer{
              array_executor, &mask, [&] { return &std::get<I>(retrieved_params); }()...};
      exec_preset.try_devirtualize(devirtualizer);
      executed_devirtualized = devirtualizer.executed();
    }

    /* If devirtualized execution was disabled or not possible, use a fallback method which is
     * slower but always works. */
    if (!executed_devirtualized) {
      if constexpr (ExecPreset::fallback_mode == CustomMF_presets::FallbackMode::Materialized) {
        materialize_detail::execute_materialized(
            TypeSequence<ParamTags...>(), std::index_sequence<I...>(), element_fn, mask, [&] {
              return &std::get<I>(retrieved_params);
            }()...);
      }
      else {
        detail::execute_array(TagsSequence(),
                              std::make_index_sequence<TagsSequence::size()>(),
                              element_fn,
                              mask,
                              std::get<I>(retrieved_params)...);
      }
    }
  }

  template<size_t... I>
  static void add_signature_parameters(MFSignatureBuilder &signature,
                                       std::index_sequence<I...> /* indices */)
  {
    (
        /* Loop over all parameter types and add an entry for each in the signature. */
        [&] {
          /* Use `typedef` instead of `using` to work around a compiler bug. */
          typedef typename TagsSequence::template at_index<I> ParamTag;
          signature.add(ParamTag(), "");
        }(),
        ...);
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    fn_(mask, params);
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single output (SO) of type Out1
 *
 * This example creates a function that adds 10 to the incoming values:
 * `CustomMF_SI_SO<int, int> fn("add 10", [](int value) { return value + 10; });`
 */
template<typename In1, typename Out1>
class CustomMF_SI_SO : public CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                                       MFParamTag<MFParamCategory::SingleOutput, Out1>> {
 public:
  template<typename ElementFn, typename ExecPreset = CustomMF_presets::Materialized>
  CustomMF_SI_SO(const char *name,
                 ElementFn element_fn,
                 ExecPreset exec_preset = CustomMF_presets::Materialized())
      : CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                 MFParamTag<MFParamCategory::SingleOutput, Out1>>(
            name,
            [element_fn](const In1 &in1, Out1 *out1) { new (out1) Out1(element_fn(in1)); },
            exec_preset)
  {
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single input (SI) of type In2
 * 3. single output (SO) of type Out1
 */
template<typename In1, typename In2, typename Out1>
class CustomMF_SI_SI_SO : public CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                                          MFParamTag<MFParamCategory::SingleInput, In2>,
                                          MFParamTag<MFParamCategory::SingleOutput, Out1>> {
 public:
  template<typename ElementFn, typename ExecPreset = CustomMF_presets::Materialized>
  CustomMF_SI_SI_SO(const char *name,
                    ElementFn element_fn,
                    ExecPreset exec_preset = CustomMF_presets::Materialized())
      : CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                 MFParamTag<MFParamCategory::SingleInput, In2>,
                 MFParamTag<MFParamCategory::SingleOutput, Out1>>(
            name,
            [element_fn](const In1 &in1, const In2 &in2, Out1 *out1) {
              new (out1) Out1(element_fn(in1, in2));
            },
            exec_preset)
  {
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single input (SI) of type In2
 * 3. single input (SI) of type In3
 * 4. single output (SO) of type Out1
 */
template<typename In1, typename In2, typename In3, typename Out1>
class CustomMF_SI_SI_SI_SO : public CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                                             MFParamTag<MFParamCategory::SingleInput, In2>,
                                             MFParamTag<MFParamCategory::SingleInput, In3>,
                                             MFParamTag<MFParamCategory::SingleOutput, Out1>> {
 public:
  template<typename ElementFn, typename ExecPreset = CustomMF_presets::Materialized>
  CustomMF_SI_SI_SI_SO(const char *name,
                       ElementFn element_fn,
                       ExecPreset exec_preset = CustomMF_presets::Materialized())
      : CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                 MFParamTag<MFParamCategory::SingleInput, In2>,
                 MFParamTag<MFParamCategory::SingleInput, In3>,
                 MFParamTag<MFParamCategory::SingleOutput, Out1>>(
            name,
            [element_fn](const In1 &in1, const In2 &in2, const In3 &in3, Out1 *out1) {
              new (out1) Out1(element_fn(in1, in2, in3));
            },
            exec_preset)
  {
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single input (SI) of type In2
 * 3. single input (SI) of type In3
 * 4. single input (SI) of type In4
 * 5. single output (SO) of type Out1
 */
template<typename In1, typename In2, typename In3, typename In4, typename Out1>
class CustomMF_SI_SI_SI_SI_SO : public CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                                                MFParamTag<MFParamCategory::SingleInput, In2>,
                                                MFParamTag<MFParamCategory::SingleInput, In3>,
                                                MFParamTag<MFParamCategory::SingleInput, In4>,
                                                MFParamTag<MFParamCategory::SingleOutput, Out1>> {
 public:
  template<typename ElementFn, typename ExecPreset = CustomMF_presets::Materialized>
  CustomMF_SI_SI_SI_SI_SO(const char *name,
                          ElementFn element_fn,
                          ExecPreset exec_preset = CustomMF_presets::Materialized())
      : CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                 MFParamTag<MFParamCategory::SingleInput, In2>,
                 MFParamTag<MFParamCategory::SingleInput, In3>,
                 MFParamTag<MFParamCategory::SingleInput, In4>,
                 MFParamTag<MFParamCategory::SingleOutput, Out1>>(
            name,
            [element_fn](
                const In1 &in1, const In2 &in2, const In3 &in3, const In4 &in4, Out1 *out1) {
              new (out1) Out1(element_fn(in1, in2, in3, in4));
            },
            exec_preset)
  {
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single mutable (SM) of type Mut1
 */
template<typename Mut1> class CustomMF_SM : public MultiFunction {
 private:
  using FunctionT = std::function<void(IndexMask, MutableSpan<Mut1>)>;
  FunctionT function_;
  MFSignature signature_;

 public:
  CustomMF_SM(const char *name, FunctionT function) : function_(std::move(function))
  {
    MFSignatureBuilder signature{name};
    signature.single_mutable<Mut1>("Mut1");
    signature_ = signature.build();
    this->set_signature(&signature_);
  }

  template<typename ElementFuncT>
  CustomMF_SM(const char *name, ElementFuncT element_fn)
      : CustomMF_SM(name, CustomMF_SM::create_function(element_fn))
  {
  }

  template<typename ElementFuncT> static FunctionT create_function(ElementFuncT element_fn)
  {
    return [=](IndexMask mask, MutableSpan<Mut1> mut1) {
      mask.to_best_mask_type([&](const auto &mask) {
        for (const int64_t i : mask) {
          element_fn(mut1[i]);
        }
      });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableSpan<Mut1> mut1 = params.single_mutable<Mut1>(0);
    function_(mask, mut1);
  }
};

/**
 * A multi-function that outputs the same value every time. The value is not owned by an instance
 * of this function. If #make_value_copy is false, the caller is responsible for destructing and
 * freeing the value.
 */
class CustomMF_GenericConstant : public MultiFunction {
 private:
  const CPPType &type_;
  const void *value_;
  MFSignature signature_;
  bool owns_value_;

  template<typename T> friend class CustomMF_Constant;

 public:
  CustomMF_GenericConstant(const CPPType &type, const void *value, bool make_value_copy);
  ~CustomMF_GenericConstant();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
  uint64_t hash() const override;
  bool equals(const MultiFunction &other) const override;
};

/**
 * A multi-function that outputs the same array every time. The array is not owned by in instance
 * of this function. The caller is responsible for destructing and freeing the values.
 */
class CustomMF_GenericConstantArray : public MultiFunction {
 private:
  GSpan array_;
  MFSignature signature_;

 public:
  CustomMF_GenericConstantArray(GSpan array);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

/**
 * Generates a multi-function that outputs a constant value.
 */
template<typename T> class CustomMF_Constant : public MultiFunction {
 private:
  T value_;
  MFSignature signature_;

 public:
  template<typename U> CustomMF_Constant(U &&value) : value_(std::forward<U>(value))
  {
    MFSignatureBuilder signature{"Constant"};
    signature.single_output<T>("Value");
    signature_ = signature.build();
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableSpan<T> output = params.uninitialized_single_output<T>(0);
    mask.to_best_mask_type([&](const auto &mask) {
      for (const int64_t i : mask) {
        new (&output[i]) T(value_);
      }
    });
  }

  uint64_t hash() const override
  {
    return get_default_hash(value_);
  }

  bool equals(const MultiFunction &other) const override
  {
    const CustomMF_Constant *other1 = dynamic_cast<const CustomMF_Constant *>(&other);
    if (other1 != nullptr) {
      return value_ == other1->value_;
    }
    const CustomMF_GenericConstant *other2 = dynamic_cast<const CustomMF_GenericConstant *>(
        &other);
    if (other2 != nullptr) {
      const CPPType &type = CPPType::get<T>();
      if (type == other2->type_) {
        return type.is_equal_or_false(static_cast<const void *>(&value_), other2->value_);
      }
    }
    return false;
  }
};

class CustomMF_DefaultOutput : public MultiFunction {
 private:
  int output_amount_;
  MFSignature signature_;

 public:
  CustomMF_DefaultOutput(Span<MFDataType> input_types, Span<MFDataType> output_types);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class CustomMF_GenericCopy : public MultiFunction {
 private:
  MFSignature signature_;

 public:
  CustomMF_GenericCopy(MFDataType data_type);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

}  // namespace blender::fn
