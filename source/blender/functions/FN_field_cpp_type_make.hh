/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_field_cpp_type.hh"

namespace blender::fn {

template<typename ValueType>
inline ValueOrFieldCPPType::ValueOrFieldCPPType(TypeTag<ValueType> /*value_type*/)
    : self(CPPType::get<ValueOrField<ValueType>>()), value(CPPType::get<ValueType>())
{
  using T = ValueType;
  construct_from_value_ = [](void *dst, const void *value_or_field) {
    new (dst) ValueOrField<T>(*(const T *)value_or_field);
  };
  construct_from_field_ = [](void *dst, GField field) {
    new (dst) ValueOrField<T>(Field<T>(std::move(field)));
  };
  get_value_ptr_ = [](const void *value_or_field) {
    return (const void *)&((ValueOrField<T> *)value_or_field)->value;
  };
  get_field_ptr_ = [](const void *value_or_field) -> const GField * {
    return &((ValueOrField<T> *)value_or_field)->field;
  };
  is_field_ = [](const void *value_or_field) {
    return ((ValueOrField<T> *)value_or_field)->is_field();
  };
  as_field_ = [](const void *value_or_field) -> GField {
    return ((ValueOrField<T> *)value_or_field)->as_field();
  };
  this->register_self();
}

}  // namespace blender::fn

/**
 * Create a new #ValueOrFieldCPPType that can be accessed through `ValueOrFieldCPPType::get<T>()`.
 */
#define FN_FIELD_CPP_TYPE_MAKE(VALUE_TYPE) \
  BLI_CPP_TYPE_MAKE(blender::fn::ValueOrField<VALUE_TYPE>, CPPTypeFlags::Printable) \
  template<> \
  const blender::fn::ValueOrFieldCPPType & \
  blender::fn::ValueOrFieldCPPType::get_impl<VALUE_TYPE>() \
  { \
    static blender::fn::ValueOrFieldCPPType type{blender::TypeTag<VALUE_TYPE>{}}; \
    return type; \
  }

/** Register a #ValueOrFieldCPPType created with #FN_FIELD_CPP_TYPE_MAKE. */
#define FN_FIELD_CPP_TYPE_REGISTER(VALUE_TYPE) blender::fn::ValueOrFieldCPPType::get<VALUE_TYPE>()
