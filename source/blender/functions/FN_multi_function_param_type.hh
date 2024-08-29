/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

template<ParamCategory Category, typename T> struct ParamTag {
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
  ParamType(InterfaceType interface_type, DataType data_type);

  static ParamType ForSingleInput(const CPPType &type);
  static ParamType ForVectorInput(const CPPType &base_type);
  static ParamType ForSingleOutput(const CPPType &type);
  static ParamType ForVectorOutput(const CPPType &base_type);
  static ParamType ForMutableSingle(const CPPType &type);
  static ParamType ForMutableVector(const CPPType &base_type);

  const DataType &data_type() const;
  InterfaceType interface_type() const;
  ParamCategory category() const;

  bool is_input_or_mutable() const;
  bool is_output_or_mutable() const;
  bool is_output() const;

  BLI_STRUCT_EQUALITY_OPERATORS_2(ParamType, interface_type_, data_type_)
};

/* -------------------------------------------------------------------- */
/** \name #ParamType Inline Methods
 * \{ */

inline ParamType::ParamType(InterfaceType interface_type, DataType data_type)
    : interface_type_(interface_type), data_type_(data_type)
{
}

inline ParamType ParamType::ForSingleInput(const CPPType &type)
{
  return ParamType(InterfaceType::Input, DataType::ForSingle(type));
}

inline ParamType ParamType::ForVectorInput(const CPPType &base_type)
{
  return ParamType(InterfaceType::Input, DataType::ForVector(base_type));
}

inline ParamType ParamType::ForSingleOutput(const CPPType &type)
{
  return ParamType(InterfaceType::Output, DataType::ForSingle(type));
}

inline ParamType ParamType::ForVectorOutput(const CPPType &base_type)
{
  return ParamType(InterfaceType::Output, DataType::ForVector(base_type));
}

inline ParamType ParamType::ForMutableSingle(const CPPType &type)
{
  return ParamType(InterfaceType::Mutable, DataType::ForSingle(type));
}

inline ParamType ParamType::ForMutableVector(const CPPType &base_type)
{
  return ParamType(InterfaceType::Mutable, DataType::ForVector(base_type));
}

inline const DataType &ParamType::data_type() const
{
  return data_type_;
}

inline ParamType::InterfaceType ParamType::interface_type() const
{
  return interface_type_;
}

inline ParamCategory ParamType::category() const
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

inline bool ParamType::is_input_or_mutable() const
{
  return ELEM(interface_type_, Input, Mutable);
}

inline bool ParamType::is_output_or_mutable() const
{
  return ELEM(interface_type_, Output, Mutable);
}

inline bool ParamType::is_output() const
{
  return interface_type_ == Output;
}

/** \} */

}  // namespace blender::fn::multi_function
