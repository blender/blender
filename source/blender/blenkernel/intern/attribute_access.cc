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
using blender::bke::ReadAttributePtr;
using blender::bke::WriteAttributePtr;

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
 * #apply_span_if_necessary should be called.
 */
fn::GMutableSpan WriteAttribute::get_span()
{
  if (size_ == 0) {
    return fn::GMutableSpan(cpp_type_);
  }
  if (array_buffer_ == nullptr) {
    this->initialize_span();
  }
  array_should_be_applied_ = true;
  return fn::GMutableSpan(cpp_type_, array_buffer_, size_);
}

void WriteAttribute::initialize_span()
{
  array_buffer_ = MEM_mallocN_aligned(cpp_type_.size() * size_, cpp_type_.alignment(), __func__);
  array_is_temporary_ = true;
  /* This does nothing for trivial types, but is necessary for general correctness. */
  cpp_type_.construct_default_n(array_buffer_, size_);
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

  void initialize_span() override
  {
    array_buffer_ = data_.data();
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

template<typename StructT, typename ElemT, typename GetFuncT, typename SetFuncT>
class DerivedArrayWriteAttribute final : public WriteAttribute {
 private:
  MutableSpan<StructT> data_;
  GetFuncT get_function_;
  SetFuncT set_function_;

 public:
  DerivedArrayWriteAttribute(AttributeDomain domain,
                             MutableSpan<StructT> data,
                             GetFuncT get_function,
                             SetFuncT set_function)
      : WriteAttribute(domain, CPPType::get<ElemT>(), data.size()),
        data_(data),
        get_function_(std::move(get_function)),
        set_function_(std::move(set_function))
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = get_function_(struct_value);
    new (r_value) ElemT(value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    StructT &struct_value = data_[index];
    const ElemT &typed_value = *reinterpret_cast<const ElemT *>(value);
    set_function_(struct_value, typed_value);
  }
};

template<typename StructT, typename ElemT, typename GetFuncT>
class DerivedArrayReadAttribute final : public ReadAttribute {
 private:
  Span<StructT> data_;
  GetFuncT get_function_;

 public:
  DerivedArrayReadAttribute(AttributeDomain domain, Span<StructT> data, GetFuncT get_function)
      : ReadAttribute(domain, CPPType::get<ElemT>(), data.size()),
        data_(data),
        get_function_(std::move(get_function))
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = get_function_(struct_value);
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
  return static_cast<CustomDataType>(-1);
}

}  // namespace blender::bke

/* -------------------------------------------------------------------- */
/** \name Utilities for Accessing Attributes
 * \{ */

static ReadAttributePtr read_attribute_from_custom_data(const CustomData &custom_data,
                                                        const int size,
                                                        const StringRef attribute_name,
                                                        const AttributeDomain domain)
{
  using namespace blender;
  using namespace blender::bke;
  for (const CustomDataLayer &layer : Span(custom_data.layers, custom_data.totlayer)) {
    if (layer.name != nullptr && layer.name == attribute_name) {
      switch (layer.type) {
        case CD_PROP_FLOAT:
          return std::make_unique<ArrayReadAttribute<float>>(
              domain, Span(static_cast<float *>(layer.data), size));
        case CD_PROP_FLOAT2:
          return std::make_unique<ArrayReadAttribute<float2>>(
              domain, Span(static_cast<float2 *>(layer.data), size));
        case CD_PROP_FLOAT3:
          return std::make_unique<ArrayReadAttribute<float3>>(
              domain, Span(static_cast<float3 *>(layer.data), size));
        case CD_PROP_INT32:
          return std::make_unique<ArrayReadAttribute<int>>(
              domain, Span(static_cast<int *>(layer.data), size));
        case CD_PROP_COLOR:
          return std::make_unique<ArrayReadAttribute<Color4f>>(
              domain, Span(static_cast<Color4f *>(layer.data), size));
      }
    }
  }
  return {};
}

static WriteAttributePtr write_attribute_from_custom_data(
    CustomData &custom_data,
    const int size,
    const StringRef attribute_name,
    const AttributeDomain domain,
    const std::function<void()> &update_customdata_pointers)
{

  using namespace blender;
  using namespace blender::bke;
  for (const CustomDataLayer &layer : Span(custom_data.layers, custom_data.totlayer)) {
    if (layer.name != nullptr && layer.name == attribute_name) {
      const void *data_before = layer.data;
      /* The data layer might be shared with someone else. Since the caller wants to modify it, we
       * copy it first. */
      CustomData_duplicate_referenced_layer_named(&custom_data, layer.type, layer.name, size);
      if (data_before != layer.data) {
        update_customdata_pointers();
      }
      switch (layer.type) {
        case CD_PROP_FLOAT:
          return std::make_unique<ArrayWriteAttribute<float>>(
              domain, MutableSpan(static_cast<float *>(layer.data), size));
        case CD_PROP_FLOAT2:
          return std::make_unique<ArrayWriteAttribute<float2>>(
              domain, MutableSpan(static_cast<float2 *>(layer.data), size));
        case CD_PROP_FLOAT3:
          return std::make_unique<ArrayWriteAttribute<float3>>(
              domain, MutableSpan(static_cast<float3 *>(layer.data), size));
        case CD_PROP_INT32:
          return std::make_unique<ArrayWriteAttribute<int>>(
              domain, MutableSpan(static_cast<int *>(layer.data), size));
        case CD_PROP_COLOR:
          return std::make_unique<ArrayWriteAttribute<Color4f>>(
              domain, MutableSpan(static_cast<Color4f *>(layer.data), size));
      }
    }
  }
  return {};
}

/* Returns true when the layer was found and is deleted. */
static bool delete_named_custom_data_layer(CustomData &custom_data,
                                           const StringRef attribute_name,
                                           const int size)
{
  for (const int index : blender::IndexRange(custom_data.totlayer)) {
    const CustomDataLayer &layer = custom_data.layers[index];
    if (layer.name == attribute_name) {
      CustomData_free_layer(&custom_data, layer.type, size, index);
      return true;
    }
  }
  return false;
}

static void get_custom_data_layer_attribute_names(const CustomData &custom_data,
                                                  const GeometryComponent &component,
                                                  const AttributeDomain domain,
                                                  Set<std::string> &r_names)
{
  for (const CustomDataLayer &layer : blender::Span(custom_data.layers, custom_data.totlayer)) {
    if (component.attribute_domain_with_type_supported(domain,
                                                       static_cast<CustomDataType>(layer.type))) {
      r_names.add(layer.name);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry Component
 * \{ */

bool GeometryComponent::attribute_domain_supported(const AttributeDomain UNUSED(domain)) const
{
  return false;
}

bool GeometryComponent::attribute_domain_with_type_supported(
    const AttributeDomain UNUSED(domain), const CustomDataType UNUSED(data_type)) const
{
  return false;
}

int GeometryComponent::attribute_domain_size(const AttributeDomain UNUSED(domain)) const
{
  BLI_assert(false);
  return 0;
}

bool GeometryComponent::attribute_is_builtin(const StringRef UNUSED(attribute_name)) const
{
  return true;
}

ReadAttributePtr GeometryComponent::attribute_try_get_for_read(
    const StringRef UNUSED(attribute_name)) const
{
  return {};
}

ReadAttributePtr GeometryComponent::attribute_try_adapt_domain(ReadAttributePtr attribute,
                                                               const AttributeDomain domain) const
{
  if (attribute && attribute->domain() == domain) {
    return attribute;
  }
  return {};
}

WriteAttributePtr GeometryComponent::attribute_try_get_for_write(
    const StringRef UNUSED(attribute_name))
{
  return {};
}

bool GeometryComponent::attribute_try_delete(const StringRef UNUSED(attribute_name))
{
  return false;
}

bool GeometryComponent::attribute_try_create(const StringRef UNUSED(attribute_name),
                                             const AttributeDomain UNUSED(domain),
                                             const CustomDataType UNUSED(data_type))
{
  return false;
}

Set<std::string> GeometryComponent::attribute_names() const
{
  return {};
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
  if (!this->attribute_domain_with_type_supported(domain, data_type)) {
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

ReadAttributePtr GeometryComponent::attribute_get_for_read(const StringRef attribute_name,
                                                           const AttributeDomain domain,
                                                           const CustomDataType data_type,
                                                           const void *default_value) const
{
  BLI_assert(this->attribute_domain_with_type_supported(domain, data_type));

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

WriteAttributePtr GeometryComponent::attribute_try_ensure_for_write(const StringRef attribute_name,
                                                                    const AttributeDomain domain,
                                                                    const CustomDataType data_type)
{
  const blender::fn::CPPType *cpp_type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);

  WriteAttributePtr attribute = this->attribute_try_get_for_write(attribute_name);
  if (attribute && attribute->domain() == domain && attribute->cpp_type() == *cpp_type) {
    return attribute;
  }

  if (attribute) {
    if (!this->attribute_try_delete(attribute_name)) {
      return {};
    }
  }
  if (!this->attribute_domain_with_type_supported(domain, data_type)) {
    return {};
  }
  if (!this->attribute_try_create(attribute_name, domain, data_type)) {
    return {};
  }
  return this->attribute_try_get_for_write(attribute_name);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Point Cloud Component
 * \{ */

bool PointCloudComponent::attribute_domain_supported(const AttributeDomain domain) const
{
  return domain == ATTR_DOMAIN_POINT;
}

bool PointCloudComponent::attribute_domain_with_type_supported(
    const AttributeDomain domain, const CustomDataType data_type) const
{
  return domain == ATTR_DOMAIN_POINT && ELEM(data_type,
                                             CD_PROP_FLOAT,
                                             CD_PROP_FLOAT2,
                                             CD_PROP_FLOAT3,
                                             CD_PROP_INT32,
                                             CD_PROP_COLOR);
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

bool PointCloudComponent::attribute_is_builtin(const StringRef attribute_name) const
{
  return attribute_name == "position";
}

ReadAttributePtr PointCloudComponent::attribute_try_get_for_read(
    const StringRef attribute_name) const
{
  if (pointcloud_ == nullptr) {
    return {};
  }

  return read_attribute_from_custom_data(
      pointcloud_->pdata, pointcloud_->totpoint, attribute_name, ATTR_DOMAIN_POINT);
}

WriteAttributePtr PointCloudComponent::attribute_try_get_for_write(const StringRef attribute_name)
{
  PointCloud *pointcloud = this->get_for_write();
  if (pointcloud == nullptr) {
    return {};
  }

  return write_attribute_from_custom_data(
      pointcloud->pdata, pointcloud->totpoint, attribute_name, ATTR_DOMAIN_POINT, [&]() {
        BKE_pointcloud_update_customdata_pointers(pointcloud);
      });
}

bool PointCloudComponent::attribute_try_delete(const StringRef attribute_name)
{
  if (this->attribute_is_builtin(attribute_name)) {
    return false;
  }
  PointCloud *pointcloud = this->get_for_write();
  if (pointcloud == nullptr) {
    return false;
  }
  delete_named_custom_data_layer(pointcloud->pdata, attribute_name, pointcloud->totpoint);
  return true;
}

static bool custom_data_has_layer_with_name(const CustomData &custom_data, const StringRef name)
{
  for (const CustomDataLayer &layer : blender::Span(custom_data.layers, custom_data.totlayer)) {
    if (layer.name == name) {
      return true;
    }
  }
  return false;
}

bool PointCloudComponent::attribute_try_create(const StringRef attribute_name,
                                               const AttributeDomain domain,
                                               const CustomDataType data_type)
{
  if (this->attribute_is_builtin(attribute_name)) {
    return false;
  }
  if (!this->attribute_domain_with_type_supported(domain, data_type)) {
    return false;
  }
  PointCloud *pointcloud = this->get_for_write();
  if (pointcloud == nullptr) {
    return false;
  }
  if (custom_data_has_layer_with_name(pointcloud->pdata, attribute_name)) {
    return false;
  }

  char attribute_name_c[MAX_NAME];
  attribute_name.copy(attribute_name_c);
  CustomData_add_layer_named(
      &pointcloud->pdata, data_type, CD_DEFAULT, nullptr, pointcloud_->totpoint, attribute_name_c);
  return true;
}

Set<std::string> PointCloudComponent::attribute_names() const
{
  if (pointcloud_ == nullptr) {
    return {};
  }

  Set<std::string> names;
  get_custom_data_layer_attribute_names(pointcloud_->pdata, *this, ATTR_DOMAIN_POINT, names);
  return names;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Component
 * \{ */

bool MeshComponent::attribute_domain_supported(const AttributeDomain domain) const
{
  return ELEM(
      domain, ATTR_DOMAIN_CORNER, ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_POLYGON);
}

bool MeshComponent::attribute_domain_with_type_supported(const AttributeDomain domain,
                                                         const CustomDataType data_type) const
{
  if (!this->attribute_domain_supported(domain)) {
    return false;
  }
  return ELEM(
      data_type, CD_PROP_FLOAT, CD_PROP_FLOAT2, CD_PROP_FLOAT3, CD_PROP_INT32, CD_PROP_COLOR);
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

bool MeshComponent::attribute_is_builtin(const StringRef attribute_name) const
{
  return attribute_name == "position";
}

ReadAttributePtr MeshComponent::attribute_try_get_for_read(const StringRef attribute_name) const
{
  if (mesh_ == nullptr) {
    return {};
  }

  if (attribute_name == "position") {
    auto get_vertex_position = [](const MVert &vert) { return float3(vert.co); };
    return std::make_unique<
        blender::bke::DerivedArrayReadAttribute<MVert, float3, decltype(get_vertex_position)>>(
        ATTR_DOMAIN_POINT, blender::Span(mesh_->mvert, mesh_->totvert), get_vertex_position);
  }

  ReadAttributePtr corner_attribute = read_attribute_from_custom_data(
      mesh_->ldata, mesh_->totloop, attribute_name, ATTR_DOMAIN_CORNER);
  if (corner_attribute) {
    return corner_attribute;
  }

  const int vertex_group_index = vertex_group_names_.lookup_default(attribute_name, -1);
  if (vertex_group_index >= 0) {
    return std::make_unique<blender::bke::VertexWeightReadAttribute>(
        mesh_->dvert, mesh_->totvert, vertex_group_index);
  }

  ReadAttributePtr vertex_attribute = read_attribute_from_custom_data(
      mesh_->vdata, mesh_->totvert, attribute_name, ATTR_DOMAIN_POINT);
  if (vertex_attribute) {
    return vertex_attribute;
  }

  ReadAttributePtr edge_attribute = read_attribute_from_custom_data(
      mesh_->edata, mesh_->totedge, attribute_name, ATTR_DOMAIN_EDGE);
  if (edge_attribute) {
    return edge_attribute;
  }

  ReadAttributePtr polygon_attribute = read_attribute_from_custom_data(
      mesh_->pdata, mesh_->totpoly, attribute_name, ATTR_DOMAIN_POLYGON);
  if (polygon_attribute) {
    return polygon_attribute;
  }

  return {};
}

WriteAttributePtr MeshComponent::attribute_try_get_for_write(const StringRef attribute_name)
{
  Mesh *mesh = this->get_for_write();
  if (mesh == nullptr) {
    return {};
  }

  const std::function<void()> update_mesh_pointers = [&]() {
    BKE_mesh_update_customdata_pointers(mesh, false);
  };

  if (attribute_name == "position") {
    CustomData_duplicate_referenced_layer(&mesh->vdata, CD_MVERT, mesh->totvert);
    update_mesh_pointers();

    auto get_vertex_position = [](const MVert &vert) { return float3(vert.co); };
    auto set_vertex_position = [](MVert &vert, const float3 &co) { copy_v3_v3(vert.co, co); };
    return std::make_unique<
        blender::bke::DerivedArrayWriteAttribute<MVert,
                                                 float3,
                                                 decltype(get_vertex_position),
                                                 decltype(set_vertex_position)>>(
        ATTR_DOMAIN_POINT,
        blender::MutableSpan(mesh_->mvert, mesh_->totvert),
        get_vertex_position,
        set_vertex_position);
  }

  WriteAttributePtr corner_attribute = write_attribute_from_custom_data(
      mesh_->ldata, mesh_->totloop, attribute_name, ATTR_DOMAIN_CORNER, update_mesh_pointers);
  if (corner_attribute) {
    return corner_attribute;
  }

  const int vertex_group_index = vertex_group_names_.lookup_default_as(attribute_name, -1);
  if (vertex_group_index >= 0) {
    if (mesh_->dvert == nullptr) {
      BKE_object_defgroup_data_create(&mesh_->id);
    }
    return std::make_unique<blender::bke::VertexWeightWriteAttribute>(
        mesh_->dvert, mesh_->totvert, vertex_group_index);
  }

  WriteAttributePtr vertex_attribute = write_attribute_from_custom_data(
      mesh_->vdata, mesh_->totvert, attribute_name, ATTR_DOMAIN_POINT, update_mesh_pointers);
  if (vertex_attribute) {
    return vertex_attribute;
  }

  WriteAttributePtr edge_attribute = write_attribute_from_custom_data(
      mesh_->edata, mesh_->totedge, attribute_name, ATTR_DOMAIN_EDGE, update_mesh_pointers);
  if (edge_attribute) {
    return edge_attribute;
  }

  WriteAttributePtr polygon_attribute = write_attribute_from_custom_data(
      mesh_->pdata, mesh_->totpoly, attribute_name, ATTR_DOMAIN_POLYGON, update_mesh_pointers);
  if (polygon_attribute) {
    return polygon_attribute;
  }

  return {};
}

bool MeshComponent::attribute_try_delete(const StringRef attribute_name)
{
  if (this->attribute_is_builtin(attribute_name)) {
    return false;
  }
  Mesh *mesh = this->get_for_write();
  if (mesh == nullptr) {
    return false;
  }

  delete_named_custom_data_layer(mesh_->ldata, attribute_name, mesh_->totloop);
  delete_named_custom_data_layer(mesh_->vdata, attribute_name, mesh_->totvert);
  delete_named_custom_data_layer(mesh_->edata, attribute_name, mesh_->totedge);
  delete_named_custom_data_layer(mesh_->pdata, attribute_name, mesh_->totpoly);

  const int vertex_group_index = vertex_group_names_.lookup_default_as(attribute_name, -1);
  if (vertex_group_index != -1) {
    for (MDeformVert &dvert : blender::MutableSpan(mesh_->dvert, mesh_->totvert)) {
      MDeformWeight *weight = BKE_defvert_find_index(&dvert, vertex_group_index);
      BKE_defvert_remove_group(&dvert, weight);
    }
    vertex_group_names_.remove_as(attribute_name);
  }

  return true;
}

bool MeshComponent::attribute_try_create(const StringRef attribute_name,
                                         const AttributeDomain domain,
                                         const CustomDataType data_type)
{
  if (this->attribute_is_builtin(attribute_name)) {
    return false;
  }
  if (!this->attribute_domain_with_type_supported(domain, data_type)) {
    return false;
  }
  Mesh *mesh = this->get_for_write();
  if (mesh == nullptr) {
    return false;
  }

  char attribute_name_c[MAX_NAME];
  attribute_name.copy(attribute_name_c);

  switch (domain) {
    case ATTR_DOMAIN_CORNER: {
      if (custom_data_has_layer_with_name(mesh->ldata, attribute_name)) {
        return false;
      }
      CustomData_add_layer_named(
          &mesh->ldata, data_type, CD_DEFAULT, nullptr, mesh->totloop, attribute_name_c);
      return true;
    }
    case ATTR_DOMAIN_POINT: {
      if (custom_data_has_layer_with_name(mesh->vdata, attribute_name)) {
        return false;
      }
      if (vertex_group_names_.contains_as(attribute_name)) {
        return false;
      }
      CustomData_add_layer_named(
          &mesh->vdata, data_type, CD_DEFAULT, nullptr, mesh->totvert, attribute_name_c);
      return true;
    }
    case ATTR_DOMAIN_EDGE: {
      if (custom_data_has_layer_with_name(mesh->edata, attribute_name)) {
        return false;
      }
      CustomData_add_layer_named(
          &mesh->edata, data_type, CD_DEFAULT, nullptr, mesh->totedge, attribute_name_c);
      return true;
    }
    case ATTR_DOMAIN_POLYGON: {
      if (custom_data_has_layer_with_name(mesh->pdata, attribute_name)) {
        return false;
      }
      CustomData_add_layer_named(
          &mesh->pdata, data_type, CD_DEFAULT, nullptr, mesh->totpoly, attribute_name_c);
      return true;
    }
    default:
      return false;
  }
}

Set<std::string> MeshComponent::attribute_names() const
{
  if (mesh_ == nullptr) {
    return {};
  }

  Set<std::string> names;
  names.add("position");
  for (StringRef name : vertex_group_names_.keys()) {
    names.add(name);
  }
  get_custom_data_layer_attribute_names(mesh_->pdata, *this, ATTR_DOMAIN_CORNER, names);
  get_custom_data_layer_attribute_names(mesh_->vdata, *this, ATTR_DOMAIN_POINT, names);
  get_custom_data_layer_attribute_names(mesh_->edata, *this, ATTR_DOMAIN_EDGE, names);
  get_custom_data_layer_attribute_names(mesh_->pdata, *this, ATTR_DOMAIN_POLYGON, names);
  return names;
}

/** \} */
