/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_memory_counter.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_list.hh"

namespace blender::nodes {

class ArrayImplicitSharingData : public ImplicitSharingInfo {
 public:
  const CPPType &type;
  void *data;
  int64_t size;

  ArrayImplicitSharingData(void *data, const int64_t size, const CPPType &type)
      : ImplicitSharingInfo(), type(type), data(data), size(size)
  {
  }

 private:
  void delete_self_with_data() override
  {
    type.destruct_n(this->data, this->size);
    MEM_delete_void(this->data);
    MEM_delete(this);
  }
};

static ImplicitSharingPtr<> sharing_ptr_for_array(void *data,
                                                  const int64_t size,
                                                  const CPPType &type)
{
  if (type.is_trivially_destructible) {
    /* Avoid storing size and type in sharing info if unnecessary. */
    return ImplicitSharingPtr<>(implicit_sharing::info_for_mem_free(data));
  }
  return ImplicitSharingPtr<>(MEM_new<ArrayImplicitSharingData>(__func__, data, size, type));
}

GList::ArrayData GList::ArrayData::ForValue(const GPointer &value, const int64_t size)
{
  GList::ArrayData data{};
  const CPPType &type = *value.type();
  const void *value_ptr = type.default_value();

  void *new_data;
  /* Prefer `calloc` to zeroing after allocation since it is faster. */
  if (memory_is_zero(value_ptr, type.size)) {
    new_data = MEM_new_array_zeroed_aligned(size, type.size, type.alignment, __func__);
  }
  else {
    new_data = MEM_new_array_uninitialized_aligned(size, type.size, type.alignment, __func__);
    type.fill_construct_n(value_ptr, new_data, size);
  }

  data.data = new_data;
  data.sharing_info = sharing_ptr_for_array(new_data, size, type);
  return data;
}

GList::ArrayData GList::ArrayData::ForDefaultValue(const CPPType &type, const int64_t size)
{
  return ForValue(GPointer(type, type.default_value()), size);
}

GList::ArrayData GList::ArrayData::ForConstructed(const CPPType &type, const int64_t size)
{
  GList::ArrayData data{};
  void *new_data = MEM_new_array_uninitialized_aligned(size, type.size, type.alignment, __func__);
  type.default_construct_n(new_data, size);
  data.data = new_data;
  data.sharing_info = sharing_ptr_for_array(new_data, size, type);
  return data;
}

GList::ArrayData GList::ArrayData::ForUninitialized(const CPPType &type, const int64_t size)
{
  GList::ArrayData data{};
  void *new_data = MEM_new_array_uninitialized_aligned(size, type.size, type.alignment, __func__);
  data.data = new_data;
  data.sharing_info = sharing_ptr_for_array(new_data, size, type);
  return data;
}

class SingleImplicitSharingData : public ImplicitSharingInfo {
 public:
  const CPPType &type;
  void *data;

  SingleImplicitSharingData(void *data, const CPPType &type)
      : ImplicitSharingInfo(), type(type), data(data)
  {
  }

 private:
  void delete_self_with_data() override
  {
    type.destruct(this->data);
    MEM_delete(this);
  }
};

static ImplicitSharingPtr<> sharing_ptr_for_value(void *data, const CPPType &type)
{
  if (type.is_trivially_destructible) {
    /* Avoid storing size and type in sharing info if unnecessary. */
    return ImplicitSharingPtr<>(implicit_sharing::info_for_mem_free(data));
  }
  return ImplicitSharingPtr<>(MEM_new<SingleImplicitSharingData>(__func__, data, type));
}

GList::SingleData GList::SingleData::ForValue(const GPointer &value)
{
  GList::SingleData data{};
  const CPPType &type = *value.type();
  void *new_value = MEM_new_uninitialized_aligned(type.size, type.alignment, __func__);
  type.copy_construct(value.get(), new_value);
  data.value = new_value;
  data.sharing_info = sharing_ptr_for_value(new_value, type);
  return data;
}

GList::SingleData GList::SingleData::ForDefaultValue(const CPPType &type)
{
  return ForValue(GPointer(type, type.default_value()));
}

void GList::delete_self()
{
  MEM_delete(this);
}

GListPtr GList::copy() const
{
  return GList::create(cpp_type_, data_, size_);
}

GVArray GList::varray() const
{
  if (const auto *array_data = std::get_if<ArrayData>(&data_)) {
    return GVArray::from_span(GSpan(cpp_type_, array_data->data, size_));
  }
  if (const auto *single_data = std::get_if<SingleData>(&data_)) {
    return GVArray::from_single_ref(cpp_type_, size_, single_data->value);
  }
  BLI_assert_unreachable();
  return {};
}

void GList::count_memory(MemoryCounter &memory) const
{
  if (const auto *array_data = std::get_if<ArrayData>(&data_)) {
    array_data->count_memory(memory, cpp_type_, size_);
    return;
  }
  if (const auto *single_data = std::get_if<SingleData>(&data_)) {
    single_data->count_memory(memory, cpp_type_);
    return;
  }
}

void GList::ensure_owns_direct_data()
{
  if (cpp_type_.is<BundlePtr>()) {
    this->typed<BundlePtr>().foreach_for_write([](BundlePtr &bundle_ptr) {
      bundle_ptr.ensure_mutable_inplace();
      const_cast<Bundle &>(*bundle_ptr).ensure_owns_direct_data();
    });
  }
  else if (cpp_type_.is<bke::SocketValueVariant>()) {
    this->typed<bke::SocketValueVariant>().foreach_for_write(
        [](bke::SocketValueVariant &value) { value.ensure_owns_direct_data(); });
  }
  else if (cpp_type_.is<bke::GeometrySet>()) {
    this->typed<bke::GeometrySet>().foreach_for_write(
        [](bke::GeometrySet &geometry) { geometry.ensure_owns_direct_data(); });
  }
}

bool GList::owns_direct_data() const
{
  const std::variant<GSpan, GPointer> &values = this->values();
  if (cpp_type_.is<BundlePtr>()) {
    return std::visit(
        []<typename T>(const T &value) {
          if constexpr (std::is_same_v<T, GSpan>) {
            const Span span = value.template typed<BundlePtr>();
            return std::all_of(span.begin(), span.end(), [](const BundlePtr &bundle_ptr) {
              if (!bundle_ptr) {
                return false;
              }
              return bundle_ptr->owns_direct_data();
            });
          }
          else if constexpr (std::is_same_v<T, GPointer>) {
            const BundlePtr *value_ptr = value.template get<BundlePtr>();
            if (!value_ptr) {
              return false;
            }
            return (*value_ptr)->owns_direct_data();
          }
          return true;
        },
        values);
  }
  if (cpp_type_.is<bke::SocketValueVariant>()) {
    return std::visit(
        []<typename T>(const T &value) {
          if constexpr (std::is_same_v<T, GSpan>) {
            const Span span = value.template typed<bke::SocketValueVariant>();
            return std::all_of(span.begin(), span.end(), [](const bke::SocketValueVariant &value) {
              return value.owns_direct_data();
            });
          }
          else if constexpr (std::is_same_v<T, GPointer>) {
            return value.template get<bke::SocketValueVariant>()->owns_direct_data();
          }
          return true;
        },
        values);
  }
  if (cpp_type_.is<bke::GeometrySet>()) {
    return std::visit(
        []<typename T>(const T &value) {
          if constexpr (std::is_same_v<T, GSpan>) {
            const Span span = value.template typed<bke::GeometrySet>();
            return std::all_of(span.begin(), span.end(), [](const bke::GeometrySet &value) {
              return value.owns_direct_data();
            });
          }
          else if constexpr (std::is_same_v<T, GPointer>) {
            return value.template get<bke::GeometrySet>()->owns_direct_data();
          }
          return true;
        },
        values);
  }
  return true;
}

void GList::ArrayData::count_memory(MemoryCounter &memory,
                                    const CPPType &type,
                                    const int64_t size) const
{
  memory.add_shared(this->sharing_info.get(), type.size * size);
}

void GList::SingleData::count_memory(MemoryCounter &memory, const CPPType &type) const
{
  memory.add(type.size);
}

GMutableSpan GList::ArrayData::span_for_write(const CPPType &type, int64_t size)
{
  if (this->sharing_info && !this->sharing_info->is_mutable()) {
    void *new_data = MEM_new_array_uninitialized_aligned(
        size, type.size, type.alignment, __func__);
    type.copy_construct_n(this->data, new_data, size);
    this->data = new_data;
    this->sharing_info = sharing_ptr_for_array(new_data, size, type);
  }
  if (this->sharing_info) {
    this->sharing_info->tag_ensured_mutable();
  }
  return {type, const_cast<void *>(this->data), size};
}

GMutablePointer GList::SingleData::value_for_write(const CPPType &type)
{
  if (this->sharing_info && !this->sharing_info->is_mutable()) {
    void *new_data = MEM_new_uninitialized_aligned(type.size, type.alignment, __func__);
    type.copy_construct(this->value, new_data);
    this->value = new_data;
    this->sharing_info = sharing_ptr_for_value(new_data, type);
  }
  if (this->sharing_info) {
    this->sharing_info->tag_ensured_mutable();
  }
  return GMutablePointer{type, const_cast<void *>(this->value)};
}

std::variant<GSpan, GPointer> GList::values() const
{
  if (const auto *array_data = std::get_if<ArrayData>(&data_)) {
    return GSpan(cpp_type_, array_data->data, size_);
  }
  if (const auto *single_data = std::get_if<SingleData>(&data_)) {
    return GPointer(cpp_type_, single_data->value);
  }
  BLI_assert_unreachable();
  return {};
}

std::variant<GMutableSpan, GMutablePointer> GList::values_for_write()
{
  if (auto *array_data = std::get_if<ArrayData>(&data_)) {
    return array_data->span_for_write(cpp_type_, size_);
  }
  if (auto *single_data = std::get_if<SingleData>(&data_)) {
    return single_data->value_for_write(cpp_type_);
  }
  BLI_assert_unreachable();
  return {};
}

GList::GList(const CPPType &type, DataVariant data, const int64_t size)
    : cpp_type_(type), data_(std::move(data)), size_(size)
{
}

GListPtr GList::create(const CPPType &type, DataVariant data, const int64_t size)
{
  return GListPtr(MEM_new<GList>(__func__, type, std::move(data), size));
}

GListPtr GList::from_garray(GArray<> array)
{
  auto *sharable_data = new ImplicitSharedValue<GArray<>>(std::move(array));
  ArrayData array_data;
  array_data.data = sharable_data->data.data();
  array_data.sharing_info = ImplicitSharingPtr<>(sharable_data);
  return GList::create(
      sharable_data->data.type(), std::move(array_data), sharable_data->data.size());
}

}  // namespace blender::nodes
