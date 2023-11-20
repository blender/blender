/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BKE_node_socket_value.hh"

#include "FN_field.hh"

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Socket Value CPP Type Class
 *
 * Contains information about how to deal with a `ValueOrField<T>` generically.
 * \{ */

class ValueOrFieldCPPType {
 private:
  void (*construct_from_value_)(void *dst, const void *value);
  void (*construct_from_field_)(void *dst, fn::GField field);
  const fn::GField *(*get_field_ptr_)(const void *value_or_field);
  bool (*is_field_)(const void *value_or_field);
  fn::GField (*as_field_)(const void *value_or_field);

 public:
  /** The #ValueOrField<T> itself. */
  const CPPType &self;
  /** The type stored in the field. */
  const CPPType &value;

  template<typename ValueType> ValueOrFieldCPPType(TypeTag<ValueType> /*value_type*/);

  void construct_from_value(void *dst, const void *value) const
  {
    construct_from_value_(dst, value);
  }

  void construct_from_field(void *dst, fn::GField field) const
  {
    construct_from_field_(dst, field);
  }

  const void *get_value_ptr(const void *value_or_field) const
  {
    static_assert(offsetof(ValueOrField<int>, value) == 0);
    return value_or_field;
  }

  void *get_value_ptr(void *value_or_field) const
  {
    static_assert(offsetof(ValueOrField<int>, value) == 0);
    return value_or_field;
  }

  const fn::GField *get_field_ptr(const void *value_or_field) const
  {
    return get_field_ptr_(value_or_field);
  }

  bool is_field(const void *value_or_field) const
  {
    return is_field_(value_or_field);
  }

  fn::GField as_field(const void *value_or_field) const
  {
    return as_field_(value_or_field);
  }

  /**
   * Try to find the #ValueOrFieldCPPType that corresponds to a #CPPType.
   */
  static const ValueOrFieldCPPType *get_from_self(const CPPType &self);

  /**
   * Try to find the #ValueOrFieldCPPType that wraps a #ValueOrField containing the given value
   * type. This only works when the type has been created with #FN_FIELD_CPP_TYPE_MAKE.
   */
  static const ValueOrFieldCPPType *get_from_value(const CPPType &value);

  template<typename ValueType> static const ValueOrFieldCPPType &get()
  {
    static const ValueOrFieldCPPType &type =
        ValueOrFieldCPPType::get_impl<std::decay_t<ValueType>>();
    return type;
  }

 private:
  template<typename ValueType> static const ValueOrFieldCPPType &get_impl();

  void register_self();
};

template<typename ValueType>
inline ValueOrFieldCPPType::ValueOrFieldCPPType(TypeTag<ValueType> /*value_type*/)
    : self(CPPType::get<ValueOrField<ValueType>>()), value(CPPType::get<ValueType>())
{
  using T = ValueType;
  construct_from_value_ = [](void *dst, const void *value_or_field) {
    new (dst) ValueOrField<T>(*(const T *)value_or_field);
  };
  construct_from_field_ = [](void *dst, fn::GField field) {
    new (dst) ValueOrField<T>(fn::Field<T>(std::move(field)));
  };
  get_field_ptr_ = [](const void *value_or_field) -> const fn::GField * {
    return &((ValueOrField<T> *)value_or_field)->field;
  };
  is_field_ = [](const void *value_or_field) {
    return ((ValueOrField<T> *)value_or_field)->is_field();
  };
  as_field_ = [](const void *value_or_field) -> fn::GField {
    return ((ValueOrField<T> *)value_or_field)->as_field();
  };
  this->register_self();
}

/** \} */

}  // namespace blender::bke

/**
 * Create a new #ValueOrFieldCPPType that can be accessed through `ValueOrFieldCPPType::get<T>()`.
 */
#define SOCKET_VALUE_CPP_TYPE_MAKE(VALUE_TYPE) \
  BLI_CPP_TYPE_MAKE(blender::bke::ValueOrField<VALUE_TYPE>, CPPTypeFlags::Printable) \
  template<> \
  const blender::bke::ValueOrFieldCPPType & \
  blender::bke::ValueOrFieldCPPType::get_impl<VALUE_TYPE>() \
  { \
    static blender::bke::ValueOrFieldCPPType type{blender::TypeTag<VALUE_TYPE>{}}; \
    return type; \
  }

/** Register a #ValueOrFieldCPPType created with #FN_FIELD_CPP_TYPE_MAKE. */
#define SOCKET_VALUE_CPP_TYPE_REGISTER(VALUE_TYPE) \
  blender::bke::ValueOrFieldCPPType::get<VALUE_TYPE>()
