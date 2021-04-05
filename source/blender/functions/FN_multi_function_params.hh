/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup fn
 *
 * This file provides an MFParams and MFParamsBuilder structure.
 *
 * `MFParamsBuilder` is used by a function caller to be prepare all parameters that are passed into
 * the function. `MFParams` is then used inside the called function to access the parameters.
 */

#include "BLI_resource_scope.hh"

#include "FN_generic_vector_array.hh"
#include "FN_generic_virtual_vector_array.hh"
#include "FN_multi_function_signature.hh"

namespace blender::fn {

class MFParamsBuilder {
 private:
  ResourceScope scope_;
  const MFSignature *signature_;
  int64_t min_array_size_;
  Vector<const GVArray *> virtual_arrays_;
  Vector<GMutableSpan> mutable_spans_;
  Vector<const GVVectorArray *> virtual_vector_arrays_;
  Vector<GVectorArray *> vector_arrays_;

  friend class MFParams;

 public:
  MFParamsBuilder(const MFSignature &signature, int64_t min_array_size)
      : signature_(&signature), min_array_size_(min_array_size)
  {
  }

  MFParamsBuilder(const class MultiFunction &fn, int64_t min_array_size);

  template<typename T> void add_readonly_single_input(const T *value, StringRef expected_name = "")
  {
    this->add_readonly_single_input(scope_.construct<GVArrayForSingleValueRef>(
                                        __func__, CPPType::get<T>(), min_array_size_, value),
                                    expected_name);
  }
  void add_readonly_single_input(const GSpan span, StringRef expected_name = "")
  {
    this->add_readonly_single_input(scope_.construct<GVArrayForGSpan>(__func__, span),
                                    expected_name);
  }
  void add_readonly_single_input(const GVArray &ref, StringRef expected_name = "")
  {
    this->assert_current_param_type(MFParamType::ForSingleInput(ref.type()), expected_name);
    BLI_assert(ref.size() >= min_array_size_);
    virtual_arrays_.append(&ref);
  }

  void add_readonly_vector_input(const GVectorArray &vector_array, StringRef expected_name = "")
  {
    this->add_readonly_vector_input(
        scope_.construct<GVVectorArrayForGVectorArray>(__func__, vector_array), expected_name);
  }
  void add_readonly_vector_input(const GVVectorArray &ref, StringRef expected_name = "")
  {
    this->assert_current_param_type(MFParamType::ForVectorInput(ref.type()), expected_name);
    BLI_assert(ref.size() >= min_array_size_);
    virtual_vector_arrays_.append(&ref);
  }

  template<typename T> void add_uninitialized_single_output(T *value, StringRef expected_name = "")
  {
    this->add_uninitialized_single_output(GMutableSpan(CPPType::get<T>(), value, 1),
                                          expected_name);
  }
  void add_uninitialized_single_output(GMutableSpan ref, StringRef expected_name = "")
  {
    this->assert_current_param_type(MFParamType::ForSingleOutput(ref.type()), expected_name);
    BLI_assert(ref.size() >= min_array_size_);
    mutable_spans_.append(ref);
  }

  void add_vector_output(GVectorArray &vector_array, StringRef expected_name = "")
  {
    this->assert_current_param_type(MFParamType::ForVectorOutput(vector_array.type()),
                                    expected_name);
    BLI_assert(vector_array.size() >= min_array_size_);
    vector_arrays_.append(&vector_array);
  }

  void add_single_mutable(GMutableSpan ref, StringRef expected_name = "")
  {
    this->assert_current_param_type(MFParamType::ForMutableSingle(ref.type()), expected_name);
    BLI_assert(ref.size() >= min_array_size_);
    mutable_spans_.append(ref);
  }

  void add_vector_mutable(GVectorArray &vector_array, StringRef expected_name = "")
  {
    this->assert_current_param_type(MFParamType::ForMutableVector(vector_array.type()),
                                    expected_name);
    BLI_assert(vector_array.size() >= min_array_size_);
    vector_arrays_.append(&vector_array);
  }

  GMutableSpan computed_array(int param_index)
  {
    BLI_assert(ELEM(signature_->param_types[param_index].category(),
                    MFParamType::SingleOutput,
                    MFParamType::SingleMutable));
    int data_index = signature_->data_index(param_index);
    return mutable_spans_[data_index];
  }

  GVectorArray &computed_vector_array(int param_index)
  {
    BLI_assert(ELEM(signature_->param_types[param_index].category(),
                    MFParamType::VectorOutput,
                    MFParamType::VectorMutable));
    int data_index = signature_->data_index(param_index);
    return *vector_arrays_[data_index];
  }

  ResourceScope &resource_scope()
  {
    return scope_;
  }

 private:
  void assert_current_param_type(MFParamType param_type, StringRef expected_name = "")
  {
    UNUSED_VARS_NDEBUG(param_type, expected_name);
#ifdef DEBUG
    int param_index = this->current_param_index();

    if (expected_name != "") {
      StringRef actual_name = signature_->param_names[param_index];
      BLI_assert(actual_name == expected_name);
    }

    MFParamType expected_type = signature_->param_types[param_index];
    BLI_assert(expected_type == param_type);
#endif
  }

  int current_param_index() const
  {
    return virtual_arrays_.size() + mutable_spans_.size() + virtual_vector_arrays_.size() +
           vector_arrays_.size();
  }
};

class MFParams {
 private:
  MFParamsBuilder *builder_;

 public:
  MFParams(MFParamsBuilder &builder) : builder_(&builder)
  {
  }

  template<typename T> const VArray<T> &readonly_single_input(int param_index, StringRef name = "")
  {
    const GVArray &array = this->readonly_single_input(param_index, name);
    return builder_->scope_.construct<VArrayForGVArray<T>>(__func__, array);
  }
  const GVArray &readonly_single_input(int param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::SingleInput);
    int data_index = builder_->signature_->data_index(param_index);
    return *builder_->virtual_arrays_[data_index];
  }

  template<typename T>
  MutableSpan<T> uninitialized_single_output(int param_index, StringRef name = "")
  {
    return this->uninitialized_single_output(param_index, name).typed<T>();
  }
  GMutableSpan uninitialized_single_output(int param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::SingleOutput);
    int data_index = builder_->signature_->data_index(param_index);
    return builder_->mutable_spans_[data_index];
  }

  template<typename T>
  const VVectorArray<T> &readonly_vector_input(int param_index, StringRef name = "")
  {
    const GVVectorArray &vector_array = this->readonly_vector_input(param_index, name);
    return builder_->scope_.construct<VVectorArrayForGVVectorArray<T>>(__func__, vector_array);
  }
  const GVVectorArray &readonly_vector_input(int param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::VectorInput);
    int data_index = builder_->signature_->data_index(param_index);
    return *builder_->virtual_vector_arrays_[data_index];
  }

  template<typename T>
  GVectorArray_TypedMutableRef<T> vector_output(int param_index, StringRef name = "")
  {
    return {this->vector_output(param_index, name)};
  }
  GVectorArray &vector_output(int param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::VectorOutput);
    int data_index = builder_->signature_->data_index(param_index);
    return *builder_->vector_arrays_[data_index];
  }

  template<typename T> MutableSpan<T> single_mutable(int param_index, StringRef name = "")
  {
    return this->single_mutable(param_index, name).typed<T>();
  }
  GMutableSpan single_mutable(int param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::SingleMutable);
    int data_index = builder_->signature_->data_index(param_index);
    return builder_->mutable_spans_[data_index];
  }

  template<typename T>
  GVectorArray_TypedMutableRef<T> vector_mutable(int param_index, StringRef name = "")
  {
    return {this->vector_mutable(param_index, name)};
  }
  GVectorArray &vector_mutable(int param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::VectorMutable);
    int data_index = builder_->signature_->data_index(param_index);
    return *builder_->vector_arrays_[data_index];
  }

 private:
  void assert_correct_param(int param_index, StringRef name, MFParamType param_type)
  {
    UNUSED_VARS_NDEBUG(param_index, name, param_type);
#ifdef DEBUG
    BLI_assert(builder_->signature_->param_types[param_index] == param_type);
    if (name.size() > 0) {
      BLI_assert(builder_->signature_->param_names[param_index] == name);
    }
#endif
  }

  void assert_correct_param(int param_index, StringRef name, MFParamType::Category category)
  {
    UNUSED_VARS_NDEBUG(param_index, name, category);
#ifdef DEBUG
    BLI_assert(builder_->signature_->param_types[param_index].category() == category);
    if (name.size() > 0) {
      BLI_assert(builder_->signature_->param_names[param_index] == name);
    }
#endif
  }
};

}  // namespace blender::fn
