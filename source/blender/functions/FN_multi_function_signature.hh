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
  std::string function_name;
  Vector<std::string> param_names;
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
  MFSignature &data_;
  int span_count_ = 0;
  int virtual_span_count_ = 0;
  int virtual_array_span_count_ = 0;
  int vector_array_count_ = 0;

 public:
  MFSignatureBuilder(MFSignature &data) : data_(data)
  {
    BLI_assert(data.param_names.is_empty());
    BLI_assert(data.param_types.is_empty());
    BLI_assert(data.param_data_indices.is_empty());
  }

  /* Input Parameter Types */

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
    data_.param_names.append(name);
    data_.param_types.append(MFParamType(MFParamType::Input, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        data_.param_data_indices.append(virtual_span_count_++);
        break;
      case MFDataType::Vector:
        data_.param_data_indices.append(virtual_array_span_count_++);
        break;
    }
  }

  /* Output Parameter Types */

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
    data_.param_names.append(name);
    data_.param_types.append(MFParamType(MFParamType::Output, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        data_.param_data_indices.append(span_count_++);
        break;
      case MFDataType::Vector:
        data_.param_data_indices.append(vector_array_count_++);
        break;
    }
  }

  /* Mutable Parameter Types */

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
    data_.param_names.append(name);
    data_.param_types.append(MFParamType(MFParamType::Mutable, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        data_.param_data_indices.append(span_count_++);
        break;
      case MFDataType::Vector:
        data_.param_data_indices.append(vector_array_count_++);
        break;
    }
  }

  /* Context */

  /** This indicates that the function accesses the context. This disables optimizations that
   * depend on the fact that the function always performers the same operation. */
  void depends_on_context()
  {
    data_.depends_on_context = true;
  }
};

}  // namespace blender::fn
