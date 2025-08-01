/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <variant>

#include "BLI_generic_pointer.hh"
#include "BLI_generic_virtual_array.hh"

#include "NOD_geometry_nodes_list_fwd.hh"

namespace blender::nodes {

class List : public ImplicitSharingMixin {
 public:
  class ArrayData {
   public:
    void *data;
    ImplicitSharingPtr<> sharing_info;
    static ArrayData ForValue(const GPointer &value, int64_t size);
    static ArrayData ForDefaultValue(const CPPType &type, int64_t size);
    static ArrayData ForConstructed(const CPPType &type, int64_t size);
    static ArrayData ForUninitialized(const CPPType &type, int64_t size);
  };

  class SingleData {
   public:
    void *value;
    ImplicitSharingPtr<> sharing_info;
    static SingleData ForValue(const GPointer &value);
    static SingleData ForDefaultValue(const CPPType &type);
  };

  using DataVariant = std::variant<ArrayData, SingleData>;

 private:
  const CPPType &cpp_type_;
  DataVariant data_;
  int64_t size_ = 0;

 public:
  explicit List(const CPPType &type, DataVariant data, const int64_t size)
      : cpp_type_(type), data_(std::move(data)), size_(size)
  {
  }

  static ListPtr create(const CPPType &type, DataVariant data, const int64_t size)
  {
    return ListPtr(MEM_new<List>(__func__, type, std::move(data), size));
  }

  const DataVariant &data() const;
  const CPPType &cpp_type() const;
  int64_t size() const;

  void delete_self() override;

  /** Access the list as virtual array. */
  GVArray varray() const;
  template<typename T> VArray<T> varray() const;
};

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

}  // namespace blender::nodes
