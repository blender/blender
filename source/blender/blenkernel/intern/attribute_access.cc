/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <utility>

#include "BKE_attribute_math.hh"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.h"
#include "BKE_type_conversions.hh"
#include "BKE_global.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_array_utils.hh"
#include "BLI_color.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "BLT_translation.h"

#include "FN_field.hh"

#include "BLT_translation.h"

#include "CLG_log.h"

#include "attribute_access_intern.hh"

using blender::float3;
using blender::GMutableSpan;
using blender::GSpan;
using blender::GVArrayImpl_For_GSpan;
using blender::Set;
using blender::StringRef;
using blender::StringRefNull;
using blender::bke::AttributeIDRef;

namespace blender::bke {

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
    "This attribute can not be accessed in a procedural context");

bool allow_procedural_attribute_access(StringRef attribute_name)
{
  if (G.debug_value == 892) {
    return true;
  }

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
static int attribute_domain_priority(const eAttrDomain domain)
{
  switch (domain) {
    case ATTR_DOMAIN_INSTANCE:
      return 0;
    case ATTR_DOMAIN_CURVE:
      return 1;
    case ATTR_DOMAIN_FACE:
      return 2;
    case ATTR_DOMAIN_EDGE:
      return 3;
    case ATTR_DOMAIN_POINT:
      return 4;
    case ATTR_DOMAIN_CORNER:
      return 5;
    default:
      /* Domain not supported in nodes yet. */
      BLI_assert_unreachable();
      return 0;
  }
}

eAttrDomain attribute_domain_highest_priority(Span<eAttrDomain> domains)
{
  int highest_priority = INT_MIN;
  eAttrDomain highest_priority_domain = ATTR_DOMAIN_CORNER;

  for (const eAttrDomain domain : domains) {
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

static bool add_builtin_type_custom_data_layer_from_init(CustomData &custom_data,
                                                         const eCustomDataType data_type,
                                                         const int domain_num,
                                                         const AttributeInit &initializer)
{
  switch (initializer.type) {
    case AttributeInit::Type::Construct: {
      void *data = CustomData_add_layer(&custom_data, data_type, CD_CONSTRUCT, domain_num);
      return data != nullptr;
    }
    case AttributeInit::Type::DefaultValue: {
      void *data = CustomData_add_layer(&custom_data, data_type, CD_SET_DEFAULT, domain_num);
      return data != nullptr;
    }
    case AttributeInit::Type::VArray: {
      void *data = CustomData_add_layer(&custom_data, data_type, CD_CONSTRUCT, domain_num);
      if (data == nullptr) {
        return false;
      }
      const GVArray &varray = static_cast<const AttributeInitVArray &>(initializer).varray;
      varray.materialize_to_uninitialized(varray.index_range(), data);
      return true;
    }
    case AttributeInit::Type::MoveArray: {
      void *src_data = static_cast<const AttributeInitMoveArray &>(initializer).data;
      const void *stored_data = CustomData_add_layer_with_data(
          &custom_data, data_type, src_data, domain_num, nullptr);
      if (stored_data == nullptr) {
        return false;
      }
      if (stored_data != src_data) {
        MEM_freeN(src_data);
        return true;
      }
      return true;
    }
    case AttributeInit::Type::Shared: {
      const AttributeInitShared &init = static_cast<const AttributeInitShared &>(initializer);
      const void *stored_data = CustomData_add_layer_with_data(
          &custom_data, data_type, const_cast<void *>(init.data), domain_num, init.sharing_info);
      return stored_data != nullptr;
    }
  }

  BLI_assert_unreachable();
  return false;
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
  char attribute_name_c[MAX_CUSTOMDATA_LAYER_NAME];
  attribute_id.name().copy(attribute_name_c);
  return CustomData_add_layer_named_with_data(
      &custom_data, data_type, layer_data, domain_size, attribute_name_c, sharing_info);
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
  if (stored_as_named_attribute_) {
    return CustomData_get_named_layer_index(&custom_data, stored_type_, name_.c_str()) != -1;
  }
  return CustomData_has_layer(&custom_data, stored_type_);
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

  int index;
  if (stored_as_named_attribute_) {
    index = CustomData_get_named_layer_index(custom_data, stored_type_, name_.c_str());
  }
  else {
    index = CustomData_get_layer_index(custom_data, stored_type_);
  }
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

  void *data = nullptr;
  if (stored_as_named_attribute_) {
    data = CustomData_get_layer_named_for_write(
        custom_data, stored_type_, name_.c_str(), element_num);
  }
  else {
    data = CustomData_get_layer_for_write(custom_data, stored_type_, element_num);
  }
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

  auto update = [&]() {
    if (update_on_change_ != nullptr) {
      update_on_change_(owner);
    }
  };

  const int element_num = custom_data_access_.get_element_num(owner);
  if (stored_as_named_attribute_) {
    if (CustomData_free_layer_named(custom_data, name_.c_str(), element_num)) {
      update();
      return true;
    }
    return false;
  }

  const int layer_index = CustomData_get_layer_index(custom_data, stored_type_);
  if (CustomData_free_layer(custom_data, stored_type_, element_num, layer_index)) {
    update();
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
  if (stored_as_named_attribute_) {
    if (CustomData_has_layer_named(custom_data, data_type_, name_.c_str())) {
      /* Exists already. */
      return false;
    }
    return add_custom_data_layer_from_attribute_init(
        name_, *custom_data, stored_type_, element_num, initializer);
  }

  if (CustomData_get_layer(custom_data, stored_type_) != nullptr) {
    /* Exists already. */
    return false;
  }
  return add_builtin_type_custom_data_layer_from_init(
      *custom_data, stored_type_, element_num, initializer);
}

bool BuiltinCustomDataLayerProvider::exists(const void *owner) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
  if (custom_data == nullptr) {
    return false;
  }
  if (stored_as_named_attribute_) {
    return CustomData_has_layer_named(custom_data, stored_type_, name_.c_str());
  }
  return CustomData_get_layer(custom_data, stored_type_) != nullptr;
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
  ;
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
                                             const eAttrDomain domain,
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

CustomDataAttributes::CustomDataAttributes()
{
  CustomData_reset(&data);
  size_ = 0;
}

CustomDataAttributes::~CustomDataAttributes()
{
  CustomData_free(&data, size_);
}

CustomDataAttributes::CustomDataAttributes(const CustomDataAttributes &other)
{
  size_ = other.size_;
  CustomData_copy(&other.data, &data, CD_MASK_ALL, size_);
}

CustomDataAttributes::CustomDataAttributes(CustomDataAttributes &&other)
{
  size_ = other.size_;
  data = other.data;
  CustomData_reset(&other.data);
  other.size_ = 0;
}

CustomDataAttributes &CustomDataAttributes::operator=(const CustomDataAttributes &other)
{
  if (this == &other) {
    return *this;
  }
  this->~CustomDataAttributes();
  new (this) CustomDataAttributes(other);
  return *this;
}

CustomDataAttributes &CustomDataAttributes::operator=(CustomDataAttributes &&other)
{
  if (this == &other) {
    return *this;
  }
  this->~CustomDataAttributes();
  new (this) CustomDataAttributes(std::move(other));
  return *this;
}

std::optional<GSpan> CustomDataAttributes::get_for_read(const AttributeIDRef &attribute_id) const
{
  for (const CustomDataLayer &layer : Span(data.layers, data.totlayer)) {
    if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      const CPPType *cpp_type = custom_data_type_to_cpp_type((eCustomDataType)layer.type);
      BLI_assert(cpp_type != nullptr);
      return GSpan(*cpp_type, layer.data, size_);
    }
  }
  return {};
}

GVArray CustomDataAttributes::get_for_read(const AttributeIDRef &attribute_id,
                                           const eCustomDataType data_type,
                                           const void *default_value) const
{
  const CPPType *type = blender::bke::custom_data_type_to_cpp_type(data_type);

  std::optional<GSpan> attribute = this->get_for_read(attribute_id);
  if (!attribute) {
    const int domain_num = this->size_;
    return GVArray::ForSingle(
        *type, domain_num, (default_value == nullptr) ? type->default_value() : default_value);
  }

  if (attribute->type() == *type) {
    return GVArray::ForSpan(*attribute);
  }
  const blender::bke::DataTypeConversions &conversions =
      blender::bke::get_implicit_type_conversions();
  return conversions.try_convert(GVArray::ForSpan(*attribute), *type);
}

std::optional<GMutableSpan> CustomDataAttributes::get_for_write(const AttributeIDRef &attribute_id)
{
  for (CustomDataLayer &layer : MutableSpan(data.layers, data.totlayer)) {
    if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      const CPPType *cpp_type = custom_data_type_to_cpp_type((eCustomDataType)layer.type);
      BLI_assert(cpp_type != nullptr);
      return GMutableSpan(*cpp_type, layer.data, size_);
    }
  }
  return {};
}

bool CustomDataAttributes::create(const AttributeIDRef &attribute_id,
                                  const eCustomDataType data_type)
{
  void *result = add_generic_custom_data_layer(
      data, data_type, CD_SET_DEFAULT, size_, attribute_id);
  return result != nullptr;
}

bool CustomDataAttributes::remove(const AttributeIDRef &attribute_id)
{
  for (const int i : IndexRange(data.totlayer)) {
    const CustomDataLayer &layer = data.layers[i];
    if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      CustomData_free_layer(&data, eCustomDataType(layer.type), size_, i);
      return true;
    }
  }
  return false;
}

void CustomDataAttributes::reallocate(const int size)
{
  const int old_size = size_;
  size_ = size;
  CustomData_realloc(&data, old_size, size_);
  if (size_ > old_size) {
    /* Fill default new values. */
    const int new_elements_num = size_ - old_size;
    this->foreach_attribute(
        [&](const bke::AttributeIDRef &id, const bke::AttributeMetaData /*meta_data*/) {
          GMutableSpan new_data = this->get_for_write(id)->take_back(new_elements_num);
          const CPPType &type = new_data.type();
          type.fill_assign_n(type.default_value(), new_data.data(), new_data.size());
          return true;
        },
        /* Dummy. */
        ATTR_DOMAIN_POINT);
  }
}

void CustomDataAttributes::clear()
{
  CustomData_free(&data, size_);
  size_ = 0;
}

bool CustomDataAttributes::foreach_attribute(const AttributeForeachCallback callback,
                                             const eAttrDomain domain) const
{
  for (const CustomDataLayer &layer : Span(data.layers, data.totlayer)) {
    AttributeMetaData meta_data{domain, (eCustomDataType)layer.type};
    const AttributeIDRef attribute_id = attribute_id_from_custom_data_layer(layer);
    if (!callback(attribute_id, meta_data)) {
      return false;
    }
  }
  return true;
}

/* -------------------------------------------------------------------- */
/** \name Attribute API
 * \{ */

static blender::GVArray try_adapt_data_type(blender::GVArray varray,
                                            const blender::CPPType &to_type)
{
  const blender::bke::DataTypeConversions &conversions =
      blender::bke::get_implicit_type_conversions();
  return conversions.try_convert(std::move(varray), to_type);
}

GAttributeReader AttributeAccessor::lookup(const AttributeIDRef &attribute_id,
                                           const std::optional<eAttrDomain> domain,
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
                                                      const eAttrDomain domain,
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
  this->for_all(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData & /* meta_data */) {
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
#ifdef DEBUG
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
#ifdef DEBUG
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
    const eAttrDomain domain,
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
    const eAttrDomain domain,
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
    const AttributeIDRef &attribute_id, const eAttrDomain domain, const eCustomDataType data_type)
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
    const bke::AttributeAccessor src_attributes,
    bke::MutableAttributeAccessor dst_attributes,
    const eAttrDomainMask domain_mask,
    const AnonymousAttributePropagationInfo &propagation_info,
    const Set<std::string> &skip)
{
  Vector<AttributeTransferData> attributes;
  src_attributes.for_all(
      [&](const bke::AttributeIDRef &id, const bke::AttributeMetaData meta_data) {
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
        bke::GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
            id, meta_data.domain, meta_data.data_type);
        attributes.append({std::move(src), meta_data, std::move(dst)});

        return true;
      });
  return attributes;
}

/** \} */

void gather_attributes(const AttributeAccessor src_attributes,
                       const eAttrDomain domain,
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
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    if (skip.contains(id.name())) {
      return true;
    }
    const bke::GAttributeReader src = src_attributes.lookup(id, domain);
    if (selection.size() == src_size && src.sharing_info && src.varray.is_span()) {
      const bke::AttributeInitShared init(src.varray.get_internal_span().data(),
                                          *src.sharing_info);
      if (dst_attributes.add(id, domain, meta_data.data_type, init)) {
        return true;
      }
    }
    bke::GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        id, domain, meta_data.data_type);
    if (!dst) {
      return true;
    }
    array_utils::gather(src.varray, selection, dst.span);
    dst.finish();
    return true;
  });
}

template<typename T>
static void gather_group_to_group(const OffsetIndices<int> src_offsets,
                                  const OffsetIndices<int> dst_offsets,
                                  const IndexMask &selection,
                                  const Span<T> src,
                                  MutableSpan<T> dst)
{
  selection.foreach_index(GrainSize(512), [&](const int64_t src_i, const int64_t dst_i) {
    dst.slice(dst_offsets[dst_i]).copy_from(src.slice(src_offsets[src_i]));
  });
}

static void gather_group_to_group(const OffsetIndices<int> src_offsets,
                                  const OffsetIndices<int> dst_offsets,
                                  const IndexMask &selection,
                                  const GSpan src,
                                  GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    gather_group_to_group(src_offsets, dst_offsets, selection, src.typed<T>(), dst.typed<T>());
  });
}

void gather_attributes_group_to_group(const AttributeAccessor src_attributes,
                                      const eAttrDomain domain,
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
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    if (skip.contains(id.name())) {
      return true;
    }
    const GVArraySpan src = *src_attributes.lookup(id, domain);
    bke::GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        id, domain, meta_data.data_type);
    if (!dst) {
      return true;
    }
    gather_group_to_group(src_offsets, dst_offsets, selection, src, dst.span);
    dst.finish();
    return true;
  });
}

void copy_attributes(const AttributeAccessor src_attributes,
                     const eAttrDomain domain,
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

}  // namespace blender::bke
