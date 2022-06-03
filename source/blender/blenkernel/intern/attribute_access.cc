/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <utility>

#include "BKE_attribute_access.hh"
#include "BKE_attribute_math.hh"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.h"
#include "BKE_pointcloud.h"
#include "BKE_type_conversions.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_color.hh"
#include "BLI_math_vec_types.hh"
#include "BLI_span.hh"

#include "BLT_translation.h"

#include "CLG_log.h"

#include "attribute_access_intern.hh"

static CLG_LogRef LOG = {"bke.attribute_access"};

using blender::float3;
using blender::GMutableSpan;
using blender::GSpan;
using blender::GVArrayImpl_For_GSpan;
using blender::Set;
using blender::StringRef;
using blender::StringRefNull;
using blender::bke::AttributeIDRef;
using blender::bke::OutputAttribute;

namespace blender::bke {

std::ostream &operator<<(std::ostream &stream, const AttributeIDRef &attribute_id)
{
  if (attribute_id.is_named()) {
    stream << attribute_id.name();
  }
  else if (attribute_id.is_anonymous()) {
    const AnonymousAttributeID &anonymous_id = attribute_id.anonymous_id();
    stream << "<" << BKE_anonymous_attribute_id_debug_name(&anonymous_id) << ">";
  }
  else {
    stream << "<none>";
  }
  return stream;
}

const char *no_procedural_access_message =
    "This attribute can not be accessed in a procedural context";

bool allow_procedural_attribute_access(StringRef attribute_name)
{
  return !attribute_name.startswith(".selection");
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
    case CD_PROP_FLOAT2:
      return 4;
    case CD_PROP_FLOAT3:
      return 5;
    case CD_PROP_BYTE_COLOR:
      return 6;
    case CD_PROP_COLOR:
      return 7;
#if 0 /* These attribute types are not supported yet. */
    case CD_PROP_STRING:
      return 6;
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

GMutableSpan OutputAttribute::as_span()
{
  if (!optional_span_varray_) {
    const bool materialize_old_values = !ignore_old_values_;
    optional_span_varray_ = std::make_unique<GVMutableArray_GSpan>(varray_,
                                                                   materialize_old_values);
  }
  GVMutableArray_GSpan &span_varray = *optional_span_varray_;
  return span_varray;
}

void OutputAttribute::save()
{
  save_has_been_called_ = true;
  if (optional_span_varray_) {
    optional_span_varray_->save();
  }
  if (save_) {
    save_(*this);
  }
}

OutputAttribute::~OutputAttribute()
{
  if (!save_has_been_called_) {
    if (varray_) {
      std::cout << "Warning: Call `save()` to make sure that changes persist in all cases.\n";
    }
  }
}

static AttributeIDRef attribute_id_from_custom_data_layer(const CustomDataLayer &layer)
{
  if (layer.anonymous_id != nullptr) {
    return layer.anonymous_id;
  }
  return layer.name;
}

static bool add_builtin_type_custom_data_layer_from_init(CustomData &custom_data,
                                                         const eCustomDataType data_type,
                                                         const int domain_num,
                                                         const AttributeInit &initializer)
{
  switch (initializer.type) {
    case AttributeInit::Type::Default: {
      void *data = CustomData_add_layer(&custom_data, data_type, CD_DEFAULT, nullptr, domain_num);
      return data != nullptr;
    }
    case AttributeInit::Type::VArray: {
      void *data = CustomData_add_layer(&custom_data, data_type, CD_DEFAULT, nullptr, domain_num);
      if (data == nullptr) {
        return false;
      }
      const GVArray &varray = static_cast<const AttributeInitVArray &>(initializer).varray;
      varray.materialize_to_uninitialized(varray.index_range(), data);
      return true;
    }
    case AttributeInit::Type::MoveArray: {
      void *source_data = static_cast<const AttributeInitMove &>(initializer).data;
      void *data = CustomData_add_layer(
          &custom_data, data_type, CD_ASSIGN, source_data, domain_num);
      if (data == nullptr) {
        MEM_freeN(source_data);
        return false;
      }
      return true;
    }
  }

  BLI_assert_unreachable();
  return false;
}

static void *add_generic_custom_data_layer(CustomData &custom_data,
                                           const eCustomDataType data_type,
                                           const eCDAllocType alloctype,
                                           void *layer_data,
                                           const int domain_num,
                                           const AttributeIDRef &attribute_id)
{
  if (attribute_id.is_named()) {
    char attribute_name_c[MAX_NAME];
    attribute_id.name().copy(attribute_name_c);
    return CustomData_add_layer_named(
        &custom_data, data_type, alloctype, layer_data, domain_num, attribute_name_c);
  }
  const AnonymousAttributeID &anonymous_id = attribute_id.anonymous_id();
  return CustomData_add_layer_anonymous(
      &custom_data, data_type, alloctype, layer_data, domain_num, &anonymous_id);
}

static bool add_custom_data_layer_from_attribute_init(const AttributeIDRef &attribute_id,
                                                      CustomData &custom_data,
                                                      const eCustomDataType data_type,
                                                      const int domain_num,
                                                      const AttributeInit &initializer)
{
  switch (initializer.type) {
    case AttributeInit::Type::Default: {
      void *data = add_generic_custom_data_layer(
          custom_data, data_type, CD_DEFAULT, nullptr, domain_num, attribute_id);
      return data != nullptr;
    }
    case AttributeInit::Type::VArray: {
      void *data = add_generic_custom_data_layer(
          custom_data, data_type, CD_DEFAULT, nullptr, domain_num, attribute_id);
      if (data == nullptr) {
        return false;
      }
      const GVArray &varray = static_cast<const AttributeInitVArray &>(initializer).varray;
      varray.materialize_to_uninitialized(varray.index_range(), data);
      return true;
    }
    case AttributeInit::Type::MoveArray: {
      void *source_data = static_cast<const AttributeInitMove &>(initializer).data;
      void *data = add_generic_custom_data_layer(
          custom_data, data_type, CD_ASSIGN, source_data, domain_num, attribute_id);
      if (data == nullptr) {
        MEM_freeN(source_data);
        return false;
      }
      return true;
    }
  }

  BLI_assert_unreachable();
  return false;
}

static bool custom_data_layer_matches_attribute_id(const CustomDataLayer &layer,
                                                   const AttributeIDRef &attribute_id)
{
  if (!attribute_id) {
    return false;
  }
  if (attribute_id.is_anonymous()) {
    return layer.anonymous_id == &attribute_id.anonymous_id();
  }
  return layer.name == attribute_id.name();
}

GVArray BuiltinCustomDataLayerProvider::try_get_for_read(const GeometryComponent &component) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }

  const void *data;
  if (stored_as_named_attribute_) {
    data = CustomData_get_layer_named(custom_data, stored_type_, name_.c_str());
  }
  else {
    data = CustomData_get_layer(custom_data, stored_type_);
  }
  if (data == nullptr) {
    return {};
  }

  const int domain_num = component.attribute_domain_num(domain_);
  return as_read_attribute_(data, domain_num);
}

WriteAttributeLookup BuiltinCustomDataLayerProvider::try_get_for_write(
    GeometryComponent &component) const
{
  if (writable_ != Writable) {
    return {};
  }
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }
  const int domain_num = component.attribute_domain_num(domain_);

  void *data;
  if (stored_as_named_attribute_) {
    data = CustomData_get_layer_named(custom_data, stored_type_, name_.c_str());
  }
  else {
    data = CustomData_get_layer(custom_data, stored_type_);
  }
  if (data == nullptr) {
    return {};
  }

  void *new_data;
  if (stored_as_named_attribute_) {
    new_data = CustomData_duplicate_referenced_layer_named(
        custom_data, stored_type_, name_.c_str(), domain_num);
  }
  else {
    new_data = CustomData_duplicate_referenced_layer(custom_data, stored_type_, domain_num);
  }

  if (data != new_data) {
    if (custom_data_access_.update_custom_data_pointers) {
      custom_data_access_.update_custom_data_pointers(component);
    }
    data = new_data;
  }

  std::function<void()> tag_modified_fn;
  if (update_on_write_ != nullptr) {
    tag_modified_fn = [component = &component, update = update_on_write_]() {
      update(*component);
    };
  }

  return {as_write_attribute_(data, domain_num), domain_, std::move(tag_modified_fn)};
}

bool BuiltinCustomDataLayerProvider::try_delete(GeometryComponent &component) const
{
  if (deletable_ != Deletable) {
    return false;
  }
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }

  const int domain_num = component.attribute_domain_num(domain_);
  int layer_index;
  if (stored_as_named_attribute_) {
    for (const int i : IndexRange(custom_data->totlayer)) {
      if (custom_data_layer_matches_attribute_id(custom_data->layers[i], name_)) {
        layer_index = i;
        break;
      }
    }
  }
  else {
    layer_index = CustomData_get_layer_index(custom_data, stored_type_);
  }

  const bool delete_success = CustomData_free_layer(
      custom_data, stored_type_, domain_num, layer_index);
  if (delete_success) {
    if (custom_data_access_.update_custom_data_pointers) {
      custom_data_access_.update_custom_data_pointers(component);
    }
  }
  return delete_success;
}

bool BuiltinCustomDataLayerProvider::try_create(GeometryComponent &component,
                                                const AttributeInit &initializer) const
{
  if (createable_ != Creatable) {
    return false;
  }
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return false;
  }

  const int domain_num = component.attribute_domain_num(domain_);
  bool success;
  if (stored_as_named_attribute_) {
    if (CustomData_get_layer_named(custom_data, data_type_, name_.c_str())) {
      /* Exists already. */
      return false;
    }
    success = add_custom_data_layer_from_attribute_init(
        name_, *custom_data, stored_type_, domain_num, initializer);
  }
  else {
    if (CustomData_get_layer(custom_data, stored_type_) != nullptr) {
      /* Exists already. */
      return false;
    }
    success = add_builtin_type_custom_data_layer_from_init(
        *custom_data, stored_type_, domain_num, initializer);
  }
  if (success) {
    if (custom_data_access_.update_custom_data_pointers) {
      custom_data_access_.update_custom_data_pointers(component);
    }
  }
  return success;
}

bool BuiltinCustomDataLayerProvider::exists(const GeometryComponent &component) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
  if (custom_data == nullptr) {
    return false;
  }
  if (stored_as_named_attribute_) {
    return CustomData_get_layer_named(custom_data, stored_type_, name_.c_str()) != nullptr;
  }
  return CustomData_get_layer(custom_data, stored_type_) != nullptr;
}

ReadAttributeLookup CustomDataAttributeProvider::try_get_for_read(
    const GeometryComponent &component, const AttributeIDRef &attribute_id) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }
  const int domain_num = component.attribute_domain_num(domain_);
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    if (!custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      continue;
    }
    const CPPType *type = custom_data_type_to_cpp_type((eCustomDataType)layer.type);
    if (type == nullptr) {
      continue;
    }
    GSpan data{*type, layer.data, domain_num};
    return {GVArray::ForSpan(data), domain_};
  }
  return {};
}

WriteAttributeLookup CustomDataAttributeProvider::try_get_for_write(
    GeometryComponent &component, const AttributeIDRef &attribute_id) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }
  const int domain_num = component.attribute_domain_num(domain_);
  for (CustomDataLayer &layer : MutableSpan(custom_data->layers, custom_data->totlayer)) {
    if (!custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      continue;
    }
    if (attribute_id.is_named()) {
      CustomData_duplicate_referenced_layer_named(custom_data, layer.type, layer.name, domain_num);
    }
    else {
      CustomData_duplicate_referenced_layer_anonymous(
          custom_data, layer.type, &attribute_id.anonymous_id(), domain_num);
    }
    const CPPType *type = custom_data_type_to_cpp_type((eCustomDataType)layer.type);
    if (type == nullptr) {
      continue;
    }
    GMutableSpan data{*type, layer.data, domain_num};
    return {GVMutableArray::ForSpan(data), domain_};
  }
  return {};
}

bool CustomDataAttributeProvider::try_delete(GeometryComponent &component,
                                             const AttributeIDRef &attribute_id) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return false;
  }
  const int domain_num = component.attribute_domain_num(domain_);
  for (const int i : IndexRange(custom_data->totlayer)) {
    const CustomDataLayer &layer = custom_data->layers[i];
    if (this->type_is_supported((eCustomDataType)layer.type) &&
        custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      CustomData_free_layer(custom_data, layer.type, domain_num, i);
      return true;
    }
  }
  return false;
}

bool CustomDataAttributeProvider::try_create(GeometryComponent &component,
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
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return false;
  }
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      return false;
    }
  }
  const int domain_num = component.attribute_domain_num(domain_);
  add_custom_data_layer_from_attribute_init(
      attribute_id, *custom_data, data_type, domain_num, initializer);
  return true;
}

bool CustomDataAttributeProvider::foreach_attribute(const GeometryComponent &component,
                                                    const AttributeForeachCallback callback) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
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

ReadAttributeLookup NamedLegacyCustomDataProvider::try_get_for_read(
    const GeometryComponent &component, const AttributeIDRef &attribute_id) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    if (layer.type == stored_type_) {
      if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
        const int domain_num = component.attribute_domain_num(domain_);
        return {as_read_attribute_(layer.data, domain_num), domain_};
      }
    }
  }
  return {};
}

WriteAttributeLookup NamedLegacyCustomDataProvider::try_get_for_write(
    GeometryComponent &component, const AttributeIDRef &attribute_id) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }
  for (CustomDataLayer &layer : MutableSpan(custom_data->layers, custom_data->totlayer)) {
    if (layer.type == stored_type_) {
      if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
        const int domain_num = component.attribute_domain_num(domain_);
        void *data_old = layer.data;
        void *data_new = CustomData_duplicate_referenced_layer_named(
            custom_data, stored_type_, layer.name, domain_num);
        if (data_old != data_new) {
          if (custom_data_access_.update_custom_data_pointers) {
            custom_data_access_.update_custom_data_pointers(component);
          }
        }
        return {as_write_attribute_(layer.data, domain_num), domain_};
      }
    }
  }
  return {};
}

bool NamedLegacyCustomDataProvider::try_delete(GeometryComponent &component,
                                               const AttributeIDRef &attribute_id) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return false;
  }
  for (const int i : IndexRange(custom_data->totlayer)) {
    const CustomDataLayer &layer = custom_data->layers[i];
    if (layer.type == stored_type_) {
      if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
        const int domain_num = component.attribute_domain_num(domain_);
        CustomData_free_layer(custom_data, stored_type_, domain_num, i);
        if (custom_data_access_.update_custom_data_pointers) {
          custom_data_access_.update_custom_data_pointers(component);
        }
        return true;
      }
    }
  }
  return false;
}

bool NamedLegacyCustomDataProvider::foreach_attribute(
    const GeometryComponent &component, const AttributeForeachCallback callback) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
  if (custom_data == nullptr) {
    return true;
  }
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    if (layer.type == stored_type_) {
      AttributeMetaData meta_data{domain_, attribute_type_};
      if (!callback(layer.name, meta_data)) {
        return false;
      }
    }
  }
  return true;
}

void NamedLegacyCustomDataProvider::foreach_domain(
    const FunctionRef<void(eAttrDomain)> callback) const
{
  callback(domain_);
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
  CustomData_copy(&other.data, &data, CD_MASK_ALL, CD_DUPLICATE, size_);
}

CustomDataAttributes::CustomDataAttributes(CustomDataAttributes &&other)
{
  size_ = other.size_;
  data = other.data;
  CustomData_reset(&other.data);
}

CustomDataAttributes &CustomDataAttributes::operator=(const CustomDataAttributes &other)
{
  if (this != &other) {
    CustomData_copy(&other.data, &data, CD_MASK_ALL, CD_DUPLICATE, other.size_);
    size_ = other.size_;
  }

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
      data, data_type, CD_DEFAULT, nullptr, size_, attribute_id);
  return result != nullptr;
}

bool CustomDataAttributes::create_by_move(const AttributeIDRef &attribute_id,
                                          const eCustomDataType data_type,
                                          void *buffer)
{
  void *result = add_generic_custom_data_layer(
      data, data_type, CD_ASSIGN, buffer, size_, attribute_id);
  return result != nullptr;
}

bool CustomDataAttributes::remove(const AttributeIDRef &attribute_id)
{
  for (const int i : IndexRange(data.totlayer)) {
    const CustomDataLayer &layer = data.layers[i];
    if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      CustomData_free_layer(&data, layer.type, size_, i);
      return true;
    }
  }
  return false;
}

void CustomDataAttributes::reallocate(const int size)
{
  size_ = size;
  CustomData_realloc(&data, size);
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

void CustomDataAttributes::reorder(Span<AttributeIDRef> new_order)
{
  BLI_assert(new_order.size() == data.totlayer);

  Map<AttributeIDRef, int> old_order;
  old_order.reserve(data.totlayer);
  Array<CustomDataLayer> old_layers(Span(data.layers, data.totlayer));
  for (const int i : old_layers.index_range()) {
    old_order.add_new(attribute_id_from_custom_data_layer(old_layers[i]), i);
  }

  MutableSpan layers(data.layers, data.totlayer);
  for (const int i : layers.index_range()) {
    const int old_index = old_order.lookup(new_order[i]);
    layers[i] = old_layers[old_index];
  }

  CustomData_update_typemap(&data);
}

}  // namespace blender::bke

/* -------------------------------------------------------------------- */
/** \name Geometry Component
 * \{ */

const blender::bke::ComponentAttributeProviders *GeometryComponent::get_attribute_providers() const
{
  return nullptr;
}

bool GeometryComponent::attribute_domain_supported(const eAttrDomain domain) const
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return false;
  }
  return providers->supported_domains().contains(domain);
}

int GeometryComponent::attribute_domain_num(const eAttrDomain UNUSED(domain)) const
{
  return 0;
}

bool GeometryComponent::attribute_is_builtin(const blender::StringRef attribute_name) const
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return false;
  }
  return providers->builtin_attribute_providers().contains_as(attribute_name);
}

bool GeometryComponent::attribute_is_builtin(const AttributeIDRef &attribute_id) const
{
  /* Anonymous attributes cannot be built-in. */
  return attribute_id.is_named() && this->attribute_is_builtin(attribute_id.name());
}

blender::bke::ReadAttributeLookup GeometryComponent::attribute_try_get_for_read(
    const AttributeIDRef &attribute_id) const
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return {};
  }
  if (attribute_id.is_named()) {
    const BuiltinAttributeProvider *builtin_provider =
        providers->builtin_attribute_providers().lookup_default_as(attribute_id.name(), nullptr);
    if (builtin_provider != nullptr) {
      return {builtin_provider->try_get_for_read(*this), builtin_provider->domain()};
    }
  }
  for (const DynamicAttributesProvider *dynamic_provider :
       providers->dynamic_attribute_providers()) {
    ReadAttributeLookup attribute = dynamic_provider->try_get_for_read(*this, attribute_id);
    if (attribute) {
      return attribute;
    }
  }
  return {};
}

blender::GVArray GeometryComponent::attribute_try_adapt_domain_impl(
    const blender::GVArray &varray,
    const eAttrDomain from_domain,
    const eAttrDomain to_domain) const
{
  if (from_domain == to_domain) {
    return varray;
  }
  return {};
}

blender::bke::WriteAttributeLookup GeometryComponent::attribute_try_get_for_write(
    const AttributeIDRef &attribute_id)
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return {};
  }
  if (attribute_id.is_named()) {
    const BuiltinAttributeProvider *builtin_provider =
        providers->builtin_attribute_providers().lookup_default_as(attribute_id.name(), nullptr);
    if (builtin_provider != nullptr) {
      return builtin_provider->try_get_for_write(*this);
    }
  }
  for (const DynamicAttributesProvider *dynamic_provider :
       providers->dynamic_attribute_providers()) {
    WriteAttributeLookup attribute = dynamic_provider->try_get_for_write(*this, attribute_id);
    if (attribute) {
      return attribute;
    }
  }
  return {};
}

bool GeometryComponent::attribute_try_delete(const AttributeIDRef &attribute_id)
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return {};
  }
  if (attribute_id.is_named()) {
    const BuiltinAttributeProvider *builtin_provider =
        providers->builtin_attribute_providers().lookup_default_as(attribute_id.name(), nullptr);
    if (builtin_provider != nullptr) {
      return builtin_provider->try_delete(*this);
    }
  }
  bool success = false;
  for (const DynamicAttributesProvider *dynamic_provider :
       providers->dynamic_attribute_providers()) {
    success = dynamic_provider->try_delete(*this, attribute_id) || success;
  }
  return success;
}

void GeometryComponent::attributes_remove_anonymous()
{
  using namespace blender;
  Vector<const AnonymousAttributeID *> anonymous_ids;
  for (const AttributeIDRef &id : this->attribute_ids()) {
    if (id.is_anonymous()) {
      anonymous_ids.append(&id.anonymous_id());
    }
  }

  while (!anonymous_ids.is_empty()) {
    this->attribute_try_delete(anonymous_ids.pop_last());
  }
}

bool GeometryComponent::attribute_try_create(const AttributeIDRef &attribute_id,
                                             const eAttrDomain domain,
                                             const eCustomDataType data_type,
                                             const AttributeInit &initializer)
{
  using namespace blender::bke;
  if (!attribute_id) {
    return false;
  }
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return false;
  }
  if (this->attribute_exists(attribute_id)) {
    return false;
  }
  if (attribute_id.is_named()) {
    const BuiltinAttributeProvider *builtin_provider =
        providers->builtin_attribute_providers().lookup_default_as(attribute_id.name(), nullptr);
    if (builtin_provider != nullptr) {
      if (builtin_provider->domain() != domain) {
        return false;
      }
      if (builtin_provider->data_type() != data_type) {
        return false;
      }
      return builtin_provider->try_create(*this, initializer);
    }
  }
  for (const DynamicAttributesProvider *dynamic_provider :
       providers->dynamic_attribute_providers()) {
    if (dynamic_provider->try_create(*this, attribute_id, domain, data_type, initializer)) {
      return true;
    }
  }
  return false;
}

bool GeometryComponent::attribute_try_create_builtin(const blender::StringRef attribute_name,
                                                     const AttributeInit &initializer)
{
  using namespace blender::bke;
  if (attribute_name.is_empty()) {
    return false;
  }
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return false;
  }
  const BuiltinAttributeProvider *builtin_provider =
      providers->builtin_attribute_providers().lookup_default_as(attribute_name, nullptr);
  if (builtin_provider == nullptr) {
    return false;
  }
  return builtin_provider->try_create(*this, initializer);
}

Set<AttributeIDRef> GeometryComponent::attribute_ids() const
{
  Set<AttributeIDRef> attributes;
  this->attribute_foreach(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &UNUSED(meta_data)) {
        attributes.add(attribute_id);
        return true;
      });
  return attributes;
}

bool GeometryComponent::attribute_foreach(const AttributeForeachCallback callback) const
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return true;
  }

  /* Keep track handled attribute names to make sure that we do not return the same name twice. */
  Set<std::string> handled_attribute_names;

  for (const BuiltinAttributeProvider *provider :
       providers->builtin_attribute_providers().values()) {
    if (provider->exists(*this)) {
      AttributeMetaData meta_data{provider->domain(), provider->data_type()};
      if (!callback(provider->name(), meta_data)) {
        return false;
      }
      handled_attribute_names.add_new(provider->name());
    }
  }
  for (const DynamicAttributesProvider *provider : providers->dynamic_attribute_providers()) {
    const bool continue_loop = provider->foreach_attribute(
        *this, [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
          if (attribute_id.is_anonymous() || handled_attribute_names.add(attribute_id.name())) {
            return callback(attribute_id, meta_data);
          }
          return true;
        });
    if (!continue_loop) {
      return false;
    }
  }

  return true;
}

bool GeometryComponent::attribute_exists(const AttributeIDRef &attribute_id) const
{
  blender::bke::ReadAttributeLookup attribute = this->attribute_try_get_for_read(attribute_id);
  if (attribute) {
    return true;
  }
  return false;
}

std::optional<AttributeMetaData> GeometryComponent::attribute_get_meta_data(
    const AttributeIDRef &attribute_id) const
{
  std::optional<AttributeMetaData> result{std::nullopt};
  this->attribute_foreach(
      [&](const AttributeIDRef &current_attribute_id, const AttributeMetaData &meta_data) {
        if (attribute_id == current_attribute_id) {
          result = meta_data;
          return false;
        }
        return true;
      });
  return result;
}

static blender::GVArray try_adapt_data_type(blender::GVArray varray,
                                            const blender::CPPType &to_type)
{
  const blender::bke::DataTypeConversions &conversions =
      blender::bke::get_implicit_type_conversions();
  return conversions.try_convert(std::move(varray), to_type);
}

blender::GVArray GeometryComponent::attribute_try_get_for_read(
    const AttributeIDRef &attribute_id,
    const eAttrDomain domain,
    const eCustomDataType data_type) const
{
  blender::bke::ReadAttributeLookup attribute = this->attribute_try_get_for_read(attribute_id);
  if (!attribute) {
    return {};
  }

  blender::GVArray varray = std::move(attribute.varray);
  if (!ELEM(domain, ATTR_DOMAIN_AUTO, attribute.domain)) {
    varray = this->attribute_try_adapt_domain(std::move(varray), attribute.domain, domain);
    if (!varray) {
      return {};
    }
  }

  const blender::CPPType *cpp_type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);
  if (varray.type() != *cpp_type) {
    varray = try_adapt_data_type(std::move(varray), *cpp_type);
    if (!varray) {
      return {};
    }
  }

  return varray;
}

blender::GVArray GeometryComponent::attribute_try_get_for_read(const AttributeIDRef &attribute_id,
                                                               const eAttrDomain domain) const
{
  if (!this->attribute_domain_supported(domain)) {
    return {};
  }

  blender::bke::ReadAttributeLookup attribute = this->attribute_try_get_for_read(attribute_id);
  if (!attribute) {
    return {};
  }

  if (attribute.domain != domain) {
    return this->attribute_try_adapt_domain(std::move(attribute.varray), attribute.domain, domain);
  }

  return std::move(attribute.varray);
}

blender::bke::ReadAttributeLookup GeometryComponent::attribute_try_get_for_read(
    const AttributeIDRef &attribute_id, const eCustomDataType data_type) const
{
  blender::bke::ReadAttributeLookup attribute = this->attribute_try_get_for_read(attribute_id);
  if (!attribute) {
    return {};
  }
  const blender::CPPType *type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(type != nullptr);
  if (attribute.varray.type() == *type) {
    return attribute;
  }
  const blender::bke::DataTypeConversions &conversions =
      blender::bke::get_implicit_type_conversions();
  return {conversions.try_convert(std::move(attribute.varray), *type), attribute.domain};
}

blender::GVArray GeometryComponent::attribute_get_for_read(const AttributeIDRef &attribute_id,
                                                           const eAttrDomain domain,
                                                           const eCustomDataType data_type,
                                                           const void *default_value) const
{
  blender::GVArray varray = this->attribute_try_get_for_read(attribute_id, domain, data_type);
  if (varray) {
    return varray;
  }
  const blender::CPPType *type = blender::bke::custom_data_type_to_cpp_type(data_type);
  if (default_value == nullptr) {
    default_value = type->default_value();
  }
  const int domain_num = this->attribute_domain_num(domain);
  return blender::GVArray::ForSingle(*type, domain_num, default_value);
}

class GVMutableAttribute_For_OutputAttribute : public blender::GVArrayImpl_For_GSpan {
 public:
  GeometryComponent *component;
  std::string attribute_name;
  blender::bke::WeakAnonymousAttributeID anonymous_attribute_id;

  GVMutableAttribute_For_OutputAttribute(GMutableSpan data,
                                         GeometryComponent &component,
                                         const AttributeIDRef &attribute_id)
      : blender::GVArrayImpl_For_GSpan(data), component(&component)
  {
    if (attribute_id.is_named()) {
      this->attribute_name = attribute_id.name();
    }
    else {
      const AnonymousAttributeID *anonymous_id = &attribute_id.anonymous_id();
      BKE_anonymous_attribute_id_increment_weak(anonymous_id);
      this->anonymous_attribute_id = blender::bke::WeakAnonymousAttributeID{anonymous_id};
    }
  }

  ~GVMutableAttribute_For_OutputAttribute() override
  {
    type_->destruct_n(data_, size_);
    MEM_freeN(data_);
  }
};

static void save_output_attribute(OutputAttribute &output_attribute)
{
  using namespace blender;
  using namespace blender::fn;
  using namespace blender::bke;

  GVMutableAttribute_For_OutputAttribute &varray =
      dynamic_cast<GVMutableAttribute_For_OutputAttribute &>(
          *output_attribute.varray().get_implementation());

  GeometryComponent &component = *varray.component;
  AttributeIDRef attribute_id;
  if (!varray.attribute_name.empty()) {
    attribute_id = varray.attribute_name;
  }
  else {
    attribute_id = varray.anonymous_attribute_id.extract();
  }
  const eAttrDomain domain = output_attribute.domain();
  const eCustomDataType data_type = output_attribute.custom_data_type();
  const CPPType &cpp_type = output_attribute.cpp_type();

  component.attribute_try_delete(attribute_id);
  if (!component.attribute_try_create(attribute_id, domain, data_type, AttributeInitDefault())) {
    if (!varray.attribute_name.empty()) {
      CLOG_WARN(&LOG,
                "Could not create the '%s' attribute with type '%s'.",
                varray.attribute_name.c_str(),
                cpp_type.name().c_str());
    }
    return;
  }
  WriteAttributeLookup write_attribute = component.attribute_try_get_for_write(attribute_id);
  BUFFER_FOR_CPP_TYPE_VALUE(varray.type(), buffer);
  for (const int i : IndexRange(varray.size())) {
    varray.get(i, buffer);
    write_attribute.varray.set_by_relocate(i, buffer);
  }
  if (write_attribute.tag_modified_fn) {
    write_attribute.tag_modified_fn();
  }
}

static std::function<void(OutputAttribute &)> get_simple_output_attribute_save_method(
    const blender::bke::WriteAttributeLookup &attribute)
{
  if (!attribute.tag_modified_fn) {
    return {};
  }
  return [tag_modified_fn = attribute.tag_modified_fn](OutputAttribute &UNUSED(attribute)) {
    tag_modified_fn();
  };
}

static OutputAttribute create_output_attribute(GeometryComponent &component,
                                               const AttributeIDRef &attribute_id,
                                               const eAttrDomain domain,
                                               const eCustomDataType data_type,
                                               const bool ignore_old_values,
                                               const void *default_value)
{
  using namespace blender;
  using namespace blender::fn;
  using namespace blender::bke;

  if (!attribute_id) {
    return {};
  }

  const CPPType *cpp_type = custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);
  const DataTypeConversions &conversions = get_implicit_type_conversions();

  if (component.attribute_is_builtin(attribute_id)) {
    const StringRef attribute_name = attribute_id.name();
    WriteAttributeLookup attribute = component.attribute_try_get_for_write(attribute_name);
    if (!attribute) {
      if (default_value) {
        const int64_t domain_num = component.attribute_domain_num(domain);
        component.attribute_try_create_builtin(
            attribute_name,
            AttributeInitVArray(GVArray::ForSingleRef(*cpp_type, domain_num, default_value)));
      }
      else {
        component.attribute_try_create_builtin(attribute_name, AttributeInitDefault());
      }
      attribute = component.attribute_try_get_for_write(attribute_name);
      if (!attribute) {
        /* Builtin attribute does not exist and can't be created. */
        return {};
      }
    }
    if (attribute.domain != domain) {
      /* Builtin attribute is on different domain. */
      return {};
    }
    GVMutableArray varray = std::move(attribute.varray);
    if (varray.type() == *cpp_type) {
      /* Builtin attribute matches exactly. */
      return OutputAttribute(std::move(varray),
                             domain,
                             get_simple_output_attribute_save_method(attribute),
                             ignore_old_values);
    }
    /* Builtin attribute is on the same domain but has a different data type. */
    varray = conversions.try_convert(std::move(varray), *cpp_type);
    return OutputAttribute(std::move(varray),
                           domain,
                           get_simple_output_attribute_save_method(attribute),
                           ignore_old_values);
  }

  const int domain_num = component.attribute_domain_num(domain);

  WriteAttributeLookup attribute = component.attribute_try_get_for_write(attribute_id);
  if (!attribute) {
    if (default_value) {
      component.attribute_try_create(
          attribute_id,
          domain,
          data_type,
          AttributeInitVArray(GVArray::ForSingleRef(*cpp_type, domain_num, default_value)));
    }
    else {
      component.attribute_try_create(attribute_id, domain, data_type, AttributeInitDefault());
    }

    attribute = component.attribute_try_get_for_write(attribute_id);
    if (!attribute) {
      /* Can't create the attribute. */
      return {};
    }
  }
  if (attribute.domain == domain && attribute.varray.type() == *cpp_type) {
    /* Existing generic attribute matches exactly. */

    return OutputAttribute(std::move(attribute.varray),
                           domain,
                           get_simple_output_attribute_save_method(attribute),
                           ignore_old_values);
  }

  /* Allocate a new array that lives next to the existing attribute. It will overwrite the existing
   * attribute after processing is done. */
  void *data = MEM_mallocN_aligned(cpp_type->size() * domain_num, cpp_type->alignment(), __func__);
  if (ignore_old_values) {
    /* This does nothing for trivially constructible types, but is necessary for correctness. */
    cpp_type->default_construct_n(data, domain);
  }
  else {
    /* Fill the temporary array with values from the existing attribute. */
    GVArray old_varray = component.attribute_get_for_read(
        attribute_id, domain, data_type, default_value);
    old_varray.materialize_to_uninitialized(IndexRange(domain_num), data);
  }
  GVMutableArray varray = GVMutableArray::For<GVMutableAttribute_For_OutputAttribute>(
      GMutableSpan{*cpp_type, data, domain_num}, component, attribute_id);

  return OutputAttribute(std::move(varray), domain, save_output_attribute, true);
}

OutputAttribute GeometryComponent::attribute_try_get_for_output(const AttributeIDRef &attribute_id,
                                                                const eAttrDomain domain,
                                                                const eCustomDataType data_type,
                                                                const void *default_value)
{
  return create_output_attribute(*this, attribute_id, domain, data_type, false, default_value);
}

OutputAttribute GeometryComponent::attribute_try_get_for_output_only(
    const AttributeIDRef &attribute_id, const eAttrDomain domain, const eCustomDataType data_type)
{
  return create_output_attribute(*this, attribute_id, domain, data_type, true, nullptr);
}

namespace blender::bke {

GVArray GeometryFieldInput::get_varray_for_context(const fn::FieldContext &context,
                                                   IndexMask mask,
                                                   ResourceScope &UNUSED(scope)) const
{
  if (const GeometryComponentFieldContext *geometry_context =
          dynamic_cast<const GeometryComponentFieldContext *>(&context)) {
    const GeometryComponent &component = geometry_context->geometry_component();
    const eAttrDomain domain = geometry_context->domain();
    return this->get_varray_for_context(component, domain, mask);
  }
  return {};
}

GVArray AttributeFieldInput::get_varray_for_context(const GeometryComponent &component,
                                                    const eAttrDomain domain,
                                                    IndexMask UNUSED(mask)) const
{
  const eCustomDataType data_type = cpp_type_to_custom_data_type(*type_);
  return component.attribute_try_get_for_read(name_, domain, data_type);
}

std::string AttributeFieldInput::socket_inspection_name() const
{
  std::stringstream ss;
  ss << '"' << name_ << '"' << TIP_(" attribute from geometry");
  return ss.str();
}

uint64_t AttributeFieldInput::hash() const
{
  return get_default_hash_2(name_, type_);
}

bool AttributeFieldInput::is_equal_to(const fn::FieldNode &other) const
{
  if (const AttributeFieldInput *other_typed = dynamic_cast<const AttributeFieldInput *>(&other)) {
    return name_ == other_typed->name_ && type_ == other_typed->type_;
  }
  return false;
}

static StringRef get_random_id_attribute_name(const eAttrDomain domain)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT:
    case ATTR_DOMAIN_INSTANCE:
      return "id";
    default:
      return "";
  }
}

GVArray IDAttributeFieldInput::get_varray_for_context(const GeometryComponent &component,
                                                      const eAttrDomain domain,
                                                      IndexMask mask) const
{

  const StringRef name = get_random_id_attribute_name(domain);
  GVArray attribute = component.attribute_try_get_for_read(name, domain, CD_PROP_INT32);
  if (attribute) {
    BLI_assert(attribute.size() == component.attribute_domain_num(domain));
    return attribute;
  }

  /* Use the index as the fallback if no random ID attribute exists. */
  return fn::IndexFieldInput::get_index_varray(mask);
}

std::string IDAttributeFieldInput::socket_inspection_name() const
{
  return TIP_("ID / Index");
}

uint64_t IDAttributeFieldInput::hash() const
{
  /* All random ID attribute inputs are the same within the same evaluation context. */
  return 92386459827;
}

bool IDAttributeFieldInput::is_equal_to(const fn::FieldNode &other) const
{
  /* All random ID attribute inputs are the same within the same evaluation context. */
  return dynamic_cast<const IDAttributeFieldInput *>(&other) != nullptr;
}

GVArray AnonymousAttributeFieldInput::get_varray_for_context(const GeometryComponent &component,
                                                             const eAttrDomain domain,
                                                             IndexMask UNUSED(mask)) const
{
  const eCustomDataType data_type = cpp_type_to_custom_data_type(*type_);
  return component.attribute_try_get_for_read(anonymous_id_.get(), domain, data_type);
}

std::string AnonymousAttributeFieldInput::socket_inspection_name() const
{
  std::stringstream ss;
  ss << '"' << debug_name_ << '"' << TIP_(" from ") << producer_name_;
  return ss.str();
}

uint64_t AnonymousAttributeFieldInput::hash() const
{
  return get_default_hash_2(anonymous_id_.get(), type_);
}

bool AnonymousAttributeFieldInput::is_equal_to(const fn::FieldNode &other) const
{
  if (const AnonymousAttributeFieldInput *other_typed =
          dynamic_cast<const AnonymousAttributeFieldInput *>(&other)) {
    return anonymous_id_.get() == other_typed->anonymous_id_.get() && type_ == other_typed->type_;
  }
  return false;
}

}  // namespace blender::bke

/** \} */
