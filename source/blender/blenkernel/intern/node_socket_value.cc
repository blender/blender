/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <sstream>

#include "BKE_customdata.hh"
#include "BKE_node.hh"
#include "BKE_node_socket_value.hh"

#include "BLI_color.hh"
#include "BLI_math_rotation_types.hh"
#include "BLI_math_vector_types.hh"

#include "FN_field.hh"

namespace blender::bke {

/**
 * Very fast (compile-time) conversion from a static C++ type to the corresponding socket type.
 */
template<typename T> static std::optional<eNodeSocketDatatype> static_type_to_socket_type()
{
  if constexpr (is_same_any_v<T, int, fn::Field<int>>) {
    return SOCK_INT;
  }
  if constexpr (is_same_any_v<T, float, fn::Field<float>>) {
    return SOCK_FLOAT;
  }
  if constexpr (is_same_any_v<T, bool, fn::Field<bool>>) {
    return SOCK_BOOLEAN;
  }
  if constexpr (is_same_any_v<T, float3, fn::Field<float3>>) {
    return SOCK_VECTOR;
  }
  if constexpr (is_same_any_v<T, ColorGeometry4f, fn::Field<ColorGeometry4f>>) {
    return SOCK_RGBA;
  }
  if constexpr (is_same_any_v<T, math::Quaternion, fn::Field<math::Quaternion>>) {
    return SOCK_ROTATION;
  }
  if constexpr (is_same_any_v<T, std::string>) {
    return SOCK_STRING;
  }
  return std::nullopt;
}

template<typename T> T SocketValueVariant::extract()
{
  if constexpr (std::is_same_v<T, fn::GField>) {
    if (kind_ == Kind::Field) {
      return std::move(value_.get<fn::GField>());
    }
    if (kind_ == Kind::Single) {
      const GPointer single_value = this->get_single_ptr();
      return fn::make_constant_field(*single_value.type(), single_value.get());
    }
  }
  else if constexpr (fn::is_field_v<T>) {
    BLI_assert(socket_type_ == static_type_to_socket_type<typename T::base_type>());
    return T(this->extract<fn::GField>());
  }
  else {
    BLI_assert(socket_type_ == static_type_to_socket_type<T>());
    if (kind_ == Kind::Single) {
      return std::move(value_.get<T>());
    }
    if (kind_ == Kind::Field) {
      T ret_value;
      std::destroy_at(&ret_value);
      fn::evaluate_constant_field(value_.get<fn::GField>(), &ret_value);
      return ret_value;
    }
  }
  BLI_assert_unreachable();
  return T();
}

template<typename T> T SocketValueVariant::get() const
{
  /* Simple implementation in terms of #extract for now. This could potentially use a specialized
   * implementation at some point, but for now it's unlikely to be a bottleneck. */
  SocketValueVariant copied_variant = *this;
  return copied_variant.extract<T>();
}

template<typename T> void SocketValueVariant::store_impl(T value)
{
  if constexpr (std::is_same_v<T, fn::GField>) {
    const std::optional<eNodeSocketDatatype> new_socket_type =
        geo_nodes_base_cpp_type_to_socket_type(value.cpp_type());
    BLI_assert(new_socket_type);
    socket_type_ = *new_socket_type;
    kind_ = Kind::Field;
    value_.emplace<fn::GField>(std::move(value));
  }
  else if constexpr (fn::is_field_v<T>) {
    /* Always store #Field<T> as #GField. */
    this->store_impl<fn::GField>(std::move(value));
  }
  else {
    const std::optional<eNodeSocketDatatype> new_socket_type = static_type_to_socket_type<T>();
    BLI_assert(new_socket_type);
    socket_type_ = *new_socket_type;
    kind_ = Kind::Single;
    value_.emplace<T>(std::move(value));
  }
}

void SocketValueVariant::store_single(const eNodeSocketDatatype socket_type, const void *value)
{
  kind_ = Kind::Single;
  socket_type_ = socket_type;
  switch (socket_type) {
    case SOCK_FLOAT: {
      value_.emplace<float>(*static_cast<const float *>(value));
      break;
    }
    case SOCK_INT: {
      value_.emplace<int>(*static_cast<const int *>(value));
      break;
    }
    case SOCK_VECTOR: {
      value_.emplace<float3>(*static_cast<const float3 *>(value));
      break;
    }
    case SOCK_BOOLEAN: {
      value_.emplace<bool>(*static_cast<const bool *>(value));
      break;
    }
    case SOCK_ROTATION: {
      value_.emplace<math::Quaternion>(*static_cast<const math::Quaternion *>(value));
      break;
    }
    case SOCK_RGBA: {
      value_.emplace<ColorGeometry4f>(*static_cast<const ColorGeometry4f *>(value));
      break;
    }
    case SOCK_STRING: {
      value_.emplace<std::string>(*static_cast<const std::string *>(value));
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }
  UNUSED_VARS(socket_type, value);
}

bool SocketValueVariant::is_context_dependent_field() const
{
  if (!value_.is<fn::GField>()) {
    return false;
  }
  const fn::GField &field = value_.get<fn::GField>();
  if (!field) {
    return false;
  }
  return field.node().depends_on_input();
}

void *SocketValueVariant::new_single_for_write(const eNodeSocketDatatype socket_type)
{
  const CPPType *cpp_type = socket_type_to_geo_nodes_base_cpp_type(socket_type);
  BLI_assert(cpp_type != nullptr);
  BUFFER_FOR_CPP_TYPE_VALUE(*cpp_type, buffer);
  cpp_type->value_initialize(buffer);
  this->store_single(socket_type, buffer);
  return value_.get();
}

void *SocketValueVariant::new_single_for_write(const CPPType &cpp_type)
{
  const std::optional<eNodeSocketDatatype> socket_type = geo_nodes_base_cpp_type_to_socket_type(
      cpp_type);
  BLI_assert(socket_type);
  return this->new_single_for_write(*socket_type);
}

void SocketValueVariant::convert_to_single()
{
  if (kind_ == Kind::Single) {
    return;
  }
  if (kind_ == Kind::Field) {
    fn::GField field = std::move(value_.get<fn::GField>());
    const CPPType &cpp_type = field.cpp_type();
    void *buffer = this->new_single_for_write(cpp_type);
    cpp_type.destruct(buffer);
    fn::evaluate_constant_field(field, buffer);
    return;
  }
  BLI_assert_unreachable();
}

GPointer SocketValueVariant::get_single_ptr() const
{
  BLI_assert(kind_ == Kind::Single);
  const CPPType *type = socket_type_to_geo_nodes_base_cpp_type(socket_type_);
  BLI_assert(type != nullptr);
  const void *data = value_.get();
  return GPointer(*type, data);
}

GMutablePointer SocketValueVariant::get_single_ptr()
{
  const GPointer ptr = const_cast<const SocketValueVariant *>(this)->get_single_ptr();
  return GMutablePointer(ptr.type(), const_cast<void *>(ptr.get()));
}

std::ostream &operator<<(std::ostream &stream, const SocketValueVariant &value_variant)
{
  SocketValueVariant variant_copy = value_variant;
  variant_copy.convert_to_single();
  if (value_variant.kind_ == SocketValueVariant::Kind::Single) {
    const GPointer value = variant_copy.get_single_ptr();
    const CPPType &cpp_type = *value.type();
    if (cpp_type.is_printable()) {
      std::stringstream ss;
      cpp_type.print(value.get(), ss);
      stream << ss.str();
      return stream;
    }
  }
  stream << "SocketValueVariant";
  return stream;
}

bool SocketValueVariant::valid_for_socket(eNodeSocketDatatype socket_type) const
{
  if (kind_ == Kind::None) {
    return false;
  }
  return socket_type_ == socket_type;
}

#define INSTANTIATE(TYPE) \
  template TYPE SocketValueVariant::extract(); \
  template TYPE SocketValueVariant::get() const; \
  template void SocketValueVariant::store_impl(TYPE);

#define INSTANTIATE_SINGLE_AND_FIELD(TYPE) \
  INSTANTIATE(TYPE) \
  INSTANTIATE(fn::Field<TYPE>)

INSTANTIATE_SINGLE_AND_FIELD(int)
INSTANTIATE_SINGLE_AND_FIELD(bool)
INSTANTIATE_SINGLE_AND_FIELD(float)
INSTANTIATE_SINGLE_AND_FIELD(blender::float3)
INSTANTIATE_SINGLE_AND_FIELD(blender::ColorGeometry4f)
INSTANTIATE_SINGLE_AND_FIELD(blender::math::Quaternion)

INSTANTIATE(std::string)
INSTANTIATE(fn::GField)

}  // namespace blender::bke
