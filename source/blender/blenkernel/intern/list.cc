/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_generic_array.hh"
#include "BLI_memory_counter.hh"

#include "BKE_list.hh"

namespace blender::bke {

/** Create array data that owns the array's buffer. */
static GList::ArrayData array_data_from_garray(GArray<> array)
{
  auto *sharing_info = new ImplicitSharedValue<GArray<>>(std::move(array));
  GList::ArrayData data;
  data.data = sharing_info->data.data();
  data.sharing_info = ImplicitSharingPtr<>(sharing_info);
  return data;
}

GList::ArrayData GList::ArrayData::ForValue(const GPointer &value, const int64_t size)
{
  const CPPType &type = *value.type();
  const void *value_ptr = value.get();

  /* Prefer `calloc` to zeroing after allocation since it is faster. */
  if (memory_is_zero(value_ptr, type.size)) {
    void *buffer = MEM_new_array_zeroed_aligned(size, type.size, type.alignment, __func__);
    return array_data_from_garray(GArray<>(type, buffer, size));
  }
  GArray<> array(type, size, NoInitialization());
  type.fill_construct_n(value_ptr, array.data(), size);
  return array_data_from_garray(std::move(array));
}

GList::ArrayData GList::ArrayData::ForDefaultValue(const CPPType &type, const int64_t size)
{
  return ForValue(GPointer(type, type.default_value()), size);
}

GList::ArrayData GList::ArrayData::ForConstructed(const CPPType &type, const int64_t size)
{
  return array_data_from_garray(GArray<>(type, size));
}

GList::ArrayData GList::ArrayData::ForUninitialized(const CPPType &type, const int64_t size)
{
  return array_data_from_garray(GArray<>(type, size, NoInitialization()));
}

GList::SingleData GList::SingleData::ForValue(const GPointer &value)
{
  const CPPType &type = *value.type();
  /* Use an array with a single element to store the value. */
  GArray<> array(type, 1, NoInitialization());
  type.copy_construct(value.get(), array.data());
  auto *sharing_info = new ImplicitSharedValue<GArray<>>(std::move(array));
  GList::SingleData data;
  data.value = sharing_info->data.data();
  data.sharing_info = ImplicitSharingPtr<>(sharing_info);
  return data;
}

GList::SingleData GList::SingleData::ForDefaultValue(const CPPType &type)
{
  return ForValue(GPointer(type, type.default_value()));
}

void GList::resize(const int64_t new_size)
{
  if (const auto *array_data = std::get_if<ArrayData>(&data_)) {
    const CPPType &type = *cpp_type_;
    GArray<> new_array(type, new_size, NoInitialization());
    type.copy_construct_n(array_data->data, new_array.data(), std::min(size_, new_size));
    if (size_ < new_size) {
      type.default_construct_n(new_array[size_], new_size - size_);
    }
    data_ = array_data_from_garray(std::move(new_array));
  }
  size_ = new_size;
}

void GList::ensure_array_storage()
{
  if (const auto *single_data = std::get_if<SingleData>(&data_)) {
    data_ = ArrayData::ForValue(GPointer(*cpp_type_, single_data->value), size_);
  }
}

GVArray GList::varray() const
{
  if (const auto *array_data = std::get_if<ArrayData>(&data_)) {
    return GVArray::from_span(GSpan(*cpp_type_, array_data->data, size_));
  }
  if (const auto *single_data = std::get_if<SingleData>(&data_)) {
    return GVArray::from_single_ref(*cpp_type_, size_, single_data->value);
  }
  BLI_assert_unreachable();
  return {};
}

void GList::count_memory(MemoryCounter &memory) const
{
  if (const auto *array_data = std::get_if<ArrayData>(&data_)) {
    array_data->count_memory(memory, *cpp_type_, size_);
    return;
  }
  if (const auto *single_data = std::get_if<SingleData>(&data_)) {
    single_data->count_memory(memory, *cpp_type_);
    return;
  }
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
    GArray<> new_array(type, size, NoInitialization());
    type.copy_construct_n(this->data, new_array.data(), size);
    *this = array_data_from_garray(std::move(new_array));
  }
  if (this->sharing_info) {
    this->sharing_info->tag_ensured_mutable();
  }
  return {type, const_cast<void *>(this->data), size};
}

GMutablePointer GList::SingleData::value_for_write(const CPPType &type)
{
  if (this->sharing_info && !this->sharing_info->is_mutable()) {
    *this = SingleData::ForValue(GPointer(type, this->value));
  }
  if (this->sharing_info) {
    this->sharing_info->tag_ensured_mutable();
  }
  return GMutablePointer{type, const_cast<void *>(this->value)};
}

std::variant<GSpan, GPointer> GList::values() const
{
  if (const auto *array_data = std::get_if<ArrayData>(&data_)) {
    return GSpan(*cpp_type_, array_data->data, size_);
  }
  if (const auto *single_data = std::get_if<SingleData>(&data_)) {
    return GPointer(*cpp_type_, single_data->value);
  }
  BLI_assert_unreachable();
  return {};
}

std::variant<GMutableSpan, GMutablePointer> GList::values_for_write()
{
  if (auto *array_data = std::get_if<ArrayData>(&data_)) {
    return array_data->span_for_write(*cpp_type_, size_);
  }
  if (auto *single_data = std::get_if<SingleData>(&data_)) {
    return single_data->value_for_write(*cpp_type_);
  }
  BLI_assert_unreachable();
  return {};
}

GList::GList(const CPPType &type, DataVariant data, const int64_t size)
    : cpp_type_(&type), data_(std::move(data)), size_(size)
{
}

GList::GList(GList &&other) noexcept
    : cpp_type_(other.cpp_type_),
      data_(std::exchange(other.data_, {})),
      size_(std::exchange(other.size_, 0))
{
}

GList::GList(GArray<> array) : GList(array.type())
{
  size_ = array.size();
  data_ = array_data_from_garray(std::move(array));
}

GList &GList::operator=(GList &&other) noexcept
{
  return move_assign_container(*this, std::move(other));
}

GListPtr GList::create(const CPPType &type, DataVariant data, const int64_t size)
{
  return GListPtr(GList(type, std::move(data), size));
}

GListPtr GList::from_garray(GArray<> array)
{
  return GListPtr(GList(std::move(array)));
}

GListPtr GList::from_single(const GPointer value, const int64_t size)
{
  const CPPType &type = *value.type();
  return GListPtr{GList(type, SingleData::ForValue(value), size)};
}

}  // namespace blender::bke
