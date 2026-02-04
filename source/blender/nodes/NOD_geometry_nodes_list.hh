/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <variant>

#include "BLI_generic_pointer.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_memory_counter_fwd.hh"

#include "NOD_geometry_nodes_list_fwd.hh"

namespace blender::nodes {

class List : public ImplicitSharingMixin {
 public:
  class ArrayData {
   public:
    /**
     * This is const because it uses implicit sharing. In some contexts the const can be cast away
     * when it's clear that the data is not shared.
     */
    const void *data;
    ImplicitSharingPtr<> sharing_info;
    static ArrayData ForValue(const GPointer &value, int64_t size);
    static ArrayData ForDefaultValue(const CPPType &type, int64_t size);
    static ArrayData ForConstructed(const CPPType &type, int64_t size);
    static ArrayData ForUninitialized(const CPPType &type, int64_t size);

    void count_memory(MemoryCounter &memory, const CPPType &type, const int64_t size) const;

    GMutableSpan span_for_write(const CPPType &type, int64_t size);
  };

  class SingleData {
   public:
    /**
     * This is const because it uses implicit sharing. In some contexts the const can be cast away
     * when it's clear that the data is not shared.
     */
    const void *value;
    ImplicitSharingPtr<> sharing_info;
    static SingleData ForValue(const GPointer &value);
    static SingleData ForDefaultValue(const CPPType &type);

    void count_memory(MemoryCounter &memory, const CPPType &type) const;

    GMutablePointer value_for_write(const CPPType &type);
  };

  using DataVariant = std::variant<ArrayData, SingleData>;

 private:
  const CPPType &cpp_type_;
  DataVariant data_;
  int64_t size_ = 0;

 public:
  explicit List(const CPPType &type, DataVariant data, const int64_t size);

  static ListPtr create(const CPPType &type, DataVariant data, const int64_t size);
  template<typename ContainerT> static ListPtr from_container(ContainerT &&container);
  static ListPtr from_garray(GArray<> array);

  DataVariant &data();
  const DataVariant &data() const;
  const CPPType &cpp_type() const;
  int64_t size() const;

  /** Access values stored in the list. This is a variant because lists support different storage
   * backends and more may be added in the future. */
  std::variant<GSpan, GPointer> values() const;
  std::variant<GMutableSpan, GMutablePointer> values_for_write();

  template<typename T> std::variant<Span<T>, const T *> values() const;
  template<typename T> std::variant<MutableSpan<T>, T *> values_for_write();

  template<typename T, typename Fn> void foreach(Fn &&fn) const;
  template<typename T, typename Fn> void foreach_for_write(Fn &&fn);

  void delete_self() override;
  ListPtr copy() const;

  /** Access the list as virtual array. */
  GVArray varray() const;
  template<typename T> VArray<T> varray() const;

  void count_memory(MemoryCounter &memory) const;
};

template<typename ContainerT> inline ListPtr List::from_container(ContainerT &&container)
{
  using T = typename std::decay_t<ContainerT>::value_type;
  static_assert(std::is_convertible_v<ContainerT, MutableSpan<T>>);
  auto *sharable_data = new ImplicitSharedValue<std::decay_t<ContainerT>>(
      std::forward<ContainerT>(container));
  ArrayData array_data;
  array_data.data = sharable_data->data.data();
  array_data.sharing_info = ImplicitSharingPtr<>(sharable_data);
  return List::create(CPPType::get<T>(), std::move(array_data), sharable_data->data.size());
}

inline List::DataVariant &List::data()
{
  return data_;
}

inline const List::DataVariant &List::data() const
{
  return data_;
}

inline const CPPType &List::cpp_type() const
{
  return cpp_type_;
}

inline int64_t List::size() const
{
  return size_;
}

template<typename T> inline VArray<T> List::varray() const
{
  return this->varray().typed<T>();
}

template<typename T> inline std::variant<Span<T>, const T *> List::values() const
{
  const std::variant<GSpan, GPointer> values = this->values();
  if (const auto *span_values = std::get_if<GSpan>(&values)) {
    return span_values->typed<T>();
  }
  if (const auto *single_value = std::get_if<GPointer>(&values)) {
    return single_value->get<T>();
  }
  BLI_assert_unreachable();
  return {};
}

template<typename T> inline std::variant<MutableSpan<T>, T *> List::values_for_write()
{
  const std::variant<GMutableSpan, GMutablePointer> values = this->values_for_write();
  if (const auto *span_values = std::get_if<GMutableSpan>(&values)) {
    return span_values->typed<T>();
  }
  if (const auto *single_value = std::get_if<GMutablePointer>(&values)) {
    return single_value->get<T>();
  }
  BLI_assert_unreachable();
  return {};
}

template<typename T, typename Fn> inline void List::foreach(Fn &&fn) const
{
  const std::variant<Span<T>, const T *> values = this->values<T>();
  if (const auto *span_values = std::get_if<Span<T>>(&values)) {
    for (const T &value : *span_values) {
      fn(value);
    }
  }
  else if (const auto *single_value = std::get_if<const T *>(&values)) {
    fn(**single_value);
  }
}
template<typename T, typename Fn> inline void List::foreach_for_write(Fn &&fn)
{
  const std::variant<MutableSpan<T>, T *> values = this->values_for_write<T>();
  if (auto *span_values = std::get_if<MutableSpan<T>>(&values)) {
    for (T &value : *span_values) {
      fn(value);
    }
  }
  else if (auto *single_value = std::get_if<T *>(&values)) {
    fn(**single_value);
  }
}

}  // namespace blender::nodes
