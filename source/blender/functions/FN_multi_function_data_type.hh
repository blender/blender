/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * A DataType describes what type of data a multi-function gets as input, outputs or mutates.
 * Currently, only individual elements or vectors of elements are supported. Adding more data types
 * is possible when necessary.
 */

#include "BLI_cpp_type.hh"
#include "BLI_struct_equality_utils.hh"

namespace blender::fn::multi_function {

class DataType {
 public:
  enum Category {
    Single,
    Vector,
  };

 private:
  Category category_;
  const CPPType *type_;

  DataType(Category category, const CPPType &type);

 public:
  DataType() = default;

  static DataType ForSingle(const CPPType &type);
  static DataType ForVector(const CPPType &type);

  template<typename T> static DataType ForSingle();
  template<typename T> static DataType ForVector();

  bool is_single() const;
  bool is_vector() const;

  Category category() const;

  const CPPType &single_type() const;
  const CPPType &vector_base_type() const;

  BLI_STRUCT_EQUALITY_OPERATORS_2(DataType, category_, type_)

  std::string to_string() const;

  uint64_t hash() const;
};

/* -------------------------------------------------------------------- */
/** \name #DataType Inline Methods
 * \{ */

inline DataType::DataType(Category category, const CPPType &type)
    : category_(category), type_(&type)
{
}

inline DataType DataType::ForSingle(const CPPType &type)
{
  return DataType(Single, type);
}

inline DataType DataType::ForVector(const CPPType &type)
{
  return DataType(Vector, type);
}

template<typename T> inline DataType DataType::ForSingle()
{
  return DataType::ForSingle(CPPType::get<T>());
}

template<typename T> inline DataType DataType::ForVector()
{
  return DataType::ForVector(CPPType::get<T>());
}

inline bool DataType::is_single() const
{
  return category_ == Single;
}

inline bool DataType::is_vector() const
{
  return category_ == Vector;
}

inline DataType::Category DataType::category() const
{
  return category_;
}

inline const CPPType &DataType::single_type() const
{
  BLI_assert(this->is_single());
  return *type_;
}

inline const CPPType &DataType::vector_base_type() const
{
  BLI_assert(this->is_vector());
  return *type_;
}

inline std::string DataType::to_string() const
{
  switch (category_) {
    case Single:
      return type_->name();
    case Vector:
      return type_->name() + " Vector";
  }
  BLI_assert(false);
  return "";
}

inline uint64_t DataType::hash() const
{
  return get_default_hash(*type_, category_);
}

/** \} */

}  // namespace blender::fn::multi_function
