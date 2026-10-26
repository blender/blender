/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * This file contains several utilities to create multi-functions with less redundant code.
 */

#include "BLI_multi_eval_varray.hh"

#include "FN_multi_function.hh"

namespace blender {

namespace fn::multi_function::build {

using multi_eval::ParamCategory;
using multi_eval::ParamTag;

namespace exec_presets {

using namespace multi_eval::exec_presets;

}  // namespace exec_presets

namespace detail {

template<typename ElementFn, typename ExecPreset, typename... ParamTags, size_t... I>
inline void execute_element_fn_as_multi_function(const ElementFn &element_fn,
                                                 const ExecPreset exec_preset,
                                                 const IndexMask &mask,
                                                 Params params,
                                                 TypeSequence<ParamTags...> /*param_tags*/,
                                                 std::index_sequence<I...> /*indices*/)
{
  multi_eval::execute(
      element_fn, exec_preset, mask, TypeSequence<ParamTags...>(), [&]() -> decltype(auto) {
        /* Use `typedef` instead of `using` to work around a compiler bug. */
        using ParamTag = ParamTags;
        if constexpr (ParamTag::category == ParamCategory::Input) {
          return params.readonly_single_input(I);
        }
        else if constexpr (ParamTag::category == ParamCategory::Output) {
          return params.uninitialized_single_output(I);
        }
        else {
          static_assert(ParamTag::category == ParamCategory::Mutable);
          return params.single_mutable(I);
        }
      }()...);
}

/**
 * `element_fn` is expected to return nothing and to have the following parameters:
 * - For single-inputs: const value or reference.
 * - For single-mutables: non-const reference.
 * - For single-outputs: non-const pointer.
 */
template<typename ElementFn, typename ExecPreset, typename... ParamTags>
inline auto build_multi_function_call_from_element_fn(ElementFn &&element_fn,
                                                      const ExecPreset exec_preset,
                                                      TypeSequence<ParamTags...> /*param_tags*/)
{
  return [element_fn = std::forward<ElementFn>(element_fn), exec_preset](const IndexMask &mask,
                                                                         Params params) {
    execute_element_fn_as_multi_function(element_fn,
                                         exec_preset,
                                         mask,
                                         params,
                                         TypeSequence<ParamTags...>(),
                                         std::index_sequence_for<ParamTags...>());
  };
}

/**
 * A multi function that just invokes the provided function in its #call method.
 */
template<typename CallFn, typename... ParamTags> class CustomMF : public MultiFunction {
 private:
  Signature signature_;
  CallFn call_fn_;

 public:
  CustomMF(const char *name, CallFn call_fn, TypeSequence<ParamTags...> /*param_tags*/)
      : call_fn_(std::move(call_fn))
  {
    SignatureBuilder builder{name, signature_};
    /* Loop over all parameter types and add an entry for each in the signature. */
    (
        [&] {
          using ParamTag = ParamTags;
          using T = typename ParamTag::base_type;
          if constexpr (ParamTag::category == ParamCategory::Input) {
            builder.single_input<T>("");
          }
          else if constexpr (ParamTag::category == ParamCategory::Output) {
            builder.single_output<T>("");
          }
          else {
            static_assert(ParamTag::category == ParamCategory::Mutable);
            builder.single_mutable<T>("");
          }
        }(),
        ...);
    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, Params params, Context /*context*/) const override
  {
    call_fn_(mask, params);
  }
};

template<typename Out, typename... In, typename ElementFn, typename ExecPreset>
inline auto build_multi_function_with_n_inputs_one_output(const char *name,
                                                          ElementFn &&element_fn,
                                                          const ExecPreset exec_preset,
                                                          TypeSequence<In...> /*in_types*/)
{
  constexpr auto param_tags =
      TypeSequence<ParamTag<ParamCategory::Input, In>..., ParamTag<ParamCategory::Output, Out>>();
  auto call_fn = build_multi_function_call_from_element_fn(
      [element_fn = std::forward<ElementFn>(element_fn)](const In &...in, Out &out) {
        new (&out) Out(element_fn(in...));
      },
      exec_preset,
      param_tags);
  return CustomMF(name, std::move(call_fn), param_tags);
}

template<typename Out1, typename Out2, typename... In, typename ElementFn, typename ExecPreset>
inline auto build_multi_function_with_n_inputs_two_outputs(const char *name,
                                                           ElementFn &&element_fn,
                                                           const ExecPreset exec_preset,
                                                           TypeSequence<In...> /*in_types*/)
{
  constexpr auto param_tags = TypeSequence<ParamTag<ParamCategory::Input, In>...,
                                           ParamTag<ParamCategory::Output, Out1>,
                                           ParamTag<ParamCategory::Output, Out2>>();
  auto call_fn = build_multi_function_call_from_element_fn(
      std::forward<ElementFn>(element_fn), exec_preset, param_tags);
  return CustomMF(name, call_fn, param_tags);
}

}  // namespace detail

/** Build multi-function with 1 single-input and 1 single-output parameter. */
template<typename In1,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI1_SO(const char *name,
                   ElementFn &&element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name, std::forward<ElementFn>(element_fn), exec_preset, TypeSequence<In1>());
}

/** Build multi-function with 2 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI2_SO(const char *name,
                   ElementFn &&element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name, std::forward<ElementFn>(element_fn), exec_preset, TypeSequence<In1, In2>());
}

/** Build multi-function with 3 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI3_SO(const char *name,
                   ElementFn &&element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name, std::forward<ElementFn>(element_fn), exec_preset, TypeSequence<In1, In2, In3>());
}

/** Build multi-function with 4 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI4_SO(const char *name,
                   ElementFn &&element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name, std::forward<ElementFn>(element_fn), exec_preset, TypeSequence<In1, In2, In3, In4>());
}

/** Build multi-function with 5 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename In5,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI5_SO(const char *name,
                   ElementFn &&element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name,
      std::forward<ElementFn>(element_fn),
      exec_preset,
      TypeSequence<In1, In2, In3, In4, In5>());
}

/** Build multi-function with 6 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename In5,
         typename In6,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI6_SO(const char *name,
                   ElementFn &&element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name,
      std::forward<ElementFn>(element_fn),
      exec_preset,
      TypeSequence<In1, In2, In3, In4, In5, In6>());
}

/** Build multi-function with 8 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename In5,
         typename In6,
         typename In7,
         typename In8,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI8_SO(const char *name,
                   ElementFn &&element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name,
      std::forward<ElementFn>(element_fn),
      exec_preset,
      TypeSequence<In1, In2, In3, In4, In5, In6, In7, In8>());
}

/** Build multi-function with 1 single-mutable parameter. */
template<typename Mut1, typename ElementFn, typename ExecPreset = exec_presets::AllSpanOrSingle>
inline auto SM(const char *name,
               ElementFn &&element_fn,
               const ExecPreset exec_preset = exec_presets::AllSpanOrSingle())
{
  constexpr auto param_tags = TypeSequence<ParamTag<ParamCategory::Mutable, Mut1>>();
  auto call_fn = detail::build_multi_function_call_from_element_fn(
      std::forward<ElementFn>(element_fn), exec_preset, param_tags);
  return detail::CustomMF(name, call_fn, param_tags);
}

/** Build multi-function with 1 single-input and 2 single-output parameter. */
template<typename In1,
         typename Out1,
         typename Out2,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI1_SO2(const char *name,
                    ElementFn &&element_fn,
                    const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_two_outputs<Out1, Out2>(
      name, std::forward<ElementFn>(element_fn), exec_preset, TypeSequence<In1>());
}

/** Build multi-function with 2 single-input and 2 single-output parameter. */
template<typename In1,
         typename In2,
         typename Out1,
         typename Out2,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI2_SO2(const char *name,
                    ElementFn &&element_fn,
                    const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_two_outputs<Out1, Out2>(
      name, std::forward<ElementFn>(element_fn), exec_preset, TypeSequence<In1, In2>());
}

/** Build multi-function with 3 single-input and 2 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename Out1,
         typename Out2,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI3_SO2(const char *name,
                    ElementFn &&element_fn,
                    const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_two_outputs<Out1, Out2>(
      name, std::forward<ElementFn>(element_fn), exec_preset, TypeSequence<In1, In2, In3>());
}

/** Build multi-function with 4 single-input and 2 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename Out1,
         typename Out2,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI4_SO2(const char *name,
                    ElementFn &&element_fn,
                    const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_two_outputs<Out1, Out2>(
      name, std::forward<ElementFn>(element_fn), exec_preset, TypeSequence<In1, In2, In3, In4>());
}

/** Build multi-function with 5 single-input and 2 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename In5,
         typename Out1,
         typename Out2,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI5_SO2(const char *name,
                    ElementFn &&element_fn,
                    const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_two_outputs<Out1, Out2>(
      name,
      std::forward<ElementFn>(element_fn),
      exec_preset,
      TypeSequence<In1, In2, In3, In4, In5>());
}

/** Build multi-function with 1 single-input and 3 single output parameter. */
template<typename In1,
         typename Out1,
         typename Out2,
         typename Out3,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI1_SO3(const char *name,
                    ElementFn &&element_fn,
                    const ExecPreset exec_preset = exec_presets::Materialized())
{
  constexpr auto param_tags = TypeSequence<ParamTag<ParamCategory::Input, In1>,
                                           ParamTag<ParamCategory::Output, Out1>,
                                           ParamTag<ParamCategory::Output, Out2>,
                                           ParamTag<ParamCategory::Output, Out3>>();
  auto call_fn = detail::build_multi_function_call_from_element_fn(
      std::forward<ElementFn>(element_fn), exec_preset, param_tags);
  return detail::CustomMF(name, call_fn, param_tags);
}

/** Build multi-function with 1 single-input and 4 single output parameter. */
template<typename In1,
         typename Out1,
         typename Out2,
         typename Out3,
         typename Out4,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI1_SO4(const char *name,
                    ElementFn &&element_fn,
                    const ExecPreset exec_preset = exec_presets::Materialized())
{
  constexpr auto param_tags = TypeSequence<ParamTag<ParamCategory::Input, In1>,
                                           ParamTag<ParamCategory::Output, Out1>,
                                           ParamTag<ParamCategory::Output, Out2>,
                                           ParamTag<ParamCategory::Output, Out3>,
                                           ParamTag<ParamCategory::Output, Out4>>();
  auto call_fn = detail::build_multi_function_call_from_element_fn(
      std::forward<ElementFn>(element_fn), exec_preset, param_tags);
  return detail::CustomMF(name, call_fn, param_tags);
}

}  // namespace fn::multi_function::build

namespace fn::multi_function {

/**
 * A multi-function that outputs the same value every time. The value is not owned by an instance
 * of this function. If #make_value_copy is false, the caller is responsible for destructing and
 * freeing the value.
 */
class CustomMF_GenericConstant : public MultiFunction {
 public:
  /* For compatible hash with typed class. */
  static constexpr int8_t HASH_ID = 0;

 private:
  const CPPType &type_;
  const void *value_;
  Signature signature_;
  bool owns_value_;

  template<typename T> friend class CustomMF_Constant;

 public:
  CustomMF_GenericConstant(const CPPType &type, const void *value, bool make_value_copy);
  ~CustomMF_GenericConstant() override;
  void call(const IndexMask &mask, Params params, Context context) const override;
  void hash_unique(UniqueHashBytes &hash) const override;
  bool equals(const MultiFunction &other) const override;
};

/**
 * A multi-function that outputs the same array every time. The array is not owned by in instance
 * of this function. The caller is responsible for destructing and freeing the values.
 */
class CustomMF_GenericConstantArray : public MultiFunction {
 private:
  GSpan array_;
  Signature signature_;

 public:
  CustomMF_GenericConstantArray(GSpan array);
  void call(const IndexMask &mask, Params params, Context context) const override;
};

/**
 * Generates a multi-function that outputs a constant value.
 */
template<typename T> class CustomMF_Constant : public MultiFunction {
 private:
  T value_;
  Signature signature_;

 public:
  template<typename U> CustomMF_Constant(U &&value) : value_(std::forward<U>(value))
  {
    SignatureBuilder builder{"Constant", signature_};
    builder.single_output<T>("Value");
    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, Params params, Context /*context*/) const override
  {
    MutableSpan<T> output = params.uninitialized_single_output<T>(0);
    mask.foreach_index_optimized<int64_t>([&](const int64_t i) { new (&output[i]) T(value_); });
  }

  void hash_unique(UniqueHashBytes &hash) const override
  {
    hash.add(&CustomMF_GenericConstant::HASH_ID);
    hash_unique_default(value_, hash);
    hash.add(&CPPType::get<T>());
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
  Signature signature_;

 public:
  CustomMF_DefaultOutput(Span<DataType> input_types, Span<DataType> output_types);
  void call(const IndexMask &mask, Params params, Context context) const override;
};

class CustomMF_GenericCopy : public MultiFunction {
 private:
  Signature signature_;

 public:
  CustomMF_GenericCopy(DataType data_type);
  void call(const IndexMask &mask, Params params, Context context) const override;
};

}  // namespace fn::multi_function

}  // namespace blender
