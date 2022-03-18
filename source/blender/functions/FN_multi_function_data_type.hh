/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * A MFDataType describes what type of data a multi-function gets as input, outputs or mutates.
 * Currently, only individual elements or vectors of elements are supported. Adding more data types
 * is possible when necessary.
 */

#include "BLI_cpp_type.hh"

namespace blender::fn {

class MFDataType {
 public:
  enum Category {
    Single,
    Vector,
  };

 private:
  Category category_;
  const CPPType *type_;

  MFDataType(Category category, const CPPType &type) : category_(category), type_(&type)
  {
  }

 public:
  MFDataType() = default;

  static MFDataType ForSingle(const CPPType &type)
  {
    return MFDataType(Single, type);
  }

  static MFDataType ForVector(const CPPType &type)
  {
    return MFDataType(Vector, type);
  }

  template<typename T> static MFDataType ForSingle()
  {
    return MFDataType::ForSingle(CPPType::get<T>());
  }

  template<typename T> static MFDataType ForVector()
  {
    return MFDataType::ForVector(CPPType::get<T>());
  }

  bool is_single() const
  {
    return category_ == Single;
  }

  bool is_vector() const
  {
    return category_ == Vector;
  }

  Category category() const
  {
    return category_;
  }

  const CPPType &single_type() const
  {
    BLI_assert(this->is_single());
    return *type_;
  }

  const CPPType &vector_base_type() const
  {
    BLI_assert(this->is_vector());
    return *type_;
  }

  friend bool operator==(const MFDataType &a, const MFDataType &b);
  friend bool operator!=(const MFDataType &a, const MFDataType &b);

  std::string to_string() const
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

  uint64_t hash() const
  {
    return get_default_hash_2(*type_, category_);
  }
};

inline bool operator==(const MFDataType &a, const MFDataType &b)
{
  return a.category_ == b.category_ && a.type_ == b.type_;
}

inline bool operator!=(const MFDataType &a, const MFDataType &b)
{
  return !(a == b);
}

}  // namespace blender::fn
