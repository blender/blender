/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <variant>

#include "BLI_generic_pointer.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_memory_counter_fwd.hh"

#include "NOD_geometry_nodes_list_fwd.hh"

namespace blender::nodes {

class GList : public ImplicitSharingMixin {
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
  /**
   * #GList is expected to always have a valid #CPPType. Therefore, it can't be default
   * constructed.
   */
  GList() = delete;
  GList(const CPPType &type) : GList(type, DataVariant{}, 0) {}
  GList(const CPPType &type, DataVariant data, const int64_t size);
  static GListPtr create(const CPPType &type, DataVariant data, const int64_t size);
  template<typename ContainerT> static GListPtr from_container(ContainerT &&container);
  static GListPtr from_garray(GArray<> array);

  DataVariant &data();
  const DataVariant &data() const;
  const CPPType &cpp_type() const;
  int64_t size() const;

  /** Access values stored in the list. This is a variant because lists support different storage
   * backends and more may be added in the future. */
  std::variant<GSpan, GPointer> values() const;
  std::variant<GMutableSpan, GMutablePointer> values_for_write();

  void delete_self() override;
  GListPtr copy() const;

  /** Access the list as virtual array. */
  GVArray varray() const;

  void count_memory(MemoryCounter &memory) const;

  void ensure_owns_direct_data();
  bool owns_direct_data() const;

  /**
   * Get a typed reference to this field. Note that #Field<T> happens to be identical to #GField on
   * a bit-level. So this is just a cast.
   */
  template<typename T> const List<T> &typed() const;
  template<typename T> List<T> &typed();
};

template<typename T> class List {
 public:
  using base_type = T;
  using generic_type = GList;

 private:
  /**
   * #List<T> just stores a #GList. This makes converting between the two types easy.
   */
  GList list_;

  friend GList;

 public:
  List();

  template<typename ContainerT>
    requires std::is_same_v<typename ContainerT::value_type, T>
  static ListPtr<T> from_container(ContainerT &&container);

  /** This is implicitly cast to #GField which is always valid. */
  operator const GList &() const;

  /** Access the list as virtual array. */
  VArray<T> varray() const;

  std::variant<Span<T>, const T *> values() const;
  std::variant<MutableSpan<T>, T *> values_for_write();

  template<typename Fn> void foreach(Fn &&fn) const;
  template<typename Fn> void foreach_for_write(Fn &&fn);
};

class GListPtr {
 private:
  ImplicitSharingPtr<GList> data_;

 public:
  GListPtr() = default;
  explicit GListPtr(const GList *data) : data_(data) {}
  explicit GListPtr(const CPPType &type) : GListPtr(MEM_new<GList>(__func__, type)) {}

  operator bool() const;
  const GList *operator->() const;
  const GList &operator*() const;

  const GList *get() const;
  GList &get_for_write();

  template<typename T> const ListPtr<T> &typed() const;
};

template<typename T> class ListPtr {
 public:
  using base_type = T;
  using generic_type = GListPtr;

 private:
  GListPtr data_;

 public:
  operator bool() const;
  operator const GListPtr &() const;
  const List<T> *operator->() const;
  const List<T> &operator*() const;

  const List<T> &get() const;
  List<T> &get_for_write();
};

template<typename T> constexpr bool is_ListPtr_v = false;
template<typename T> constexpr bool is_ListPtr_v<ListPtr<T>> = true;

template<typename ContainerT> inline GListPtr GList::from_container(ContainerT &&container)
{
  using T = typename std::decay_t<ContainerT>::value_type;
  static_assert(std::is_convertible_v<ContainerT, MutableSpan<T>>);
  auto *sharable_data = new ImplicitSharedValue<std::decay_t<ContainerT>>(
      std::forward<ContainerT>(container));
  ArrayData array_data;
  array_data.data = sharable_data->data.data();
  array_data.sharing_info = ImplicitSharingPtr<>(sharable_data);
  return GList::create(CPPType::get<T>(), std::move(array_data), sharable_data->data.size());
}

template<typename T>
template<typename ContainerT>
  requires std::is_same_v<typename ContainerT::value_type, T>
inline ListPtr<T> List<T>::from_container(ContainerT &&container)
{
  return GList::from_container(std::forward<ContainerT>(container)).template typed<T>();
}

inline GList::DataVariant &GList::data()
{
  return data_;
}

inline const GList::DataVariant &GList::data() const
{
  return data_;
}

inline const CPPType &GList::cpp_type() const
{
  return cpp_type_;
}

inline int64_t GList::size() const
{
  return size_;
}

template<typename T> inline const List<T> &GList::typed() const
{
  static_assert(sizeof(GList) == sizeof(List<T>));
  BLI_assert(this->cpp_type().is<T>());
  return reinterpret_cast<const List<T> &>(*this);
}

template<typename T> inline List<T> &GList::typed()
{
  static_assert(sizeof(GList) == sizeof(List<T>));
  BLI_assert(this->cpp_type().is<T>());
  return reinterpret_cast<List<T> &>(*this);
}

template<typename T> inline List<T>::List() : list_(CPPType::get<T>()) {}

template<typename T> inline VArray<T> List<T>::varray() const
{
  return list_.varray().template typed<T>();
}

template<typename T> inline std::variant<Span<T>, const T *> List<T>::values() const
{
  const std::variant<GSpan, GPointer> values = list_.values();
  if (const auto *span_values = std::get_if<GSpan>(&values)) {
    return span_values->typed<T>();
  }
  if (const auto *single_value = std::get_if<GPointer>(&values)) {
    return single_value->get<T>();
  }
  BLI_assert_unreachable();
  return {};
}

template<typename T> inline std::variant<MutableSpan<T>, T *> List<T>::values_for_write()
{
  const std::variant<GMutableSpan, GMutablePointer> values = list_.values_for_write();
  if (const auto *span_values = std::get_if<GMutableSpan>(&values)) {
    return span_values->typed<T>();
  }
  if (const auto *single_value = std::get_if<GMutablePointer>(&values)) {
    return single_value->get<T>();
  }
  BLI_assert_unreachable();
  return {};
}

template<typename T> template<typename Fn> inline void List<T>::foreach(Fn &&fn) const
{
  const std::variant<Span<T>, const T *> values = this->values();
  if (const auto *span_values = std::get_if<Span<T>>(&values)) {
    for (const T &value : *span_values) {
      fn(value);
    }
  }
  else if (const auto *single_value = std::get_if<const T *>(&values)) {
    fn(**single_value);
  }
}

template<typename T> template<typename Fn> inline void List<T>::foreach_for_write(Fn &&fn)
{
  const std::variant<MutableSpan<T>, T *> values = this->values_for_write();
  if (auto *span_values = std::get_if<MutableSpan<T>>(&values)) {
    for (T &value : *span_values) {
      fn(value);
    }
  }
  else if (auto *single_value = std::get_if<T *>(&values)) {
    fn(**single_value);
  }
}

inline GListPtr::operator bool() const
{
  return data_;
}

inline const GList *GListPtr::operator->() const
{
  return data_.get();
}

inline const GList &GListPtr::operator*() const
{
  return *data_;
}

inline const GList *GListPtr::get() const
{
  return data_.get();
}

inline GList &GListPtr::get_for_write()
{
  BLI_assert(data_);
  if (!data_->is_mutable()) {
    *this = data_->copy();
  }
  BLI_assert(data_->is_mutable());
  data_->tag_ensured_mutable();
  return const_cast<GList &>(*data_);
}

template<typename T> inline const ListPtr<T> &GListPtr::typed() const
{
  static_assert(sizeof(GList) == sizeof(List<T>));
  BLI_assert(!data_ || data_->cpp_type().is<T>());
  return reinterpret_cast<const ListPtr<T> &>(*this);
}

template<typename T> inline ListPtr<T>::operator bool() const
{
  return data_;
}

template<typename T> inline ListPtr<T>::operator const GListPtr &() const
{
  return data_;
}

template<typename T> inline const List<T> *ListPtr<T>::operator->() const
{
  if (!data_) {
    return nullptr;
  }
  return &data_->typed<T>();
}

template<typename T> inline const List<T> &ListPtr<T>::operator*() const
{
  return data_->typed<T>();
}

template<typename T> inline const List<T> &ListPtr<T>::get() const
{
  return data_->typed<T>();
}

template<typename T> inline List<T> &ListPtr<T>::get_for_write()
{
  return data_.get_for_write().template typed<T>();
}

}  // namespace blender::nodes
