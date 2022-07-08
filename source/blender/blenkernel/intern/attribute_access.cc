/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <utility>

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

GVArray BuiltinCustomDataLayerProvider::try_get_for_read(const void *owner) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
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

  const int element_num = custom_data_access_.get_element_num(owner);
  return as_read_attribute_(data, element_num);
}

GAttributeWriter BuiltinCustomDataLayerProvider::try_get_for_write(void *owner) const
{
  if (writable_ != Writable) {
    return {};
  }
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return {};
  }
  const int element_num = custom_data_access_.get_element_num(owner);

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
        custom_data, stored_type_, name_.c_str(), element_num);
  }
  else {
    new_data = CustomData_duplicate_referenced_layer(custom_data, stored_type_, element_num);
  }

  if (data != new_data) {
    if (custom_data_access_.update_custom_data_pointers) {
      custom_data_access_.update_custom_data_pointers(owner);
    }
    data = new_data;
  }

  std::function<void()> tag_modified_fn;
  if (update_on_write_ != nullptr) {
    tag_modified_fn = [owner, update = update_on_write_]() { update(owner); };
  }

  return {as_write_attribute_(data, element_num), domain_, std::move(tag_modified_fn)};
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
  if (stored_as_named_attribute_) {
    if (CustomData_free_layer_named(custom_data, name_.c_str(), element_num)) {
      if (custom_data_access_.update_custom_data_pointers) {
        custom_data_access_.update_custom_data_pointers(owner);
      }
      return true;
    }
    return false;
  }

  const int layer_index = CustomData_get_layer_index(custom_data, stored_type_);
  if (CustomData_free_layer(custom_data, stored_type_, element_num, layer_index)) {
    if (custom_data_access_.update_custom_data_pointers) {
      custom_data_access_.update_custom_data_pointers(owner);
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
  bool success;
  if (stored_as_named_attribute_) {
    if (CustomData_get_layer_named(custom_data, data_type_, name_.c_str())) {
      /* Exists already. */
      return false;
    }
    success = add_custom_data_layer_from_attribute_init(
        name_, *custom_data, stored_type_, element_num, initializer);
  }
  else {
    if (CustomData_get_layer(custom_data, stored_type_) != nullptr) {
      /* Exists already. */
      return false;
    }
    success = add_builtin_type_custom_data_layer_from_init(
        *custom_data, stored_type_, element_num, initializer);
  }
  if (success) {
    if (custom_data_access_.update_custom_data_pointers) {
      custom_data_access_.update_custom_data_pointers(owner);
    }
  }
  return success;
}

bool BuiltinCustomDataLayerProvider::exists(const void *owner) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
  if (custom_data == nullptr) {
    return false;
  }
  if (stored_as_named_attribute_) {
    return CustomData_get_layer_named(custom_data, stored_type_, name_.c_str()) != nullptr;
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
    return {GVArray::ForSpan(data), domain_};
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
    if (attribute_id.is_named()) {
      CustomData_duplicate_referenced_layer_named(
          custom_data, layer.type, layer.name, element_num);
    }
    else {
      CustomData_duplicate_referenced_layer_anonymous(
          custom_data, layer.type, &attribute_id.anonymous_id(), element_num);
    }
    const CPPType *type = custom_data_type_to_cpp_type((eCustomDataType)layer.type);
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
        custom_data_layer_matches_attribute_id(layer, attribute_id)) {
      CustomData_free_layer(custom_data, layer.type, element_num, i);
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

GAttributeReader NamedLegacyCustomDataProvider::try_get_for_read(
    const void *owner, const AttributeIDRef &attribute_id) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
  if (custom_data == nullptr) {
    return {};
  }
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    if (layer.type == stored_type_) {
      if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
        const int domain_num = custom_data_access_.get_element_num(owner);
        return {as_read_attribute_(layer.data, domain_num), domain_};
      }
    }
  }
  return {};
}

GAttributeWriter NamedLegacyCustomDataProvider::try_get_for_write(
    void *owner, const AttributeIDRef &attribute_id) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return {};
  }
  for (CustomDataLayer &layer : MutableSpan(custom_data->layers, custom_data->totlayer)) {
    if (layer.type == stored_type_) {
      if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
        const int element_num = custom_data_access_.get_element_num(owner);
        void *data_old = layer.data;
        void *data_new = CustomData_duplicate_referenced_layer_named(
            custom_data, stored_type_, layer.name, element_num);
        if (data_old != data_new) {
          if (custom_data_access_.update_custom_data_pointers) {
            custom_data_access_.update_custom_data_pointers(owner);
          }
        }
        return {as_write_attribute_(layer.data, element_num), domain_};
      }
    }
  }
  return {};
}

bool NamedLegacyCustomDataProvider::try_delete(void *owner,
                                               const AttributeIDRef &attribute_id) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(owner);
  if (custom_data == nullptr) {
    return false;
  }
  for (const int i : IndexRange(custom_data->totlayer)) {
    const CustomDataLayer &layer = custom_data->layers[i];
    if (layer.type == stored_type_) {
      if (custom_data_layer_matches_attribute_id(layer, attribute_id)) {
        const int element_num = custom_data_access_.get_element_num(owner);
        CustomData_free_layer(custom_data, stored_type_, element_num, i);
        if (custom_data_access_.update_custom_data_pointers) {
          custom_data_access_.update_custom_data_pointers(owner);
        }
        return true;
      }
    }
  }
  return false;
}

bool NamedLegacyCustomDataProvider::foreach_attribute(
    const void *owner, const AttributeForeachCallback callback) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(owner);
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

/* -------------------------------------------------------------------- */
/** \name Geometry Component
 * \{ */

static blender::GVArray try_adapt_data_type(blender::GVArray varray,
                                            const blender::CPPType &to_type)
{
  const blender::bke::DataTypeConversions &conversions =
      blender::bke::get_implicit_type_conversions();
  return conversions.try_convert(std::move(varray), to_type);
}

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
  if (auto attributes = component.attributes()) {
    return attributes->lookup(name_, domain, data_type);
  }
  return {};
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
  if (auto attributes = component.attributes()) {
    if (GVArray attribute = attributes->lookup(name, domain, CD_PROP_INT32)) {
      return attribute;
    }
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
  return component.attributes()->lookup(anonymous_id_.get(), domain, data_type);
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

GVArray AttributeAccessor::lookup(const AttributeIDRef &attribute_id,
                                  const std::optional<eAttrDomain> domain,
                                  const std::optional<eCustomDataType> data_type) const
{
  GAttributeReader attribute = this->lookup(attribute_id);
  if (!attribute) {
    return {};
  }
  GVArray varray = std::move(attribute.varray);
  if (domain.has_value()) {
    if (attribute.domain != domain) {
      varray = this->adapt_domain(varray, attribute.domain, *domain);
      if (!varray) {
        return {};
      }
    }
  }
  if (data_type.has_value()) {
    const CPPType &type = *custom_data_type_to_cpp_type(*data_type);
    if (varray.type() != type) {
      varray = try_adapt_data_type(std::move(varray), type);
      if (!varray) {
        return {};
      }
    }
  }
  return varray;
}

GVArray AttributeAccessor::lookup_or_default(const AttributeIDRef &attribute_id,
                                             const eAttrDomain domain,
                                             const eCustomDataType data_type,
                                             const void *default_value) const
{
  GVArray varray = this->lookup(attribute_id, domain, data_type);
  if (varray) {
    return varray;
  }
  const CPPType &type = *custom_data_type_to_cpp_type(data_type);
  const int64_t domain_size = this->domain_size(domain);
  if (default_value == nullptr) {
    return GVArray::ForSingleRef(type, domain_size, type.default_value());
  }
  return GVArray::ForSingle(type, domain_size, default_value);
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
    this->remove(anonymous_ids.pop_last());
  }
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
  GAttributeWriter attribute = this->lookup_or_add_for_write(attribute_id, domain, data_type);
  if (attribute) {
    return GSpanAttributeWriter{std::move(attribute), false};
  }
  return {};
}

}  // namespace blender::bke

/** \} */
