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

#ifndef __FN_MULTI_FUNCTION_PARAM_TYPE_HH__
#define __FN_MULTI_FUNCTION_PARAM_TYPE_HH__

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

namespace blender {
namespace fn {

class MFParamType {
 public:
  enum InterfaceType {
    Input,
    Output,
    Mutable,
  };

  enum Category {
    SingleInput,
    VectorInput,
    SingleOutput,
    VectorOutput,
    SingleMutable,
    VectorMutable,
  };

 private:
  InterfaceType m_interface_type;
  MFDataType m_data_type;

 public:
  MFParamType(InterfaceType interface_type, MFDataType data_type)
      : m_interface_type(interface_type), m_data_type(data_type)
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
    return m_data_type;
  }

  InterfaceType interface_type() const
  {
    return m_interface_type;
  }

  Category category() const
  {
    switch (m_data_type.category()) {
      case MFDataType::Single: {
        switch (m_interface_type) {
          case Input:
            return SingleInput;
          case Output:
            return SingleOutput;
          case Mutable:
            return SingleMutable;
        }
        break;
      }
      case MFDataType::Vector: {
        switch (m_interface_type) {
          case Input:
            return VectorInput;
          case Output:
            return VectorOutput;
          case Mutable:
            return VectorMutable;
        }
        break;
      }
    }
    BLI_assert(false);
    return SingleInput;
  }

  friend bool operator==(const MFParamType &a, const MFParamType &b);
  friend bool operator!=(const MFParamType &a, const MFParamType &b);
};

inline bool operator==(const MFParamType &a, const MFParamType &b)
{
  return a.m_interface_type == b.m_interface_type && a.m_data_type == b.m_data_type;
}

inline bool operator!=(const MFParamType &a, const MFParamType &b)
{
  return !(a == b);
}

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_PARAM_TYPE_HH__ */
