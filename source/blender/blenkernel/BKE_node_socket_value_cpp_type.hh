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
 * Contains information about how to deal with a `SocketValueVariant<T>` generically.
 * \{ */

class SocketValueVariantCPPType {
 private:
  void (*construct_from_value_)(void *dst, const void *value);
  void (*construct_from_field_)(void *dst, fn::GField field);
  const fn::GField *(*get_field_ptr_)(const void *value_variant);
  bool (*is_field_)(const void *value_variant);
  fn::GField (*as_field_)(const void *value_variant);

 public:
  /** The #SocketValueVariant<T> itself. */
  const CPPType &self;
  /** The type stored in the field. */
  const CPPType &value;

  template<typename ValueType> SocketValueVariantCPPType(TypeTag<ValueType> /*value_type*/);

  void construct_from_value(void *dst, const void *value) const
  {
    construct_from_value_(dst, value);
  }

  void construct_from_field(void *dst, fn::GField field) const
  {
    construct_from_field_(dst, field);
  }

  const void *get_value_ptr(const void *value_variant) const
  {
    static_assert(offsetof(SocketValueVariant<int>, value) == 0);
    return value_variant;
  }

  void *get_value_ptr(void *value_variant) const
  {
    static_assert(offsetof(SocketValueVariant<int>, value) == 0);
    return value_variant;
  }

  const fn::GField *get_field_ptr(const void *value_variant) const
  {
    return get_field_ptr_(value_variant);
  }

  bool is_field(const void *value_variant) const
  {
    return is_field_(value_variant);
  }

  fn::GField as_field(const void *value_variant) const
  {
    return as_field_(value_variant);
  }

  /**
   * Try to find the #SocketValueVariantCPPType that corresponds to a #CPPType.
   */
  static const SocketValueVariantCPPType *get_from_self(const CPPType &self);

  /**
   * Try to find the #SocketValueVariantCPPType that wraps a #SocketValueVariant containing the
   * given value type. This only works when the type has been created with #FN_FIELD_CPP_TYPE_MAKE.
   */
  static const SocketValueVariantCPPType *get_from_value(const CPPType &value);

  template<typename ValueType> static const SocketValueVariantCPPType &get()
  {
    static const SocketValueVariantCPPType &type =
        SocketValueVariantCPPType::get_impl<std::decay_t<ValueType>>();
    return type;
  }

 private:
  template<typename ValueType> static const SocketValueVariantCPPType &get_impl();

  void register_self();
};

template<typename ValueType>
inline SocketValueVariantCPPType::SocketValueVariantCPPType(TypeTag<ValueType> /*value_type*/)
    : self(CPPType::get<SocketValueVariant<ValueType>>()), value(CPPType::get<ValueType>())
{
  using T = ValueType;
  construct_from_value_ = [](void *dst, const void *value) {
    new (dst) SocketValueVariant<T>(*(const T *)value);
  };
  construct_from_field_ = [](void *dst, fn::GField field) {
    new (dst) SocketValueVariant<T>(fn::Field<T>(std::move(field)));
  };
  get_field_ptr_ = [](const void *value_variant) -> const fn::GField * {
    return &((SocketValueVariant<T> *)value_variant)->field;
  };
  is_field_ = [](const void *value_variant) {
    return ((SocketValueVariant<T> *)value_variant)->is_field();
  };
  as_field_ = [](const void *value_variant) -> fn::GField {
    return ((SocketValueVariant<T> *)value_variant)->as_field();
  };
  this->register_self();
}

/** \} */

}  // namespace blender::bke

/**
 * Create a new #SocketValueVariantCPPType that can be accessed through
 * `SocketValueVariantCPPType::get<T>()`.
 */
#define SOCKET_VALUE_CPP_TYPE_MAKE(VALUE_TYPE) \
  BLI_CPP_TYPE_MAKE(blender::bke::SocketValueVariant<VALUE_TYPE>, CPPTypeFlags::Printable) \
  template<> \
  const blender::bke::SocketValueVariantCPPType & \
  blender::bke::SocketValueVariantCPPType::get_impl<VALUE_TYPE>() \
  { \
    static blender::bke::SocketValueVariantCPPType type{blender::TypeTag<VALUE_TYPE>{}}; \
    return type; \
  }

/** Register a #SocketValueVariantCPPType created with #FN_FIELD_CPP_TYPE_MAKE. */
#define SOCKET_VALUE_CPP_TYPE_REGISTER(VALUE_TYPE) \
  blender::bke::SocketValueVariantCPPType::get<VALUE_TYPE>()
