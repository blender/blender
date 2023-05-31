/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_cpp_type_make.hh"
#include "BLI_cpp_types.hh"

namespace blender {

template<typename ValueType>
inline VectorCPPType::VectorCPPType(TypeTag<ValueType> /*value_type*/)
    : self(CPPType::get<Vector<ValueType>>()), value(CPPType::get<ValueType>())
{
  this->register_self();
}

}  // namespace blender

/** Create a new #VectorCPPType that can be accessed through `VectorCPPType::get<T>()`. */
#define BLI_VECTOR_CPP_TYPE_MAKE(VALUE_TYPE) \
  BLI_CPP_TYPE_MAKE(blender::Vector<VALUE_TYPE>, CPPTypeFlags::None) \
  template<> const blender::VectorCPPType &blender::VectorCPPType::get_impl<VALUE_TYPE>() \
  { \
    static blender::VectorCPPType type{blender::TypeTag<VALUE_TYPE>{}}; \
    return type; \
  }

/** Register a #VectorCPPType created with #BLI_VECTOR_CPP_TYPE_MAKE. */
#define BLI_VECTOR_CPP_TYPE_REGISTER(VALUE_TYPE) blender::VectorCPPType::get<VALUE_TYPE>()
