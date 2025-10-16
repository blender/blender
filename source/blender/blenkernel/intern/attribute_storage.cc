/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "CLG_log.h"

#include "BLI_assert.h"
#include "BLI_color_types.hh"
#include "BLI_implicit_sharing.hh"
#include "BLI_memory_counter.hh"
#include "BLI_resource_scope.hh"
#include "BLI_string_utils.hh"
#include "BLI_vector_set.hh"

#include "BLO_read_write.hh"

#include "DNA_attribute_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_userdef_types.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_attribute_storage_blend_write.hh"
#include "BKE_idtype.hh"

static CLG_LogRef LOG = {"geom.attribute"};

namespace blender::bke {

class ArrayDataImplicitSharing : public ImplicitSharingInfo {
 private:
  void *data_;
  int64_t size_;
  const CPPType &type_;
  /* This struct could also store caches about the array data, like the min and max values. */

 public:
  ArrayDataImplicitSharing(void *data, const int64_t size, const CPPType &type)
      : ImplicitSharingInfo(), data_(data), size_(size), type_(type)
  {
  }

 private:
  void delete_self_with_data() override
  {
    if (data_ != nullptr) {
      type_.destruct_n(data_, size_);
      MEM_freeN(data_);
    }
    MEM_delete(this);
  }

  void delete_data_only() override
  {
    type_.destruct_n(data_, size_);
    MEM_freeN(data_);
    data_ = nullptr;
    size_ = 0;
  }
};

Attribute::ArrayData Attribute::ArrayData::from_value(const GPointer &value,
                                                      const int64_t domain_size)
{
  Attribute::ArrayData data{};
  const CPPType &type = *value.type();
  const void *value_ptr = value.get();

  /* Prefer `calloc` to zeroing after allocation since it is faster. */
  if (BLI_memory_is_zero(value_ptr, type.size)) {
    data.data = MEM_calloc_arrayN_aligned(domain_size, type.size, type.alignment, __func__);
  }
  else {
    data.data = MEM_malloc_arrayN_aligned(domain_size, type.size, type.alignment, __func__);
    type.fill_construct_n(value_ptr, data.data, domain_size);
  }

  data.size = domain_size;
  BLI_assert(type.is_trivially_destructible);
  data.sharing_info = ImplicitSharingPtr<>(implicit_sharing::info_for_mem_free(data.data));
  return data;
}

Attribute::ArrayData Attribute::ArrayData::from_default_value(const CPPType &type,
                                                              const int64_t domain_size)
{
  return from_value(GPointer(type, type.default_value()), domain_size);
}

Attribute::ArrayData Attribute::ArrayData::from_uninitialized(const CPPType &type,
                                                              const int64_t domain_size)
{
  Attribute::ArrayData data{};
  data.data = MEM_malloc_arrayN_aligned(domain_size, type.size, type.alignment, __func__);
  data.size = domain_size;
  BLI_assert(type.is_trivially_destructible);
  data.sharing_info = ImplicitSharingPtr<>(implicit_sharing::info_for_mem_free(data.data));
  return data;
}

Attribute::ArrayData Attribute::ArrayData::from_constructed(const CPPType &type,
                                                            const int64_t domain_size)
{
  Attribute::ArrayData data = Attribute::ArrayData::from_uninitialized(type, domain_size);
  type.default_construct_n(data.data, domain_size);
  return data;
}

Attribute::SingleData Attribute::SingleData::from_value(const GPointer &value)
{
  Attribute::SingleData data{};
  const CPPType &type = *value.type();
  data.value = MEM_mallocN_aligned(type.size, type.alignment, __func__);
  type.copy_construct(value.get(), data.value);
  BLI_assert(type.is_trivially_destructible);
  data.sharing_info = ImplicitSharingPtr<>(implicit_sharing::info_for_mem_free(data.value));
  return data;
}

Attribute::SingleData Attribute::SingleData::from_default_value(const CPPType &type)
{
  return from_value(GPointer(type, type.default_value()));
}

void AttributeStorage::foreach(FunctionRef<void(Attribute &)> fn)
{
  for (const std::unique_ptr<Attribute> &attribute : this->runtime->attributes) {
    fn(*attribute);
  }
}
void AttributeStorage::foreach(FunctionRef<void(const Attribute &)> fn) const
{
  for (const std::unique_ptr<Attribute> &attribute : this->runtime->attributes) {
    fn(*attribute);
  }
}

void AttributeStorage::foreach_with_stop(FunctionRef<bool(Attribute &)> fn)
{
  for (const std::unique_ptr<Attribute> &attribute : this->runtime->attributes) {
    if (!fn(*attribute)) {
      break;
    }
  }
}
void AttributeStorage::foreach_with_stop(FunctionRef<bool(const Attribute &)> fn) const
{
  for (const std::unique_ptr<Attribute> &attribute : this->runtime->attributes) {
    if (!fn(*attribute)) {
      break;
    }
  }
}

AttrStorageType Attribute::storage_type() const
{
  if (std::get_if<Attribute::ArrayData>(&data_)) {
    return AttrStorageType::Array;
  }
  if (std::get_if<Attribute::SingleData>(&data_)) {
    return AttrStorageType::Single;
  }
  BLI_assert_unreachable();
  return AttrStorageType::Array;
}

Attribute::DataVariant &Attribute::data_for_write()
{
  if (auto *data = std::get_if<Attribute::ArrayData>(&data_)) {
    if (!data->sharing_info) {
      BLI_assert(data->size == 0);
      return data_;
    }
    if (data->sharing_info->is_mutable()) {
      data->sharing_info->tag_ensured_mutable();
      return data_;
    }
    const CPPType &type = attribute_type_to_cpp_type(type_);
    ArrayData new_data = ArrayData::from_uninitialized(type, data->size);
    type.copy_construct_n(data->data, new_data.data, data->size);
    *data = std::move(new_data);
  }
  else if (auto *data = std::get_if<Attribute::SingleData>(&data_)) {
    if (data->sharing_info->is_mutable()) {
      data->sharing_info->tag_ensured_mutable();
      return data_;
    }
    const CPPType &type = attribute_type_to_cpp_type(type_);
    *data = SingleData::from_value(GPointer(type, data->value));
  }
  return data_;
}

AttributeStorage::AttributeStorage()
{
  this->dna_attributes = nullptr;
  this->dna_attributes_num = 0;
  memset(this->_pad, 0, sizeof(this->_pad));
  this->runtime = MEM_new<AttributeStorageRuntime>(__func__);
}

AttributeStorage::AttributeStorage(const AttributeStorage &other)
{
  this->dna_attributes = nullptr;
  this->dna_attributes_num = 0;
  this->runtime = MEM_new<AttributeStorageRuntime>(__func__);
  this->runtime->attributes.reserve(other.runtime->attributes.size());
  other.foreach([&](const Attribute &attribute) {
    this->runtime->attributes.add_new(std::make_unique<Attribute>(attribute));
  });
}

AttributeStorage &AttributeStorage::operator=(const AttributeStorage &other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) AttributeStorage(other);
  return *this;
}

AttributeStorage::AttributeStorage(AttributeStorage &&other)
{
  this->dna_attributes = nullptr;
  this->dna_attributes_num = 0;
  this->runtime = MEM_new<AttributeStorageRuntime>(__func__, std::move(*other.runtime));
}

AttributeStorage &AttributeStorage::operator=(AttributeStorage &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) AttributeStorage(std::move(other));
  return *this;
}

AttributeStorage::~AttributeStorage()
{
  MEM_delete(this->runtime);
}

int AttributeStorage::count() const
{
  return this->runtime->attributes.size();
}

Attribute &AttributeStorage::at_index(int index)
{
  return *this->runtime->attributes[index];
}
const Attribute &AttributeStorage::at_index(int index) const
{
  return *this->runtime->attributes[index];
}

int AttributeStorage::index_of(StringRef name) const
{
  return this->runtime->attributes.index_of_try_as(name);
}

const Attribute *AttributeStorage::lookup(const StringRef name) const
{
  const std::unique_ptr<blender::bke::Attribute> *attribute =
      this->runtime->attributes.lookup_key_ptr_as(name);
  if (!attribute) {
    return nullptr;
  }
  return attribute->get();
}

Attribute *AttributeStorage::lookup(const StringRef name)
{
  const std::unique_ptr<blender::bke::Attribute> *attribute =
      this->runtime->attributes.lookup_key_ptr_as(name);
  if (!attribute) {
    return nullptr;
  }
  return attribute->get();
}

Attribute &AttributeStorage::add(std::string name,
                                 const AttrDomain domain,
                                 const AttrType data_type,
                                 Attribute::DataVariant data)
{
  BLI_assert(!this->lookup(name));
  std::unique_ptr<Attribute> ptr = std::make_unique<Attribute>();
  Attribute &attribute = *ptr;
  attribute.name_ = std::move(name);
  attribute.domain_ = domain;
  attribute.type_ = data_type;
  attribute.data_ = std::move(data);
  this->runtime->attributes.add_new(std::move(ptr));
  return attribute;
}

bool AttributeStorage::remove(const StringRef name)
{
  return this->runtime->attributes.remove_as(name);
}

std::string AttributeStorage::unique_name_calc(const StringRef name) const
{
  return BLI_uniquename_cb(
      [&](const StringRef check_name) { return this->lookup(check_name) != nullptr; }, '.', name);
}

void AttributeStorage::rename(const StringRef old_name, std::string new_name)
{
  /* The VectorSet must be rebuilt from scratch because the data used to create the hash is
   * changed. */
  const int index = this->runtime->attributes.index_of_try_as(old_name);
  Vector<std::unique_ptr<Attribute>> old_vector = this->runtime->attributes.extract_vector();
  old_vector[index]->name_ = std::move(new_name);
  this->runtime->attributes.reserve(old_vector.size());
  for (std::unique_ptr<Attribute> &attribute : old_vector) {
    this->runtime->attributes.add_new(std::move(attribute));
  }
}

void AttributeStorage::resize(const AttrDomain domain, const int64_t new_size)
{
  this->foreach([&](Attribute &attr) {
    if (attr.domain() != domain) {
      return;
    }
    const CPPType &type = attribute_type_to_cpp_type(attr.data_type());
    switch (attr.storage_type()) {
      case bke::AttrStorageType::Array: {
        const auto &data = std::get<bke::Attribute::ArrayData>(attr.data());
        const int64_t old_size = data.size;

        auto new_data = bke::Attribute::ArrayData::from_uninitialized(type, new_size);
        type.copy_construct_n(data.data, new_data.data, std::min(old_size, new_size));
        if (old_size < new_size) {
          type.default_construct_n(POINTER_OFFSET(new_data.data, type.size * old_size),
                                   new_size - old_size);
        }

        attr.assign_data(std::move(new_data));
      }
      case bke::AttrStorageType::Single: {
        return;
      }
    }
  });
}

static void read_array_data(BlendDataReader &reader,
                            const int8_t dna_attr_type,
                            const int64_t size,
                            void **data)
{
  switch (dna_attr_type) {
    case int8_t(AttrType::Bool):
      static_assert(sizeof(bool) == sizeof(int8_t));
      BLO_read_int8_array(&reader, size, reinterpret_cast<int8_t **>(data));
      return;
    case int8_t(AttrType::Int8):
      BLO_read_int8_array(&reader, size, reinterpret_cast<int8_t **>(data));
      return;
    case int8_t(AttrType::Int16_2D):
      BLO_read_int16_array(&reader, size * 2, reinterpret_cast<int16_t **>(data));
      return;
    case int8_t(AttrType::Int32):
      BLO_read_int32_array(&reader, size, reinterpret_cast<int32_t **>(data));
      return;
    case int8_t(AttrType::Int32_2D):
      BLO_read_int32_array(&reader, size * 2, reinterpret_cast<int32_t **>(data));
      return;
    case int8_t(AttrType::Float):
      BLO_read_float_array(&reader, size, reinterpret_cast<float **>(data));
      return;
    case int8_t(AttrType::Float2):
      BLO_read_float_array(&reader, size * 2, reinterpret_cast<float **>(data));
      return;
    case int8_t(AttrType::Float3):
      BLO_read_float3_array(&reader, size, reinterpret_cast<float **>(data));
      return;
    case int8_t(AttrType::Float4x4):
      BLO_read_float_array(&reader, size * 16, reinterpret_cast<float **>(data));
      return;
    case int8_t(AttrType::ColorByte):
      BLO_read_uint8_array(&reader, size * 4, reinterpret_cast<uint8_t **>(data));
      return;
    case int8_t(AttrType::ColorFloat):
      BLO_read_float_array(&reader, size * 4, reinterpret_cast<float **>(data));
      return;
    case int8_t(AttrType::Quaternion):
      BLO_read_float_array(&reader, size * 4, reinterpret_cast<float **>(data));
      return;
    case int8_t(AttrType::String):
      BLO_read_struct_array(
          &reader, MStringProperty, size, reinterpret_cast<MStringProperty **>(data));
      return;
    default:
      *data = nullptr;
      return;
  }
}

static void read_shared_array(BlendDataReader &reader,
                              const int8_t dna_attr_type,
                              const int64_t size,
                              void **data,
                              const ImplicitSharingInfo **sharing_info)
{
  const char *func = __func__;
  *sharing_info = BLO_read_shared(&reader, data, [&]() -> const ImplicitSharingInfo * {
    read_array_data(reader, dna_attr_type, size, data);
    if (*data == nullptr) {
      return nullptr;
    }
    const CPPType &cpp_type = attribute_type_to_cpp_type(AttrType(dna_attr_type));
    return MEM_new<ArrayDataImplicitSharing>(func, *data, size, cpp_type);
  });
}

static std::optional<Attribute::DataVariant> read_attr_data(BlendDataReader &reader,
                                                            const int8_t dna_storage_type,
                                                            const int8_t dna_attr_type,
                                                            ::Attribute &dna_attr)
{
  switch (dna_storage_type) {
    case int8_t(AttrStorageType::Array): {
      BLO_read_struct(&reader, AttributeArray, &dna_attr.data);
      auto &data = *static_cast<::AttributeArray *>(dna_attr.data);
      read_shared_array(reader, dna_attr_type, data.size, &data.data, &data.sharing_info);
      if (data.size != 0 && !data.data) {
        return std::nullopt;
      }
      return Attribute::ArrayData{data.data, data.size, ImplicitSharingPtr<>(data.sharing_info)};
    }
    case int8_t(AttrStorageType::Single): {
      BLO_read_struct(&reader, AttributeSingle, &dna_attr.data);
      auto &data = *static_cast<::AttributeSingle *>(dna_attr.data);
      read_shared_array(reader, dna_attr_type, 1, &data.data, &data.sharing_info);
      if (!data.data) {
        return std::nullopt;
      }
      return Attribute::SingleData{data.data, ImplicitSharingPtr<>(data.sharing_info)};
    }
    default:
      return std::nullopt;
  }
}

void AttributeStorage::count_memory(MemoryCounter &memory) const
{
  for (const std::unique_ptr<Attribute> &attr : this->runtime->attributes) {
    const CPPType &type = attribute_type_to_cpp_type(attr->data_type());
    if (const auto *data = std::get_if<Attribute::ArrayData>(&attr->data())) {
      memory.add_shared(data->sharing_info.get(), [&](MemoryCounter &shared_memory) {
        shared_memory.add(data->size * type.size);
      });
    }
    else if (const auto *data = std::get_if<Attribute::SingleData>(&attr->data())) {
      memory.add_shared(data->sharing_info.get(),
                        [&](MemoryCounter &shared_memory) { shared_memory.add(type.size); });
    }
  }
}

static std::optional<AttrDomain> read_attr_domain(const int8_t dna_domain)
{
  switch (dna_domain) {
    case int8_t(AttrDomain::Point):
    case int8_t(AttrDomain::Edge):
    case int8_t(AttrDomain::Face):
    case int8_t(AttrDomain::Corner):
    case int8_t(AttrDomain::Curve):
    case int8_t(AttrDomain::Instance):
    case int8_t(AttrDomain::Layer):
      return AttrDomain(dna_domain);
    default:
      return std::nullopt;
  }
}

void AttributeStorage::blend_read(BlendDataReader &reader)
{
  this->runtime = MEM_new<AttributeStorageRuntime>(__func__);
  this->runtime->attributes.reserve(this->dna_attributes_num);

  BLO_read_struct_array(&reader, ::Attribute, this->dna_attributes_num, &this->dna_attributes);
  for (const int i : IndexRange(this->dna_attributes_num)) {
    ::Attribute &dna_attr = this->dna_attributes[i];
    BLO_read_string(&reader, &dna_attr.name);
    BLI_SCOPED_DEFER([&]() { MEM_SAFE_FREE(dna_attr.name); });

    const std::optional<AttrDomain> domain = read_attr_domain(dna_attr.domain);
    if (!domain) {
      continue;
    }

    std::optional<Attribute::DataVariant> data = read_attr_data(
        reader, dna_attr.storage_type, dna_attr.data_type, dna_attr);
    BLI_SCOPED_DEFER([&]() { MEM_SAFE_FREE(dna_attr.data); });
    if (!data) {
      continue;
    }

    std::unique_ptr<Attribute> attribute = std::make_unique<Attribute>();
    attribute->name_ = dna_attr.name;
    attribute->domain_ = *domain;
    attribute->type_ = AttrType(dna_attr.data_type);
    attribute->data_ = std::move(*data);

    if (!this->runtime->attributes.add(std::move(attribute))) {
      CLOG_ERROR(&LOG, "Ignoring attribute with duplicate name: \"%s\"", dna_attr.name);
    }
  }

  /* These fields are not used at runtime. */
  MEM_SAFE_FREE(this->dna_attributes);
  this->dna_attributes_num = 0;
}

static void write_array_data(BlendWriter &writer,
                             const AttrType data_type,
                             const void *data,
                             const int64_t size)
{
  switch (data_type) {
    case AttrType::Bool:
      static_assert(sizeof(bool) == sizeof(int8_t));
      BLO_write_int8_array(&writer, size, static_cast<const int8_t *>(data));
      break;
    case AttrType::Int8:
      BLO_write_int8_array(&writer, size, static_cast<const int8_t *>(data));
      break;
    case AttrType::Int16_2D:
      BLO_write_int16_array(&writer, size * 2, static_cast<const int16_t *>(data));
      break;
    case AttrType::Int32:
      BLO_write_int32_array(&writer, size, static_cast<const int32_t *>(data));
      break;
    case AttrType::Int32_2D:
      BLO_write_int32_array(&writer, size * 2, static_cast<const int32_t *>(data));
      break;
    case AttrType::Float:
      BLO_write_float_array(&writer, size, static_cast<const float *>(data));
      break;
    case AttrType::Float2:
      BLO_write_float_array(&writer, size * 2, static_cast<const float *>(data));
      break;
    case AttrType::Float3:
      BLO_write_float3_array(&writer, size, static_cast<const float *>(data));
      break;
    case AttrType::Float4x4:
      BLO_write_float_array(&writer, size * 16, static_cast<const float *>(data));
      break;
    case AttrType::ColorByte:
      BLO_write_uint8_array(&writer, size * 4, static_cast<const uint8_t *>(data));
      break;
    case AttrType::ColorFloat:
      BLO_write_float_array(&writer, size * 4, static_cast<const float *>(data));
      break;
    case AttrType::Quaternion:
      BLO_write_float_array(&writer, size * 4, static_cast<const float *>(data));
      break;
    case AttrType::String:
      BLO_write_struct_array(
          &writer, MStringProperty, size, static_cast<const MStringProperty *>(data));
      break;
  }
}

void attribute_storage_blend_write_prepare(AttributeStorage &data,
                                           AttributeStorage::BlendWriteData &write_data)
{
  data.foreach([&](Attribute &attr) {
    ::Attribute attribute_dna{};
    attribute_dna.name = attr.name().c_str();
    attribute_dna.data_type = int16_t(attr.data_type());
    attribute_dna.domain = int8_t(attr.domain());
    attribute_dna.storage_type = int8_t(attr.storage_type());

    /* The idea is to use a separate DNA struct for each #AttrStorageType. They each need to have a
     * unique address (while writing a specific ID anyway) in order to be identified when
     * reading the file, so we add them to the resource scope which outlives this function call.
     * Using a #ResourceScope is a simple way to get pointer stability when adding every new data
     * struct without the cost of many small allocations or unnecessary overhead of storing a full
     * array for every storage type. */

    if (const auto *data = std::get_if<Attribute::ArrayData>(&attr.data())) {
      auto &array_dna = write_data.scope.construct<::AttributeArray>();
      array_dna.data = data->data;
      array_dna.sharing_info = data->sharing_info.get();
      array_dna.size = data->size;
      attribute_dna.data = &array_dna;
    }
    else if (const auto *data = std::get_if<Attribute::SingleData>(&attr.data())) {
      auto &single_dna = write_data.scope.construct<::AttributeSingle>();
      single_dna.data = data->value;
      single_dna.sharing_info = data->sharing_info.get();
      attribute_dna.data = &single_dna;
    }

    write_data.attributes.append(attribute_dna);
  });
  data.runtime = nullptr;
}

static void write_shared_array(BlendWriter &writer,
                               const AttrType data_type,
                               const void *data,
                               const int64_t size,
                               const ImplicitSharingInfo *sharing_info)
{
  const CPPType &cpp_type = attribute_type_to_cpp_type(data_type);
  BLO_write_shared(&writer, data, cpp_type.size * size, sharing_info, [&]() {
    write_array_data(writer, data_type, data, size);
  });
}

AttributeStorage::BlendWriteData::BlendWriteData(ResourceScope &scope)
    : scope(scope), attributes(scope.construct<Vector<::Attribute, 16>>())
{
}

void AttributeStorage::blend_write(BlendWriter &writer,
                                   const AttributeStorage::BlendWriteData &write_data)
{
  /* Use string argument to avoid confusion with the C++ class with the same name. */
  BLO_write_struct_array_by_name(
      &writer, "Attribute", write_data.attributes.size(), write_data.attributes.data());
  for (const ::Attribute &attr_dna : write_data.attributes) {
    BLO_write_string(&writer, attr_dna.name);
    switch (AttrStorageType(attr_dna.storage_type)) {
      case AttrStorageType::Single: {
        ::AttributeSingle *single_dna = static_cast<::AttributeSingle *>(attr_dna.data);
        write_shared_array(
            writer, AttrType(attr_dna.data_type), single_dna->data, 1, single_dna->sharing_info);
        BLO_write_struct(&writer, AttributeSingle, single_dna);
        break;
      }
      case AttrStorageType::Array: {
        ::AttributeArray *array_dna = static_cast<::AttributeArray *>(attr_dna.data);
        write_shared_array(writer,
                           AttrType(attr_dna.data_type),
                           array_dna->data,
                           array_dna->size,
                           array_dna->sharing_info);
        BLO_write_struct(&writer, AttributeArray, array_dna);
        break;
      }
    }
  }

  this->dna_attributes = nullptr;
  this->dna_attributes_num = 0;
}

void AttributeStorage::foreach_working_space_color(const IDTypeForeachColorFunctionCallback &fn)
{
  for (const std::unique_ptr<Attribute> &attribute : this->runtime->attributes) {
    if (attribute->type_ == blender::bke::AttrType::ColorFloat) {
      if (auto *data = std::get_if<Attribute::ArrayData>(&attribute->data_)) {
        fn.implicit_sharing_array(
            data->sharing_info, reinterpret_cast<ColorGeometry4f *&>(data->data), data->size);
      }
      else if (auto *data = std::get_if<Attribute::SingleData>(&attribute->data_)) {
        fn.implicit_sharing_array(
            data->sharing_info, reinterpret_cast<ColorGeometry4f *&>(data->value), 1);
      }
    }
    /* Byte colors are always Rec.709 sRGB, no conversion needed. */
  };
}

}  // namespace blender::bke
