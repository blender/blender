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

#include "CLG_log.h"

#include "NOD_node_tree_multi_function.hh"

static CLG_LogRef LOG = {"bke.attribute_access"};

using blender::float3;
using blender::Set;
using blender::StringRef;
using blender::StringRefNull;
using blender::bke::ReadAttributePtr;
using blender::bke::WriteAttributePtr;
using blender::fn::GMutableSpan;

/* Can't include BKE_object_deform.h right now, due to an enum forward declaration.  */
extern "C" MDeformVert *BKE_object_defgroup_data_create(ID *id);

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

class VertexWeightWriteAttribute final : public WriteAttribute {
 private:
  MDeformVert *dverts_;
  const int dvert_index_;

 public:
  VertexWeightWriteAttribute(MDeformVert *dverts, const int totvert, const int dvert_index)
      : WriteAttribute(ATTR_DOMAIN_POINT, CPPType::get<float>(), totvert),
        dverts_(dverts),
        dvert_index_(dvert_index)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    get_internal(dverts_, dvert_index_, index, r_value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    MDeformWeight *weight = BKE_defvert_ensure_index(&dverts_[index], dvert_index_);
    weight->weight = *reinterpret_cast<const float *>(value);
  }

  static void get_internal(const MDeformVert *dverts,
                           const int dvert_index,
                           const int64_t index,
                           void *r_value)
  {
    if (dverts == nullptr) {
      *(float *)r_value = 0.0f;
      return;
    }
    const MDeformVert &dvert = dverts[index];
    for (const MDeformWeight &weight : Span(dvert.dw, dvert.totweight)) {
      if (weight.def_nr == dvert_index) {
        *(float *)r_value = weight.weight;
        return;
      }
    }
    *(float *)r_value = 0.0f;
  }
};

class VertexWeightReadAttribute final : public ReadAttribute {
 private:
  const MDeformVert *dverts_;
  const int dvert_index_;

 public:
  VertexWeightReadAttribute(const MDeformVert *dverts, const int totvert, const int dvert_index)
      : ReadAttribute(ATTR_DOMAIN_POINT, CPPType::get<float>(), totvert),
        dverts_(dverts),
        dvert_index_(dvert_index)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    VertexWeightWriteAttribute::get_internal(dverts_, dvert_index_, index, r_value);
  }
};

template<typename T> class ArrayWriteAttribute final : public WriteAttribute {
 private:
  MutableSpan<T> data_;

 public:
  ArrayWriteAttribute(AttributeDomain domain, MutableSpan<T> data)
      : WriteAttribute(domain, CPPType::get<T>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    data_[index] = *reinterpret_cast<const T *>(value);
  }

  void initialize_span(const bool UNUSED(write_only)) override
  {
    array_buffer_ = data_.data();
    array_is_temporary_ = false;
  }

  void apply_span_if_necessary() override
  {
    /* Do nothing, because the span contains the attribute itself already. */
  }
};

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

template<typename T> class ArrayReadAttribute final : public ReadAttribute {
 private:
  Span<T> data_;

 public:
  ArrayReadAttribute(AttributeDomain domain, Span<T> data)
      : ReadAttribute(domain, CPPType::get<T>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  void initialize_span() const override
  {
    /* The data will not be modified, so this const_cast is fine. */
    array_buffer_ = const_cast<T *>(data_.data());
    array_is_temporary_ = false;
  }
};

template<typename T> class OwnedArrayReadAttribute final : public ReadAttribute {
 private:
  Array<T> data_;

 public:
  OwnedArrayReadAttribute(AttributeDomain domain, Array<T> data)
      : ReadAttribute(domain, CPPType::get<T>(), data.size()), data_(std::move(data))
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  void initialize_span() const override
  {
    /* The data will not be modified, so this const_cast is fine. */
    array_buffer_ = const_cast<T *>(data_.data());
    array_is_temporary_ = false;
  }
};

template<typename StructT,
         typename ElemT,
         ElemT (*GetFunc)(const StructT &),
         void (*SetFunc)(StructT &, const ElemT &)>
class DerivedArrayWriteAttribute final : public WriteAttribute {
 private:
  MutableSpan<StructT> data_;

 public:
  DerivedArrayWriteAttribute(AttributeDomain domain, MutableSpan<StructT> data)
      : WriteAttribute(domain, CPPType::get<ElemT>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = GetFunc(struct_value);
    new (r_value) ElemT(value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    StructT &struct_value = data_[index];
    const ElemT &typed_value = *reinterpret_cast<const ElemT *>(value);
    SetFunc(struct_value, typed_value);
  }
};

template<typename StructT, typename ElemT, ElemT (*GetFunc)(const StructT &)>
class DerivedArrayReadAttribute final : public ReadAttribute {
 private:
  Span<StructT> data_;

 public:
  DerivedArrayReadAttribute(AttributeDomain domain, Span<StructT> data)
      : ReadAttribute(domain, CPPType::get<ElemT>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = GetFunc(struct_value);
    new (r_value) ElemT(value);
  }
};

class ConstantReadAttribute final : public ReadAttribute {
 private:
  void *value_;

 public:
  ConstantReadAttribute(AttributeDomain domain,
                        const int64_t size,
                        const CPPType &type,
                        const void *value)
      : ReadAttribute(domain, type, size)
  {
    value_ = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
    type.copy_to_uninitialized(value, value_);
  }

  ~ConstantReadAttribute() override
  {
    this->cpp_type_.destruct(value_);
    MEM_freeN(value_);
  }

  void get_internal(const int64_t UNUSED(index), void *r_value) const override
  {
    this->cpp_type_.copy_to_uninitialized(value_, r_value);
  }

  void initialize_span() const override
  {
    const int element_size = cpp_type_.size();
    array_buffer_ = MEM_mallocN_aligned(size_ * element_size, cpp_type_.alignment(), __func__);
    array_is_temporary_ = true;
    cpp_type_.fill_uninitialized(value_, array_buffer_, size_);
  }
};

class ConvertedReadAttribute final : public ReadAttribute {
 private:
  const CPPType &from_type_;
  const CPPType &to_type_;
  ReadAttributePtr base_attribute_;
  const nodes::DataTypeConversions &conversions_;

  static constexpr int MaxValueSize = 64;
  static constexpr int MaxValueAlignment = 64;

 public:
  ConvertedReadAttribute(ReadAttributePtr base_attribute, const CPPType &to_type)
      : ReadAttribute(base_attribute->domain(), to_type, base_attribute->size()),
        from_type_(base_attribute->cpp_type()),
        to_type_(to_type),
        base_attribute_(std::move(base_attribute)),
        conversions_(nodes::get_implicit_type_conversions())
  {
    if (from_type_.size() > MaxValueSize || from_type_.alignment() > MaxValueAlignment) {
      throw std::runtime_error(
          "type is larger than expected, the buffer size has to be increased");
    }
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    AlignedBuffer<MaxValueSize, MaxValueAlignment> buffer;
    base_attribute_->get(index, buffer.ptr());
    conversions_.convert(from_type_, to_type_, buffer.ptr(), r_value);
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

/**
 * A #BuiltinAttributeProvider is responsible for exactly one attribute on a geometry component.
 * The attribute is identified by its name and has a fixed domain and type. Builtin attributes do
 * not follow the same loose rules as other attributes, because they are mapped to internal
 * "legacy" data structures. For example, some builtin attributes cannot be deleted. */
class BuiltinAttributeProvider {
 public:
  /* Some utility enums to avoid hard to read booleans in function calls. */
  enum CreatableEnum {
    Creatable,
    NonCreatable,
  };
  enum WritableEnum {
    Writable,
    Readonly,
  };
  enum DeletableEnum {
    Deletable,
    NonDeletable,
  };

 protected:
  const std::string name_;
  const AttributeDomain domain_;
  const CustomDataType data_type_;
  const CreatableEnum createable_;
  const WritableEnum writable_;
  const DeletableEnum deletable_;

 public:
  BuiltinAttributeProvider(std::string name,
                           const AttributeDomain domain,
                           const CustomDataType data_type,
                           const CreatableEnum createable,
                           const WritableEnum writable,
                           const DeletableEnum deletable)
      : name_(std::move(name)),
        domain_(domain),
        data_type_(data_type),
        createable_(createable),
        writable_(writable),
        deletable_(deletable)
  {
  }

  virtual ReadAttributePtr try_get_for_read(const GeometryComponent &component) const = 0;
  virtual WriteAttributePtr try_get_for_write(GeometryComponent &component) const = 0;
  virtual bool try_delete(GeometryComponent &component) const = 0;
  virtual bool try_create(GeometryComponent &UNUSED(component)) const = 0;
  virtual bool exists(const GeometryComponent &component) const = 0;

  StringRefNull name() const
  {
    return name_;
  }

  AttributeDomain domain() const
  {
    return domain_;
  }

  CustomDataType data_type() const
  {
    return data_type_;
  }
};

/**
 * A #DynamicAttributesProvider manages a set of named attributes on a geometry component. Each
 * attribute has a name, domain and type.
 */
class DynamicAttributesProvider {
 public:
  virtual ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                            const StringRef attribute_name) const = 0;
  virtual WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                              const StringRef attribute_name) const = 0;
  virtual bool try_delete(GeometryComponent &component, const StringRef attribute_name) const = 0;
  virtual bool try_create(GeometryComponent &UNUSED(component),
                          const StringRef UNUSED(attribute_name),
                          const AttributeDomain UNUSED(domain),
                          const CustomDataType UNUSED(data_type)) const
  {
    /* Some providers should not create new attributes. */
    return false;
  };

  virtual bool foreach_attribute(const GeometryComponent &component,
                                 const AttributeForeachCallback callback) const = 0;
  virtual void supported_domains(Vector<AttributeDomain> &r_domains) const = 0;
};

/**
 * Utility to group together multiple functions that are used to access custom data on geometry
 * components in a generic way.
 */
struct CustomDataAccessInfo {
  using CustomDataGetter = CustomData *(*)(GeometryComponent &component);
  using ConstCustomDataGetter = const CustomData *(*)(const GeometryComponent &component);
  using UpdateCustomDataPointers = void (*)(GeometryComponent &component);

  CustomDataGetter get_custom_data;
  ConstCustomDataGetter get_const_custom_data;
  UpdateCustomDataPointers update_custom_data_pointers;
};

/**
 * This provider is used to provide access to builtin attributes. It supports making internal types
 * available as different types. For example, the vertex position attribute is stored as part of
 * the #MVert struct, but is exposed as float3 attribute.
 */
class BuiltinCustomDataLayerProvider final : public BuiltinAttributeProvider {
  using AsReadAttribute = ReadAttributePtr (*)(const void *data, const int domain_size);
  using AsWriteAttribute = WriteAttributePtr (*)(void *data, const int domain_size);
  using UpdateOnWrite = void (*)(GeometryComponent &component);
  const CustomDataType stored_type_;
  const CustomDataAccessInfo custom_data_access_;
  const AsReadAttribute as_read_attribute_;
  const AsWriteAttribute as_write_attribute_;
  const UpdateOnWrite update_on_write_;

 public:
  BuiltinCustomDataLayerProvider(std::string attribute_name,
                                 const AttributeDomain domain,
                                 const CustomDataType attribute_type,
                                 const CustomDataType stored_type,
                                 const CreatableEnum creatable,
                                 const WritableEnum writable,
                                 const DeletableEnum deletable,
                                 const CustomDataAccessInfo custom_data_access,
                                 const AsReadAttribute as_read_attribute,
                                 const AsWriteAttribute as_write_attribute,
                                 const UpdateOnWrite update_on_write)
      : BuiltinAttributeProvider(
            std::move(attribute_name), domain, attribute_type, creatable, writable, deletable),
        stored_type_(stored_type),
        custom_data_access_(custom_data_access),
        as_read_attribute_(as_read_attribute),
        as_write_attribute_(as_write_attribute),
        update_on_write_(update_on_write)
  {
  }

  ReadAttributePtr try_get_for_read(const GeometryComponent &component) const final
  {
    const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
    if (custom_data == nullptr) {
      return {};
    }
    const int domain_size = component.attribute_domain_size(domain_);
    const void *data = CustomData_get_layer(custom_data, stored_type_);
    if (data == nullptr) {
      return {};
    }
    return as_read_attribute_(data, domain_size);
  }

  WriteAttributePtr try_get_for_write(GeometryComponent &component) const final
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

  bool try_delete(GeometryComponent &component) const final
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

  bool try_create(GeometryComponent &component) const final
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

  bool exists(const GeometryComponent &component) const final
  {
    const CustomData *custom_data = custom_data_access_.get_const_custom_data(component);
    if (custom_data == nullptr) {
      return false;
    }
    const void *data = CustomData_get_layer(custom_data, stored_type_);
    return data != nullptr;
  }
};

/**
 * This is the attribute provider for most user generated attributes.
 */
class CustomDataAttributeProvider final : public DynamicAttributesProvider {
 private:
  static constexpr uint64_t supported_types_mask = CD_MASK_PROP_FLOAT | CD_MASK_PROP_FLOAT2 |
                                                   CD_MASK_PROP_FLOAT3 | CD_MASK_PROP_INT32 |
                                                   CD_MASK_PROP_COLOR | CD_MASK_PROP_BOOL;
  const AttributeDomain domain_;
  const CustomDataAccessInfo custom_data_access_;

 public:
  CustomDataAttributeProvider(const AttributeDomain domain,
                              const CustomDataAccessInfo custom_data_access)
      : domain_(domain), custom_data_access_(custom_data_access)
  {
  }

  ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                    const StringRef attribute_name) const final
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

  WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                      const StringRef attribute_name) const final
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
      CustomData_duplicate_referenced_layer_named(
          custom_data, layer.type, layer.name, domain_size);
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

  bool try_delete(GeometryComponent &component, const StringRef attribute_name) const final
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

  bool try_create(GeometryComponent &component,
                  const StringRef attribute_name,
                  const AttributeDomain domain,
                  const CustomDataType data_type) const final
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

  bool foreach_attribute(const GeometryComponent &component,
                         const AttributeForeachCallback callback) const final
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

  void supported_domains(Vector<AttributeDomain> &r_domains) const final
  {
    r_domains.append_non_duplicates(domain_);
  }

 private:
  template<typename T>
  ReadAttributePtr layer_to_read_attribute(const CustomDataLayer &layer,
                                           const int domain_size) const
  {
    return std::make_unique<ArrayReadAttribute<T>>(
        domain_, Span(static_cast<const T *>(layer.data), domain_size));
  }

  template<typename T>
  WriteAttributePtr layer_to_write_attribute(CustomDataLayer &layer, const int domain_size) const
  {
    return std::make_unique<ArrayWriteAttribute<T>>(
        domain_, MutableSpan(static_cast<T *>(layer.data), domain_size));
  }

  bool type_is_supported(CustomDataType data_type) const
  {
    return ((1ULL << data_type) & supported_types_mask) != 0;
  }
};

static Mesh *get_mesh_from_component_for_write(GeometryComponent &component)
{
  BLI_assert(component.type() == GeometryComponentType::Mesh);
  MeshComponent &mesh_component = static_cast<MeshComponent &>(component);
  return mesh_component.get_for_write();
}

static const Mesh *get_mesh_from_component_for_read(const GeometryComponent &component)
{
  BLI_assert(component.type() == GeometryComponentType::Mesh);
  const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
  return mesh_component.get_for_read();
}

/**
 * This attribute provider is used for uv maps and vertex colors.
 */
class NamedLegacyCustomDataProvider final : public DynamicAttributesProvider {
 private:
  using AsReadAttribute = ReadAttributePtr (*)(const void *data, const int domain_size);
  using AsWriteAttribute = WriteAttributePtr (*)(void *data, const int domain_size);
  const AttributeDomain domain_;
  const CustomDataType attribute_type_;
  const CustomDataType stored_type_;
  const CustomDataAccessInfo custom_data_access_;
  const AsReadAttribute as_read_attribute_;
  const AsWriteAttribute as_write_attribute_;

 public:
  NamedLegacyCustomDataProvider(const AttributeDomain domain,
                                const CustomDataType attribute_type,
                                const CustomDataType stored_type,
                                const CustomDataAccessInfo custom_data_access,
                                const AsReadAttribute as_read_attribute,
                                const AsWriteAttribute as_write_attribute)
      : domain_(domain),
        attribute_type_(attribute_type),
        stored_type_(stored_type),
        custom_data_access_(custom_data_access),
        as_read_attribute_(as_read_attribute),
        as_write_attribute_(as_write_attribute)
  {
  }

  ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                    const StringRef attribute_name) const final
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

  WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                      const StringRef attribute_name) const final
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

  bool try_delete(GeometryComponent &component, const StringRef attribute_name) const final
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

  bool foreach_attribute(const GeometryComponent &component,
                         const AttributeForeachCallback callback) const final
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

  void supported_domains(Vector<AttributeDomain> &r_domains) const final
  {
    r_domains.append_non_duplicates(domain_);
  }
};

/**
 * This provider makes vertex groups available as float attributes.
 */
class VertexGroupsAttributeProvider final : public DynamicAttributesProvider {
 public:
  ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                    const StringRef attribute_name) const final
  {
    BLI_assert(component.type() == GeometryComponentType::Mesh);
    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    const Mesh *mesh = mesh_component.get_for_read();
    const int vertex_group_index = mesh_component.vertex_group_names().lookup_default_as(
        attribute_name, -1);
    if (vertex_group_index < 0) {
      return {};
    }
    if (mesh == nullptr || mesh->dvert == nullptr) {
      static const float default_value = 0.0f;
      return std::make_unique<ConstantReadAttribute>(
          ATTR_DOMAIN_POINT, mesh->totvert, CPPType::get<float>(), &default_value);
    }
    return std::make_unique<VertexWeightReadAttribute>(
        mesh->dvert, mesh->totvert, vertex_group_index);
  }

  WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                      const StringRef attribute_name) const final
  {
    BLI_assert(component.type() == GeometryComponentType::Mesh);
    MeshComponent &mesh_component = static_cast<MeshComponent &>(component);
    Mesh *mesh = mesh_component.get_for_write();
    if (mesh == nullptr) {
      return {};
    }
    const int vertex_group_index = mesh_component.vertex_group_names().lookup_default_as(
        attribute_name, -1);
    if (vertex_group_index < 0) {
      return {};
    }
    if (mesh->dvert == nullptr) {
      BKE_object_defgroup_data_create(&mesh->id);
    }
    else {
      /* Copy the data layer if it is shared with some other mesh. */
      mesh->dvert = (MDeformVert *)CustomData_duplicate_referenced_layer(
          &mesh->vdata, CD_MDEFORMVERT, mesh->totvert);
    }
    return std::make_unique<blender::bke::VertexWeightWriteAttribute>(
        mesh->dvert, mesh->totvert, vertex_group_index);
  }

  bool try_delete(GeometryComponent &component, const StringRef attribute_name) const final
  {
    BLI_assert(component.type() == GeometryComponentType::Mesh);
    MeshComponent &mesh_component = static_cast<MeshComponent &>(component);

    const int vertex_group_index = mesh_component.vertex_group_names().pop_default_as(
        attribute_name, -1);
    if (vertex_group_index < 0) {
      return false;
    }
    Mesh *mesh = mesh_component.get_for_write();
    if (mesh == nullptr) {
      return true;
    }
    if (mesh->dvert == nullptr) {
      return true;
    }
    for (MDeformVert &dvert : MutableSpan(mesh->dvert, mesh->totvert)) {
      MDeformWeight *weight = BKE_defvert_find_index(&dvert, vertex_group_index);
      BKE_defvert_remove_group(&dvert, weight);
    }
    return true;
  }

  bool foreach_attribute(const GeometryComponent &component,
                         const AttributeForeachCallback callback) const final
  {
    BLI_assert(component.type() == GeometryComponentType::Mesh);
    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    for (const auto &item : mesh_component.vertex_group_names().items()) {
      const StringRefNull name = item.key;
      const int vertex_group_index = item.value;
      if (vertex_group_index >= 0) {
        AttributeMetaData meta_data{ATTR_DOMAIN_POINT, CD_PROP_FLOAT};
        if (!callback(name, meta_data)) {
          return false;
        }
      }
    }
    return true;
  }

  void supported_domains(Vector<AttributeDomain> &r_domains) const final
  {
    r_domains.append_non_duplicates(ATTR_DOMAIN_POINT);
  }
};

/**
 * This is a container for multiple attribute providers that are used by one geometry component
 * type (e.g. there is a set of attribute providers for mesh components).
 */
class ComponentAttributeProviders {
 private:
  /**
   * Builtin attribute providers are identified by their name. Attribute names that are in this
   * map will only be accessed using builtin attribute providers. Therefore, these providers have
   * higher priority when an attribute name is looked up. Usually, that means that builtin
   * providers are checked before dynamic ones.
   */
  Map<std::string, const BuiltinAttributeProvider *> builtin_attribute_providers_;
  /**
   * An ordered list of dynamic attribute providers. The order is important because that is order
   * in which they are checked when an attribute is looked up.
   */
  Vector<const DynamicAttributesProvider *> dynamic_attribute_providers_;
  /**
   * All the domains that are supported by at least one of the providers above.
   */
  Vector<AttributeDomain> supported_domains_;

 public:
  ComponentAttributeProviders(Span<const BuiltinAttributeProvider *> builtin_attribute_providers,
                              Span<const DynamicAttributesProvider *> dynamic_attribute_providers)
      : dynamic_attribute_providers_(dynamic_attribute_providers)
  {
    Set<AttributeDomain> domains;
    for (const BuiltinAttributeProvider *provider : builtin_attribute_providers) {
      /* Use #add_new to make sure that no two builtin attributes have the same name. */
      builtin_attribute_providers_.add_new(provider->name(), provider);
      supported_domains_.append_non_duplicates(provider->domain());
    }
    for (const DynamicAttributesProvider *provider : dynamic_attribute_providers) {
      provider->supported_domains(supported_domains_);
    }
  }

  const Map<std::string, const BuiltinAttributeProvider *> &builtin_attribute_providers() const
  {
    return builtin_attribute_providers_;
  }

  Span<const DynamicAttributesProvider *> dynamic_attribute_providers() const
  {
    return dynamic_attribute_providers_;
  }

  Span<AttributeDomain> supported_domains() const
  {
    return supported_domains_;
  }
};

static float3 get_vertex_position(const MVert &vert)
{
  return float3(vert.co);
}

static void set_vertex_position(MVert &vert, const float3 &position)
{
  copy_v3_v3(vert.co, position);
}

static ReadAttributePtr make_vertex_position_read_attribute(const void *data,
                                                            const int domain_size)
{
  return std::make_unique<DerivedArrayReadAttribute<MVert, float3, get_vertex_position>>(
      ATTR_DOMAIN_POINT, Span<MVert>((const MVert *)data, domain_size));
}

static WriteAttributePtr make_vertex_position_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<
      DerivedArrayWriteAttribute<MVert, float3, get_vertex_position, set_vertex_position>>(
      ATTR_DOMAIN_POINT, MutableSpan<MVert>((MVert *)data, domain_size));
}

static void tag_normals_dirty_when_writing_position(GeometryComponent &component)
{
  Mesh *mesh = get_mesh_from_component_for_write(component);
  if (mesh != nullptr) {
    mesh->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  }
}

static int get_material_index(const MPoly &mpoly)
{
  return static_cast<int>(mpoly.mat_nr);
}

static void set_material_index(MPoly &mpoly, const int &index)
{
  mpoly.mat_nr = static_cast<short>(std::clamp(index, 0, SHRT_MAX));
}

static ReadAttributePtr make_material_index_read_attribute(const void *data, const int domain_size)
{
  return std::make_unique<DerivedArrayReadAttribute<MPoly, int, get_material_index>>(
      ATTR_DOMAIN_POLYGON, Span<MPoly>((const MPoly *)data, domain_size));
}

static WriteAttributePtr make_material_index_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<
      DerivedArrayWriteAttribute<MPoly, int, get_material_index, set_material_index>>(
      ATTR_DOMAIN_POLYGON, MutableSpan<MPoly>((MPoly *)data, domain_size));
}

static float2 get_loop_uv(const MLoopUV &uv)
{
  return float2(uv.uv);
}

static void set_loop_uv(MLoopUV &uv, const float2 &co)
{
  copy_v2_v2(uv.uv, co);
}

static ReadAttributePtr make_uvs_read_attribute(const void *data, const int domain_size)
{
  return std::make_unique<DerivedArrayReadAttribute<MLoopUV, float2, get_loop_uv>>(
      ATTR_DOMAIN_CORNER, Span((const MLoopUV *)data, domain_size));
}

static WriteAttributePtr make_uvs_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<DerivedArrayWriteAttribute<MLoopUV, float2, get_loop_uv, set_loop_uv>>(
      ATTR_DOMAIN_CORNER, MutableSpan((MLoopUV *)data, domain_size));
}

static Color4f get_loop_color(const MLoopCol &col)
{
  Color4f value;
  rgba_uchar_to_float(value, &col.r);
  return value;
}

static void set_loop_color(MLoopCol &col, const Color4f &value)
{
  rgba_float_to_uchar(&col.r, value);
}

static ReadAttributePtr make_vertex_color_read_attribute(const void *data, const int domain_size)
{
  return std::make_unique<DerivedArrayReadAttribute<MLoopCol, Color4f, get_loop_color>>(
      ATTR_DOMAIN_CORNER, Span((const MLoopCol *)data, domain_size));
}

static WriteAttributePtr make_vertex_color_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<
      DerivedArrayWriteAttribute<MLoopCol, Color4f, get_loop_color, set_loop_color>>(
      ATTR_DOMAIN_CORNER, MutableSpan((MLoopCol *)data, domain_size));
}

template<typename T, AttributeDomain Domain>
static ReadAttributePtr make_array_read_attribute(const void *data, const int domain_size)
{
  return std::make_unique<ArrayReadAttribute<T>>(Domain, Span<T>((const T *)data, domain_size));
}

template<typename T, AttributeDomain Domain>
static WriteAttributePtr make_array_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<ArrayWriteAttribute<T>>(Domain, MutableSpan<T>((T *)data, domain_size));
}

/**
 * In this function all the attribute providers for a mesh component are created. Most data in this
 * function is statically allocated, because it does not change over time.
 */
static ComponentAttributeProviders create_attribute_providers_for_mesh()
{
  static auto update_custom_data_pointers = [](GeometryComponent &component) {
    Mesh *mesh = get_mesh_from_component_for_write(component);
    if (mesh != nullptr) {
      BKE_mesh_update_customdata_pointers(mesh, false);
    }
  };

#define MAKE_MUTABLE_CUSTOM_DATA_GETTER(NAME) \
  [](GeometryComponent &component) -> CustomData * { \
    Mesh *mesh = get_mesh_from_component_for_write(component); \
    return mesh ? &mesh->NAME : nullptr; \
  }
#define MAKE_CONST_CUSTOM_DATA_GETTER(NAME) \
  [](const GeometryComponent &component) -> const CustomData * { \
    const Mesh *mesh = get_mesh_from_component_for_read(component); \
    return mesh ? &mesh->NAME : nullptr; \
  }

  static CustomDataAccessInfo corner_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(ldata),
                                               MAKE_CONST_CUSTOM_DATA_GETTER(ldata),
                                               update_custom_data_pointers};
  static CustomDataAccessInfo point_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(vdata),
                                              MAKE_CONST_CUSTOM_DATA_GETTER(vdata),
                                              update_custom_data_pointers};
  static CustomDataAccessInfo edge_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(edata),
                                             MAKE_CONST_CUSTOM_DATA_GETTER(edata),
                                             update_custom_data_pointers};
  static CustomDataAccessInfo polygon_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(pdata),
                                                MAKE_CONST_CUSTOM_DATA_GETTER(pdata),
                                                update_custom_data_pointers};

#undef MAKE_CONST_CUSTOM_DATA_GETTER
#undef MAKE_MUTABLE_CUSTOM_DATA_GETTER

  static BuiltinCustomDataLayerProvider position("position",
                                                 ATTR_DOMAIN_POINT,
                                                 CD_PROP_FLOAT3,
                                                 CD_MVERT,
                                                 BuiltinAttributeProvider::NonCreatable,
                                                 BuiltinAttributeProvider::Writable,
                                                 BuiltinAttributeProvider::NonDeletable,
                                                 point_access,
                                                 make_vertex_position_read_attribute,
                                                 make_vertex_position_write_attribute,
                                                 tag_normals_dirty_when_writing_position);

  static BuiltinCustomDataLayerProvider material_index("material_index",
                                                       ATTR_DOMAIN_POLYGON,
                                                       CD_PROP_INT32,
                                                       CD_MPOLY,
                                                       BuiltinAttributeProvider::NonCreatable,
                                                       BuiltinAttributeProvider::Writable,
                                                       BuiltinAttributeProvider::NonDeletable,
                                                       polygon_access,
                                                       make_material_index_read_attribute,
                                                       make_material_index_write_attribute,
                                                       nullptr);

  static NamedLegacyCustomDataProvider uvs(ATTR_DOMAIN_CORNER,
                                           CD_PROP_FLOAT2,
                                           CD_MLOOPUV,
                                           corner_access,
                                           make_uvs_read_attribute,
                                           make_uvs_write_attribute);

  static NamedLegacyCustomDataProvider vertex_colors(ATTR_DOMAIN_CORNER,
                                                     CD_PROP_COLOR,
                                                     CD_MLOOPCOL,
                                                     corner_access,
                                                     make_vertex_color_read_attribute,
                                                     make_vertex_color_write_attribute);

  static VertexGroupsAttributeProvider vertex_groups;
  static CustomDataAttributeProvider corner_custom_data(ATTR_DOMAIN_CORNER, corner_access);
  static CustomDataAttributeProvider point_custom_data(ATTR_DOMAIN_POINT, point_access);
  static CustomDataAttributeProvider edge_custom_data(ATTR_DOMAIN_EDGE, edge_access);
  static CustomDataAttributeProvider polygon_custom_data(ATTR_DOMAIN_POLYGON, polygon_access);

  return ComponentAttributeProviders({&position, &material_index},
                                     {&uvs,
                                      &vertex_colors,
                                      &corner_custom_data,
                                      &vertex_groups,
                                      &point_custom_data,
                                      &edge_custom_data,
                                      &polygon_custom_data});
}

/**
 * In this function all the attribute providers for a point cloud component are created. Most data
 * in this function is statically allocated, because it does not change over time.
 */
static ComponentAttributeProviders create_attribute_providers_for_point_cloud()
{
  static auto update_custom_data_pointers = [](GeometryComponent &component) {
    PointCloudComponent &pointcloud_component = static_cast<PointCloudComponent &>(component);
    PointCloud *pointcloud = pointcloud_component.get_for_write();
    if (pointcloud != nullptr) {
      BKE_pointcloud_update_customdata_pointers(pointcloud);
    }
  };
  static CustomDataAccessInfo point_access = {
      [](GeometryComponent &component) -> CustomData * {
        PointCloudComponent &pointcloud_component = static_cast<PointCloudComponent &>(component);
        PointCloud *pointcloud = pointcloud_component.get_for_write();
        return pointcloud ? &pointcloud->pdata : nullptr;
      },
      [](const GeometryComponent &component) -> const CustomData * {
        const PointCloudComponent &pointcloud_component = static_cast<const PointCloudComponent &>(
            component);
        const PointCloud *pointcloud = pointcloud_component.get_for_read();
        return pointcloud ? &pointcloud->pdata : nullptr;
      },
      update_custom_data_pointers};

  static BuiltinCustomDataLayerProvider position(
      "position",
      ATTR_DOMAIN_POINT,
      CD_PROP_FLOAT3,
      CD_PROP_FLOAT3,
      BuiltinAttributeProvider::NonCreatable,
      BuiltinAttributeProvider::Writable,
      BuiltinAttributeProvider::NonDeletable,
      point_access,
      make_array_read_attribute<float3, ATTR_DOMAIN_POINT>,
      make_array_write_attribute<float3, ATTR_DOMAIN_POINT>,
      nullptr);
  static BuiltinCustomDataLayerProvider radius(
      "radius",
      ATTR_DOMAIN_POINT,
      CD_PROP_FLOAT,
      CD_PROP_FLOAT,
      BuiltinAttributeProvider::Creatable,
      BuiltinAttributeProvider::Writable,
      BuiltinAttributeProvider::Deletable,
      point_access,
      make_array_read_attribute<float, ATTR_DOMAIN_POINT>,
      make_array_write_attribute<float, ATTR_DOMAIN_POINT>,
      nullptr);
  static CustomDataAttributeProvider point_custom_data(ATTR_DOMAIN_POINT, point_access);
  return ComponentAttributeProviders({&position, &radius}, {&point_custom_data});
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
    if (default_value != nullptr) {
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

/* -------------------------------------------------------------------- */
/** \name Point Cloud Component
 * \{ */

const blender::bke::ComponentAttributeProviders *PointCloudComponent::get_attribute_providers()
    const
{
  static blender::bke::ComponentAttributeProviders providers =
      blender::bke::create_attribute_providers_for_point_cloud();
  return &providers;
}

int PointCloudComponent::attribute_domain_size(const AttributeDomain domain) const
{
  BLI_assert(domain == ATTR_DOMAIN_POINT);
  UNUSED_VARS_NDEBUG(domain);
  if (pointcloud_ == nullptr) {
    return 0;
  }
  return pointcloud_->totpoint;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Component
 * \{ */

const blender::bke::ComponentAttributeProviders *MeshComponent::get_attribute_providers() const
{
  static blender::bke::ComponentAttributeProviders providers =
      blender::bke::create_attribute_providers_for_mesh();
  return &providers;
}

int MeshComponent::attribute_domain_size(const AttributeDomain domain) const
{
  BLI_assert(this->attribute_domain_supported(domain));
  if (mesh_ == nullptr) {
    return 0;
  }
  switch (domain) {
    case ATTR_DOMAIN_CORNER:
      return mesh_->totloop;
    case ATTR_DOMAIN_POINT:
      return mesh_->totvert;
    case ATTR_DOMAIN_EDGE:
      return mesh_->totedge;
    case ATTR_DOMAIN_POLYGON:
      return mesh_->totpoly;
    default:
      BLI_assert(false);
      break;
  }
  return 0;
}

namespace blender::bke {

template<typename T>
static void adapt_mesh_domain_corner_to_point_impl(const Mesh &mesh,
                                                   const TypedReadAttribute<T> &attribute,
                                                   MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.totvert);
  attribute_math::DefaultMixer<T> mixer(r_values);

  for (const int loop_index : IndexRange(mesh.totloop)) {
    const T value = attribute[loop_index];
    const MLoop &loop = mesh.mloop[loop_index];
    const int point_index = loop.v;
    mixer.mix_in(point_index, value);
  }
  mixer.finalize();
}

static ReadAttributePtr adapt_mesh_domain_corner_to_point(const Mesh &mesh,
                                                          ReadAttributePtr attribute)
{
  ReadAttributePtr new_attribute;
  const CustomDataType data_type = attribute->custom_data_type();
  attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      /* We compute all interpolated values at once, because for this interpolation, one has to
       * iterate over all loops anyway. */
      Array<T> values(mesh.totvert);
      adapt_mesh_domain_corner_to_point_impl<T>(mesh, *attribute, values);
      new_attribute = std::make_unique<OwnedArrayReadAttribute<T>>(ATTR_DOMAIN_POINT,
                                                                   std::move(values));
    }
  });
  return new_attribute;
}

template<typename T>
static void adapt_mesh_domain_point_to_corner_impl(const Mesh &mesh,
                                                   const TypedReadAttribute<T> &attribute,
                                                   MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.totloop);

  for (const int loop_index : IndexRange(mesh.totloop)) {
    const int vertex_index = mesh.mloop[loop_index].v;
    r_values[loop_index] = attribute[vertex_index];
  }
}

static ReadAttributePtr adapt_mesh_domain_point_to_corner(const Mesh &mesh,
                                                          ReadAttributePtr attribute)
{
  ReadAttributePtr new_attribute;
  const CustomDataType data_type = attribute->custom_data_type();
  attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    /* It is not strictly necessary to compute the value for all corners here. Instead one could
     * lazily lookup the mesh topology when a specific index accessed. This can be more efficient
     * when an algorithm only accesses very few of the corner values. However, for the algorithms
     * we currently have, precomputing the array is fine. Also, it is easier to implement. */
    Array<T> values(mesh.totloop);
    adapt_mesh_domain_point_to_corner_impl<T>(mesh, *attribute, values);
    new_attribute = std::make_unique<OwnedArrayReadAttribute<T>>(ATTR_DOMAIN_CORNER,
                                                                 std::move(values));
  });
  return new_attribute;
}

}  // namespace blender::bke

ReadAttributePtr MeshComponent::attribute_try_adapt_domain(ReadAttributePtr attribute,
                                                           const AttributeDomain new_domain) const
{
  if (!attribute) {
    return {};
  }
  if (attribute->size() == 0) {
    return {};
  }
  const AttributeDomain old_domain = attribute->domain();
  if (old_domain == new_domain) {
    return attribute;
  }

  switch (old_domain) {
    case ATTR_DOMAIN_CORNER: {
      switch (new_domain) {
        case ATTR_DOMAIN_POINT:
          return blender::bke::adapt_mesh_domain_corner_to_point(*mesh_, std::move(attribute));
        default:
          break;
      }
      break;
    }
    case ATTR_DOMAIN_POINT: {
      switch (new_domain) {
        case ATTR_DOMAIN_CORNER:
          return blender::bke::adapt_mesh_domain_point_to_corner(*mesh_, std::move(attribute));
        default:
          break;
      }
    }
    default:
      break;
  }

  return {};
}

/** \} */
