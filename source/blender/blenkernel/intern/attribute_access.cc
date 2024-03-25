/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <utility>

#include "BKE_attribute_math.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_geometry_set.hh"
#include "BKE_type_conversions.hh"

#include "DNA_meshdata_types.h"

#include "BLI_array_utils.hh"
#include "BLI_color.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "BLT_translation.hh"

#include "FN_field.hh"

#include "CLG_log.h"

#include "attribute_access_intern.hh"

namespace blender::bke {

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
  if (type.is<MStringProperty>()) {
    return CD_PROP_STRING;
  }
  return static_cast<eCustomDataType>(-1);
}

std::ostream &operator<<(std::ostream &stream, const AttributeIDRef &attribute_id)
{
  if (attribute_id) {
    stream << attribute_id.name();
  }
  else {
    stream << "<none>";
  }
  return stream;
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
  if (attribute_name.startswith("." UV_VERTSEL_NAME ".")) {
    return false;
  }
  if (attribute_name.startswith("." UV_EDGESEL_NAME ".")) {
    return false;
  }
  if (attribute_name.startswith("." UV_PINNED_NAME ".")) {
    return false;
  }
  return true;
}

static int attribute_data_type_complexity(const eCustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_BOOL:
      return 0;
    case CD_PROP_INT8:
      return 1;
    case CD_PROP_INT32:
      return 2;
    case CD_PROP_FLOAT:
      return 3;
    case CD_PROP_INT32_2D:
      return 4;
    case CD_PROP_FLOAT2:
      return 5;
    case CD_PROP_FLOAT3:
      return 6;
    case CD_PROP_BYTE_COLOR:
      return 7;
    case CD_PROP_QUATERNION:
      return 8;
    case CD_PROP_COLOR:
      return 9;
    case CD_PROP_FLOAT4X4:
      return 10;
#if 0 /* These attribute types are not supported yet. */
    case CD_PROP_STRING:
      return 10;
#endif
    default:
      /* Only accept "generic" custom data types used by the attribute system. */
      BLI_assert_unreachable();
      return 0;
  }
}

eCustomDataType attribute_data_type_highest_complexity(Span<eCustomDataType> data_types)
{
  int highest_complexity = INT_MIN;
  eCustomDataType most_complex_type = CD_PROP_COLOR;

  for (const eCustomDataType data_type : data_types) {
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
 * established in each component's ComponentAttributeProviders.
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

static AttributeIDRef attribute_id_from_custom_data_layer(const CustomDataLayer &layer)
{
  if (layer.anonymous_id != nullptr) {
    return *layer.anonymous_id;
  }
  return layer.name;
}

static void *add_generic_custom_data_layer(CustomData &custom_data,
                                           const eCustomDataType data_type,
                                           const eCDAllocType alloctype,
                                           const int domain_size,
                                           const AttributeIDRef &attribute_id)
{
  if (!attribute_id.is_anonymous()) {
    char attribute_name_c[MAX_CUSTOMDATA_LAYER_NAME];
    attribute_id.name().copy(attribute_name_c);
    return CustomData_add_layer_named(
        &custom_data, data_type, alloctype, domain_size, attribute_name_c);
  }
  const AnonymousAttributeID &anonymous_id = attribute_id.anonymous_id();
  return CustomData_add_layer_anonymous(
      &custom_data, data_type, alloctype, domain_size, &anonymous_id);
}

static const void *add_generic_custom_data_layer_with_existing_data(
    CustomData &custom_data,
    const eCustomDataType data_type,
    const AttributeIDRef &attribute_id,
    const int domain_size,
    void *layer_data,
    const ImplicitSharingInfo *sharing_info)
{
  if (attribute_id.is_anonymous()) {
    const AnonymousAttributeID &anonymous_id = attribute_id.anonymous_id();
    return CustomData_add_layer_anonymous_with_data(
        &custom_data, data_type, &anonymous_id, domain_size, layer_data, sharing_info);
  }
  return CustomData_add_layer_named_with_data(
      &custom_data, data_type, layer_data, domain_size, attribute_id.name(), sharing_info);
}

static bool add_custom_data_layer_from_attribute_init(const AttributeIDRef &attribute_id,
                                                      CustomData &custom_data,
                                                      const eCustomDataType data_type,
                                                      const int domain_num,
                                                      const AttributeInit &initializer)
{
  const int old_layer_num = custom_data.totlayer;
  switch (initializer.type) {
    case AttributeInit::Type::Construct: {
      add_generic_custom_data_layer(
          custom_data, data_type, CD_CONSTRUCT, domain_num, attribute_id);
      break;
    }
    case AttributeInit::Type::DefaultValue: {
      add_generic_custom_data_layer(
          custom_data, data_type, CD_SET_DEFAULT, domain_num, attribute_id);
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

static bool custom_data_layer_matches_attribute_id(const CustomDataLayer &layer,
                                                   const AttributeIDRef &attribute_id)
{
  if (!attribute_id) {
    return false;
  }
  return layer.name == attribute_id.name();
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
      return {GVArray::ForSpan({type, nullptr, 0}), domain_, nullptr};
    }
    return {};
  }

  const int index = CustomData_get_named_layer_index(custom_data, data_type_, name_);
  if (index == -1) {
    return {};
  }
  const CustomDataLayer &layer = custom_data->layers[index];
  return {GVArray::ForSpan({type, layer.data, element_num}), domain_, layer.sharing_info};
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
      return {GVMutableArray::ForSpan({type, nullptr, 0}), domain_, std::move(tag_modified_fn)};
    }
    return {};
  }

  void *data = CustomData_get_layer_named_for_write(custom_data, data_type_, name_, element_num);
  if (data == nullptr) {
    return {};
  }
  return {GVMutableArray::ForSpan({type, data, element_num}), domain_, std::move(tag_modified_fn)};
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

  const int element_num = custom_data_access_.get_element_num(owner);
  if (CustomData_free_layer_named(custom_data, name_, element_num)) {
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
  if (createable_ != Creatable) {
    return false;
  }
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
          name_, *custom_data, data_type_, element_num, initializer))
  {
    if (update_on_change_ != nullptr) {
      update_on_change_(owner);
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

GAttributeReader CustomDataAttributeProvider::try_get_for_read(
    const void *owner, const AttributeIDRef &attribute_id) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
  if (custom_data == nullptr) {
    return {};
  }
  const int element_num = custom_data_access_.get_element_num(owner);
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    if (!custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      continue;
    }
    const CPPType *type = custom_data_type_to_cpp_type((eCustomDataType)layer.type);
    if (type == nullptr) {
      continue;
    }
    GSpan data{*type, layer.data, element_num};
    return {GVArray::ForSpan(data), domain_, layer.sharing_info};
  }
  return {};
}

GAttributeWriter CustomDataAttributeProvider::try_get_for_write(
    void *owner, const AttributeIDRef &attribute_id) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return {};
  }
  const int element_num = custom_data_access_.get_element_num(owner);
  for (CustomDataLayer &layer : MutableSpan(custom_data->layers, custom_data->totlayer)) {
    if (!custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      continue;
    }
    CustomData_get_layer_named_for_write(
        custom_data, eCustomDataType(layer.type), layer.name, element_num);

    const CPPType *type = custom_data_type_to_cpp_type(eCustomDataType(layer.type));
    if (type == nullptr) {
      continue;
    }
    GMutableSpan data{*type, layer.data, element_num};
    return {GVMutableArray::ForSpan(data), domain_};
  }
  return {};
}

bool CustomDataAttributeProvider::try_delete(void *owner, const AttributeIDRef &attribute_id) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return false;
  }
  const int element_num = custom_data_access_.get_element_num(owner);
  for (const int i : IndexRange(custom_data->totlayer)) {
    const CustomDataLayer &layer = custom_data->layers[i];
    if (this->type_is_supported((eCustomDataType)layer.type) &&
        custom_data_layer_matches_attribute_id(layer, attribute_id))
    {
      CustomData_free_layer(custom_data, eCustomDataType(layer.type), element_num, i);
      return true;
    }
  }
  return false;
}

bool CustomDataAttributeProvider::try_create(void *owner,
                                             const AttributeIDRef &attribute_id,
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
    if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      return false;
    }
  }
  const int element_num = custom_data_access_.get_element_num(owner);
  add_custom_data_layer_from_attribute_init(
      attribute_id, *custom_data, data_type, element_num, initializer);
  return true;
}

bool CustomDataAttributeProvider::foreach_attribute(const void *owner,
                                                    const AttributeForeachCallback callback) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
  if (custom_data == nullptr) {
    return true;
  }
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    const eCustomDataType data_type = (eCustomDataType)layer.type;
    if (this->type_is_supported(data_type)) {
      AttributeMetaData meta_data{domain_, data_type};
      const AttributeIDRef attribute_id = attribute_id_from_custom_data_layer(layer);
      if (!callback(attribute_id, meta_data)) {
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

GAttributeReader AttributeAccessor::lookup(const AttributeIDRef &attribute_id,
                                           const std::optional<AttrDomain> domain,
                                           const std::optional<eCustomDataType> data_type) const
{
  GAttributeReader attribute = this->lookup(attribute_id);
  if (!attribute) {
    return {};
  }
  if (domain.has_value()) {
    if (attribute.domain != domain) {
      attribute.varray = this->adapt_domain(attribute.varray, attribute.domain, *domain);
      attribute.domain = *domain;
      attribute.sharing_info = nullptr;
      if (!attribute.varray) {
        return {};
      }
    }
  }
  if (data_type.has_value()) {
    const CPPType &type = *custom_data_type_to_cpp_type(*data_type);
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

GAttributeReader AttributeAccessor::lookup_or_default(const AttributeIDRef &attribute_id,
                                                      const AttrDomain domain,
                                                      const eCustomDataType data_type,
                                                      const void *default_value) const
{
  GAttributeReader attribute = this->lookup(attribute_id, domain, data_type);
  if (attribute) {
    return attribute;
  }
  const CPPType &type = *custom_data_type_to_cpp_type(data_type);
  const int64_t domain_size = this->domain_size(domain);
  if (default_value == nullptr) {
    return {GVArray::ForSingleRef(type, domain_size, type.default_value()), domain, nullptr};
  }
  return {GVArray::ForSingle(type, domain_size, default_value), domain, nullptr};
}

Set<AttributeIDRef> AttributeAccessor::all_ids() const
{
  Set<AttributeIDRef> ids;
  this->for_all([&](const AttributeIDRef &attribute_id, const AttributeMetaData & /*meta_data*/) {
    ids.add(attribute_id);
    return true;
  });
  return ids;
}

void MutableAttributeAccessor::remove_anonymous()
{
  Vector<const AnonymousAttributeID *> anonymous_ids;
  for (const AttributeIDRef &id : this->all_ids()) {
    if (id.is_anonymous()) {
      anonymous_ids.append(&id.anonymous_id());
    }
  }

  while (!anonymous_ids.is_empty()) {
    this->remove(*anonymous_ids.pop_last());
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

GAttributeWriter MutableAttributeAccessor::lookup_for_write(const AttributeIDRef &attribute_id)
{
  GAttributeWriter attribute = fn_->lookup_for_write(owner_, attribute_id);
  /* Check that the #finish method is called in debug builds. */
#ifndef NDEBUG
  if (attribute) {
    auto checker = std::make_shared<FinishCallChecker>();
    checker->name = attribute_id.name();
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

GSpanAttributeWriter MutableAttributeAccessor::lookup_for_write_span(
    const AttributeIDRef &attribute_id)
{
  GAttributeWriter attribute = this->lookup_for_write(attribute_id);
  if (attribute) {
    return GSpanAttributeWriter{std::move(attribute), true};
  }
  return {};
}

GAttributeWriter MutableAttributeAccessor::lookup_or_add_for_write(
    const AttributeIDRef &attribute_id,
    const AttrDomain domain,
    const eCustomDataType data_type,
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
    const AttributeIDRef &attribute_id,
    const AttrDomain domain,
    const eCustomDataType data_type,
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
    const AttributeIDRef &attribute_id, const AttrDomain domain, const eCustomDataType data_type)
{
  GAttributeWriter attribute = this->lookup_or_add_for_write(
      attribute_id, domain, data_type, AttributeInitConstruct());
  if (attribute) {
    return GSpanAttributeWriter{std::move(attribute), false};
  }
  return {};
}

bool MutableAttributeAccessor::rename(const AttributeIDRef &old_attribute_id,
                                      const AttributeIDRef &new_attribute_id)
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
  const eCustomDataType type = cpp_type_to_custom_data_type(old_attribute.varray.type());
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
    auto validate_op = fn::FieldOperation::Create(*function, {field});
    return fn::GField(validate_op);
  }
  return field;
}

Vector<AttributeTransferData> retrieve_attributes_for_transfer(
    const AttributeAccessor src_attributes,
    MutableAttributeAccessor dst_attributes,
    const AttrDomainMask domain_mask,
    const AnonymousAttributePropagationInfo &propagation_info,
    const Set<std::string> &skip)
{
  Vector<AttributeTransferData> attributes;
  src_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (!(ATTR_DOMAIN_AS_MASK(meta_data.domain) & domain_mask)) {
      return true;
    }
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    if (skip.contains(id.name())) {
      return true;
    }

    const GVArray src = *src_attributes.lookup(id, meta_data.domain);
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        id, meta_data.domain, meta_data.data_type);
    attributes.append({std::move(src), meta_data, std::move(dst)});

    return true;
  });
  return attributes;
}

/** \} */

void gather_attributes(const AttributeAccessor src_attributes,
                       const AttrDomain domain,
                       const AnonymousAttributePropagationInfo &propagation_info,
                       const Set<std::string> &skip,
                       const IndexMask &selection,
                       MutableAttributeAccessor dst_attributes)
{
  const int src_size = src_attributes.domain_size(domain);
  src_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (meta_data.domain != domain) {
      return true;
    }
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    if (skip.contains(id.name())) {
      return true;
    }
    const GAttributeReader src = src_attributes.lookup(id, domain);
    if (selection.size() == src_size && src.sharing_info && src.varray.is_span()) {
      const AttributeInitShared init(src.varray.get_internal_span().data(), *src.sharing_info);
      if (dst_attributes.add(id, domain, meta_data.data_type, init)) {
        return true;
      }
    }
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        id, domain, meta_data.data_type);
    if (!dst) {
      return true;
    }
    array_utils::gather(src.varray, selection, dst.span);
    dst.finish();
    return true;
  });
}

void gather_attributes(const AttributeAccessor src_attributes,
                       const AttrDomain domain,
                       const AnonymousAttributePropagationInfo &propagation_info,
                       const Set<std::string> &skip,
                       const Span<int> indices,
                       MutableAttributeAccessor dst_attributes)
{
  if (array_utils::indices_are_range(indices, IndexRange(src_attributes.domain_size(domain)))) {
    copy_attributes(src_attributes, domain, propagation_info, skip, dst_attributes);
  }
  else {
    src_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
      if (meta_data.domain != domain) {
        return true;
      }
      if (meta_data.data_type == CD_PROP_STRING) {
        return true;
      }
      if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
        return true;
      }
      if (skip.contains(id.name())) {
        return true;
      }
      const GAttributeReader src = src_attributes.lookup(id, domain);
      GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
          id, domain, meta_data.data_type);
      if (!dst) {
        return true;
      }
      attribute_math::gather(src.varray, indices, dst.span);
      dst.finish();
      return true;
    });
  }
}

void gather_attributes_group_to_group(const AttributeAccessor src_attributes,
                                      const AttrDomain domain,
                                      const AnonymousAttributePropagationInfo &propagation_info,
                                      const Set<std::string> &skip,
                                      const OffsetIndices<int> src_offsets,
                                      const OffsetIndices<int> dst_offsets,
                                      const IndexMask &selection,
                                      MutableAttributeAccessor dst_attributes)
{
  src_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (meta_data.domain != domain) {
      return true;
    }
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    if (skip.contains(id.name())) {
      return true;
    }
    const GVArraySpan src = *src_attributes.lookup(id, domain);
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        id, domain, meta_data.data_type);
    if (!dst) {
      return true;
    }
    attribute_math::gather_group_to_group(src_offsets, dst_offsets, selection, src, dst.span);
    dst.finish();
    return true;
  });
}

void gather_attributes_to_groups(const AttributeAccessor src_attributes,
                                 const AttrDomain domain,
                                 const AnonymousAttributePropagationInfo &propagation_info,
                                 const Set<std::string> &skip,
                                 const OffsetIndices<int> dst_offsets,
                                 const IndexMask &src_selection,
                                 MutableAttributeAccessor dst_attributes)
{
  src_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (meta_data.domain != domain) {
      return true;
    }
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    if (skip.contains(id.name())) {
      return true;
    }
    const GVArraySpan src = *src_attributes.lookup(id, domain);
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        id, domain, meta_data.data_type);
    if (!dst) {
      return true;
    }
    attribute_math::gather_to_groups(dst_offsets, src_selection, src, dst.span);
    dst.finish();
    return true;
  });
}

void copy_attributes(const AttributeAccessor src_attributes,
                     const AttrDomain domain,
                     const AnonymousAttributePropagationInfo &propagation_info,
                     const Set<std::string> &skip,
                     MutableAttributeAccessor dst_attributes)
{
  BLI_assert(src_attributes.domain_size(domain) == dst_attributes.domain_size(domain));
  return gather_attributes(src_attributes,
                           domain,
                           propagation_info,
                           skip,
                           IndexMask(src_attributes.domain_size(domain)),
                           dst_attributes);
}

void copy_attributes_group_to_group(const AttributeAccessor src_attributes,
                                    const AttrDomain domain,
                                    const AnonymousAttributePropagationInfo &propagation_info,
                                    const Set<std::string> &skip,
                                    const OffsetIndices<int> src_offsets,
                                    const OffsetIndices<int> dst_offsets,
                                    const IndexMask &selection,
                                    MutableAttributeAccessor dst_attributes)
{
  if (selection.is_empty()) {
    return;
  }
  src_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (meta_data.domain != domain) {
      return true;
    }
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    if (skip.contains(id.name())) {
      return true;
    }
    const GVArraySpan src = *src_attributes.lookup(id, domain);
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        id, domain, meta_data.data_type);
    if (!dst) {
      return true;
    }
    array_utils::copy_group_to_group(src_offsets, dst_offsets, selection, src, dst.span);
    dst.finish();
    return true;
  });
}

void fill_attribute_range_default(MutableAttributeAccessor attributes,
                                  const AttrDomain domain,
                                  const Set<std::string> &skip,
                                  const IndexRange range)
{
  attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (meta_data.domain != domain) {
      return true;
    }
    if (skip.contains(id.name())) {
      return true;
    }
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);
    const CPPType &type = attribute.span.type();
    GMutableSpan data = attribute.span.slice(range);
    type.fill_assign_n(type.default_value(), data.data(), data.size());
    attribute.finish();
    return true;
  });
}

}  // namespace blender::bke
