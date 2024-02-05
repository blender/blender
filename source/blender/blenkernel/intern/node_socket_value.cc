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
#include "BKE_volume_grid.hh"

#include "BLI_color.hh"
#include "BLI_math_rotation_types.hh"
#include "BLI_math_vector_types.hh"

#include "FN_field.hh"

namespace blender::bke {

template<typename T, typename U>
static constexpr bool is_single_or_field_or_grid_v = is_same_any_v<T,
                                                                   U,
                                                                   fn::Field<U>
#ifdef WITH_OPENVDB
                                                                   ,
                                                                   VolumeGrid<U>
#endif
                                                                   >;

/**
 * Very fast (compile-time) conversion from a static C++ type to the corresponding socket type.
 */
template<typename T> static std::optional<eNodeSocketDatatype> static_type_to_socket_type()
{
  if constexpr (is_single_or_field_or_grid_v<T, int>) {
    return SOCK_INT;
  }
  if constexpr (is_single_or_field_or_grid_v<T, float>) {
    return SOCK_FLOAT;
  }
  if constexpr (is_single_or_field_or_grid_v<T, bool>) {
    return SOCK_BOOLEAN;
  }
  if constexpr (is_single_or_field_or_grid_v<T, float3>) {
    return SOCK_VECTOR;
  }
  if constexpr (is_single_or_field_or_grid_v<T, ColorGeometry4f>) {
    return SOCK_RGBA;
  }
  if constexpr (is_single_or_field_or_grid_v<T, math::Quaternion>) {
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
    switch (kind_) {
      case Kind::Field: {
        return std::move(value_.get<fn::GField>());
      }
      case Kind::Single: {
        const GPointer single_value = this->get_single_ptr();
        return fn::make_constant_field(*single_value.type(), single_value.get());
      }
      case Kind::Grid: {
        const CPPType *cpp_type = socket_type_to_geo_nodes_base_cpp_type(socket_type_);
        BLI_assert(cpp_type);
        return fn::make_constant_field(*cpp_type, cpp_type->default_value());
      }
      case Kind::None: {
        BLI_assert_unreachable();
        break;
      }
    }
  }
  else if constexpr (fn::is_field_v<T>) {
    BLI_assert(socket_type_ == static_type_to_socket_type<typename T::base_type>());
    return T(this->extract<fn::GField>());
  }
#ifdef WITH_OPENVDB
  else if constexpr (std::is_same_v<T, GVolumeGrid>) {
    switch (kind_) {
      case Kind::Grid: {
        return std::move(value_.get<GVolumeGrid>());
      }
      case Kind::Single:
      case Kind::Field: {
        const std::optional<VolumeGridType> grid_type = socket_type_to_grid_type(socket_type_);
        BLI_assert(grid_type);
        return GVolumeGrid(*grid_type);
      }
      case Kind::None: {
        BLI_assert_unreachable();
        break;
      }
    }
  }
  else if constexpr (is_VolumeGrid_v<T>) {
    BLI_assert(socket_type_ == static_type_to_socket_type<typename T::base_type>());
    return this->extract<GVolumeGrid>().typed<typename T::base_type>();
  }
#endif
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
#ifdef WITH_OPENVDB
  else if constexpr (std::is_same_v<T, GVolumeGrid>) {
    const VolumeGridType volume_grid_type = value->grid_type();
    const std::optional<eNodeSocketDatatype> new_socket_type = grid_type_to_socket_type(
        volume_grid_type);
    BLI_assert(new_socket_type);
    socket_type_ = *new_socket_type;
    kind_ = Kind::Grid;
    value_.emplace<GVolumeGrid>(std::move(value));
  }
  else if constexpr (is_VolumeGrid_v<T>) {
    this->store_impl<GVolumeGrid>(std::move(value));
  }
#endif
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

void SocketValueVariant::convert_to_single()
{
  switch (kind_) {
    case Kind::Single: {
      /* Nothing to do. */
      break;
    }
    case Kind::Field: {
      /* Evaluates the field without inputs to try to get a single value. If the field depends on
       * context, the default value is used instead. */
      fn::GField field = std::move(value_.get<fn::GField>());
      void *buffer = this->allocate_single(socket_type_);
      fn::evaluate_constant_field(field, buffer);
      break;
    }
    case Kind::Grid: {
      /* Can't convert a grid to a single value, so just use the default value of the current
       * socket type. */
      const CPPType &cpp_type = *socket_type_to_geo_nodes_base_cpp_type(socket_type_);
      this->store_single(socket_type_, cpp_type.default_value());
      break;
    }
    case Kind::None: {
      BLI_assert_unreachable();
      break;
    }
  }
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

void *SocketValueVariant::allocate_single(const eNodeSocketDatatype socket_type)
{
  kind_ = Kind::Single;
  socket_type_ = socket_type;
  switch (socket_type) {
    case SOCK_FLOAT:
      return value_.allocate<float>();
    case SOCK_INT:
      return value_.allocate<int>();
    case SOCK_VECTOR:
      return value_.allocate<float3>();
    case SOCK_BOOLEAN:
      return value_.allocate<bool>();
    case SOCK_ROTATION:
      return value_.allocate<math::Quaternion>();
    case SOCK_RGBA:
      return value_.allocate<ColorGeometry4f>();
    case SOCK_STRING:
      return value_.allocate<std::string>();
    default: {
      BLI_assert_unreachable();
      return nullptr;
    }
  }
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

#ifdef WITH_OPENVDB
#  define INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(TYPE) \
    INSTANTIATE(TYPE) \
    INSTANTIATE(fn::Field<TYPE>) \
    INSTANTIATE(VolumeGrid<TYPE>)
#else
#  define INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(TYPE) \
    INSTANTIATE(TYPE) \
    INSTANTIATE(fn::Field<TYPE>)
#endif

INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(int)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(bool)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(float)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(blender::float3)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(blender::ColorGeometry4f)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(blender::math::Quaternion)

INSTANTIATE(std::string)
INSTANTIATE(fn::GField)

#ifdef WITH_OPENVDB
INSTANTIATE(GVolumeGrid)
#endif

}  // namespace blender::bke
