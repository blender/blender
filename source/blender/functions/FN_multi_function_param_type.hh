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
 * Furthermore, every parameter has a MFDataType that describes what kind of data is being passed
 * around.
 */

#include "FN_multi_function_data_type.hh"

namespace blender::fn {

enum class MFParamCategory {
  SingleInput,
  VectorInput,
  SingleOutput,
  VectorOutput,
  SingleMutable,
  VectorMutable,
};

template<MFParamCategory Category, typename T> struct MFParamTag {
  static constexpr MFParamCategory category = Category;
  using base_type = T;
  /* TODO: Doesn't support all categories yet, this can be generalized when necessary. */
  using array_type =
      std::conditional_t<Category == MFParamCategory::SingleInput, VArray<T>, MutableSpan<T>>;
};

class MFParamType {
 public:
  enum InterfaceType {
    Input,
    Output,
    Mutable,
  };

 private:
  InterfaceType interface_type_;
  MFDataType data_type_;

 public:
  MFParamType(InterfaceType interface_type, MFDataType data_type)
      : interface_type_(interface_type), data_type_(data_type)
  {
  }

  static MFParamType ForSingleInput(const CPPType &type)
  {
    return MFParamType(InterfaceType::Input, MFDataType::ForSingle(type));
  }

  static MFParamType ForVectorInput(const CPPType &base_type)
  {
    return MFParamType(InterfaceType::Input, MFDataType::ForVector(base_type));
  }

  static MFParamType ForSingleOutput(const CPPType &type)
  {
    return MFParamType(InterfaceType::Output, MFDataType::ForSingle(type));
  }

  static MFParamType ForVectorOutput(const CPPType &base_type)
  {
    return MFParamType(InterfaceType::Output, MFDataType::ForVector(base_type));
  }

  static MFParamType ForMutableSingle(const CPPType &type)
  {
    return MFParamType(InterfaceType::Mutable, MFDataType::ForSingle(type));
  }

  static MFParamType ForMutableVector(const CPPType &base_type)
  {
    return MFParamType(InterfaceType::Mutable, MFDataType::ForVector(base_type));
  }

  MFDataType data_type() const
  {
    return data_type_;
  }

  InterfaceType interface_type() const
  {
    return interface_type_;
  }

  MFParamCategory category() const
  {
    switch (data_type_.category()) {
      case MFDataType::Single: {
        switch (interface_type_) {
          case Input:
            return MFParamCategory::SingleInput;
          case Output:
            return MFParamCategory::SingleOutput;
          case Mutable:
            return MFParamCategory::SingleMutable;
        }
        break;
      }
      case MFDataType::Vector: {
        switch (interface_type_) {
          case Input:
            return MFParamCategory::VectorInput;
          case Output:
            return MFParamCategory::VectorOutput;
          case Mutable:
            return MFParamCategory::VectorMutable;
        }
        break;
      }
    }
    BLI_assert_unreachable();
    return MFParamCategory::SingleInput;
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

  friend bool operator==(const MFParamType &a, const MFParamType &b);
  friend bool operator!=(const MFParamType &a, const MFParamType &b);
};

inline bool operator==(const MFParamType &a, const MFParamType &b)
{
  return a.interface_type_ == b.interface_type_ && a.data_type_ == b.data_type_;
}

inline bool operator!=(const MFParamType &a, const MFParamType &b)
{
  return !(a == b);
}

}  // namespace blender::fn
