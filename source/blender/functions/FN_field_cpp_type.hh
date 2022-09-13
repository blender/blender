/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "BLI_cpp_type_make.hh"
#include "FN_field.hh"

namespace blender::fn {

template<typename T> struct FieldCPPTypeParam {
};

class FieldCPPType : public CPPType {
 private:
  const CPPType &base_type_;

 public:
  template<typename T>
  FieldCPPType(FieldCPPTypeParam<Field<T>> /* unused */, StringRef debug_name)
      : CPPType(CPPTypeParam<Field<T>, CPPTypeFlags::None>(), debug_name),
        base_type_(CPPType::get<T>())
  {
  }

  const CPPType &base_type() const
  {
    return base_type_;
  }

  /* Ensure that #GField and #Field<T> have the same layout, to enable casting between the two. */
  static_assert(sizeof(Field<int>) == sizeof(GField));
  static_assert(sizeof(Field<int>) == sizeof(Field<std::string>));

  const GField &get_gfield(const void *field) const
  {
    return *(const GField *)field;
  }

  void construct_from_gfield(void *r_value, const GField &gfield) const
  {
    new (r_value) GField(gfield);
  }
};

class ValueOrFieldCPPType : public CPPType {
 private:
  const CPPType &base_type_;
  void (*construct_from_value_)(void *dst, const void *value);
  void (*construct_from_field_)(void *dst, GField field);
  const void *(*get_value_ptr_)(const void *value_or_field);
  const GField *(*get_field_ptr_)(const void *value_or_field);
  bool (*is_field_)(const void *value_or_field);
  GField (*as_field_)(const void *value_or_field);

 public:
  template<typename T>
  ValueOrFieldCPPType(FieldCPPTypeParam<ValueOrField<T>> /* unused */, StringRef debug_name)
      : CPPType(CPPTypeParam<ValueOrField<T>, CPPTypeFlags::Printable>(), debug_name),
        base_type_(CPPType::get<T>())
  {
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
  }

  const CPPType &base_type() const
  {
    return base_type_;
  }

  void construct_from_value(void *dst, const void *value) const
  {
    construct_from_value_(dst, value);
  }

  void construct_from_field(void *dst, GField field) const
  {
    construct_from_field_(dst, field);
  }

  const void *get_value_ptr(const void *value_or_field) const
  {
    return get_value_ptr_(value_or_field);
  }

  void *get_value_ptr(void *value_or_field) const
  {
    /* Use `const_cast` to avoid duplicating the callback for the non-const case. */
    return const_cast<void *>(get_value_ptr_(value_or_field));
  }

  const GField *get_field_ptr(const void *value_or_field) const
  {
    return get_field_ptr_(value_or_field);
  }

  bool is_field(const void *value_or_field) const
  {
    return is_field_(value_or_field);
  }

  GField as_field(const void *value_or_field) const
  {
    return as_field_(value_or_field);
  }
};

}  // namespace blender::fn

#define MAKE_FIELD_CPP_TYPE(DEBUG_NAME, FIELD_TYPE) \
  template<> const blender::CPPType &blender::CPPType::get_impl<blender::fn::Field<FIELD_TYPE>>() \
  { \
    static blender::fn::FieldCPPType cpp_type{ \
        blender::fn::FieldCPPTypeParam<blender::fn::Field<FIELD_TYPE>>(), STRINGIFY(DEBUG_NAME)}; \
    return cpp_type; \
  } \
  template<> \
  const blender::CPPType &blender::CPPType::get_impl<blender::fn::ValueOrField<FIELD_TYPE>>() \
  { \
    static blender::fn::ValueOrFieldCPPType cpp_type{ \
        blender::fn::FieldCPPTypeParam<blender::fn::ValueOrField<FIELD_TYPE>>(), \
        STRINGIFY(DEBUG_NAME##OrValue)}; \
    return cpp_type; \
  }
