/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include "BLI_generic_array.hh"
#include "BLI_memory_counter.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_list.hh"

namespace blender::nodes {

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
      if (!bundle_ptr) {
        return;
      }
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
    /* Null bundles don't reference any data, so they are trivially owned. */
    return std::visit(
        []<typename T>(const T &value) {
          if constexpr (std::is_same_v<T, GSpan>) {
            const Span span = value.template typed<BundlePtr>();
            return std::all_of(span.begin(), span.end(), [](const BundlePtr &bundle_ptr) {
              if (!bundle_ptr) {
                return true;
              }
              return bundle_ptr->owns_direct_data();
            });
          }
          else if constexpr (std::is_same_v<T, GPointer>) {
            const BundlePtr *value_ptr = value.template get<BundlePtr>();
            if (!value_ptr || !*value_ptr) {
              return true;
            }
            return (*value_ptr)->owns_direct_data();
          }
          else {
            BLI_assert_unreachable_static_t(T);
          }
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
          else {
            BLI_assert_unreachable_static_t(T);
          }
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
          else {
            BLI_assert_unreachable_static_t(T);
          }
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
  const CPPType &type = array.type();
  const int64_t size = array.size();
  return GList::create(type, array_data_from_garray(std::move(array)), size);
}

GListPtr GList::from_single(const GPointer value, const int64_t size)
{
  const CPPType &type = *value.type();
  return GListPtr{MEM_new<GList>(__func__, type, SingleData::ForValue(value), size)};
}

}  // namespace blender::nodes
