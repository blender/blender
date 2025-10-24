/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <utility>

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_geometry_set.hh"
#include "BKE_type_conversions.hh"

#include "DNA_ID.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_array_utils.hh"
#include "BLI_color.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "BLT_translation.hh"

#include "FN_field.hh"

#include "attribute_access_intern.hh"

#ifndef NDEBUG
#  include <iostream>
#endif

namespace blender::bke {

const CPPType &attribute_type_to_cpp_type(const AttrType type)
{
  switch (type) {
    case AttrType::Bool:
      return CPPType::get<bool>();
    case AttrType::Int8:
      return CPPType::get<int8_t>();
    case AttrType::Int16_2D:
      return CPPType::get<short2>();
    case AttrType::Int32:
      return CPPType::get<int>();
    case AttrType::Int32_2D:
      return CPPType::get<int2>();
    case AttrType::Float:
      return CPPType::get<float>();
    case AttrType::Float2:
      return CPPType::get<float2>();
    case AttrType::Float3:
      return CPPType::get<float3>();
    case AttrType::Float4x4:
      return CPPType::get<float4x4>();
    case AttrType::ColorByte:
      return CPPType::get<ColorGeometry4b>();
    case AttrType::ColorFloat:
      return CPPType::get<ColorGeometry4f>();
    case AttrType::Quaternion:
      return CPPType::get<math::Quaternion>();
    case AttrType::String:
      return CPPType::get<MStringProperty>();
  }
  BLI_assert_unreachable();
  return CPPType::get<bool>();
}

AttrType cpp_type_to_attribute_type(const CPPType &type)
{
  if (type.is<float>()) {
    return AttrType::Float;
  }
  if (type.is<float2>()) {
    return AttrType::Float2;
  }
  if (type.is<float3>()) {
    return AttrType::Float3;
  }
  if (type.is<int>()) {
    return AttrType::Int32;
  }
  if (type.is<int2>()) {
    return AttrType::Int32_2D;
  }
  if (type.is<ColorGeometry4f>()) {
    return AttrType::ColorFloat;
  }
  if (type.is<bool>()) {
    return AttrType::Bool;
  }
  if (type.is<int8_t>()) {
    return AttrType::Int8;
  }
  if (type.is<ColorGeometry4b>()) {
    return AttrType::ColorByte;
  }
  if (type.is<math::Quaternion>()) {
    return AttrType::Quaternion;
  }
  if (type.is<float4x4>()) {
    return AttrType::Float4x4;
  }
  if (type.is<short2>()) {
    return AttrType::Int16_2D;
  }
  if (type.is<MStringProperty>()) {
    return AttrType::String;
  }
  BLI_assert_unreachable();
  return AttrType::Bool;
}

const blender::CPPType *custom_data_type_to_cpp_type(const eCustomDataType type)
{
  switch (type) {
    case CD_PROP_FLOAT:
      return &CPPType::get<float>();
    case CD_PROP_FLOAT2:
      return &CPPType::get<float2>();
    case CD_PROP_FLOAT3:
      return &CPPType::get<float3>();
    case CD_PROP_INT32:
      return &CPPType::get<int>();
    case CD_PROP_INT32_2D:
      return &CPPType::get<int2>();
    case CD_PROP_COLOR:
      return &CPPType::get<ColorGeometry4f>();
    case CD_PROP_BOOL:
      return &CPPType::get<bool>();
    case CD_PROP_INT8:
      return &CPPType::get<int8_t>();
    case CD_PROP_BYTE_COLOR:
      return &CPPType::get<ColorGeometry4b>();
    case CD_PROP_QUATERNION:
      return &CPPType::get<math::Quaternion>();
    case CD_PROP_FLOAT4X4:
      return &CPPType::get<float4x4>();
    case CD_PROP_INT16_2D:
      return &CPPType::get<short2>();
    case CD_PROP_STRING:
      return &CPPType::get<MStringProperty>();
    default:
      return nullptr;
  }
}

eCustomDataType cpp_type_to_custom_data_type(const blender::CPPType &type)
{
  if (type.is<float>()) {
    return CD_PROP_FLOAT;
  }
  if (type.is<float2>()) {
    return CD_PROP_FLOAT2;
  }
  if (type.is<float3>()) {
    return CD_PROP_FLOAT3;
  }
  if (type.is<int>()) {
    return CD_PROP_INT32;
  }
  if (type.is<int2>()) {
    return CD_PROP_INT32_2D;
  }
  if (type.is<ColorGeometry4f>()) {
    return CD_PROP_COLOR;
  }
  if (type.is<bool>()) {
    return CD_PROP_BOOL;
  }
  if (type.is<int8_t>()) {
    return CD_PROP_INT8;
  }
  if (type.is<ColorGeometry4b>()) {
    return CD_PROP_BYTE_COLOR;
  }
  if (type.is<math::Quaternion>()) {
    return CD_PROP_QUATERNION;
  }
  if (type.is<float4x4>()) {
    return CD_PROP_FLOAT4X4;
  }
  if (type.is<short2>()) {
    return CD_PROP_INT16_2D;
  }
  if (type.is<MStringProperty>()) {
    return CD_PROP_STRING;
  }
  BLI_assert_unreachable();
  return CD_PROP_FLOAT;
}

const char *no_procedural_access_message = N_(
    "This attribute cannot be accessed in a procedural context");

bool allow_procedural_attribute_access(StringRef attribute_name)
{
  if (attribute_name.startswith(".corner")) {
    return false;
  }
  if (attribute_name.startswith(".edge")) {
    return false;
  }
  if (attribute_name.startswith(".select")) {
    return false;
  }
  if (attribute_name.startswith(".sculpt")) {
    return false;
  }
  if (attribute_name.startswith(".hide")) {
    return false;
  }
  if (attribute_name.startswith(".uv")) {
    return false;
  }
  if (attribute_name == ".reference_index") {
    return false;
  }
  if (attribute_name.startswith("." UV_PINNED_NAME ".")) {
    return false;
  }
  return true;
}

static int attribute_data_type_complexity(const AttrType data_type)
{
  switch (data_type) {
    case AttrType::Bool:
      return 0;
    case AttrType::Int8:
      return 1;
    case AttrType::Int32:
      return 2;
    case AttrType::Float:
      return 3;
    case AttrType::Int16_2D:
      return 4;
    case AttrType::Int32_2D:
      return 5;
    case AttrType::Float2:
      return 6;
    case AttrType::Float3:
      return 7;
    case AttrType::ColorByte:
      return 8;
    case AttrType::Quaternion:
      return 9;
    case AttrType::ColorFloat:
      return 10;
    case AttrType::Float4x4:
      return 11;
#if 0 /* These attribute types are not supported yet. */
    case AttrType::String:
      return 12;
#endif
    default:
      /* Only accept "generic" custom data types used by the attribute system. */
      BLI_assert_unreachable();
      return 0;
  }
}

AttrType attribute_data_type_highest_complexity(Span<AttrType> data_types)
{
  int highest_complexity = INT_MIN;
  AttrType most_complex_type = AttrType::ColorFloat;

  for (const AttrType data_type : data_types) {
    const int complexity = attribute_data_type_complexity(data_type);
    if (complexity > highest_complexity) {
      highest_complexity = complexity;
      most_complex_type = data_type;
    }
  }

  return most_complex_type;
}

/**
 * \note Generally the order should mirror the order of the domains
 * established in each component's GeometryAttributeProviders.
 */
static int attribute_domain_priority(const AttrDomain domain)
{
  switch (domain) {
    case AttrDomain::Instance:
      return 0;
    case AttrDomain::Layer:
      return 1;
    case AttrDomain::Curve:
      return 2;
    case AttrDomain::Face:
      return 3;
    case AttrDomain::Edge:
      return 4;
    case AttrDomain::Point:
      return 5;
    case AttrDomain::Corner:
      return 6;
    default:
      /* Domain not supported in nodes yet. */
      BLI_assert_unreachable();
      return 0;
  }
}

AttrDomain attribute_domain_highest_priority(Span<AttrDomain> domains)
{
  int highest_priority = INT_MIN;
  AttrDomain highest_priority_domain = AttrDomain::Corner;

  for (const AttrDomain domain : domains) {
    const int priority = attribute_domain_priority(domain);
    if (priority > highest_priority) {
      highest_priority = priority;
      highest_priority_domain = domain;
    }
  }

  return highest_priority_domain;
}

static void *add_generic_custom_data_layer(CustomData &custom_data,
                                           const eCustomDataType data_type,
                                           const eCDAllocType alloctype,
                                           const int domain_size,
                                           const StringRef attribute_id)
{
  return CustomData_add_layer_named(&custom_data, data_type, alloctype, domain_size, attribute_id);
}

static const void *add_generic_custom_data_layer_with_existing_data(
    CustomData &custom_data,
    const eCustomDataType data_type,
    const StringRef attribute_id,
    const int domain_size,
    void *layer_data,
    const ImplicitSharingInfo *sharing_info)
{
  return CustomData_add_layer_named_with_data(
      &custom_data, data_type, layer_data, domain_size, attribute_id, sharing_info);
}

static bool add_custom_data_layer_from_attribute_init(const StringRef attribute_id,
                                                      CustomData &custom_data,
                                                      const eCustomDataType data_type,
                                                      const int domain_num,
                                                      const AttributeInit &initializer,
                                                      const GPointer custom_default_value_ptr)
{
  const int old_layer_num = custom_data.totlayer;
  switch (initializer.type) {
    case AttributeInit::Type::Construct: {
      add_generic_custom_data_layer(
          custom_data, data_type, CD_CONSTRUCT, domain_num, attribute_id);
      break;
    }
    case AttributeInit::Type::DefaultValue: {
      if (const void *default_value = custom_default_value_ptr.get()) {
        const CPPType &type = *custom_default_value_ptr.type();
        void *data = add_generic_custom_data_layer(
            custom_data, data_type, CD_CONSTRUCT, domain_num, attribute_id);
        type.fill_assign_n(default_value, data, domain_num);
      }
      else {
        add_generic_custom_data_layer(
            custom_data, data_type, CD_SET_DEFAULT, domain_num, attribute_id);
      }
      break;
    }
    case AttributeInit::Type::VArray: {
      void *data = add_generic_custom_data_layer(
          custom_data, data_type, CD_CONSTRUCT, domain_num, attribute_id);
      if (data != nullptr) {
        const GVArray &varray = static_cast<const AttributeInitVArray &>(initializer).varray;
        varray.materialize_to_uninitialized(varray.index_range(), data);
      }
      break;
    }
    case AttributeInit::Type::MoveArray: {
      void *data = static_cast<const AttributeInitMoveArray &>(initializer).data;
      add_generic_custom_data_layer_with_existing_data(
          custom_data, data_type, attribute_id, domain_num, data, nullptr);
      break;
    }
    case AttributeInit::Type::Shared: {
      const AttributeInitShared &init = static_cast<const AttributeInitShared &>(initializer);
      add_generic_custom_data_layer_with_existing_data(custom_data,
                                                       data_type,
                                                       attribute_id,
                                                       domain_num,
                                                       const_cast<void *>(init.data),
                                                       init.sharing_info);
      break;
    }
  }
  return old_layer_num < custom_data.totlayer;
}

bool BuiltinCustomDataLayerProvider::layer_exists(const CustomData &custom_data) const
{
  return CustomData_get_named_layer_index(&custom_data, data_type_, name_) != -1;
}

GAttributeReader BuiltinCustomDataLayerProvider::try_get_for_read(const void *owner) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
  if (custom_data == nullptr) {
    return {};
  }

  /* When the number of elements is zero, layers might have null data but still exist. */
  const CPPType &type = *custom_data_type_to_cpp_type(data_type_);
  const int element_num = custom_data_access_.get_element_num(owner);
  if (element_num == 0) {
    if (this->layer_exists(*custom_data)) {
      return {GVArray::from_span({type, nullptr, 0}), domain_, nullptr};
    }
    return {};
  }

  const int index = CustomData_get_named_layer_index(custom_data, data_type_, name_);
  if (index == -1) {
    return {};
  }
  const CustomDataLayer &layer = custom_data->layers[index];
  return {GVArray::from_span({type, layer.data, element_num}), domain_, layer.sharing_info};
}

GAttributeWriter BuiltinCustomDataLayerProvider::try_get_for_write(void *owner) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return {};
  }

  std::function<void()> tag_modified_fn;
  if (update_on_change_ != nullptr) {
    tag_modified_fn = [owner, update = update_on_change_]() { update(owner); };
  }

  /* When the number of elements is zero, layers might have null data but still exist. */
  const CPPType &type = *custom_data_type_to_cpp_type(data_type_);
  const int element_num = custom_data_access_.get_element_num(owner);
  if (element_num == 0) {
    if (this->layer_exists(*custom_data)) {
      return {GVMutableArray::from_span({type, nullptr, 0}), domain_, std::move(tag_modified_fn)};
    }
    return {};
  }

  void *data = CustomData_get_layer_named_for_write(custom_data, data_type_, name_, element_num);
  if (data == nullptr) {
    return {};
  }
  return {
      GVMutableArray::from_span({type, data, element_num}), domain_, std::move(tag_modified_fn)};
}

bool BuiltinCustomDataLayerProvider::try_delete(void *owner) const
{
  if (deletable_ != Deletable) {
    return false;
  }
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return {};
  }

  if (CustomData_free_layer_named(custom_data, name_)) {
    if (update_on_change_ != nullptr) {
      update_on_change_(owner);
    }
    return true;
  }
  return false;
}

bool BuiltinCustomDataLayerProvider::try_create(void *owner,
                                                const AttributeInit &initializer) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return false;
  }

  const int element_num = custom_data_access_.get_element_num(owner);
  if (CustomData_has_layer_named(custom_data, data_type_, name_)) {
    /* Exists already. */
    return false;
  }
  if (add_custom_data_layer_from_attribute_init(
          name_, *custom_data, data_type_, element_num, initializer, default_value_))
  {
    if (initializer.type != AttributeInit::Type::Construct) {
      /* Avoid calling update function when values are not default-initialized. Without default
       * initialization or otherwise meaningful initial values, they should be set elsewhere
       * anyway, which will cause a separate update tag. */
      if (update_on_change_ != nullptr) {
        update_on_change_(owner);
      }
    }
    return true;
  }
  return false;
}

bool BuiltinCustomDataLayerProvider::exists(const void *owner) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
  if (custom_data == nullptr) {
    return false;
  }
  return CustomData_has_layer_named(custom_data, data_type_, name_);
}

GAttributeReader CustomDataAttributeProvider::try_get_for_read(const void *owner,
                                                               const StringRef attribute_id) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
  if (custom_data == nullptr) {
    return {};
  }
  const int element_num = custom_data_access_.get_element_num(owner);
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    if (layer.name != attribute_id) {
      continue;
    }
    const CPPType *type = custom_data_type_to_cpp_type(eCustomDataType(layer.type));
    if (type == nullptr) {
      continue;
    }
    GSpan data{*type, layer.data, element_num};
    return {GVArray::from_span(data), domain_, layer.sharing_info};
  }
  return {};
}

GAttributeWriter CustomDataAttributeProvider::try_get_for_write(void *owner,
                                                                const StringRef attribute_id) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return {};
  }
  const int element_num = custom_data_access_.get_element_num(owner);
  for (CustomDataLayer &layer : MutableSpan(custom_data->layers, custom_data->totlayer)) {
    if (layer.name != attribute_id) {
      continue;
    }
    CustomData_get_layer_named_for_write(
        custom_data, eCustomDataType(layer.type), layer.name, element_num);

    const CPPType *type = custom_data_type_to_cpp_type(eCustomDataType(layer.type));
    if (type == nullptr) {
      continue;
    }
    std::function<void()> tag_modified_fn;
    if (custom_data_access_.get_tag_modified_function != nullptr) {
      tag_modified_fn = custom_data_access_.get_tag_modified_function(owner, attribute_id);
    }
    GMutableSpan data{*type, layer.data, element_num};
    return {GVMutableArray::from_span(data), domain_, tag_modified_fn};
  }
  return {};
}

bool CustomDataAttributeProvider::try_delete(void *owner, const StringRef attribute_id) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return false;
  }
  for (const int i : IndexRange(custom_data->totlayer)) {
    const CustomDataLayer &layer = custom_data->layers[i];
    if (this->type_is_supported(eCustomDataType(layer.type)) && layer.name == attribute_id) {
      CustomData_free_layer(custom_data, eCustomDataType(layer.type), i);
      if (custom_data_access_.get_tag_modified_function != nullptr) {
        if (const std::function<void()> fn = custom_data_access_.get_tag_modified_function(
                owner, attribute_id))
        {
          fn();
        }
      }
      return true;
    }
  }
  return false;
}

bool CustomDataAttributeProvider::try_create(void *owner,
                                             const StringRef attribute_id,
                                             const AttrDomain domain,
                                             const eCustomDataType data_type,
                                             const AttributeInit &initializer) const
{
  if (domain_ != domain) {
    return false;
  }
  if (!this->type_is_supported(data_type)) {
    return false;
  }
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return false;
  }
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    if (layer.name == attribute_id) {
      return false;
    }
  }
  const int element_num = custom_data_access_.get_element_num(owner);
  add_custom_data_layer_from_attribute_init(
      attribute_id, *custom_data, data_type, element_num, initializer, {});
  if (initializer.type != AttributeInit::Type::Construct) {
    /* Avoid calling update function when values are not default-initialized. Without default
     * initialization or otherwise meaningful initial values, they should be set elsewhere
     * anyway, which will cause a separate update tag. */
    if (custom_data_access_.get_tag_modified_function != nullptr) {
      if (const std::function<void()> fn = custom_data_access_.get_tag_modified_function(
              owner, attribute_id))
      {
        fn();
      }
    }
  }
  return true;
}

bool CustomDataAttributeProvider::foreach_attribute(
    const void *owner, const FunctionRef<void(const AttributeIter &)> fn) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
  if (custom_data == nullptr) {
    return true;
  }
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    const eCustomDataType cd_type = eCustomDataType(layer.type);
    if (this->type_is_supported(cd_type)) {
      const auto get_fn = [&]() {
        const CPPType *type = custom_data_type_to_cpp_type(cd_type);
        BLI_assert(type);
        GSpan data{*type, layer.data, custom_data_access_.get_element_num(owner)};
        return GAttributeReader{GVArray::from_span(data), domain_, layer.sharing_info};
      };

      const AttrType data_type = *custom_data_type_to_attr_type(cd_type);
      AttributeIter iter{layer.name, domain_, data_type, get_fn};
      fn(iter);
      if (iter.is_stopped()) {
        return false;
      }
    }
  }
  return true;
}

/* -------------------------------------------------------------------- */
/** \name Attribute API
 * \{ */

static GVArray try_adapt_data_type(GVArray varray, const CPPType &to_type)
{
  const DataTypeConversions &conversions = get_implicit_type_conversions();
  return conversions.try_convert(std::move(varray), to_type);
}

std::optional<AttributeAccessor> AttributeAccessor::from_id(const ID &id)
{
  switch (GS(id.name)) {
    case ID_ME:
      return reinterpret_cast<const Mesh &>(id).attributes();
    case ID_PT:
      return reinterpret_cast<const PointCloud &>(id).attributes();
    case ID_CV:
      return reinterpret_cast<const Curves &>(id).geometry.wrap().attributes();
    case ID_GP:
      return reinterpret_cast<const GreasePencil &>(id).attributes();
    default:
      return {};
  }
  return {};
}

static GAttributeReader adapt_domain_and_type_if_necessary(GAttributeReader attribute,
                                                           const std::optional<AttrDomain> domain,
                                                           const std::optional<AttrType> data_type,
                                                           const AttributeAccessor &accessor)
{
  if (!attribute) {
    return {};
  }
  if (domain.has_value()) {
    if (attribute.domain != domain) {
      attribute.varray = accessor.adapt_domain(attribute.varray, attribute.domain, *domain);
      attribute.domain = *domain;
      attribute.sharing_info = nullptr;
      if (!attribute.varray) {
        return {};
      }
    }
  }
  if (data_type.has_value()) {
    const CPPType &type = attribute_type_to_cpp_type(*data_type);
    if (attribute.varray.type() != type) {
      attribute.varray = try_adapt_data_type(std::move(attribute.varray), type);
      attribute.sharing_info = nullptr;
      if (!attribute.varray) {
        return {};
      }
    }
  }
  return attribute;
}

GAttributeReader AttributeAccessor::lookup(const StringRef attribute_id,
                                           const std::optional<AttrDomain> domain,
                                           const std::optional<AttrType> data_type) const
{
  return adapt_domain_and_type_if_necessary(this->lookup(attribute_id), domain, data_type, *this);
}

GAttributeReader AttributeIter::get(std::optional<AttrDomain> domain,
                                    std::optional<AttrType> data_type) const
{
  BLI_assert(this->accessor != nullptr);
  return adapt_domain_and_type_if_necessary(this->get(), domain, data_type, *accessor);
}

GAttributeReader AttributeAccessor::lookup_or_default(const StringRef attribute_id,
                                                      const AttrDomain domain,
                                                      const AttrType data_type,
                                                      const void *default_value) const
{
  GAttributeReader attribute = this->lookup(attribute_id, domain, data_type);
  if (attribute) {
    return attribute;
  }
  const CPPType &type = attribute_type_to_cpp_type(data_type);
  const int64_t domain_size = this->domain_size(domain);
  if (default_value == nullptr) {
    return {GVArray::from_single_ref(type, domain_size, type.default_value()), domain, nullptr};
  }
  return {GVArray::from_single(type, domain_size, default_value), domain, nullptr};
}

bool AttributeAccessor::contains(const StringRef attribute_id) const
{
  bool found = false;
  this->foreach_attribute([&](const AttributeIter &iter) {
    if (attribute_id == iter.name) {
      found = true;
      iter.stop();
    }
  });
  return found;
}

std::optional<AttributeMetaData> AttributeAccessor::lookup_meta_data(
    const StringRef attribute_id) const
{
  std::optional<AttributeMetaData> meta_data;
  this->foreach_attribute([&](const AttributeIter &iter) {
    if (attribute_id == iter.name) {
      meta_data = AttributeMetaData{iter.domain, iter.data_type};
      iter.stop();
    }
  });
  return meta_data;
}

Set<StringRefNull> AttributeAccessor::all_ids() const
{
  Set<StringRefNull> ids;
  this->foreach_attribute([&](const AttributeIter &iter) { ids.add(iter.name); });
  return ids;
}

void MutableAttributeAccessor::remove_anonymous()
{
  Vector<std::string> anonymous_ids;
  for (const StringRef id : this->all_ids()) {
    if (attribute_name_is_anonymous(id)) {
      anonymous_ids.append(id);
    }
  }

  while (!anonymous_ids.is_empty()) {
    this->remove(anonymous_ids.pop_last());
  }
}

/**
 * Debug utility that checks whether the #finish function of an #AttributeWriter has been called.
 */
#ifndef NDEBUG
struct FinishCallChecker {
  std::string name;
  bool finish_called = false;
  std::function<void()> real_finish_fn;

  ~FinishCallChecker()
  {
    if (!this->finish_called) {
      std::cerr << "Forgot to call `finish()` for '" << this->name << "'.\n";
    }
  }
};
#endif

GAttributeWriter MutableAttributeAccessor::lookup_for_write(const StringRef attribute_id)
{
  GAttributeWriter attribute = fn_->lookup_for_write(owner_, attribute_id);
  /* Check that the #finish method is called in debug builds. */
#ifndef NDEBUG
  if (attribute) {
    auto checker = std::make_shared<FinishCallChecker>();
    checker->name = attribute_id;
    checker->real_finish_fn = attribute.tag_modified_fn;
    attribute.tag_modified_fn = [checker]() {
      if (checker->real_finish_fn) {
        checker->real_finish_fn();
      }
      checker->finish_called = true;
    };
  }
#endif
  return attribute;
}

GSpanAttributeWriter MutableAttributeAccessor::lookup_for_write_span(const StringRef attribute_id)
{
  GAttributeWriter attribute = this->lookup_for_write(attribute_id);
  if (attribute) {
    return GSpanAttributeWriter{std::move(attribute), true};
  }
  return {};
}

GAttributeWriter MutableAttributeAccessor::lookup_or_add_for_write(
    const StringRef attribute_id,
    const AttrDomain domain,
    const AttrType data_type,
    const AttributeInit &initializer)
{
  std::optional<AttributeMetaData> meta_data = this->lookup_meta_data(attribute_id);
  if (meta_data.has_value()) {
    if (meta_data->domain == domain && meta_data->data_type == data_type) {
      return this->lookup_for_write(attribute_id);
    }
    return {};
  }
  if (this->add(attribute_id, domain, data_type, initializer)) {
    return this->lookup_for_write(attribute_id);
  }
  return {};
}

GSpanAttributeWriter MutableAttributeAccessor::lookup_or_add_for_write_span(
    const StringRef attribute_id,
    const AttrDomain domain,
    const AttrType data_type,
    const AttributeInit &initializer)
{
  GAttributeWriter attribute = this->lookup_or_add_for_write(
      attribute_id, domain, data_type, initializer);
  if (attribute) {
    return GSpanAttributeWriter{std::move(attribute), true};
  }
  return {};
}

GSpanAttributeWriter MutableAttributeAccessor::lookup_or_add_for_write_only_span(
    const StringRef attribute_id, const AttrDomain domain, const AttrType data_type)
{
  GAttributeWriter attribute = this->lookup_or_add_for_write(
      attribute_id, domain, data_type, AttributeInitConstruct());
  if (attribute) {
    return GSpanAttributeWriter{std::move(attribute), false};
  }
  return {};
}

bool MutableAttributeAccessor::rename(const StringRef old_attribute_id,
                                      const StringRef new_attribute_id)
{
  if (old_attribute_id == new_attribute_id) {
    return true;
  }
  if (this->contains(new_attribute_id)) {
    return false;
  }
  const GAttributeReader old_attribute = this->lookup(old_attribute_id);
  if (!old_attribute) {
    return false;
  }
  const AttrType type = cpp_type_to_attribute_type(old_attribute.varray.type());
  if (old_attribute.sharing_info != nullptr && old_attribute.varray.is_span()) {
    if (!this->add(new_attribute_id,
                   old_attribute.domain,
                   type,
                   AttributeInitShared{old_attribute.varray.get_internal_span().data(),
                                       *old_attribute.sharing_info}))
    {
      return false;
    }
  }
  else {
    if (!this->add(new_attribute_id,
                   old_attribute.domain,
                   type,
                   AttributeInitVArray{old_attribute.varray}))
    {
      return false;
    }
  }
  this->remove(old_attribute_id);
  return true;
}

fn::GField AttributeValidator::validate_field_if_necessary(const fn::GField &field) const
{
  if (function) {
    auto validate_op = fn::FieldOperation::from(*function, {field});
    return fn::GField(validate_op);
  }
  return field;
}

Vector<AttributeTransferData> retrieve_attributes_for_transfer(
    const AttributeAccessor src_attributes,
    MutableAttributeAccessor dst_attributes,
    Span<AttrDomain> domains,
    const bke::AttributeFilter &attribute_filter)
{
  Vector<AttributeTransferData> attributes;
  src_attributes.foreach_attribute([&](const AttributeIter &iter) {
    if (!domains.contains(iter.domain)) {
      return;
    }
    if (iter.data_type == AttrType::String) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    GVArray src = *iter.get();
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, iter.domain, iter.data_type);
    /* Skip unsupported attributes. */
    if (!dst) {
      return;
    }
    attributes.append({std::move(src), iter.name, {iter.domain, iter.data_type}, std::move(dst)});
  });
  return attributes;
}

/** \} */

void gather_attributes(const AttributeAccessor src_attributes,
                       const AttrDomain src_domain,
                       const AttrDomain dst_domain,
                       const AttributeFilter &attribute_filter,
                       const IndexMask &selection,
                       MutableAttributeAccessor dst_attributes)
{
  const int src_size = src_attributes.domain_size(src_domain);
  src_attributes.foreach_attribute([&](const AttributeIter &iter) {
    if (iter.domain != src_domain) {
      return;
    }
    if (iter.data_type == AttrType::String) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    const GAttributeReader src = iter.get(src_domain);
    if (selection.size() == src_size && src.sharing_info && src.varray.is_span()) {
      const AttributeInitShared init(src.varray.get_internal_span().data(), *src.sharing_info);
      if (dst_attributes.add(iter.name, dst_domain, iter.data_type, init)) {
        return;
      }
    }
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, dst_domain, iter.data_type);
    if (!dst) {
      return;
    }
    array_utils::gather(src.varray, selection, dst.span);
    dst.finish();
  });
}

void gather_attributes(const AttributeAccessor src_attributes,
                       const AttrDomain src_domain,
                       const AttrDomain dst_domain,
                       const AttributeFilter &attribute_filter,
                       const Span<int> indices,
                       MutableAttributeAccessor dst_attributes)
{
  if (array_utils::indices_are_range(indices, IndexRange(src_attributes.domain_size(src_domain))))
  {
    copy_attributes(src_attributes, src_domain, dst_domain, attribute_filter, dst_attributes);
  }
  else {
    src_attributes.foreach_attribute([&](const AttributeIter &iter) {
      if (iter.domain != src_domain) {
        return;
      }
      if (iter.data_type == AttrType::String) {
        return;
      }
      if (attribute_filter.allow_skip(iter.name)) {
        return;
      }
      const GAttributeReader src = iter.get(src_domain);
      GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
          iter.name, dst_domain, iter.data_type);
      if (!dst) {
        return;
      }
      attribute_math::gather(src.varray, indices, dst.span);
      dst.finish();
    });
  }
}

void gather_attributes_group_to_group(const AttributeAccessor src_attributes,
                                      const AttrDomain src_domain,
                                      const AttrDomain dst_domain,
                                      const AttributeFilter &attribute_filter,
                                      const OffsetIndices<int> src_offsets,
                                      const OffsetIndices<int> dst_offsets,
                                      const IndexMask &selection,
                                      MutableAttributeAccessor dst_attributes)
{
  if (selection.size() == src_offsets.size()) {
    if (src_attributes.domain_size(src_domain) == dst_attributes.domain_size(src_domain)) {
      /* When all groups are selected and the domains are the same size, all values are copied,
       * because corresponding groups are required to be the same size. */
      copy_attributes(src_attributes, src_domain, dst_domain, attribute_filter, dst_attributes);
      return;
    }
  }
  src_attributes.foreach_attribute([&](const AttributeIter &iter) {
    if (iter.domain != src_domain) {
      return;
    }
    if (iter.data_type == AttrType::String) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    const GVArraySpan src = *iter.get(src_domain);
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, dst_domain, iter.data_type);
    if (!dst) {
      return;
    }
    attribute_math::gather_group_to_group(src_offsets, dst_offsets, selection, src, dst.span);
    dst.finish();
  });
}

void gather_attributes_to_groups(const AttributeAccessor src_attributes,
                                 const AttrDomain src_domain,
                                 const AttrDomain dst_domain,
                                 const AttributeFilter &attribute_filter,
                                 const OffsetIndices<int> dst_offsets,
                                 const IndexMask &src_selection,
                                 MutableAttributeAccessor dst_attributes)
{
  src_attributes.foreach_attribute([&](const AttributeIter &iter) {
    if (iter.domain != src_domain) {
      return;
    }
    if (iter.data_type == AttrType::String) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    const GVArraySpan src = *iter.get(src_domain);
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, dst_domain, iter.data_type);
    if (!dst) {
      return;
    }
    attribute_math::gather_to_groups(dst_offsets, src_selection, src, dst.span);
    dst.finish();
  });
}

void copy_attributes(const AttributeAccessor src_attributes,
                     const AttrDomain src_domain,
                     const AttrDomain dst_domain,
                     const AttributeFilter &attribute_filter,
                     MutableAttributeAccessor dst_attributes)
{
  BLI_assert(src_attributes.domain_size(src_domain) == dst_attributes.domain_size(dst_domain));
  gather_attributes(src_attributes,
                    src_domain,
                    dst_domain,
                    attribute_filter,
                    IndexMask(src_attributes.domain_size(src_domain)),
                    dst_attributes);
}

void copy_attributes_group_to_group(const AttributeAccessor src_attributes,
                                    const AttrDomain src_domain,
                                    const AttrDomain dst_domain,
                                    const AttributeFilter &attribute_filter,
                                    const OffsetIndices<int> src_offsets,
                                    const OffsetIndices<int> dst_offsets,
                                    const IndexMask &selection,
                                    MutableAttributeAccessor dst_attributes)
{
  if (selection.is_empty()) {
    return;
  }
  src_attributes.foreach_attribute([&](const AttributeIter &iter) {
    if (iter.domain != src_domain) {
      return;
    }
    if (iter.data_type == AttrType::String) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    const GVArraySpan src = *iter.get(src_domain);
    const bool dst_already_exists = dst_attributes.contains(iter.name);
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, dst_domain, iter.data_type);
    if (!dst) {
      return;
    }
    if (!dst_already_exists) {
      /* Skip filling with the default value if all of the data is going to be filled. */
      if (!(dst_offsets.total_size() == dst.span.size() && selection.size() == dst_offsets.size()))
      {
        const CPPType &type = dst.span.type();
        if (dst_attributes.is_builtin(iter.name)) {
          if (const GPointer value = dst_attributes.get_builtin_default(iter.name)) {
            type.fill_construct_n(value.get(), dst.span.data(), dst.span.size());
          }
          else {
            type.fill_construct_n(type.default_value(), dst.span.data(), dst.span.size());
          }
        }
        else {
          type.fill_construct_n(type.default_value(), dst.span.data(), dst.span.size());
        }
      }
    }
    array_utils::copy_group_to_group(src_offsets, dst_offsets, selection, src, dst.span);
    dst.finish();
  });
}

void fill_attribute_range_default(MutableAttributeAccessor attributes,
                                  const AttrDomain domain,
                                  const AttributeFilter &attribute_filter,
                                  const IndexRange range)
{
  /* While it is valid to call this function for any valid range which can be placed in target
   * domain, it is computationally costly to perform this loop. This check is COW elision and not
   * just loop skip. */
  if (range.is_empty()) {
    return;
  }

  attributes.foreach_attribute([&](const AttributeIter &iter) {
    if (iter.domain != domain) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    if (iter.data_type == AttrType::String) {
      return;
    }
    GSpanAttributeWriter attribute = attributes.lookup_for_write_span(iter.name);
    const CPPType &type = attribute.span.type();
    GMutableSpan data = attribute.span.slice(range);
    if (attributes.is_builtin(iter.name)) {
      if (const GPointer value = attributes.get_builtin_default(iter.name)) {
        type.fill_assign_n(value.get(), data.data(), data.size());
      }
      else {
        type.fill_assign_n(type.default_value(), data.data(), data.size());
      }
    }
    else {
      type.fill_assign_n(type.default_value(), data.data(), data.size());
    }
    attribute.finish();
  });
}

void transform_custom_normal_attribute(const float4x4 &transform,
                                       MutableAttributeAccessor &attributes)
{
  const GAttributeReader normals = attributes.lookup("custom_normal");
  if (!normals) {
    return;
  }
  if (!normals.varray.type().is<float3>()) {
    return;
  }
  if (normals.sharing_info->is_mutable()) {
    SpanAttributeWriter<float3> normals = attributes.lookup_for_write_span<float3>(
        "custom_normal");
    math::transform_normals(float3x3(transform), normals.span);
    normals.finish();
  }
  else {
    /* It's a bit faster to combine transforming and copying the attribute if it's shared. */
    float3 *new_data = MEM_malloc_arrayN<float3>(size_t(normals.varray.size()), __func__);
    math::transform_normals(VArraySpan(normals.varray.typed<float3>()),
                            float3x3(transform),
                            {new_data, normals.varray.size()});
    const AttrDomain domain = normals.domain;
    attributes.remove("custom_normal");
    attributes.add<float3>("custom_normal", domain, AttributeInitMoveArray(new_data));
  }
}

}  // namespace blender::bke
