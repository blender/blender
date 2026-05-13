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

#include "BLI_color_types.hh"
#include "BLI_math_rotation_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_memory_counter.hh"

#include "FN_field_evaluation.hh"

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
    case SOCK_INT_VECTOR:
      return false;
  }
  BLI_assert_unreachable();
  return false;
}

template<typename T> T SocketValueVariant::extract()
{
  if constexpr (std::is_same_v<T, fn::GField>) {
    switch (this->kind()) {
      case Kind::Field: {
        return std::move(value_.get<fn::GField>());
      }
      case Kind::Single: {
        const GPointer single_value = this->get_single_ptr();
        return fn::GField::from_constant(*single_value.type(), single_value.get());
      }
      case Kind::List:
      case Kind::Grid: {
        const CPPType *cpp_type = socket_type_to_geo_nodes_base_cpp_type(this->socket_type());
        BLI_assert(cpp_type);
        return fn::GField::from_constant(*cpp_type, cpp_type->default_value());
      }
      case Kind::None: {
        BLI_assert_unreachable();
        break;
      }
    }
  }
  else if constexpr (fn::is_field_v<T>) {
    using base_type = typename T::base_type;
    BLI_assert(static_type_is_base_socket_type<base_type>(this->socket_type()));
    return this->extract<fn::GField>().typed<base_type>();
  }
  else if constexpr (std::is_same_v<T, nodes::GListPtr>) {
    if (this->kind() != Kind::List) {
      return {};
    }
    return std::move(value_.get<nodes::GListPtr>());
  }
  else if constexpr (nodes::is_ListPtr_v<T>) {
    if (this->kind() != Kind::List) {
      return {};
    }
    using base_type = typename T::base_type;
    BLI_assert(static_type_is_base_socket_type<base_type>(this->socket_type()));
    return this->extract<nodes::GListPtr>().typed<base_type>();
  }
#ifdef WITH_OPENVDB
  else if constexpr (std::is_same_v<T, GVolumeGrid>) {
    switch (this->kind()) {
      case Kind::Grid: {
        BLI_assert(value_);
        return std::move(value_.get<GVolumeGrid>());
      }
      case Kind::Single:
      case Kind::List:
      case Kind::Field: {
        const std::optional<VolumeGridType> grid_type = socket_type_to_grid_type(
            this->socket_type());
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
    BLI_assert(static_type_is_base_socket_type<typename T::base_type>(this->socket_type()));
    return this->extract<GVolumeGrid>().typed<typename T::base_type>();
  }
#endif
  else {
    BLI_assert(static_type_is_base_socket_type<T>(this->socket_type()));
    if (this->kind() == Kind::Single) {
      return std::move(value_.get<T>());
    }
    if (this->kind() == Kind::Field) {
      T ret_value;
      std::destroy_at(&ret_value);
      fn::evaluate_constant_field(value_.get<fn::GField>(), &ret_value);
      return ret_value;
    }
    if (this->kind() == Kind::List) {
      return {};
    }
  }
  BLI_assert_unreachable();
  if constexpr (std::is_same_v<T, fn::GField>) {
    return fn::GField(CPPType::get<float>());
  }
  else {
    return T();
  }
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
    value_.emplace<fn::GField>(std::move(value));
    value_.extra.socket_type = *new_socket_type;
    value_.extra.kind = Kind::Field;
    static_assert(decltype(value_)::is_inline_v<fn::GField>);
  }
  else if constexpr (fn::is_field_v<T>) {
    /* Always store #Field<T> as #GField. */
    this->store_impl<fn::GField>(std::move(value));
  }
  else if constexpr (std::is_same_v<T, nodes::GListPtr>) {
    const CPPType &list_cpp_type = value->cpp_type();
    eNodeSocketDatatype socket_type = SOCK_CUSTOM;
    if (list_cpp_type.is<bke::SocketValueVariant>()) {
      /* For lists of #SocketValueVariant, use the socket type of the first element. */
      const GVArray gvarray = value->varray();
      const VArray varray = gvarray.typed<bke::SocketValueVariant>();
      if (!varray.is_empty()) {
        socket_type = varray[0].socket_type();
      }
    }
    else {
      const std::optional<eNodeSocketDatatype> new_socket_type =
          geo_nodes_base_cpp_type_to_socket_type(list_cpp_type);
      BLI_assert(new_socket_type);
      socket_type = *new_socket_type;
    }
    value_.emplace<nodes::GListPtr>(std::move(value));
    value_.extra.socket_type = socket_type;
    value_.extra.kind = Kind::List;
  }
  else if constexpr (nodes::is_ListPtr_v<T>) {
    /* Always store #ListPtr as #GListPtr. */
    this->store_impl<nodes::GListPtr>(std::move(value));
  }
#ifdef WITH_OPENVDB
  else if constexpr (std::is_same_v<T, GVolumeGrid>) {
    BLI_assert(value);
    const VolumeGridType volume_grid_type = value->grid_type();
    const std::optional<eNodeSocketDatatype> new_socket_type = grid_type_to_socket_type(
        volume_grid_type);
    BLI_assert(new_socket_type);
    value_.emplace<GVolumeGrid>(std::move(value));
    value_.extra.socket_type = *new_socket_type;
    value_.extra.kind = Kind::Grid;
  }
  else if constexpr (is_VolumeGrid_v<T>) {
    BLI_assert(value);
    this->store_impl<GVolumeGrid>(std::move(value));
  }
#endif
  else {
    const std::optional<eNodeSocketDatatype> new_socket_type = static_type_to_socket_type<T>();
    BLI_assert(new_socket_type);
    value_.emplace<T>(std::move(value));
    value_.extra.socket_type = *new_socket_type;
    value_.extra.kind = Kind::Single;
  }
  BLI_assert(this->kind() != Kind::None);
}

void SocketValueVariant::store_single(const eNodeSocketDatatype socket_type, const void *value)
{
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
  value_.extra.kind = Kind::Single;
  value_.extra.socket_type = socket_type;
}

bool SocketValueVariant::is_context_dependent_field() const
{
  if (!value_.is<fn::GField>()) {
    return false;
  }
  const fn::GField &field = value_.get<fn::GField>();
  return field.depends_on_input();
}

bool SocketValueVariant::is_field() const
{
  return this->kind() == Kind::Field;
}

bool SocketValueVariant::is_volume_grid() const
{
  return this->kind() == Kind::Grid;
}

bool SocketValueVariant::is_single() const
{
  return this->kind() == Kind::Single;
}

bool SocketValueVariant::is_list() const
{
  return this->kind() == Kind::List;
}

void SocketValueVariant::convert_to_single()
{
  switch (this->kind()) {
    case Kind::Single: {
      /* Nothing to do. */
      break;
    }
    case Kind::Field: {
      /* Evaluates the field without inputs to try to get a single value. If the field depends on
       * context, the default value is used instead. */
      fn::GField field = std::move(value_.get<fn::GField>());
      void *buffer = this->allocate_single(this->socket_type());
      fn::evaluate_constant_field(field, buffer);
      break;
    }
    case Kind::List:
    case Kind::Grid: {
      /* Can't convert a grid to a single value, so just use the default value of the current
       * socket type. */
      const CPPType &cpp_type = *socket_type_to_geo_nodes_base_cpp_type(this->socket_type());
      this->store_single(this->socket_type(), cpp_type.default_value());
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
  BLI_assert(this->kind() == Kind::Single);
  const CPPType *type = socket_type_to_geo_nodes_base_cpp_type(this->socket_type());
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
  void *ptr = nullptr;
  switch (socket_type) {
    case SOCK_FLOAT:
      ptr = value_.allocate<float>();
      break;
    case SOCK_INT:
      ptr = value_.allocate<int>();
      break;
    case SOCK_VECTOR:
      ptr = value_.allocate<float3>();
      break;
    case SOCK_BOOLEAN:
      ptr = value_.allocate<bool>();
      break;
    case SOCK_ROTATION:
      ptr = value_.allocate<math::Quaternion>();
      break;
    case SOCK_MATRIX:
      ptr = value_.allocate<float4x4>();
      break;
    case SOCK_RGBA:
      ptr = value_.allocate<ColorGeometry4f>();
      break;
    case SOCK_STRING:
      ptr = value_.allocate<std::string>();
      break;
    case SOCK_MENU:
      ptr = value_.allocate<nodes::MenuValue>();
      break;
    case SOCK_BUNDLE:
      ptr = value_.allocate<nodes::BundlePtr>();
      break;
    case SOCK_CLOSURE:
      ptr = value_.allocate<nodes::ClosurePtr>();
      break;
    case SOCK_OBJECT:
      ptr = value_.allocate<Object *>();
      break;
    case SOCK_COLLECTION:
      ptr = value_.allocate<Collection *>();
      break;
    case SOCK_TEXTURE:
      ptr = value_.allocate<Tex *>();
      break;
    case SOCK_IMAGE:
      ptr = value_.allocate<Image *>();
      break;
    case SOCK_MATERIAL:
      ptr = value_.allocate<Material *>();
      break;
    case SOCK_FONT:
      ptr = value_.allocate<VFont *>();
      break;
    case SOCK_SCENE:
      ptr = value_.allocate<Scene *>();
      break;
    case SOCK_TEXT_ID:
      ptr = value_.allocate<Text *>();
      break;
    case SOCK_MASK:
      ptr = value_.allocate<Mask *>();
      break;
    case SOCK_SOUND:
      ptr = value_.allocate<bSound *>();
      break;
    case SOCK_GEOMETRY:
      ptr = value_.allocate<bke::GeometrySet>();
      break;
    default: {
      BLI_assert_unreachable();
      return nullptr;
    }
  }
  value_.extra.kind = Kind::Single;
  value_.extra.socket_type = socket_type;
  return ptr;
}

void SocketValueVariant::ensure_owns_direct_data()
{
  if (this->owns_direct_data()) {
    return;
  }
  switch (this->socket_type()) {
    case SOCK_FLOAT:
    case SOCK_INT:
    case SOCK_VECTOR:
    case SOCK_INT_VECTOR:
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
          bundle_ptr.ensure_mutable_inplace();
          nodes::Bundle &bundle = const_cast<nodes::Bundle &>(*bundle_ptr);
          bundle.ensure_owns_direct_data();
        }
      }
      if (this->is_list()) {
        if (nodes::GListPtr &list_ptr = value_.get<nodes::GListPtr>()) {
          auto &list = list_ptr.get_for_write();
          list.ensure_owns_direct_data();
        }
      }
      break;
    }
    case SOCK_GEOMETRY: {
      if (this->is_single()) {
        GeometrySet &geometry = value_.get<GeometrySet>();
        geometry.ensure_owns_direct_data();
      }
      if (this->is_list()) {
        if (nodes::GListPtr &list_ptr = value_.get<nodes::GListPtr>()) {
          auto &list = list_ptr.get_for_write();
          list.ensure_owns_direct_data();
        }
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
  switch (this->socket_type()) {
    case SOCK_FLOAT:
    case SOCK_INT:
    case SOCK_VECTOR:
    case SOCK_INT_VECTOR:
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
        if (const auto &list_ptr = value_.get<nodes::GListPtr>()) {
          return list_ptr->owns_direct_data();
        }
      }
      return true;
    }
    case SOCK_GEOMETRY: {
      if (this->is_single()) {
        const GeometrySet &geometry = value_.get<GeometrySet>();
        return geometry.owns_direct_data();
      }
      if (this->is_list()) {
        if (const auto &list_ptr = value_.get<nodes::GListPtr>()) {
          return list_ptr->owns_direct_data();
        }
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
  if (value_variant.kind() == SocketValueVariant::Kind::Single) {
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
  if (this->kind() == Kind::None) {
    return false;
  }
  return this->socket_type() == socket_type;
}

void SocketValueVariant::count_memory(MemoryCounter &memory) const
{
  switch (this->kind()) {
    case Kind::None: {
      break;
    }
    case Kind::Single: {
      const GPointer value = this->get_single_ptr();
      const CPPType &cpp_type = *value.type();
      memory.add(cpp_type.size);
      if (cpp_type.is<GeometrySet>()) {
        const GeometrySet &geometry = *value.get<GeometrySet>();
        geometry.count_memory(memory);
      }
      if (cpp_type.is<nodes::BundlePtr>()) {
        const nodes::BundlePtr &bundle_ptr = *value.get<nodes::BundlePtr>();
        if (bundle_ptr) {
          bundle_ptr->count_memory(memory);
        }
      }
      break;
    }
    case Kind::Field: {
      break;
    }
    case Kind::Grid: {
#ifdef WITH_OPENVDB
      if (const GVolumeGrid &grid = value_.get<GVolumeGrid>()) {
        grid->count_memory(memory);
      }
#endif
      break;
    }
    case Kind::List: {
      if (const auto &list = value_.get<nodes::GListPtr>()) {
        list->count_memory(memory);
      }
      break;
    }
  }
}

#define INSTANTIATE(TYPE) \
  template TYPE SocketValueVariant::extract(); \
  template TYPE SocketValueVariant::get() const; \
  template void SocketValueVariant::store_impl(TYPE);

#ifdef WITH_OPENVDB
#  define INSTANTIATE_SINGLE_AND_FIELD_AND_GRID_AND_LIST(TYPE) \
    INSTANTIATE(TYPE) \
    INSTANTIATE(fn::Field<TYPE>) \
    INSTANTIATE(nodes::ListPtr<TYPE>) \
    INSTANTIATE(VolumeGrid<TYPE>)
#else
#  define INSTANTIATE_SINGLE_AND_FIELD_AND_GRID_AND_LIST(TYPE) \
    INSTANTIATE(TYPE) \
    INSTANTIATE(fn::Field<TYPE>) \
    INSTANTIATE(nodes::ListPtr<TYPE>)
#endif

#define INSTANTIATE_SINGLE_AND_LIST(TYPE) \
  INSTANTIATE(TYPE) \
  INSTANTIATE(nodes::ListPtr<TYPE>)

INSTANTIATE_SINGLE_AND_FIELD_AND_GRID_AND_LIST(int)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID_AND_LIST(bool)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID_AND_LIST(float)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID_AND_LIST(float3)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID_AND_LIST(ColorGeometry4f)
INSTANTIATE_SINGLE_AND_FIELD_AND_GRID_AND_LIST(math::Quaternion)

INSTANTIATE_SINGLE_AND_LIST(std::string)
INSTANTIATE(fn::GField)
INSTANTIATE_SINGLE_AND_LIST(nodes::BundlePtr)
INSTANTIATE_SINGLE_AND_LIST(nodes::ClosurePtr)
INSTANTIATE(nodes::GListPtr)
INSTANTIATE_SINGLE_AND_LIST(bke::GeometrySet)

INSTANTIATE_SINGLE_AND_LIST(Object *)
INSTANTIATE_SINGLE_AND_LIST(Collection *)
INSTANTIATE_SINGLE_AND_LIST(Tex *)
INSTANTIATE_SINGLE_AND_LIST(Image *)
INSTANTIATE_SINGLE_AND_LIST(Material *)
INSTANTIATE_SINGLE_AND_LIST(VFont *)
INSTANTIATE_SINGLE_AND_LIST(Scene *)
INSTANTIATE_SINGLE_AND_LIST(Text *)
INSTANTIATE_SINGLE_AND_LIST(Mask *)
INSTANTIATE_SINGLE_AND_LIST(bSound *)

INSTANTIATE_SINGLE_AND_LIST(float4x4)
INSTANTIATE(fn::Field<float4x4>)

INSTANTIATE_SINGLE_AND_LIST(nodes::MenuValue)
INSTANTIATE(fn::Field<nodes::MenuValue>)

#ifdef WITH_OPENVDB
INSTANTIATE(GVolumeGrid)
#endif

}  // namespace blender::bke
