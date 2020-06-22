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

#ifndef __FN_MULTI_FUNCTION_SIGNATURE_HH__
#define __FN_MULTI_FUNCTION_SIGNATURE_HH__

/** \file
 * \ingroup fn
 *
 * The signature of a multi-function contains the functions name and expected parameters. New
 * signatures should be build using the MFSignatureBuilder class.
 */

#include "FN_multi_function_param_type.hh"

#include "BLI_vector.hh"

namespace blender {
namespace fn {

struct MFSignature {
  std::string function_name;
  Vector<std::string> param_names;
  Vector<MFParamType> param_types;
  Vector<uint> param_data_indices;

  uint data_index(uint param_index) const
  {
    return param_data_indices[param_index];
  }
};

class MFSignatureBuilder {
 private:
  MFSignature &m_data;
  uint m_span_count = 0;
  uint m_virtual_span_count = 0;
  uint m_virtual_array_span_count = 0;
  uint m_vector_array_count = 0;

 public:
  MFSignatureBuilder(MFSignature &data) : m_data(data)
  {
    BLI_assert(data.param_names.is_empty());
    BLI_assert(data.param_types.is_empty());
    BLI_assert(data.param_data_indices.is_empty());
  }

  /* Input Param Types */

  template<typename T> void single_input(StringRef name)
  {
    this->single_input(name, CPPType::get<T>());
  }
  void single_input(StringRef name, const CPPType &type)
  {
    this->input(name, MFDataType::ForSingle(type));
  }
  template<typename T> void vector_input(StringRef name)
  {
    this->vector_input(name, CPPType::get<T>());
  }
  void vector_input(StringRef name, const CPPType &base_type)
  {
    this->input(name, MFDataType::ForVector(base_type));
  }
  void input(StringRef name, MFDataType data_type)
  {
    m_data.param_names.append(name);
    m_data.param_types.append(MFParamType(MFParamType::Input, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        m_data.param_data_indices.append(m_virtual_span_count++);
        break;
      case MFDataType::Vector:
        m_data.param_data_indices.append(m_virtual_array_span_count++);
        break;
    }
  }

  /* Output Param Types */

  template<typename T> void single_output(StringRef name)
  {
    this->single_output(name, CPPType::get<T>());
  }
  void single_output(StringRef name, const CPPType &type)
  {
    this->output(name, MFDataType::ForSingle(type));
  }
  template<typename T> void vector_output(StringRef name)
  {
    this->vector_output(name, CPPType::get<T>());
  }
  void vector_output(StringRef name, const CPPType &base_type)
  {
    this->output(name, MFDataType::ForVector(base_type));
  }
  void output(StringRef name, MFDataType data_type)
  {
    m_data.param_names.append(name);
    m_data.param_types.append(MFParamType(MFParamType::Output, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        m_data.param_data_indices.append(m_span_count++);
        break;
      case MFDataType::Vector:
        m_data.param_data_indices.append(m_vector_array_count++);
        break;
    }
  }

  /* Mutable Param Types */

  template<typename T> void single_mutable(StringRef name)
  {
    this->single_mutable(name, CPPType::get<T>());
  }
  void single_mutable(StringRef name, const CPPType &type)
  {
    this->mutable_(name, MFDataType::ForSingle(type));
  }
  template<typename T> void vector_mutable(StringRef name)
  {
    this->vector_mutable(name, CPPType::get<T>());
  }
  void vector_mutable(StringRef name, const CPPType &base_type)
  {
    this->mutable_(name, MFDataType::ForVector(base_type));
  }
  void mutable_(StringRef name, MFDataType data_type)
  {
    m_data.param_names.append(name);
    m_data.param_types.append(MFParamType(MFParamType::Mutable, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        m_data.param_data_indices.append(m_span_count++);
        break;
      case MFDataType::Vector:
        m_data.param_data_indices.append(m_vector_array_count++);
        break;
    }
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_SIGNATURE_HH__ */
