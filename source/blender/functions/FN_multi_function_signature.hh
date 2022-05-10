/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * The signature of a multi-function contains the functions name and expected parameters. New
 * signatures should be build using the #MFSignatureBuilder class.
 */

#include "FN_multi_function_param_type.hh"

#include "BLI_vector.hh"

namespace blender::fn {

struct MFSignature {
  /**
   * The name should be statically allocated so that it lives longer than this signature. This is
   * used instead of an #std::string because of the overhead when many functions are created.
   * If the name of the function has to be more dynamic for debugging purposes, override
   * #MultiFunction::debug_name() instead. Then the dynamic name will only be computed when it is
   * actually needed.
   */
  const char *function_name;
  Vector<const char *> param_names;
  Vector<MFParamType> param_types;
  Vector<int> param_data_indices;
  bool depends_on_context = false;

  int data_index(int param_index) const
  {
    return param_data_indices[param_index];
  }
};

class MFSignatureBuilder {
 private:
  MFSignature signature_;
  int span_count_ = 0;
  int virtual_array_count_ = 0;
  int virtual_vector_array_count_ = 0;
  int vector_array_count_ = 0;

 public:
  MFSignatureBuilder(const char *function_name)
  {
    signature_.function_name = function_name;
  }

  MFSignature build() const
  {
    return std::move(signature_);
  }

  /* Input Parameter Types */

  template<typename T> void single_input(const char *name)
  {
    this->single_input(name, CPPType::get<T>());
  }
  void single_input(const char *name, const CPPType &type)
  {
    this->input(name, MFDataType::ForSingle(type));
  }
  template<typename T> void vector_input(const char *name)
  {
    this->vector_input(name, CPPType::get<T>());
  }
  void vector_input(const char *name, const CPPType &base_type)
  {
    this->input(name, MFDataType::ForVector(base_type));
  }
  void input(const char *name, MFDataType data_type)
  {
    signature_.param_names.append(name);
    signature_.param_types.append(MFParamType(MFParamType::Input, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        signature_.param_data_indices.append(virtual_array_count_++);
        break;
      case MFDataType::Vector:
        signature_.param_data_indices.append(virtual_vector_array_count_++);
        break;
    }
  }

  /* Output Parameter Types */

  template<typename T> void single_output(const char *name)
  {
    this->single_output(name, CPPType::get<T>());
  }
  void single_output(const char *name, const CPPType &type)
  {
    this->output(name, MFDataType::ForSingle(type));
  }
  template<typename T> void vector_output(const char *name)
  {
    this->vector_output(name, CPPType::get<T>());
  }
  void vector_output(const char *name, const CPPType &base_type)
  {
    this->output(name, MFDataType::ForVector(base_type));
  }
  void output(const char *name, MFDataType data_type)
  {
    signature_.param_names.append(name);
    signature_.param_types.append(MFParamType(MFParamType::Output, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        signature_.param_data_indices.append(span_count_++);
        break;
      case MFDataType::Vector:
        signature_.param_data_indices.append(vector_array_count_++);
        break;
    }
  }

  /* Mutable Parameter Types */

  template<typename T> void single_mutable(const char *name)
  {
    this->single_mutable(name, CPPType::get<T>());
  }
  void single_mutable(const char *name, const CPPType &type)
  {
    this->mutable_(name, MFDataType::ForSingle(type));
  }
  template<typename T> void vector_mutable(const char *name)
  {
    this->vector_mutable(name, CPPType::get<T>());
  }
  void vector_mutable(const char *name, const CPPType &base_type)
  {
    this->mutable_(name, MFDataType::ForVector(base_type));
  }
  void mutable_(const char *name, MFDataType data_type)
  {
    signature_.param_names.append(name);
    signature_.param_types.append(MFParamType(MFParamType::Mutable, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        signature_.param_data_indices.append(span_count_++);
        break;
      case MFDataType::Vector:
        signature_.param_data_indices.append(vector_array_count_++);
        break;
    }
  }

  void add(const char *name, const MFParamType &param_type)
  {
    switch (param_type.interface_type()) {
      case MFParamType::Input:
        this->input(name, param_type.data_type());
        break;
      case MFParamType::Mutable:
        this->mutable_(name, param_type.data_type());
        break;
      case MFParamType::Output:
        this->output(name, param_type.data_type());
        break;
    }
  }

  template<MFParamCategory Category, typename T>
  void add(MFParamTag<Category, T> /* tag */, const char *name)
  {
    switch (Category) {
      case MFParamCategory::SingleInput:
        this->single_input<T>(name);
        return;
      case MFParamCategory::VectorInput:
        this->vector_input<T>(name);
        return;
      case MFParamCategory::SingleOutput:
        this->single_output<T>(name);
        return;
      case MFParamCategory::VectorOutput:
        this->vector_output<T>(name);
        return;
      case MFParamCategory::SingleMutable:
        this->single_mutable<T>(name);
        return;
      case MFParamCategory::VectorMutable:
        this->vector_mutable<T>(name);
        return;
    }
    BLI_assert_unreachable();
  }

  /* Context */

  /** This indicates that the function accesses the context. This disables optimizations that
   * depend on the fact that the function always performers the same operation. */
  void depends_on_context()
  {
    signature_.depends_on_context = true;
  }
};

}  // namespace blender::fn
