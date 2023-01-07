/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * The signature of a multi-function contains the functions name and expected parameters. New
 * signatures should be build using the #SignatureBuilder class.
 */

#include "FN_multi_function_param_type.hh"

#include "BLI_vector.hh"

namespace blender::fn::multi_function {

struct Signature {
  struct ParamInfo {
    ParamType type;
    const char *name;
  };

  /**
   * The name should be statically allocated so that it lives longer than this signature. This is
   * used instead of an #std::string because of the overhead when many functions are created.
   * If the name of the function has to be more dynamic for debugging purposes, override
   * #MultiFunction::debug_name() instead. Then the dynamic name will only be computed when it is
   * actually needed.
   */
  const char *function_name;
  Vector<ParamInfo> params;
};

class SignatureBuilder {
 private:
  Signature &signature_;

 public:
  SignatureBuilder(const char *function_name, Signature &signature_to_build)
      : signature_(signature_to_build)
  {
    signature_.function_name = function_name;
  }

  /* Input Parameter Types */

  template<typename T> void single_input(const char *name)
  {
    this->single_input(name, CPPType::get<T>());
  }
  void single_input(const char *name, const CPPType &type)
  {
    this->input(name, DataType::ForSingle(type));
  }
  template<typename T> void vector_input(const char *name)
  {
    this->vector_input(name, CPPType::get<T>());
  }
  void vector_input(const char *name, const CPPType &base_type)
  {
    this->input(name, DataType::ForVector(base_type));
  }
  void input(const char *name, DataType data_type)
  {
    signature_.params.append({ParamType(ParamType::Input, data_type), name});
  }

  /* Output Parameter Types */

  template<typename T> void single_output(const char *name)
  {
    this->single_output(name, CPPType::get<T>());
  }
  void single_output(const char *name, const CPPType &type)
  {
    this->output(name, DataType::ForSingle(type));
  }
  template<typename T> void vector_output(const char *name)
  {
    this->vector_output(name, CPPType::get<T>());
  }
  void vector_output(const char *name, const CPPType &base_type)
  {
    this->output(name, DataType::ForVector(base_type));
  }
  void output(const char *name, DataType data_type)
  {
    signature_.params.append({ParamType(ParamType::Output, data_type), name});
  }

  /* Mutable Parameter Types */

  template<typename T> void single_mutable(const char *name)
  {
    this->single_mutable(name, CPPType::get<T>());
  }
  void single_mutable(const char *name, const CPPType &type)
  {
    this->mutable_(name, DataType::ForSingle(type));
  }
  template<typename T> void vector_mutable(const char *name)
  {
    this->vector_mutable(name, CPPType::get<T>());
  }
  void vector_mutable(const char *name, const CPPType &base_type)
  {
    this->mutable_(name, DataType::ForVector(base_type));
  }
  void mutable_(const char *name, DataType data_type)
  {
    signature_.params.append({ParamType(ParamType::Mutable, data_type), name});
  }

  void add(const char *name, const ParamType &param_type)
  {
    switch (param_type.interface_type()) {
      case ParamType::Input:
        this->input(name, param_type.data_type());
        break;
      case ParamType::Mutable:
        this->mutable_(name, param_type.data_type());
        break;
      case ParamType::Output:
        this->output(name, param_type.data_type());
        break;
    }
  }

  template<ParamCategory Category, typename T>
  void add(MFParamTag<Category, T> /* tag */, const char *name)
  {
    switch (Category) {
      case ParamCategory::SingleInput:
        this->single_input<T>(name);
        return;
      case ParamCategory::VectorInput:
        this->vector_input<T>(name);
        return;
      case ParamCategory::SingleOutput:
        this->single_output<T>(name);
        return;
      case ParamCategory::VectorOutput:
        this->vector_output<T>(name);
        return;
      case ParamCategory::SingleMutable:
        this->single_mutable<T>(name);
        return;
      case ParamCategory::VectorMutable:
        this->vector_mutable<T>(name);
        return;
    }
    BLI_assert_unreachable();
  }
};

}  // namespace blender::fn::multi_function
