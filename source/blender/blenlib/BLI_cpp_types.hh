/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_cpp_type.hh"
#include "BLI_vector.hh"

namespace blender {

/**
 * Contains information about how to deal with a #Vector<T> generically.
 */
class VectorCPPType {
 public:
  /** The #Vector<T> itself. */
  const CPPType &self;
  /** The type stored in the vector. */
  const CPPType &value;

  template<typename ValueType> VectorCPPType(TypeTag<ValueType> /*value_type*/);

  /**
   * Try to find the #VectorCPPType that corresponds to a #CPPType.
   */
  static const VectorCPPType *get_from_self(const CPPType &self);
  /**
   * Try to find the #VectorCPPType that wraps a vector containing the given value type.
   * This only works when the vector type has been created with #BLI_VECTOR_CPP_TYPE_MAKE.
   */
  static const VectorCPPType *get_from_value(const CPPType &value);

  template<typename ValueType> static const VectorCPPType &get()
  {
    static const VectorCPPType &type = VectorCPPType::get_impl<std::decay_t<ValueType>>();
    return type;
  }

  template<typename ValueType> static const VectorCPPType &get_impl();

 private:
  void register_self();
};

}  // namespace blender
