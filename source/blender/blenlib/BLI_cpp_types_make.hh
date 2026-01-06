/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

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

/** Create a new #VectorCPPType that can be accessed through `VectorCPPType::get<T>()`. */
#define BLI_VECTOR_CPP_TYPE_MAKE(VALUE_TYPE) \
  BLI_CPP_TYPE_MAKE(Vector<VALUE_TYPE>, CPPTypeFlags::None) \
  template<> const VectorCPPType &VectorCPPType::get_impl<VALUE_TYPE>() \
  { \
    static VectorCPPType type{TypeTag<VALUE_TYPE>{}}; \
    return type; \
  }

/** Register a #VectorCPPType created with #BLI_VECTOR_CPP_TYPE_MAKE. */
#define BLI_VECTOR_CPP_TYPE_REGISTER(VALUE_TYPE) VectorCPPType::get<VALUE_TYPE>()

}  // namespace blender
