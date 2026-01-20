/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <sstream>

#include "BKE_geometry_set.hh"
#include "BKE_node.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_volume_grid.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_closure.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_menu_value.hh"

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
  if constexpr (is_same_any_v<T, nodes::MenuValue, fn::Field<nodes::MenuValue>>) {
    return SOCK_MENU;
  }
  if constexpr (is_same_any_v<T, float4x4, fn::Field<float4x4>>) {
    return SOCK_MATRIX;
  }
  if constexpr (is_same_any_v<T, std::string>) {
    return SOCK_STRING;
  }
  if constexpr (is_same_any_v<T, nodes::BundlePtr>) {
    return SOCK_BUNDLE;
  }
  if constexpr (is_same_any_v<T, nodes::ClosurePtr>) {
    return SOCK_CLOSURE;
  }
  if constexpr (is_same_any_v<T, Object *>) {
    return SOCK_OBJECT;
  }
  if constexpr (is_same_any_v<T, Collection *>) {
    return SOCK_COLLECTION;
  }
  if constexpr (is_same_any_v<T, Tex *>) {
    return SOCK_TEXTURE;
  }
  if constexpr (is_same_any_v<T, Image *>) {
    return SOCK_IMAGE;
  }
  if constexpr (is_same_any_v<T, Material *>) {
    return SOCK_MATERIAL;
  }
  if constexpr (is_same_any_v<T, VFont *>) {
    return SOCK_FONT;
  }
  if constexpr (is_same_any_v<T, Scene *>) {
    return SOCK_SCENE;
  }
  if constexpr (is_same_any_v<T, Text *>) {
    return SOCK_TEXT_ID;
  }
  if constexpr (is_same_any_v<T, Mask *>) {
    return SOCK_MASK;
  }
  if constexpr (is_same_any_v<T, bSound *>) {
    return SOCK_SOUND;
  }
  if constexpr (is_same_any_v<T, bke::GeometrySet>) {
    return SOCK_GEOMETRY;
  }
  return std::nullopt;
}

/**
 * Check if a socket type stores the static C++ type.
 */
template<typename T>
static bool static_type_is_base_socket_type(const eNodeSocketDatatype socket_type)
{
  switch (socket_type) {
    case SOCK_INT:
      return std::is_same_v<T, int>;
    case SOCK_FLOAT:
      return std::is_same_v<T, float>;
    case SOCK_BOOLEAN:
      return std::is_same_v<T, bool>;
    case SOCK_VECTOR:
      return std::is_same_v<T, float3>;
    case SOCK_RGBA:
      return std::is_same_v<T, ColorGeometry4f>;
    case SOCK_ROTATION:
      return std::is_same_v<T, math::Quaternion>;
    case SOCK_MATRIX:
      return std::is_same_v<T, float4x4>;
    case SOCK_STRING:
      return std::is_same_v<T, std::string>;
    case SOCK_MENU:
      return std::is_same_v<T, nodes::MenuValue>;
    case SOCK_BUNDLE:
      return std::is_same_v<T, nodes::BundlePtr>;
    case SOCK_CLOSURE:
      return std::is_same_v<T, nodes::ClosurePtr>;
    case SOCK_OBJECT:
      return std::is_same_v<T, Object *>;
    case SOCK_COLLECTION:
      return std::is_same_v<T, Collection *>;
    case SOCK_TEXTURE:
      return std::is_same_v<T, Tex *>;
    case SOCK_IMAGE:
      return std::is_same_v<T, Image *>;
    case SOCK_MATERIAL:
      return std::is_same_v<T, Material *>;
    case SOCK_FONT:
      return std::is_same_v<T, VFont *>;
    case SOCK_SCENE:
      return std::is_same_v<T, Scene *>;
    case SOCK_TEXT_ID:
      return std::is_same_v<T, Text *>;
    case SOCK_MASK:
      return std::is_same_v<T, Mask *>;
    case SOCK_SOUND:
      return std::is_same_v<T, bSound *>;
    case SOCK_GEOMETRY:
      return std::is_same_v<T, bke::GeometrySet>;
    case SOCK_CUSTOM:
    case SOCK_SHADER:
      return false;
  }
  BLI_assert_unreachable();
  return false;
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
      case Kind::List:
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
    BLI_assert(static_type_is_base_socket_type<typename T::base_type>(socket_type_));
    return T(this->extract<fn::GField>());
  }
  else if constexpr (std::is_same_v<T, nodes::ListPtr>) {
    if (kind_ != Kind::List) {
      return {};
    }
    return std::move(value_.get<nodes::ListPtr>());
  }
#ifdef WITH_OPENVDB
  else if constexpr (std::is_same_v<T, GVolumeGrid>) {
    switch (kind_) {
      case Kind::Grid: {
        BLI_assert(value_);
        return std::move(value_.get<GVolumeGrid>());
      }
      case Kind::Single:
      case Kind::List:
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
    BLI_assert(static_type_is_base_socket_type<typename T::base_type>(socket_type_));
    return this->extract<GVolumeGrid>().typed<typename T::base_type>();
  }
#endif
  else {
    BLI_assert(static_type_is_base_socket_type<T>(socket_type_));
    if (kind_ == Kind::Single) {
      return std::move(value_.get<T>());
    }
    if (kind_ == Kind::Field) {
      T ret_value;
      std::destroy_at(&ret_value);
      fn::evaluate_constant_field(value_.get<fn::GField>(), &ret_value);
      return ret_value;
    }
    if (kind_ == Kind::List) {
      return {};
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
  else if constexpr (std::is_same_v<T, nodes::ListPtr>) {
    kind_ = Kind::List;
    const CPPType &list_cpp_type = value->cpp_type();
    if (list_cpp_type.is<bke::SocketValueVariant>()) {
      /* For lists of #SocketValueVariant, use the socket type of the first element. */
      const GVArray gvarray = value->varray();
      const VArray varray = gvarray.typed<bke::SocketValueVariant>();
      if (!varray.is_empty()) {
        socket_type_ = varray[0].socket_type_;
      }
    }
    else {
      const std::optional<eNodeSocketDatatype> new_socket_type =
          geo_nodes_base_cpp_type_to_socket_type(list_cpp_type);
      BLI_assert(new_socket_type);
      socket_type_ = *new_socket_type;
    }
    value_.emplace<nodes::ListPtr>(std::move(value));
  }
#ifdef WITH_OPENVDB
  else if constexpr (std::is_same_v<T, GVolumeGrid>) {
    BLI_assert(value);
    const VolumeGridType volume_grid_type = value->grid_type();
    const std::optional<eNodeSocketDatatype> new_socket_type = grid_type_to_socket_type(
        volume_grid_type);
    BLI_assert(new_socket_type);
    socket_type_ = *new_socket_type;
    kind_ = Kind::Grid;
    value_.emplace<GVolumeGrid>(std::move(value));
  }
  else if constexpr (is_VolumeGrid_v<T>) {
    BLI_assert(value);
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
    case SOCK_MENU: {
      value_.emplace<nodes::MenuValue>(*static_cast<const nodes::MenuValue *>(value));
      break;
    }
    case SOCK_MATRIX: {
      value_.emplace<float4x4>(*static_cast<const float4x4 *>(value));
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
    case SOCK_BUNDLE: {
      value_.emplace<nodes::BundlePtr>(*static_cast<const nodes::BundlePtr *>(value));
      break;
    }
    case SOCK_CLOSURE: {
      value_.emplace<nodes::ClosurePtr>(*static_cast<const nodes::ClosurePtr *>(value));
      break;
    }
    case SOCK_OBJECT: {
      value_.emplace<Object *>(*static_cast<Object *const *>(value));
      break;
    }
    case SOCK_COLLECTION: {
      value_.emplace<Collection *>(*static_cast<Collection *const *>(value));
      break;
    }
    case SOCK_TEXTURE: {
      value_.emplace<Tex *>(*static_cast<Tex *const *>(value));
      break;
    }
    case SOCK_IMAGE: {
      value_.emplace<Image *>(*static_cast<Image *const *>(value));
      break;
    }
    case SOCK_MATERIAL: {
      value_.emplace<Material *>(*static_cast<Material *const *>(value));
      break;
    }
    case SOCK_FONT: {
      value_.emplace<VFont *>(*static_cast<VFont *const *>(value));
      break;
    }
    case SOCK_SCENE: {
      value_.emplace<Scene *>(*static_cast<Scene *const *>(value));
      break;
    }
    case SOCK_TEXT_ID: {
      value_.emplace<Text *>(*static_cast<Text *const *>(value));
      break;
    }
    case SOCK_MASK: {
      value_.emplace<Mask *>(*static_cast<Mask *const *>(value));
      break;
    }
    case SOCK_SOUND: {
      value_.emplace<bSound *>(*static_cast<bSound *const *>(value));
      break;
    }
    case SOCK_GEOMETRY: {
      value_.emplace<bke::GeometrySet>(*static_cast<const bke::GeometrySet *>(value));
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

bool SocketValueVariant::is_field() const
{
  return kind_ == Kind::Field;
}

bool SocketValueVariant::is_volume_grid() const
{
  return kind_ == Kind::Grid;
}

bool SocketValueVariant::is_single() const
{
  return kind_ == Kind::Single;
}

bool SocketValueVariant::is_list() const
{
  return kind_ == Kind::List;
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
    case Kind::List:
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
    case SOCK_MATRIX:
      return value_.allocate<float4x4>();
    case SOCK_RGBA:
      return value_.allocate<ColorGeometry4f>();
    case SOCK_STRING:
      return value_.allocate<std::string>();
    case SOCK_MENU:
      return value_.allocate<nodes::MenuValue>();
    case SOCK_BUNDLE:
      return value_.allocate<nodes::BundlePtr>();
    case SOCK_CLOSURE:
      return value_.allocate<nodes::ClosurePtr>();
    case SOCK_OBJECT:
      return value_.allocate<Object *>();
    case SOCK_COLLECTION:
      return value_.allocate<Collection *>();
    case SOCK_TEXTURE:
      return value_.allocate<Tex *>();
    case SOCK_IMAGE:
      return value_.allocate<Image *>();
    case SOCK_MATERIAL:
      return value_.allocate<Material *>();
    case SOCK_FONT:
      return value_.allocate<VFont *>();
    case SOCK_SCENE:
      return value_.allocate<Scene *>();
    case SOCK_TEXT_ID:
      return value_.allocate<Text *>();
    case SOCK_MASK:
      return value_.allocate<Mask *>();
    case SOCK_SOUND:
      return value_.allocate<bSound *>();
    case SOCK_GEOMETRY:
      return value_.allocate<bke::GeometrySet>();
    default: {
      BLI_assert_unreachable();
      return nullptr;
    }
  }
}

void SocketValueVariant::ensure_owns_direct_data()
{
  if (this->owns_direct_data()) {
    return;
  }
  switch (socket_type_) {
    case SOCK_FLOAT:
    case SOCK_INT:
    case SOCK_VECTOR:
    case SOCK_BOOLEAN:
    case SOCK_ROTATION:
    case SOCK_MATRIX:
    case SOCK_RGBA:
    case SOCK_STRING:
    case SOCK_MENU:
    case SOCK_OBJECT:
    case SOCK_COLLECTION:
    case SOCK_TEXTURE:
    case SOCK_IMAGE:
    case SOCK_MATERIAL:
    case SOCK_FONT:
    case SOCK_SCENE:
    case SOCK_TEXT_ID:
    case SOCK_MASK:
    case SOCK_SOUND:
    case SOCK_CLOSURE: {
      break;
    }
    case SOCK_BUNDLE: {
      if (this->is_single()) {
        if (nodes::BundlePtr &bundle_ptr = value_.get<nodes::BundlePtr>()) {
          if (!bundle_ptr->is_mutable()) {
            bundle_ptr = bundle_ptr->copy();
          }
          nodes::Bundle &bundle = const_cast<nodes::Bundle &>(*bundle_ptr);
          bundle.ensure_owns_direct_data();
        }
      }
      else if (this->is_list()) {
        /* TODO: Handle lists before #use_geometry_nodes_lists is removed. */
      }
      break;
    }
    case SOCK_GEOMETRY: {
      if (this->is_single()) {
        GeometrySet &geometry = value_.get<GeometrySet>();
        geometry.ensure_owns_direct_data();
      }
      else if (this->is_list()) {
        /* TODO: Handle lists before #use_geometry_nodes_lists is removed. */
      }
      break;
    }
    default: {
      break;
    }
  }
  BLI_assert(this->owns_direct_data());
}

bool SocketValueVariant::owns_direct_data() const
{
  switch (socket_type_) {
    case SOCK_FLOAT:
    case SOCK_INT:
    case SOCK_VECTOR:
    case SOCK_BOOLEAN:
    case SOCK_ROTATION:
    case SOCK_MATRIX:
    case SOCK_RGBA:
    case SOCK_STRING:
    case SOCK_MENU:
    case SOCK_OBJECT:
    case SOCK_COLLECTION:
    case SOCK_TEXTURE:
    case SOCK_IMAGE:
    case SOCK_MATERIAL:
    case SOCK_FONT:
    case SOCK_SCENE:
    case SOCK_TEXT_ID:
    case SOCK_MASK:
    case SOCK_SOUND:
    case SOCK_CLOSURE: {
      return true;
    }
    case SOCK_BUNDLE: {
      if (this->is_single()) {
        if (const nodes::BundlePtr &bundle_ptr = value_.get<nodes::BundlePtr>()) {
          return bundle_ptr->owns_direct_data();
        }
      }
      else if (this->is_list()) {
        /* TODO: Handle lists before #use_geometry_nodes_lists is removed. */
      }
      return true;
    }
    case SOCK_GEOMETRY: {
      if (this->is_single()) {
        const GeometrySet &geometry = value_.get<GeometrySet>();
        return geometry.owns_direct_data();
      }
      if (this->is_list()) {
        /* TODO: Handle lists before #use_geometry_nodes_lists is removed. */
      }
      return true;
    }
    default:
      return true;
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
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(float3)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(ColorGeometry4f)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID(math::Quaternion)

INSTANTIATE(std::string)
INSTANTIATE(fn::GField)
INSTANTIATE(nodes::BundlePtr)
INSTANTIATE(nodes::ClosurePtr)
INSTANTIATE(nodes::ListPtr)
INSTANTIATE(bke::GeometrySet)

INSTANTIATE(Object *)
INSTANTIATE(Collection *)
INSTANTIATE(Tex *)
INSTANTIATE(Image *)
INSTANTIATE(Material *)
INSTANTIATE(VFont *)
INSTANTIATE(Scene *)
INSTANTIATE(Text *)
INSTANTIATE(Mask *)
INSTANTIATE(bSound *)

INSTANTIATE(float4x4)
INSTANTIATE(fn::Field<float4x4>)

INSTANTIATE(nodes::MenuValue)
INSTANTIATE(fn::Field<nodes::MenuValue>)

#ifdef WITH_OPENVDB
INSTANTIATE(GVolumeGrid)
#endif

}  // namespace blender::bke
