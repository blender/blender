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

#ifndef __FN_MULTI_FUNCTION_PARAMS_HH__
#define __FN_MULTI_FUNCTION_PARAMS_HH__

/** \file
 * \ingroup fn
 *
 * This file provides an MFParams and MFParamsBuilder structure.
 *
 * `MFParamsBuilder` is used by a function caller to be prepare all parameters that are passed into
 * the function. `MFParams` is then used inside the called function to access the parameters.
 */

#include "FN_generic_vector_array.hh"
#include "FN_multi_function_signature.hh"

namespace blender {
namespace fn {

class MFParamsBuilder {
 private:
  const MFSignature *m_signature;
  uint m_min_array_size;
  Vector<GVSpan> m_virtual_spans;
  Vector<GMutableSpan> m_mutable_spans;
  Vector<GVArraySpan> m_virtual_array_spans;
  Vector<GVectorArray *> m_vector_arrays;

  friend class MFParams;

 public:
  MFParamsBuilder(const MFSignature &signature, uint min_array_size)
      : m_signature(&signature), m_min_array_size(min_array_size)
  {
  }

  MFParamsBuilder(const class MultiFunction &fn, uint min_array_size);

  template<typename T> void add_readonly_single_input(const T *value)
  {
    this->add_readonly_single_input(
        GVSpan::FromSingle(CPPType::get<T>(), value, m_min_array_size));
  }
  void add_readonly_single_input(GVSpan ref)
  {
    this->assert_current_param_type(MFParamType::ForSingleInput(ref.type()));
    BLI_assert(ref.size() >= m_min_array_size);
    m_virtual_spans.append(ref);
  }

  void add_readonly_vector_input(GVArraySpan ref)
  {
    this->assert_current_param_type(MFParamType::ForVectorInput(ref.type()));
    BLI_assert(ref.size() >= m_min_array_size);
    m_virtual_array_spans.append(ref);
  }

  void add_uninitialized_single_output(GMutableSpan ref)
  {
    this->assert_current_param_type(MFParamType::ForSingleOutput(ref.type()));
    BLI_assert(ref.size() >= m_min_array_size);
    m_mutable_spans.append(ref);
  }

  void add_vector_output(GVectorArray &vector_array)
  {
    this->assert_current_param_type(MFParamType::ForVectorOutput(vector_array.type()));
    BLI_assert(vector_array.size() >= m_min_array_size);
    m_vector_arrays.append(&vector_array);
  }

  void add_single_mutable(GMutableSpan ref)
  {
    this->assert_current_param_type(MFParamType::ForMutableSingle(ref.type()));
    BLI_assert(ref.size() >= m_min_array_size);
    m_mutable_spans.append(ref);
  }

  void add_vector_mutable(GVectorArray &vector_array)
  {
    this->assert_current_param_type(MFParamType::ForMutableVector(vector_array.type()));
    BLI_assert(vector_array.size() >= m_min_array_size);
    m_vector_arrays.append(&vector_array);
  }

  GMutableSpan computed_array(uint param_index)
  {
    BLI_assert(ELEM(m_signature->param_types[param_index].category(),
                    MFParamType::SingleOutput,
                    MFParamType::SingleMutable));
    uint data_index = m_signature->data_index(param_index);
    return m_mutable_spans[data_index];
  }

  GVectorArray &computed_vector_array(uint param_index)
  {
    BLI_assert(ELEM(m_signature->param_types[param_index].category(),
                    MFParamType::VectorOutput,
                    MFParamType::VectorMutable));
    uint data_index = m_signature->data_index(param_index);
    return *m_vector_arrays[data_index];
  }

 private:
  void assert_current_param_type(MFParamType param_type)
  {
    UNUSED_VARS_NDEBUG(param_type);
#ifdef DEBUG
    uint param_index = this->current_param_index();
    MFParamType expected_type = m_signature->param_types[param_index];
    BLI_assert(expected_type == param_type);
#endif
  }

  uint current_param_index() const
  {
    return m_virtual_spans.size() + m_mutable_spans.size() + m_virtual_array_spans.size() +
           m_vector_arrays.size();
  }
};

class MFParams {
 private:
  MFParamsBuilder *m_builder;

 public:
  MFParams(MFParamsBuilder &builder) : m_builder(&builder)
  {
  }

  template<typename T> VSpan<T> readonly_single_input(uint param_index, StringRef name = "")
  {
    return this->readonly_single_input(param_index, name).typed<T>();
  }
  GVSpan readonly_single_input(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::SingleInput);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return m_builder->m_virtual_spans[data_index];
  }

  template<typename T>
  MutableSpan<T> uninitialized_single_output(uint param_index, StringRef name = "")
  {
    return this->uninitialized_single_output(param_index, name).typed<T>();
  }
  GMutableSpan uninitialized_single_output(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::SingleOutput);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return m_builder->m_mutable_spans[data_index];
  }

  template<typename T> VArraySpan<T> readonly_vector_input(uint param_index, StringRef name = "")
  {
    return this->readonly_vector_input(param_index, name).typed<T>();
  }
  GVArraySpan readonly_vector_input(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::VectorInput);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return m_builder->m_virtual_array_spans[data_index];
  }

  template<typename T> GVectorArrayRef<T> vector_output(uint param_index, StringRef name = "")
  {
    return this->vector_output(param_index, name).typed<T>();
  }
  GVectorArray &vector_output(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::VectorOutput);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return *m_builder->m_vector_arrays[data_index];
  }

  template<typename T> MutableSpan<T> single_mutable(uint param_index, StringRef name = "")
  {
    return this->single_mutable(param_index, name).typed<T>();
  }
  GMutableSpan single_mutable(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::SingleMutable);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return m_builder->m_mutable_spans[data_index];
  }

  template<typename T> GVectorArrayRef<T> vector_mutable(uint param_index, StringRef name = "")
  {
    return this->vector_mutable(param_index, name).typed<T>();
  }
  GVectorArray &vector_mutable(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::VectorMutable);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return *m_builder->m_vector_arrays[data_index];
  }

 private:
  void assert_correct_param(uint param_index, StringRef name, MFParamType param_type)
  {
    UNUSED_VARS_NDEBUG(param_index, name, param_type);
#ifdef DEBUG
    BLI_assert(m_builder->m_signature->param_types[param_index] == param_type);
    if (name.size() > 0) {
      BLI_assert(m_builder->m_signature->param_names[param_index] == name);
    }
#endif
  }

  void assert_correct_param(uint param_index, StringRef name, MFParamType::Category category)
  {
    UNUSED_VARS_NDEBUG(param_index, name, category);
#ifdef DEBUG
    BLI_assert(m_builder->m_signature->param_types[param_index].category() == category);
    if (name.size() > 0) {
      BLI_assert(m_builder->m_signature->param_names[param_index] == name);
    }
#endif
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_PARAMS_HH__ */
