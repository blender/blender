/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <utility>

#include "BKE_attribute_access.hh"
#include "BKE_attribute_math.hh"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.h"
#include "BKE_pointcloud.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_span.hh"
#include "BLI_threads.h"

#include "CLG_log.h"

#include "NOD_node_tree_multi_function.hh"

#include "attribute_access_intern.hh"

static CLG_LogRef LOG = {"bke.attribute_access"};

using blender::float3;
using blender::Set;
using blender::StringRef;
using blender::StringRefNull;
using blender::bke::ReadAttributePtr;
using blender::bke::WriteAttributePtr;
using blender::fn::GMutableSpan;

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Attribute Accessor implementations
 * \{ */

ReadAttribute::~ReadAttribute()
{
  if (array_is_temporary_ && array_buffer_ != nullptr) {
    cpp_type_.destruct_n(array_buffer_, size_);
    MEM_freeN(array_buffer_);
  }
}

fn::GSpan ReadAttribute::get_span() const
{
  if (size_ == 0) {
    return fn::GSpan(cpp_type_);
  }
  if (array_buffer_ == nullptr) {
    std::lock_guard lock{span_mutex_};
    if (array_buffer_ == nullptr) {
      this->initialize_span();
    }
  }
  return fn::GSpan(cpp_type_, array_buffer_, size_);
}

void ReadAttribute::initialize_span() const
{
  const int element_size = cpp_type_.size();
  array_buffer_ = MEM_mallocN_aligned(size_ * element_size, cpp_type_.alignment(), __func__);
  array_is_temporary_ = true;
  for (const int i : IndexRange(size_)) {
    this->get_internal(i, POINTER_OFFSET(array_buffer_, i * element_size));
  }
}

WriteAttribute::~WriteAttribute()
{
  if (array_should_be_applied_) {
    CLOG_ERROR(&LOG, "Forgot to call apply_span.");
  }
  if (array_is_temporary_ && array_buffer_ != nullptr) {
    cpp_type_.destruct_n(array_buffer_, size_);
    MEM_freeN(array_buffer_);
  }
}

/**
 * Get a mutable span that can be modified. When all modifications to the attribute are done,
 * #apply_span should be called. */
fn::GMutableSpan WriteAttribute::get_span()
{
  if (size_ == 0) {
    return fn::GMutableSpan(cpp_type_);
  }
  if (array_buffer_ == nullptr) {
    this->initialize_span(false);
  }
  array_should_be_applied_ = true;
  return fn::GMutableSpan(cpp_type_, array_buffer_, size_);
}

fn::GMutableSpan WriteAttribute::get_span_for_write_only()
{
  if (size_ == 0) {
    return fn::GMutableSpan(cpp_type_);
  }
  if (array_buffer_ == nullptr) {
    this->initialize_span(true);
  }
  array_should_be_applied_ = true;
  return fn::GMutableSpan(cpp_type_, array_buffer_, size_);
}

void WriteAttribute::initialize_span(const bool write_only)
{
  const int element_size = cpp_type_.size();
  array_buffer_ = MEM_mallocN_aligned(element_size * size_, cpp_type_.alignment(), __func__);
  array_is_temporary_ = true;
  if (write_only) {
    /* This does nothing for trivial types, but is necessary for general correctness. */
    cpp_type_.construct_default_n(array_buffer_, size_);
  }
  else {
    for (const int i : IndexRange(size_)) {
      this->get(i, POINTER_OFFSET(array_buffer_, i * element_size));
    }
  }
}

void WriteAttribute::apply_span()
{
  this->apply_span_if_necessary();
  array_should_be_applied_ = false;
}

void WriteAttribute::apply_span_if_necessary()
{
  /* Only works when the span has been initialized beforehand. */
  BLI_assert(array_buffer_ != nullptr);

  const int element_size = cpp_type_.size();
  for (const int i : IndexRange(size_)) {
    this->set_internal(i, POINTER_OFFSET(array_buffer_, i * element_size));
  }
}

/* This is used by the #OutputAttributePtr class. */
class TemporaryWriteAttribute final : public WriteAttribute {
 public:
  GMutableSpan data;
  GeometryComponent &component;
  std::string final_name;

  TemporaryWriteAttribute(AttributeDomain domain,
                          GMutableSpan data,
                          GeometryComponent &component,
                          std::string final_name)
      : WriteAttribute(domain, data.type(), data.size()),
        data(data),
        component(component),
        final_name(std::move(final_name))
  {
  }

  ~TemporaryWriteAttribute() override
  {
    if (data.data() != nullptr) {
      cpp_type_.destruct_n(data.data(), data.size());
      MEM_freeN(data.data());
    }
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    data.type().copy_to_uninitialized(data[index], r_value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    data.type().copy_to_initialized(value, data[index]);
  }

  void initialize_span(const bool UNUSED(write_only)) override
  {
    array_buffer_ = data.data();
    array_is_temporary_ = false;
  }

  void apply_span_if_necessary() override
  {
    /* Do nothing, because the span contains the attribute itself already. */
  }
};

class ConvertedReadAttribute final : public ReadAttribute {
 private:
  const CPPType &from_type_;
  const CPPType &to_type_;
  ReadAttributePtr base_attribute_;
  const nodes::DataTypeConversions &conversions_;

 public:
  ConvertedReadAttribute(ReadAttributePtr base_attribute, const CPPType &to_type)
      : ReadAttribute(base_attribute->domain(), to_type, base_attribute->size()),
        from_type_(base_attribute->cpp_type()),
        to_type_(to_type),
        base_attribute_(std::move(base_attribute)),
        conversions_(nodes::get_implicit_type_conversions())
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    BUFFER_FOR_CPP_TYPE_VALUE(from_type_, buffer);
    base_attribute_->get(index, buffer);
    conversions_.convert(from_type_, to_type_, buffer, r_value);
  }
};

/** \} */

const blender::fn::CPPType *custom_data_type_to_cpp_type(const CustomDataType type)
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
    case CD_PROP_COLOR:
      return &CPPType::get<Color4f>();
    case CD_PROP_BOOL:
      return &CPPType::get<bool>();
    default:
      return nullptr;
  }
  return nullptr;
}

CustomDataType cpp_type_to_custom_data_type(const blender::fn::CPPType &type)
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
  if (type.is<Color4f>()) {
    return CD_PROP_COLOR;
  }
  if (type.is<bool>()) {
    return CD_PROP_BOOL;
  }
  return static_cast<CustomDataType>(-1);
}

static int attribute_data_type_complexity(const CustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_BOOL:
      return 0;
    case CD_PROP_INT32:
      return 1;
    case CD_PROP_FLOAT:
      return 2;
    case CD_PROP_FLOAT2:
      return 3;
    case CD_PROP_FLOAT3:
      return 4;
    case CD_PROP_COLOR:
      return 5;
#if 0 /* These attribute types are not supported yet. */
    case CD_MLOOPCOL:
      return 3;
    case CD_PROP_STRING:
      return 6;
#endif
    default:
      /* Only accept "generic" custom data types used by the attribute system. */
      BLI_assert(false);
      return 0;
  }
}

CustomDataType attribute_data_type_highest_complexity(Span<CustomDataType> data_types)
{
  int highest_complexity = INT_MIN;
  CustomDataType most_complex_type = CD_PROP_COLOR;

  for (const CustomDataType data_type : data_types) {
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
static int attribute_domain_priority(const AttributeDomain domain)
{
  switch (domain) {
#if 0
    case ATTR_DOMAIN_CURVE:
      return 0;
#endif
    case ATTR_DOMAIN_POLYGON:
      return 1;
    case ATTR_DOMAIN_EDGE:
      return 2;
    case ATTR_DOMAIN_POINT:
      return 3;
    case ATTR_DOMAIN_CORNER:
      return 4;
    default:
      /* Domain not supported in nodes yet. */
      BLI_assert(false);
      return 0;
  }
}

/**
 * Domains with a higher "information density" have a higher priority, in order
 * to choose a domain that will not lose data through domain conversion.
 */
AttributeDomain attribute_domain_highest_priority(Span<AttributeDomain> domains)
{
  int highest_priority = INT_MIN;
  AttributeDomain highest_priority_domain = ATTR_DOMAIN_CORNER;

  for (const AttributeDomain domain : domains) {
    const int priority = attribute_domain_priority(domain);
    if (priority > highest_priority) {
      highest_priority = priority;
      highest_priority_domain = domain;
    }
  }

  return highest_priority_domain;
}

ReadAttributePtr BuiltinCustomDataLayerProvider::try_get_for_read(
    const GeometryComponent &component) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }

  if (update_on_read_ != nullptr) {
    update_on_read_(component);
  }

  const int domain_size = component.attribute_domain_size(domain_);
  const void *data = CustomData_get_layer(custom_data, stored_type_);
  if (data == nullptr) {
    return {};
  }
  return as_read_attribute_(data, domain_size);
}

WriteAttributePtr BuiltinCustomDataLayerProvider::try_get_for_write(
    GeometryComponent &component) const
{
  if (writable_ != Writable) {
    return {};
  }
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }
  const int domain_size = component.attribute_domain_size(domain_);
  void *data = CustomData_get_layer(custom_data, stored_type_);
  if (data == nullptr) {
    return {};
  }
  void *new_data = CustomData_duplicate_referenced_layer(custom_data, stored_type_, domain_size);
  if (data != new_data) {
    custom_data_access_.update_custom_data_pointers(component);
    data = new_data;
  }
  if (update_on_write_ != nullptr) {
    update_on_write_(component);
  }
  return as_write_attribute_(data, domain_size);
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

  const int domain_size = component.attribute_domain_size(domain_);
  const int layer_index = CustomData_get_layer_index(custom_data, stored_type_);
  const bool delete_success = CustomData_free_layer(
      custom_data, stored_type_, domain_size, layer_index);
  if (delete_success) {
    custom_data_access_.update_custom_data_pointers(component);
  }
  return delete_success;
}

bool BuiltinCustomDataLayerProvider::try_create(GeometryComponent &component) const
{
  if (createable_ != Creatable) {
    return false;
  }
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return false;
  }
  if (CustomData_get_layer(custom_data, stored_type_) != nullptr) {
    /* Exists already. */
    return false;
  }
  const int domain_size = component.attribute_domain_size(domain_);
  const void *data = CustomData_add_layer(
      custom_data, stored_type_, CD_DEFAULT, nullptr, domain_size);
  const bool success = data != nullptr;
  if (success) {
    custom_data_access_.update_custom_data_pointers(component);
  }
  return success;
}

bool BuiltinCustomDataLayerProvider::exists(const GeometryComponent &component) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
  if (custom_data == nullptr) {
    return false;
  }
  const void *data = CustomData_get_layer(custom_data, stored_type_);
  return data != nullptr;
}

ReadAttributePtr CustomDataAttributeProvider::try_get_for_read(
    const GeometryComponent &component, const StringRef attribute_name) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }
  const int domain_size = component.attribute_domain_size(domain_);
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    if (layer.name != attribute_name) {
      continue;
    }
    const CustomDataType data_type = (CustomDataType)layer.type;
    switch (data_type) {
      case CD_PROP_FLOAT:
        return this->layer_to_read_attribute<float>(layer, domain_size);
      case CD_PROP_FLOAT2:
        return this->layer_to_read_attribute<float2>(layer, domain_size);
      case CD_PROP_FLOAT3:
        return this->layer_to_read_attribute<float3>(layer, domain_size);
      case CD_PROP_INT32:
        return this->layer_to_read_attribute<int>(layer, domain_size);
      case CD_PROP_COLOR:
        return this->layer_to_read_attribute<Color4f>(layer, domain_size);
      case CD_PROP_BOOL:
        return this->layer_to_read_attribute<bool>(layer, domain_size);
      default:
        break;
    }
  }
  return {};
}

WriteAttributePtr CustomDataAttributeProvider::try_get_for_write(
    GeometryComponent &component, const StringRef attribute_name) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }
  const int domain_size = component.attribute_domain_size(domain_);
  for (CustomDataLayer &layer : MutableSpan(custom_data->layers, custom_data->totlayer)) {
    if (layer.name != attribute_name) {
      continue;
    }
    CustomData_duplicate_referenced_layer_named(custom_data, layer.type, layer.name, domain_size);
    const CustomDataType data_type = (CustomDataType)layer.type;
    switch (data_type) {
      case CD_PROP_FLOAT:
        return this->layer_to_write_attribute<float>(layer, domain_size);
      case CD_PROP_FLOAT2:
        return this->layer_to_write_attribute<float2>(layer, domain_size);
      case CD_PROP_FLOAT3:
        return this->layer_to_write_attribute<float3>(layer, domain_size);
      case CD_PROP_INT32:
        return this->layer_to_write_attribute<int>(layer, domain_size);
      case CD_PROP_COLOR:
        return this->layer_to_write_attribute<Color4f>(layer, domain_size);
      case CD_PROP_BOOL:
        return this->layer_to_write_attribute<bool>(layer, domain_size);
      default:
        break;
    }
  }
  return {};
}

bool CustomDataAttributeProvider::try_delete(GeometryComponent &component,
                                             const StringRef attribute_name) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return false;
  }
  const int domain_size = component.attribute_domain_size(domain_);
  for (const int i : IndexRange(custom_data->totlayer)) {
    const CustomDataLayer &layer = custom_data->layers[i];
    if (this->type_is_supported((CustomDataType)layer.type) && layer.name == attribute_name) {
      CustomData_free_layer(custom_data, layer.type, domain_size, i);
      return true;
    }
  }
  return false;
}

bool CustomDataAttributeProvider::try_create(GeometryComponent &component,
                                             const StringRef attribute_name,
                                             const AttributeDomain domain,
                                             const CustomDataType data_type) const
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
    if (layer.name == attribute_name) {
      return false;
    }
  }
  const int domain_size = component.attribute_domain_size(domain_);
  char attribute_name_c[MAX_NAME];
  attribute_name.copy(attribute_name_c);
  CustomData_add_layer_named(
      custom_data, data_type, CD_DEFAULT, nullptr, domain_size, attribute_name_c);
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
    const CustomDataType data_type = (CustomDataType)layer.type;
    if (this->type_is_supported(data_type)) {
      AttributeMetaData meta_data{domain_, data_type};
      if (!callback(layer.name, meta_data)) {
        return false;
      }
    }
  }
  return true;
}

ReadAttributePtr NamedLegacyCustomDataProvider::try_get_for_read(
    const GeometryComponent &component, const StringRef attribute_name) const
{
  const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }
  for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
    if (layer.type == stored_type_) {
      if (layer.name == attribute_name) {
        const int domain_size = component.attribute_domain_size(domain_);
        return as_read_attribute_(layer.data, domain_size);
      }
    }
  }
  return {};
}

WriteAttributePtr NamedLegacyCustomDataProvider::try_get_for_write(
    GeometryComponent &component, const StringRef attribute_name) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return {};
  }
  for (CustomDataLayer &layer : MutableSpan(custom_data->layers, custom_data->totlayer)) {
    if (layer.type == stored_type_) {
      if (layer.name == attribute_name) {
        const int domain_size = component.attribute_domain_size(domain_);
        void *data_old = layer.data;
        void *data_new = CustomData_duplicate_referenced_layer_named(
            custom_data, stored_type_, layer.name, domain_size);
        if (data_old != data_new) {
          custom_data_access_.update_custom_data_pointers(component);
        }
        return as_write_attribute_(layer.data, domain_size);
      }
    }
  }
  return {};
}

bool NamedLegacyCustomDataProvider::try_delete(GeometryComponent &component,
                                               const StringRef attribute_name) const
{
  CustomData *custom_data = custom_data_access_.get_custom_data(component);
  if (custom_data == nullptr) {
    return false;
  }
  for (const int i : IndexRange(custom_data->totlayer)) {
    const CustomDataLayer &layer = custom_data->layers[i];
    if (layer.type == stored_type_) {
      if (layer.name == attribute_name) {
        const int domain_size = component.attribute_domain_size(domain_);
        CustomData_free_layer(custom_data, stored_type_, domain_size, i);
        custom_data_access_.update_custom_data_pointers(component);
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

void NamedLegacyCustomDataProvider::supported_domains(Vector<AttributeDomain> &r_domains) const
{
  r_domains.append_non_duplicates(domain_);
}

}  // namespace blender::bke

/* -------------------------------------------------------------------- */
/** \name Geometry Component
 * \{ */

const blender::bke::ComponentAttributeProviders *GeometryComponent::get_attribute_providers() const
{
  return nullptr;
}

bool GeometryComponent::attribute_domain_supported(const AttributeDomain domain) const
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return false;
  }
  return providers->supported_domains().contains(domain);
}

int GeometryComponent::attribute_domain_size(const AttributeDomain UNUSED(domain)) const
{
  BLI_assert(false);
  return 0;
}

ReadAttributePtr GeometryComponent::attribute_try_get_for_read(
    const StringRef attribute_name) const
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return {};
  }
  const BuiltinAttributeProvider *builtin_provider =
      providers->builtin_attribute_providers().lookup_default_as(attribute_name, nullptr);
  if (builtin_provider != nullptr) {
    return builtin_provider->try_get_for_read(*this);
  }
  for (const DynamicAttributesProvider *dynamic_provider :
       providers->dynamic_attribute_providers()) {
    ReadAttributePtr attribute = dynamic_provider->try_get_for_read(*this, attribute_name);
    if (attribute) {
      return attribute;
    }
  }
  return {};
}

ReadAttributePtr GeometryComponent::attribute_try_adapt_domain(
    ReadAttributePtr attribute, const AttributeDomain new_domain) const
{
  if (attribute && attribute->domain() == new_domain) {
    return attribute;
  }
  return {};
}

WriteAttributePtr GeometryComponent::attribute_try_get_for_write(const StringRef attribute_name)
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return {};
  }
  const BuiltinAttributeProvider *builtin_provider =
      providers->builtin_attribute_providers().lookup_default_as(attribute_name, nullptr);
  if (builtin_provider != nullptr) {
    return builtin_provider->try_get_for_write(*this);
  }
  for (const DynamicAttributesProvider *dynamic_provider :
       providers->dynamic_attribute_providers()) {
    WriteAttributePtr attribute = dynamic_provider->try_get_for_write(*this, attribute_name);
    if (attribute) {
      return attribute;
    }
  }
  return {};
}

bool GeometryComponent::attribute_try_delete(const StringRef attribute_name)
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return {};
  }
  const BuiltinAttributeProvider *builtin_provider =
      providers->builtin_attribute_providers().lookup_default_as(attribute_name, nullptr);
  if (builtin_provider != nullptr) {
    return builtin_provider->try_delete(*this);
  }
  bool success = false;
  for (const DynamicAttributesProvider *dynamic_provider :
       providers->dynamic_attribute_providers()) {
    success = dynamic_provider->try_delete(*this, attribute_name) || success;
  }
  return success;
}

bool GeometryComponent::attribute_try_create(const StringRef attribute_name,
                                             const AttributeDomain domain,
                                             const CustomDataType data_type)
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
  if (builtin_provider != nullptr) {
    if (builtin_provider->domain() != domain) {
      return false;
    }
    if (builtin_provider->data_type() != data_type) {
      return false;
    }
    return builtin_provider->try_create(*this);
  }
  for (const DynamicAttributesProvider *dynamic_provider :
       providers->dynamic_attribute_providers()) {
    if (dynamic_provider->try_create(*this, attribute_name, domain, data_type)) {
      return true;
    }
  }
  return false;
}

Set<std::string> GeometryComponent::attribute_names() const
{
  Set<std::string> attributes;
  this->attribute_foreach([&](StringRefNull name, const AttributeMetaData &UNUSED(meta_data)) {
    attributes.add(name);
    return true;
  });
  return attributes;
}

void GeometryComponent::attribute_foreach(const AttributeForeachCallback callback) const
{
  using namespace blender::bke;
  const ComponentAttributeProviders *providers = this->get_attribute_providers();
  if (providers == nullptr) {
    return;
  }

  /* Keep track handled attribute names to make sure that we do not return the same name twice. */
  Set<std::string> handled_attribute_names;

  for (const BuiltinAttributeProvider *provider :
       providers->builtin_attribute_providers().values()) {
    if (provider->exists(*this)) {
      AttributeMetaData meta_data{provider->domain(), provider->data_type()};
      if (!callback(provider->name(), meta_data)) {
        return;
      }
      handled_attribute_names.add_new(provider->name());
    }
  }
  for (const DynamicAttributesProvider *provider : providers->dynamic_attribute_providers()) {
    const bool continue_loop = provider->foreach_attribute(
        *this, [&](StringRefNull name, const AttributeMetaData &meta_data) {
          if (handled_attribute_names.add(name)) {
            return callback(name, meta_data);
          }
          return true;
        });
    if (!continue_loop) {
      return;
    }
  }
}

bool GeometryComponent::attribute_exists(const blender::StringRef attribute_name) const
{
  ReadAttributePtr attribute = this->attribute_try_get_for_read(attribute_name);
  if (attribute) {
    return true;
  }
  return false;
}

static ReadAttributePtr try_adapt_data_type(ReadAttributePtr attribute,
                                            const blender::fn::CPPType &to_type)
{
  const blender::fn::CPPType &from_type = attribute->cpp_type();
  if (from_type == to_type) {
    return attribute;
  }

  const blender::nodes::DataTypeConversions &conversions =
      blender::nodes::get_implicit_type_conversions();
  if (!conversions.is_convertible(from_type, to_type)) {
    return {};
  }

  return std::make_unique<blender::bke::ConvertedReadAttribute>(std::move(attribute), to_type);
}

ReadAttributePtr GeometryComponent::attribute_try_get_for_read(
    const StringRef attribute_name,
    const AttributeDomain domain,
    const CustomDataType data_type) const
{
  ReadAttributePtr attribute = this->attribute_try_get_for_read(attribute_name);
  if (!attribute) {
    return {};
  }

  if (attribute->domain() != domain) {
    attribute = this->attribute_try_adapt_domain(std::move(attribute), domain);
    if (!attribute) {
      return {};
    }
  }

  const blender::fn::CPPType *cpp_type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);
  if (attribute->cpp_type() != *cpp_type) {
    attribute = try_adapt_data_type(std::move(attribute), *cpp_type);
    if (!attribute) {
      return {};
    }
  }

  return attribute;
}

ReadAttributePtr GeometryComponent::attribute_try_get_for_read(const StringRef attribute_name,
                                                               const AttributeDomain domain) const
{
  if (!this->attribute_domain_supported(domain)) {
    return {};
  }

  ReadAttributePtr attribute = this->attribute_try_get_for_read(attribute_name);
  if (!attribute) {
    return {};
  }

  if (attribute->domain() != domain) {
    attribute = this->attribute_try_adapt_domain(std::move(attribute), domain);
    if (!attribute) {
      return {};
    }
  }

  return attribute;
}

ReadAttributePtr GeometryComponent::attribute_get_for_read(const StringRef attribute_name,
                                                           const AttributeDomain domain,
                                                           const CustomDataType data_type,
                                                           const void *default_value) const
{
  ReadAttributePtr attribute = this->attribute_try_get_for_read(attribute_name, domain, data_type);
  if (attribute) {
    return attribute;
  }
  return this->attribute_get_constant_for_read(domain, data_type, default_value);
}

blender::bke::ReadAttributePtr GeometryComponent::attribute_get_constant_for_read(
    const AttributeDomain domain, const CustomDataType data_type, const void *value) const
{
  BLI_assert(this->attribute_domain_supported(domain));
  const blender::fn::CPPType *cpp_type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);
  if (value == nullptr) {
    value = cpp_type->default_value();
  }
  const int domain_size = this->attribute_domain_size(domain);
  return std::make_unique<blender::bke::ConstantReadAttribute>(
      domain, domain_size, *cpp_type, value);
}

blender::bke::ReadAttributePtr GeometryComponent::attribute_get_constant_for_read_converted(
    const AttributeDomain domain,
    const CustomDataType in_data_type,
    const CustomDataType out_data_type,
    const void *value) const
{
  BLI_assert(this->attribute_domain_supported(domain));
  if (value == nullptr || in_data_type == out_data_type) {
    return this->attribute_get_constant_for_read(domain, out_data_type, value);
  }

  const blender::fn::CPPType *in_cpp_type = blender::bke::custom_data_type_to_cpp_type(
      in_data_type);
  const blender::fn::CPPType *out_cpp_type = blender::bke::custom_data_type_to_cpp_type(
      out_data_type);
  BLI_assert(in_cpp_type != nullptr);
  BLI_assert(out_cpp_type != nullptr);

  const blender::nodes::DataTypeConversions &conversions =
      blender::nodes::get_implicit_type_conversions();
  BLI_assert(conversions.is_convertible(*in_cpp_type, *out_cpp_type));

  void *out_value = alloca(out_cpp_type->size());
  conversions.convert(*in_cpp_type, *out_cpp_type, value, out_value);

  const int domain_size = this->attribute_domain_size(domain);
  blender::bke::ReadAttributePtr attribute = std::make_unique<blender::bke::ConstantReadAttribute>(
      domain, domain_size, *out_cpp_type, out_value);

  out_cpp_type->destruct(out_value);
  return attribute;
}

OutputAttributePtr GeometryComponent::attribute_try_get_for_output(const StringRef attribute_name,
                                                                   const AttributeDomain domain,
                                                                   const CustomDataType data_type,
                                                                   const void *default_value)
{
  const blender::fn::CPPType *cpp_type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);

  WriteAttributePtr attribute = this->attribute_try_get_for_write(attribute_name);

  /* If the attribute doesn't exist, make a new one with the correct type. */
  if (!attribute) {
    this->attribute_try_create(attribute_name, domain, data_type);
    attribute = this->attribute_try_get_for_write(attribute_name);
    if (attribute && default_value != nullptr) {
      void *data = attribute->get_span_for_write_only().data();
      cpp_type->fill_initialized(default_value, data, attribute->size());
      attribute->apply_span();
    }
    return OutputAttributePtr(std::move(attribute));
  }

  /* If an existing attribute has a matching domain and type, just use that. */
  if (attribute->domain() == domain && attribute->cpp_type() == *cpp_type) {
    return OutputAttributePtr(std::move(attribute));
  }

  /* Otherwise create a temporary buffer to use before saving the new attribute. */
  return OutputAttributePtr(*this, domain, attribute_name, data_type);
}

/* Construct from an attribute that already exists in the geometry component. */
OutputAttributePtr::OutputAttributePtr(WriteAttributePtr attribute)
    : attribute_(std::move(attribute))
{
}

/* Construct a temporary attribute that has to replace an existing one later on. */
OutputAttributePtr::OutputAttributePtr(GeometryComponent &component,
                                       AttributeDomain domain,
                                       std::string final_name,
                                       CustomDataType data_type)
{
  const blender::fn::CPPType *cpp_type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);

  const int domain_size = component.attribute_domain_size(domain);
  void *buffer = MEM_malloc_arrayN(domain_size, cpp_type->size(), __func__);
  GMutableSpan new_span{*cpp_type, buffer, domain_size};

  /* Copy converted values from conflicting attribute, in case the value is read.
   * TODO: An optimization could be to not do this, when the caller says that the attribute will
   * only be written. */
  ReadAttributePtr src_attribute = component.attribute_get_for_read(
      final_name, domain, data_type, nullptr);
  for (const int i : blender::IndexRange(domain_size)) {
    src_attribute->get(i, new_span[i]);
  }

  attribute_ = std::make_unique<blender::bke::TemporaryWriteAttribute>(
      domain, new_span, component, std::move(final_name));
}

/* Store the computed attribute. If it was stored from the beginning already, nothing is done. This
 * might delete another attribute with the same name. */
void OutputAttributePtr::save()
{
  if (!attribute_) {
    CLOG_WARN(&LOG, "Trying to save an attribute that does not exist anymore.");
    return;
  }

  blender::bke::TemporaryWriteAttribute *attribute =
      dynamic_cast<blender::bke::TemporaryWriteAttribute *>(attribute_.get());

  if (attribute == nullptr) {
    /* The attribute is saved already. */
    attribute_.reset();
    return;
  }

  StringRefNull name = attribute->final_name;
  const blender::fn::CPPType &cpp_type = attribute->cpp_type();

  /* Delete an existing attribute with the same name if necessary. */
  attribute->component.attribute_try_delete(name);

  if (!attribute->component.attribute_try_create(
          name, attribute_->domain(), attribute_->custom_data_type())) {
    /* Cannot create the target attribute for some reason. */
    CLOG_WARN(&LOG,
              "Creating the '%s' attribute with type '%s' failed.",
              name.c_str(),
              cpp_type.name().c_str());
    attribute_.reset();
    return;
  }

  WriteAttributePtr new_attribute = attribute->component.attribute_try_get_for_write(name);

  GMutableSpan temp_span = attribute->data;
  GMutableSpan new_span = new_attribute->get_span_for_write_only();
  BLI_assert(temp_span.size() == new_span.size());

  /* Currently we copy over the attribute. In the future we want to reuse the buffer. */
  cpp_type.move_to_initialized_n(temp_span.data(), new_span.data(), new_span.size());
  new_attribute->apply_span();

  attribute_.reset();
}

OutputAttributePtr::~OutputAttributePtr()
{
  if (attribute_) {
    CLOG_ERROR(&LOG, "Forgot to call #save or #apply_span_and_save.");
  }
}

/* Utility function to call #apply_span and #save in the right order. */
void OutputAttributePtr::apply_span_and_save()
{
  BLI_assert(attribute_);
  attribute_->apply_span();
  this->save();
}

/** \} */
