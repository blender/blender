/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * A multi-function has an arbitrary amount of parameters. Every parameter belongs to one of three
 * interface types:
 * - Input: An input parameter is readonly inside the function. The values have to be provided by
 *     the caller.
 * - Output: An output parameter has to be initialized by the function. However, the caller
 *     provides the memory where the data has to be constructed.
 * - Mutable: A mutable parameter can be considered to be an input and output. The caller has to
 *     initialize the data, but the function is allowed to modify it.
 *
 * Furthermore, every parameter has a DataType that describes what kind of data is being passed
 * around.
 */

#include "FN_multi_function_data_type.hh"

namespace blender::fn::multi_function {

enum class ParamCategory {
  SingleInput,
  VectorInput,
  SingleOutput,
  VectorOutput,
  SingleMutable,
  VectorMutable,
};

template<ParamCategory Category, typename T> struct MFParamTag {
  static constexpr ParamCategory category = Category;
  using base_type = T;
};

class ParamType {
 public:
  enum InterfaceType {
    Input,
    Output,
    Mutable,
  };

 private:
  InterfaceType interface_type_;
  DataType data_type_;

 public:
  ParamType(InterfaceType interface_type, DataType data_type)
      : interface_type_(interface_type), data_type_(data_type)
  {
  }

  static ParamType ForSingleInput(const CPPType &type)
  {
    return ParamType(InterfaceType::Input, DataType::ForSingle(type));
  }

  static ParamType ForVectorInput(const CPPType &base_type)
  {
    return ParamType(InterfaceType::Input, DataType::ForVector(base_type));
  }

  static ParamType ForSingleOutput(const CPPType &type)
  {
    return ParamType(InterfaceType::Output, DataType::ForSingle(type));
  }

  static ParamType ForVectorOutput(const CPPType &base_type)
  {
    return ParamType(InterfaceType::Output, DataType::ForVector(base_type));
  }

  static ParamType ForMutableSingle(const CPPType &type)
  {
    return ParamType(InterfaceType::Mutable, DataType::ForSingle(type));
  }

  static ParamType ForMutableVector(const CPPType &base_type)
  {
    return ParamType(InterfaceType::Mutable, DataType::ForVector(base_type));
  }

  DataType data_type() const
  {
    return data_type_;
  }

  InterfaceType interface_type() const
  {
    return interface_type_;
  }

  ParamCategory category() const
  {
    switch (data_type_.category()) {
      case DataType::Single: {
        switch (interface_type_) {
          case Input:
            return ParamCategory::SingleInput;
          case Output:
            return ParamCategory::SingleOutput;
          case Mutable:
            return ParamCategory::SingleMutable;
        }
        break;
      }
      case DataType::Vector: {
        switch (interface_type_) {
          case Input:
            return ParamCategory::VectorInput;
          case Output:
            return ParamCategory::VectorOutput;
          case Mutable:
            return ParamCategory::VectorMutable;
        }
        break;
      }
    }
    BLI_assert_unreachable();
    return ParamCategory::SingleInput;
  }

  bool is_input_or_mutable() const
  {
    return ELEM(interface_type_, Input, Mutable);
  }

  bool is_output_or_mutable() const
  {
    return ELEM(interface_type_, Output, Mutable);
  }

  bool is_output() const
  {
    return interface_type_ == Output;
  }

  friend bool operator==(const ParamType &a, const ParamType &b);
  friend bool operator!=(const ParamType &a, const ParamType &b);
};

inline bool operator==(const ParamType &a, const ParamType &b)
{
  return a.interface_type_ == b.interface_type_ && a.data_type_ == b.data_type_;
}

inline bool operator!=(const ParamType &a, const ParamType &b)
{
  return !(a == b);
}

}  // namespace blender::fn::multi_function
