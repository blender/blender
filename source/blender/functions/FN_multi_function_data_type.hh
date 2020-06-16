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

#ifndef __FN_MULTI_FUNCTION_DATA_TYPE_HH__
#define __FN_MULTI_FUNCTION_DATA_TYPE_HH__

/** \file
 * \ingroup fn
 *
 * A MFDataType describes what type of data a multi-function gets as input, outputs or mutates.
 * Currently, only individual elements or vectors of elements are supported. Adding more data types
 * is possible when necessary.
 */

#include "FN_cpp_type.hh"

namespace blender {
namespace fn {

class MFDataType {
 public:
  enum Category {
    Single,
    Vector,
  };

 private:
  Category m_category;
  const CPPType *m_type;

  MFDataType(Category category, const CPPType &type) : m_category(category), m_type(&type)
  {
  }

 public:
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
    return m_category == Single;
  }

  bool is_vector() const
  {
    return m_category == Vector;
  }

  Category category() const
  {
    return m_category;
  }

  const CPPType &single_type() const
  {
    BLI_assert(this->is_single());
    return *m_type;
  }

  const CPPType &vector_base_type() const
  {
    BLI_assert(this->is_vector());
    return *m_type;
  }

  friend bool operator==(const MFDataType &a, const MFDataType &b);
  friend bool operator!=(const MFDataType &a, const MFDataType &b);

  std::string to_string() const
  {
    switch (m_category) {
      case Single:
        return m_type->name();
      case Vector:
        return m_type->name() + " Vector";
    }
    BLI_assert(false);
    return "";
  }
};

inline bool operator==(const MFDataType &a, const MFDataType &b)
{
  return a.m_category == b.m_category && a.m_type == b.m_type;
}

inline bool operator!=(const MFDataType &a, const MFDataType &b)
{
  return !(a == b);
}

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_DATA_TYPE_HH__ */
