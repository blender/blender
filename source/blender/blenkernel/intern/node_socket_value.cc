/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node_socket_value.hh"
#include "BKE_type_conversions.hh"

#include "FN_field.hh"
#include "FN_field_evaluation.hh"

#include "BKE_volume_grid.hh"

#include "BKE_volume_grid_multi_function_eval.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_closure.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_menu_value.hh"

#include "BLI_color_types.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_memory_counter.hh"

struct Object;
struct VFont;
struct bSound;
struct Text;
struct Mask;

namespace blender::bke {

using fn::Field;
using fn::GField;
using nodes::BundlePtr;
using nodes::ClosurePtr;
using nodes::GList;
using nodes::GListPtr;
using nodes::List;
using nodes::ListPtr;
using volume_grid::GVolumeGrid;
using volume_grid::VolumeGrid;

/**
 * All types that can be stored as a "single" value in a #SocketValueVariant.
 */
#define SOCKET_VALUE_SINGLE_TYPES \
  X(float) \
  X(int) \
  X(float2) \
  X(float3) \
  X(float4) \
  X(int2) \
  X(bool) \
  X(int8_t) \
  X(short2) \
  X(ColorGeometry4f) \
  X(ColorGeometry4b) \
  X(math::Quaternion) \
  X(float4x4) \
  X(nodes::MenuValue) \
  X(std::string) \
  X(BundlePtr) \
  X(ClosurePtr) \
  X(GeometrySet) \
  X(Object *) \
  X(Material *) \
  X(Image *) \
  X(VFont *) \
  X(Scene *) \
  X(bSound *) \
  X(Collection *) \
  X(Tex *) \
  X(Text *) \
  X(Mask *)

/**
 * The subset of #SOCKET_VALUE_SINGLE_TYPES that also support #Field<T> and #ListPtr<T>.
 */
#define SOCKET_VALUE_FIELD_AND_LIST_TYPES \
  X(float) \
  X(int) \
  X(float2) \
  X(float3) \
  X(float4) \
  X(int2) \
  X(bool) \
  X(int8_t) \
  X(short2) \
  X(ColorGeometry4f) \
  X(ColorGeometry4b) \
  X(math::Quaternion) \
  X(float4x4) \
  X(nodes::MenuValue) \
  X(std::string)

namespace detail {

template<typename CurrentT>
bool SocketValueVariantTypeInfo::try_convert_fn(const CPPType &dst_type,
                                                SocketValueVariantAny &value)
{
  if (CPPType::get<CurrentT>() == dst_type) {
    return true;
  }
  static const DataTypeConversions &conversions = get_implicit_type_conversions();
  if constexpr (std::is_same_v<CurrentT, GField>) {
    const GField &src_field = value.get<GField>();
    const CPPType &src_base_type = src_field.cpp_type();
    if (dst_type.generic_type && dst_type.generic_type->is<GField>()) {
      if (src_base_type == *dst_type.base_type) {
        /* Nothing to do.*/
        return true;
      }
      const ConversionFunctions *fns = conversions.get_conversion_functions(src_base_type,
                                                                            *dst_type.base_type);
      if (!fns) {
        return false;
      }
      if (const void *src_single_value = src_field.get_if_constant()) {
        if (!fns->convert_single_to_initialized) {
          return false;
        }
        BUFFER_FOR_CPP_TYPE_VALUE(*dst_type.base_type, dst_single_value);
        fns->convert_single_to_initialized(src_single_value, dst_single_value);
        value.emplace<GField>(GField::from_constant(*dst_type.base_type, dst_single_value));
        dst_type.base_type->destruct(dst_single_value);
        return true;
      }
      if (!fns->multi_function) {
        return false;
      }
      fn::FieldOperationPtr op = fn::FieldOperation::from(*fns->multi_function, {src_field});
      value.emplace<GField>(GField(std::move(op), 0));
      return true;
    }

    if (src_base_type == dst_type) {
      BUFFER_FOR_CPP_TYPE_VALUE(dst_type, tmp_buffer);
      fn::evaluate_constant_field(src_field, tmp_buffer);
      void *dst_value = SocketValueVariant::allocate(dst_type, value);
      dst_type.move_construct(tmp_buffer, dst_value);
      dst_type.destruct(tmp_buffer);
      return true;
    }
    const ConversionFunctions *fns = conversions.get_conversion_functions(src_base_type, dst_type);
    if (!fns || !fns->convert_single_to_initialized) {
      return false;
    }
    BUFFER_FOR_CPP_TYPE_VALUE(src_base_type, src_single_value);
    fn::evaluate_constant_field(src_field, src_single_value);
    void *dst_value = SocketValueVariant::allocate(dst_type, value);
    fns->convert_single_to_uninitialized(src_single_value, dst_value);
    src_base_type.destruct(src_single_value);
    return true;
  }
  else if constexpr (std::is_same_v<CurrentT, GListPtr>) {
    GListPtr &src_list = value.get<GListPtr>();
    if (dst_type.generic_type && dst_type.generic_type->is<GListPtr>()) {
      if (!src_list) {
        /* Nothing to do. */
        return true;
      }
      const CPPType &src_base_type = src_list->cpp_type();
      if (src_base_type == *dst_type.base_type) {
        /* Nothing to do. */
        return true;
      }
      const ConversionFunctions *fns = conversions.get_conversion_functions(src_base_type,
                                                                            *dst_type.base_type);
      if (!fns || !fns->multi_function || !fns->convert_single_to_uninitialized) {
        return false;
      }
      const int64_t size = src_list->size();
      const std::variant<GSpan, GPointer> src_values = src_list->values();
      if (const auto *src_span = std::get_if<GSpan>(&src_values)) {
        GArray<> dst_values(*dst_type.base_type, size, NoInitialization{});
        IndexMask mask{size};
        mf::ParamsBuilder params{*fns->multi_function, &mask};
        params.add_readonly_single_input(*src_span);
        params.add_uninitialized_single_output(dst_values);
        mf::ContextBuilder context;
        fns->multi_function->call_auto(mask, params, context);
        src_list = GList::from_garray(std::move(dst_values));
        return true;
      }
      if (const auto *src_single_value = std::get_if<GPointer>(&src_values)) {
        BUFFER_FOR_CPP_TYPE_VALUE(*dst_type.base_type, dst_single_value);
        fns->convert_single_to_uninitialized(src_single_value->get(), dst_single_value);
        src_list = GList::from_single({*dst_type.base_type, dst_single_value}, size);
        dst_type.base_type->destruct(dst_single_value);
        return true;
      }
    }
    return false;
  }
#ifdef WITH_OPENVDB
  else if constexpr (std::is_same_v<CurrentT, GVolumeGrid>) {
    GVolumeGrid &src_grid = value.get<GVolumeGrid>();
    if (dst_type.generic_type && dst_type.generic_type->is<GVolumeGrid>()) {
      if (!src_grid) {
        /* Nothing to do. */
        return true;
      }
      const CPPType *src_base_type = src_grid->cpp_type();
      if (!src_base_type) {
        /* Unknown type. */
        return false;
      }
      if (src_base_type == dst_type.base_type) {
        /* Nothing to do. */
        return true;
      }
      const ConversionFunctions *fns = conversions.get_conversion_functions(*src_base_type,
                                                                            *dst_type.base_type);
      if (!fns || !fns->multi_function) {
        return false;
      }
      VolumeTreeAccessToken tree_token;
      const openvdb::GridBase &src_grid_base = src_grid->grid(tree_token);
      using namespace volume_grid::multi_function_eval;
      EvalResult conversion_result = evaluate_multi_function_on_grid(
          *fns->multi_function, {&src_grid_base}, {true});
      if (std::holds_alternative<EvalResult::Failure>(conversion_result.result)) {
        return false;
      }
      src_grid = GVolumeGrid(
          std::move(std::get<EvalResult::Success>(conversion_result.result).output_grids[0]));
      return true;
    }
    return false;
  }
#endif
  else {
    /* The stored value is a single value. */

    if (dst_type.is<GField>()) {
      GField field = GField::from_constant(CPPType::get<CurrentT>(), value.get());
      value.emplace<GField>(std::move(field));
      return true;
    }
    if (dst_type.generic_type && dst_type.generic_type->is<GField>()) {
      if (dst_type.base_type->is<CurrentT>()) {
        GField field = GField::from_constant(CPPType::get<CurrentT>(), value.get());
        value.emplace<GField>(std::move(field));
        return true;
      }
      const ConversionFunctions *fns = conversions.get_conversion_functions(
          CPPType::get<CurrentT>(), *dst_type.base_type);
      if (!fns || !fns->convert_single_to_initialized) {
        return false;
      }
      BUFFER_FOR_CPP_TYPE_VALUE(*dst_type.base_type, tmp_buffer);
      fns->convert_single_to_initialized(value.get(), tmp_buffer);
      value.emplace<GField>(GField::from_constant(*dst_type.base_type, tmp_buffer));
      dst_type.base_type->destruct(tmp_buffer);
      return true;
    }
    const ConversionFunctions *fns = conversions.get_conversion_functions(CPPType::get<CurrentT>(),
                                                                          dst_type);
    if (!fns || !fns->convert_single_to_uninitialized) {
      return false;
    }
    BUFFER_FOR_CPP_TYPE_VALUE(dst_type, tmp_buffer);
    fns->convert_single_to_uninitialized(value.get(), tmp_buffer);
    void *dst_value = SocketValueVariant::allocate(dst_type, value);
    dst_type.move_construct(tmp_buffer, dst_value);
    dst_type.destruct(tmp_buffer);
    return true;
  }
}

template<typename CurrentT>
bool SocketValueVariantTypeInfo::is_interpretable_as_fn(const CPPType &dst_type,
                                                        const SocketValueVariantAny &value)
{
  if constexpr (std::is_same_v<CurrentT, GField>) {
    if (dst_type.is<GField>()) {
      return true;
    }
    if (!dst_type.generic_type) {
      return false;
    }
    if (dst_type.generic_type->is<GField>()) {
      const GField &field = value.get<GField>();
      const CPPType &base_type = field.cpp_type();
      return base_type == *dst_type.base_type;
    }
    return false;
  }
  else if constexpr (std::is_same_v<CurrentT, GListPtr>) {
    if (dst_type.is<GListPtr>()) {
      return true;
    }
    if (!dst_type.generic_type) {
      return false;
    }
    if (dst_type.generic_type->is<GListPtr>()) {
      const GListPtr &list_ptr = value.get<GListPtr>();
      if (!list_ptr) {
        return true;
      }
      const CPPType &base_type = list_ptr->cpp_type();
      return base_type == *dst_type.base_type;
    }
    return false;
  }
#ifdef WITH_OPENVDB
  else if constexpr (std::is_same_v<CurrentT, GVolumeGrid>) {
    if (dst_type.is<GVolumeGrid>()) {
      return true;
    }
    if (!dst_type.generic_type) {
      return false;
    }
    if (dst_type.generic_type->is<GVolumeGrid>()) {
      const GVolumeGrid &grid = value.get<GVolumeGrid>();
      if (!grid) {
        return true;
      }
      const CPPType *grid_value_type = grid->cpp_type();
      return grid_value_type == dst_type.base_type;
    }
    return false;
  }
#endif

  else {
    return CPPType::get<CurrentT>() == dst_type;
  }
}

#define DEFINE_TYPE(TYPE) \
  template bool SocketValueVariantTypeInfo::try_convert_fn<TYPE>(const CPPType &dst_type, \
                                                                 SocketValueVariantAny &value); \
  template bool SocketValueVariantTypeInfo::is_interpretable_as_fn<TYPE>( \
      const CPPType &dst_type, const SocketValueVariantAny &value);

#define X(TYPE) DEFINE_TYPE(TYPE)
SOCKET_VALUE_SINGLE_TYPES
#undef X

/**
 * Even though #Field<T>, #ListPtr<T> and #VolumeGrid<T> are never stored directly (they are
 * always redirected to their generic storage type, see #to_storage_type), #SocketValueVariant's
 * templated accessors (#get_if, #try_convert, #ensure_type) still instantiate their function
 * bodies for these compile-time types.
 */
#define X(TYPE) \
  DEFINE_TYPE(Field<TYPE>) \
  DEFINE_TYPE(ListPtr<TYPE>)
SOCKET_VALUE_FIELD_AND_LIST_TYPES
#undef X

DEFINE_TYPE(GField)
DEFINE_TYPE(GListPtr)

#ifdef WITH_OPENVDB
DEFINE_TYPE(volume_grid::GVolumeGrid)
DEFINE_TYPE(VolumeGrid<float>)
DEFINE_TYPE(VolumeGrid<int>)
DEFINE_TYPE(VolumeGrid<float3>)
DEFINE_TYPE(VolumeGrid<bool>)
#endif

}  // namespace detail

template<typename T>
inline T &SocketValueVariant::init_default(detail::SocketValueVariantAny &value)
{
  if constexpr (requires { typename T::generic_type; }) {
    using GenericType = typename T::generic_type;
    using BaseType = typename T::base_type;
    if constexpr (std::is_same_v<GenericType, GField>) {
      const CPPType &base_cpp_type = CPPType::get<BaseType>();
      return value.emplace<GField>(base_cpp_type).typed<BaseType>();
    }
    else if constexpr (std::is_same_v<GenericType, GListPtr>) {
      return value.emplace<GListPtr>().typed<BaseType>();
    }
#ifdef WITH_OPENVDB
    else if constexpr (std::is_same_v<GenericType, GVolumeGrid>) {
      return value.emplace<GVolumeGrid>().typed<BaseType>();
    }
#endif
  }
  else if constexpr (std::is_same_v<T, GField>) {
    /* Some default fallback type. */
    return value.emplace<GField>(CPPType::get<float>());
  }
#ifdef WITH_OPENVDB
  else if constexpr (std::is_same_v<T, GVolumeGrid>) {
    return value.emplace<GVolumeGrid>();
  }
#endif
  else {
    return value.emplace<T>();
  }
}

/** Explicit instantiations. */
#define DEFINE_INIT_DEFAULT(TYPE) \
  template TYPE &SocketValueVariant::init_default<TYPE>(detail::SocketValueVariantAny & value);

#define X(TYPE) DEFINE_INIT_DEFAULT(TYPE)
SOCKET_VALUE_SINGLE_TYPES
#undef X

#define X(TYPE) \
  DEFINE_INIT_DEFAULT(Field<TYPE>) \
  DEFINE_INIT_DEFAULT(ListPtr<TYPE>)
SOCKET_VALUE_FIELD_AND_LIST_TYPES
#undef X

DEFINE_INIT_DEFAULT(GField)
DEFINE_INIT_DEFAULT(GListPtr)
#ifdef WITH_OPENVDB
DEFINE_INIT_DEFAULT(GVolumeGrid)
DEFINE_INIT_DEFAULT(VolumeGrid<float>)
DEFINE_INIT_DEFAULT(VolumeGrid<int>)
DEFINE_INIT_DEFAULT(VolumeGrid<float3>)
DEFINE_INIT_DEFAULT(VolumeGrid<bool>)
#endif

#undef DEFINE_INIT_DEFAULT

void *SocketValueVariant::init_default(const CPPType &type, detail::SocketValueVariantAny &value)
{
#define X(TYPE) \
  if (type.is<TYPE>()) { \
    return &SocketValueVariant::init_default<TYPE>(value); \
  }
  SOCKET_VALUE_SINGLE_TYPES
#undef X

#define X(TYPE) \
  if (type.is<Field<TYPE>>()) { \
    return &SocketValueVariant::init_default<Field<TYPE>>(value); \
  } \
  if (type.is<ListPtr<TYPE>>()) { \
    return &SocketValueVariant::init_default<ListPtr<TYPE>>(value); \
  }
  SOCKET_VALUE_FIELD_AND_LIST_TYPES
#undef X

  if (type.is<GField>()) {
    return &SocketValueVariant::init_default<GField>(value);
  }
  if (type.is<GListPtr>()) {
    return &SocketValueVariant::init_default<GListPtr>(value);
  }
#ifdef WITH_OPENVDB
  if (type.is<GVolumeGrid>()) {
    return &SocketValueVariant::init_default<GVolumeGrid>(value);
  }
  if (type.is<VolumeGrid<float>>()) {
    return &SocketValueVariant::init_default<VolumeGrid<float>>(value);
  }
  if (type.is<VolumeGrid<int>>()) {
    return &SocketValueVariant::init_default<VolumeGrid<int>>(value);
  }
  if (type.is<VolumeGrid<float3>>()) {
    return &SocketValueVariant::init_default<VolumeGrid<float3>>(value);
  }
  if (type.is<VolumeGrid<bool>>()) {
    return &SocketValueVariant::init_default<VolumeGrid<bool>>(value);
  }
#endif
  return nullptr;
}

void *SocketValueVariant::allocate(const CPPType &type, detail::SocketValueVariantAny &value)
{
#define X(TYPE) \
  if (type.is<TYPE>()) { \
    return value.allocate<TYPE>(); \
  }
  SOCKET_VALUE_SINGLE_TYPES
#undef X

  if (type.is<GField>()) {
    return value.allocate<GField>();
  }
  if (type.is<GListPtr>()) {
    return value.allocate<GListPtr>();
  }
#ifdef WITH_OPENVDB
  if (type.is<GVolumeGrid>()) {
    return value.allocate<GVolumeGrid>();
  }
#endif
  BLI_assert_unreachable();
  return nullptr;
}

void *SocketValueVariant::allocate_single(const CPPType &type)
{
  return SocketValueVariant::allocate(type, value_);
}

bool SocketValueVariant::is_single() const
{
  const CPPType *type = this->get().type();
  if (!type) {
    return false;
  }
#define X(TYPE) \
  if (type->is<TYPE>()) { \
    return true; \
  }
  SOCKET_VALUE_SINGLE_TYPES
#undef X
  return false;
}

bool SocketValueVariant::is_field() const
{
  return this->get().type()->is<fn::GField>();
}

bool SocketValueVariant::is_list() const
{
  return this->get().type()->is<nodes::GListPtr>();
}
bool SocketValueVariant::is_volume_grid() const
{
#ifdef WITH_OPENVDB
  return this->get().type()->is<bke::GVolumeGrid>();
#else
  return false;
#endif
}

bool SocketValueVariant::is_context_dependent_field() const
{
  const fn::GField *field = this->get_if<fn::GField>();
  if (!field) {
    return false;
  }
  return field->depends_on_input();
}

void SocketValueVariant::ensure_owns_direct_data()
{
  if (this->owns_direct_data()) {
    return;
  }
  if (this->get().is_type<nodes::BundlePtr>()) {
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
  if (this->get().is_type<GeometrySet>()) {
    GeometrySet &geometry = value_.get<GeometrySet>();
    geometry.ensure_owns_direct_data();
  }
  BLI_assert(this->owns_direct_data());
}

bool SocketValueVariant::owns_direct_data() const
{
  if (this->get().is_type<nodes::BundlePtr>()) {
    if (const nodes::BundlePtr &bundle_ptr = value_.get<nodes::BundlePtr>()) {
      return bundle_ptr->owns_direct_data();
    }
  }
  if (this->is_list()) {
    if (const nodes::GListPtr &list_ptr = value_.get<nodes::GListPtr>()) {
      return list_ptr->owns_direct_data();
    }
  }
  if (this->get().is_type<GeometrySet>()) {
    const GeometrySet &geometry = value_.get<GeometrySet>();
    return geometry.owns_direct_data();
  }
  return true;
}

void SocketValueVariant::count_memory(MemoryCounter &memory) const
{
  if (!value_.has_value()) {
    return;
  }
  const GPointer value = this->get();
  const CPPType &cpp_type = *value.type();
  memory.add(cpp_type.size);
  if (cpp_type.is<GeometrySet>()) {
    value.get<GeometrySet>()->count_memory(memory);
  }
  else if (cpp_type.is<nodes::BundlePtr>()) {
    if (const nodes::BundlePtr &bundle_ptr = *value.get<nodes::BundlePtr>()) {
      bundle_ptr->count_memory(memory);
    }
  }
  else if (cpp_type.is<nodes::GListPtr>()) {
    if (const nodes::GListPtr &list = *value.get<nodes::GListPtr>()) {
      list->count_memory(memory);
    }
  }
#ifdef WITH_OPENVDB
  else if (cpp_type.is<GVolumeGrid>()) {
    if (const GVolumeGrid &grid = *value.get<GVolumeGrid>()) {
      grid->count_memory(memory);
    }
  }
#endif
}

}  // namespace blender::bke
