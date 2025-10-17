/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * This file provides an Params and ParamsBuilder structure.
 *
 * `ParamsBuilder` is used by a function caller to be prepare all parameters that are passed into
 * the function. `Params` is then used inside the called function to access the parameters.
 */

#include <variant>

#include "BLI_generic_pointer.hh"
#include "BLI_generic_vector_array.hh"
#include "BLI_generic_virtual_vector_array.hh"
#include "BLI_resource_scope.hh"

#include "FN_multi_function_signature.hh"

namespace blender::fn::multi_function {

class ParamsBuilder {
 private:
  std::unique_ptr<ResourceScope> scope_;
  const Signature *signature_;
  const IndexMask &mask_;
  int64_t min_array_size_;
  Vector<std::variant<GVArray, GMutableSpan, const GVVectorArray *, GVectorArray *>>
      actual_params_;

  friend class Params;

  ParamsBuilder(const Signature &signature, const IndexMask &mask);

 public:
  /**
   * The indices referenced by the #mask has to live longer than the params builder. This is
   * because the it might have to destruct elements for all masked indices in the end.
   */
  ParamsBuilder(const class MultiFunction &fn, const IndexMask *mask);

  template<typename T> void add_readonly_single_input_value(T value, StringRef expected_name = "");
  template<typename T>
  void add_readonly_single_input(const T *value, StringRef expected_name = "");
  void add_readonly_single_input(const GSpan span, StringRef expected_name = "");
  void add_readonly_single_input(GPointer value, StringRef expected_name = "");
  void add_readonly_single_input(GVArray varray, StringRef expected_name = "");

  void add_readonly_vector_input(const GVectorArray &vector_array, StringRef expected_name = "");
  void add_readonly_vector_input(const GSpan single_vector, StringRef expected_name = "");
  void add_readonly_vector_input(const GVVectorArray &ref, StringRef expected_name = "");

  template<typename T>
  void add_uninitialized_single_output(T *value, StringRef expected_name = "");
  void add_uninitialized_single_output(GMutableSpan ref, StringRef expected_name = "");

  void add_ignored_single_output(StringRef expected_name = "");

  void add_vector_output(GVectorArray &vector_array, StringRef expected_name = "");

  void add_single_mutable(GMutableSpan ref, StringRef expected_name = "");

  void add_vector_mutable(GVectorArray &vector_array, StringRef expected_name = "");

  int next_param_index() const;

  GMutableSpan computed_array(int param_index);

  GVectorArray &computed_vector_array(int param_index);

 private:
  void assert_current_param_type(ParamType param_type, StringRef expected_name = "");
  void assert_current_param_name(StringRef expected_name);

  int current_param_index() const;

  ResourceScope &resource_scope();

  void add_unused_output_for_unsupporting_function(const CPPType &type);
};

class Params {
 private:
  ParamsBuilder *builder_;

 public:
  Params(ParamsBuilder &builder) : builder_(&builder) {}

  template<typename T> VArray<T> readonly_single_input(int param_index, StringRef name = "");
  const GVArray &readonly_single_input(int param_index, StringRef name = "");

  /**
   * \return True when the caller provided a buffer for this output parameter. This allows the
   * called multi-function to skip some computation. It is still valid to call
   * #uninitialized_single_output when this returns false. In this case a new temporary buffer is
   * allocated.
   */
  bool single_output_is_required(int param_index, StringRef name = "");

  template<typename T>
  MutableSpan<T> uninitialized_single_output(int param_index, StringRef name = "");
  GMutableSpan uninitialized_single_output(int param_index, StringRef name = "");

  /**
   * Same as #uninitialized_single_output, but returns an empty span when the output is not
   * required.
   */
  template<typename T>
  MutableSpan<T> uninitialized_single_output_if_required(int param_index, StringRef name = "");
  GMutableSpan uninitialized_single_output_if_required(int param_index, StringRef name = "");

  template<typename T>
  const VVectorArray<T> &readonly_vector_input(int param_index, StringRef name = "");
  const GVVectorArray &readonly_vector_input(int param_index, StringRef name = "");

  template<typename T>
  GVectorArray_TypedMutableRef<T> vector_output(int param_index, StringRef name = "");
  GVectorArray &vector_output(int param_index, StringRef name = "");

  template<typename T> MutableSpan<T> single_mutable(int param_index, StringRef name = "");
  GMutableSpan single_mutable(int param_index, StringRef name = "");

  template<typename T>
  GVectorArray_TypedMutableRef<T> vector_mutable(int param_index, StringRef name = "");
  GVectorArray &vector_mutable(int param_index, StringRef name = "");

 private:
  void assert_correct_param(int param_index, StringRef name, ParamType param_type);
  void assert_correct_param(int param_index, StringRef name, ParamCategory category);
};

/* -------------------------------------------------------------------- */
/** \name #Paramsbuilder Inline Methods
 * \{ */

inline ParamsBuilder::ParamsBuilder(const Signature &signature, const IndexMask &mask)
    : signature_(&signature), mask_(mask), min_array_size_(mask.min_array_size())
{
  actual_params_.reserve(signature.params.size());
}

template<typename T>
inline void ParamsBuilder::add_readonly_single_input_value(T value, StringRef expected_name)
{
  this->assert_current_param_type(ParamType::ForSingleInput(CPPType::get<T>()), expected_name);
  actual_params_.append_unchecked_as(std::in_place_type<GVArray>,
                                     varray_tag::single{},
                                     CPPType::get<T>(),
                                     min_array_size_,
                                     &value);
}

template<typename T>
inline void ParamsBuilder::add_readonly_single_input(const T *value, StringRef expected_name)
{
  this->assert_current_param_type(ParamType::ForSingleInput(CPPType::get<T>()), expected_name);
  actual_params_.append_unchecked_as(std::in_place_type<GVArray>,
                                     varray_tag::single_ref{},
                                     CPPType::get<T>(),
                                     min_array_size_,
                                     value);
}

inline void ParamsBuilder::add_readonly_single_input(const GSpan span, StringRef expected_name)
{
  this->assert_current_param_type(ParamType::ForSingleInput(span.type()), expected_name);
  BLI_assert(span.size() >= min_array_size_);
  actual_params_.append_unchecked_as(std::in_place_type<GVArray>, varray_tag::span{}, span);
}

inline void ParamsBuilder::add_readonly_single_input(GPointer value, StringRef expected_name)
{
  this->assert_current_param_type(ParamType::ForSingleInput(*value.type()), expected_name);
  actual_params_.append_unchecked_as(std::in_place_type<GVArray>,
                                     varray_tag::single_ref{},
                                     *value.type(),
                                     min_array_size_,
                                     value.get());
}

inline void ParamsBuilder::add_readonly_single_input(GVArray varray, StringRef expected_name)
{
  this->assert_current_param_type(ParamType::ForSingleInput(varray.type()), expected_name);
  BLI_assert(varray.size() >= min_array_size_);
  actual_params_.append_unchecked_as(std::in_place_type<GVArray>, std::move(varray));
}

inline void ParamsBuilder::add_readonly_vector_input(const GVectorArray &vector_array,
                                                     StringRef expected_name)
{
  this->add_readonly_vector_input(
      this->resource_scope().construct<GVVectorArray_For_GVectorArray>(vector_array),
      expected_name);
}

inline void ParamsBuilder::add_readonly_vector_input(const GSpan single_vector,
                                                     StringRef expected_name)
{
  this->add_readonly_vector_input(this->resource_scope().construct<GVVectorArray_For_SingleGSpan>(
                                      single_vector, min_array_size_),
                                  expected_name);
}

inline void ParamsBuilder::add_readonly_vector_input(const GVVectorArray &ref,
                                                     StringRef expected_name)
{
  this->assert_current_param_type(ParamType::ForVectorInput(ref.type()), expected_name);
  BLI_assert(ref.size() >= min_array_size_);
  actual_params_.append_unchecked_as(std::in_place_type<const GVVectorArray *>, &ref);
}

template<typename T>
inline void ParamsBuilder::add_uninitialized_single_output(T *value, StringRef expected_name)
{
  this->add_uninitialized_single_output(GMutableSpan(CPPType::get<T>(), value, 1), expected_name);
}

inline void ParamsBuilder::add_uninitialized_single_output(GMutableSpan ref,
                                                           StringRef expected_name)
{
  this->assert_current_param_type(ParamType::ForSingleOutput(ref.type()), expected_name);
  BLI_assert(ref.size() >= min_array_size_);
  actual_params_.append_unchecked_as(std::in_place_type<GMutableSpan>, ref);
}

inline void ParamsBuilder::add_ignored_single_output(StringRef expected_name)
{
  this->assert_current_param_name(expected_name);
  const int param_index = this->current_param_index();
  const ParamType &param_type = signature_->params[param_index].type;
  BLI_assert(param_type.category() == ParamCategory::SingleOutput);
  const DataType data_type = param_type.data_type();
  const CPPType &type = data_type.single_type();

  if (flag_is_set(signature_->params[param_index].flag, ParamFlag::SupportsUnusedOutput)) {
    /* An empty span indicates that this is ignored. */
    const GMutableSpan dummy_span{type};
    actual_params_.append_unchecked_as(std::in_place_type<GMutableSpan>, dummy_span);
  }
  else {
    this->add_unused_output_for_unsupporting_function(type);
  }
}

inline void ParamsBuilder::add_vector_output(GVectorArray &vector_array, StringRef expected_name)
{
  this->assert_current_param_type(ParamType::ForVectorOutput(vector_array.type()), expected_name);
  BLI_assert(vector_array.size() >= min_array_size_);
  actual_params_.append_unchecked_as(std::in_place_type<GVectorArray *>, &vector_array);
}

inline void ParamsBuilder::add_single_mutable(GMutableSpan ref, StringRef expected_name)
{
  this->assert_current_param_type(ParamType::ForMutableSingle(ref.type()), expected_name);
  BLI_assert(ref.size() >= min_array_size_);
  actual_params_.append_unchecked_as(std::in_place_type<GMutableSpan>, ref);
}

inline void ParamsBuilder::add_vector_mutable(GVectorArray &vector_array, StringRef expected_name)
{
  this->assert_current_param_type(ParamType::ForMutableVector(vector_array.type()), expected_name);
  BLI_assert(vector_array.size() >= min_array_size_);
  actual_params_.append_unchecked_as(std::in_place_type<GVectorArray *>, &vector_array);
}

inline int ParamsBuilder::next_param_index() const
{
  return actual_params_.size();
}

inline GMutableSpan ParamsBuilder::computed_array(int param_index)
{
  BLI_assert(ELEM(signature_->params[param_index].type.category(),
                  ParamCategory::SingleOutput,
                  ParamCategory::SingleMutable));
  return std::get<GMutableSpan>(actual_params_[param_index]);
}

inline GVectorArray &ParamsBuilder::computed_vector_array(int param_index)
{
  BLI_assert(ELEM(signature_->params[param_index].type.category(),
                  ParamCategory::VectorOutput,
                  ParamCategory::VectorMutable));
  return *std::get<GVectorArray *>(actual_params_[param_index]);
}

inline void ParamsBuilder::assert_current_param_type(ParamType param_type, StringRef expected_name)
{
  UNUSED_VARS_NDEBUG(param_type, expected_name);
#ifndef NDEBUG
  int param_index = this->current_param_index();

  if (expected_name != "") {
    StringRef actual_name = signature_->params[param_index].name;
    BLI_assert(actual_name == expected_name);
  }

  ParamType expected_type = signature_->params[param_index].type;
  BLI_assert(expected_type == param_type);
#endif
}

inline void ParamsBuilder::assert_current_param_name(StringRef expected_name)
{
  UNUSED_VARS_NDEBUG(expected_name);
#ifndef NDEBUG
  if (expected_name.is_empty()) {
    return;
  }
  const int param_index = this->current_param_index();
  StringRef actual_name = signature_->params[param_index].name;
  BLI_assert(actual_name == expected_name);
#endif
}

inline int ParamsBuilder::current_param_index() const
{
  return actual_params_.size();
}

inline ResourceScope &ParamsBuilder::resource_scope()
{
  if (!scope_) {
    scope_ = std::make_unique<ResourceScope>();
  }
  return *scope_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Params Inline Methods
 * \{ */

template<typename T>
inline VArray<T> Params::readonly_single_input(int param_index, StringRef name)
{
  const GVArray &varray = this->readonly_single_input(param_index, name);
  return varray.typed<T>();
}

inline const GVArray &Params::readonly_single_input(int param_index, StringRef name)
{
  this->assert_correct_param(param_index, name, ParamCategory::SingleInput);
  return std::get<GVArray>(builder_->actual_params_[param_index]);
}

inline bool Params::single_output_is_required(int param_index, StringRef name)
{
  this->assert_correct_param(param_index, name, ParamCategory::SingleOutput);
  return !std::get<GMutableSpan>(builder_->actual_params_[param_index]).is_empty();
}

template<typename T>
inline MutableSpan<T> Params::uninitialized_single_output(int param_index, StringRef name)
{
  return this->uninitialized_single_output(param_index, name).typed<T>();
}

inline GMutableSpan Params::uninitialized_single_output(int param_index, StringRef name)
{
  this->assert_correct_param(param_index, name, ParamCategory::SingleOutput);
  BLI_assert(!flag_is_set(builder_->signature_->params[param_index].flag,
                          ParamFlag::SupportsUnusedOutput));
  GMutableSpan span = std::get<GMutableSpan>(builder_->actual_params_[param_index]);
  BLI_assert(span.size() >= builder_->min_array_size_);
  return span;
}

template<typename T>
inline MutableSpan<T> Params::uninitialized_single_output_if_required(int param_index,
                                                                      StringRef name)
{
  return this->uninitialized_single_output_if_required(param_index, name).typed<T>();
}

inline GMutableSpan Params::uninitialized_single_output_if_required(int param_index,
                                                                    StringRef name)
{
  this->assert_correct_param(param_index, name, ParamCategory::SingleOutput);
  BLI_assert(flag_is_set(builder_->signature_->params[param_index].flag,
                         ParamFlag::SupportsUnusedOutput));
  return std::get<GMutableSpan>(builder_->actual_params_[param_index]);
}

template<typename T>
inline const VVectorArray<T> &Params::readonly_vector_input(int param_index, StringRef name)
{
  const GVVectorArray &vector_array = this->readonly_vector_input(param_index, name);
  return builder_->resource_scope().construct<VVectorArray_For_GVVectorArray<T>>(vector_array);
}

inline const GVVectorArray &Params::readonly_vector_input(int param_index, StringRef name)
{
  this->assert_correct_param(param_index, name, ParamCategory::VectorInput);
  return *std::get<const GVVectorArray *>(builder_->actual_params_[param_index]);
}

template<typename T>
inline GVectorArray_TypedMutableRef<T> Params::vector_output(int param_index, StringRef name)
{
  return {this->vector_output(param_index, name)};
}

inline GVectorArray &Params::vector_output(int param_index, StringRef name)
{
  this->assert_correct_param(param_index, name, ParamCategory::VectorOutput);
  return *std::get<GVectorArray *>(builder_->actual_params_[param_index]);
}

template<typename T> inline MutableSpan<T> Params::single_mutable(int param_index, StringRef name)
{
  return this->single_mutable(param_index, name).typed<T>();
}

inline GMutableSpan Params::single_mutable(int param_index, StringRef name)
{
  this->assert_correct_param(param_index, name, ParamCategory::SingleMutable);
  return std::get<GMutableSpan>(builder_->actual_params_[param_index]);
}

template<typename T>
inline GVectorArray_TypedMutableRef<T> Params::vector_mutable(int param_index, StringRef name)
{
  return {this->vector_mutable(param_index, name)};
}

inline GVectorArray &Params::vector_mutable(int param_index, StringRef name)
{
  this->assert_correct_param(param_index, name, ParamCategory::VectorMutable);
  return *std::get<GVectorArray *>(builder_->actual_params_[param_index]);
}

inline void Params::assert_correct_param(int param_index, StringRef name, ParamType param_type)
{
  UNUSED_VARS_NDEBUG(param_index, name, param_type);
#ifndef NDEBUG
  BLI_assert(builder_->signature_->params[param_index].type == param_type);
  if (name.size() > 0) {
    BLI_assert(builder_->signature_->params[param_index].name == name);
  }
#endif
}

inline void Params::assert_correct_param(int param_index, StringRef name, ParamCategory category)
{
  UNUSED_VARS_NDEBUG(param_index, name, category);
#ifndef NDEBUG
  BLI_assert(builder_->signature_->params[param_index].type.category() == category);
  if (name.size() > 0) {
    BLI_assert(builder_->signature_->params[param_index].name == name);
  }
#endif
}

/** \} */

}  // namespace blender::fn::multi_function
