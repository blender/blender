/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_memory_counter.hh"

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

List::ArrayData List::ArrayData::ForValue(const GPointer &value, const int64_t size)
{
  List::ArrayData data{};
  const CPPType &type = *value.type();
  const void *value_ptr = type.default_value();

  void *new_data;
  /* Prefer `calloc` to zeroing after allocation since it is faster. */
  if (BLI_memory_is_zero(value_ptr, type.size)) {
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

List::ArrayData List::ArrayData::ForDefaultValue(const CPPType &type, const int64_t size)
{
  return ForValue(GPointer(type, type.default_value()), size);
}

List::ArrayData List::ArrayData::ForConstructed(const CPPType &type, const int64_t size)
{
  List::ArrayData data{};
  void *new_data = MEM_new_array_uninitialized_aligned(size, type.size, type.alignment, __func__);
  type.default_construct_n(new_data, size);
  data.data = new_data;
  data.sharing_info = sharing_ptr_for_array(new_data, size, type);
  return data;
}

List::ArrayData List::ArrayData::ForUninitialized(const CPPType &type, const int64_t size)
{
  List::ArrayData data{};
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

List::SingleData List::SingleData::ForValue(const GPointer &value)
{
  List::SingleData data{};
  const CPPType &type = *value.type();
  void *new_value = MEM_new_uninitialized_aligned(type.size, type.alignment, __func__);
  type.copy_construct(value.get(), new_value);
  data.value = new_value;
  data.sharing_info = sharing_ptr_for_value(new_value, type);
  return data;
}

List::SingleData List::SingleData::ForDefaultValue(const CPPType &type)
{
  return ForValue(GPointer(type, type.default_value()));
}

void List::delete_self()
{
  MEM_delete(this);
}

ListPtr List::copy() const
{
  return List::create(cpp_type_, data_, size_);
}

GVArray List::varray() const
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

void List::count_memory(MemoryCounter &memory) const
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

void List::ArrayData::count_memory(MemoryCounter &memory,
                                   const CPPType &type,
                                   const int64_t size) const
{
  memory.add_shared(this->sharing_info.get(), type.size * size);
}

void List::SingleData::count_memory(MemoryCounter &memory, const CPPType &type) const
{
  memory.add(type.size);
}

GMutableSpan List::ArrayData::span_for_write(const CPPType &type, int64_t size)
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

GMutablePointer List::SingleData::value_for_write(const CPPType &type)
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

std::variant<GSpan, GPointer> List::values() const
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

std::variant<GMutableSpan, GMutablePointer> List::values_for_write()
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

List::List(const CPPType &type, DataVariant data, const int64_t size)
    : cpp_type_(type), data_(std::move(data)), size_(size)
{
}

ListPtr List::create(const CPPType &type, DataVariant data, const int64_t size)
{
  return ListPtr(MEM_new<List>(__func__, type, std::move(data), size));
}

ListPtr List::from_garray(GArray<> array)
{
  auto *sharable_data = new ImplicitSharedValue<GArray<>>(std::move(array));
  ArrayData array_data;
  array_data.data = sharable_data->data.data();
  array_data.sharing_info = ImplicitSharingPtr<>(sharable_data);
  return List::create(
      sharable_data->data.type(), std::move(array_data), sharable_data->data.size());
}

}  // namespace blender::nodes
