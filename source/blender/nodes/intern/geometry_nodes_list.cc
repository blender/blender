/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
    MEM_freeN(this->data);
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

  /* Prefer `calloc` to zeroing after allocation since it is faster. */
  if (BLI_memory_is_zero(value_ptr, type.size)) {
    data.data = MEM_calloc_arrayN_aligned(size, type.size, type.alignment, __func__);
  }
  else {
    data.data = MEM_malloc_arrayN_aligned(size, type.size, type.alignment, __func__);
    type.fill_construct_n(value_ptr, data.data, size);
  }

  data.sharing_info = sharing_ptr_for_array(data.data, size, type);
  return data;
}

List::ArrayData List::ArrayData::ForDefaultValue(const CPPType &type, const int64_t size)
{
  return ForValue(GPointer(type, type.default_value()), size);
}

List::ArrayData List::ArrayData::ForConstructed(const CPPType &type, const int64_t size)
{
  List::ArrayData data{};
  data.data = MEM_malloc_arrayN_aligned(size, type.size, type.alignment, __func__);
  type.default_construct_n(data.data, size);
  data.sharing_info = sharing_ptr_for_array(data.data, size, type);
  return data;
}

List::ArrayData List::ArrayData::ForUninitialized(const CPPType &type, const int64_t size)
{
  List::ArrayData data{};
  data.data = MEM_malloc_arrayN_aligned(size, type.size, type.alignment, __func__);
  data.sharing_info = sharing_ptr_for_array(data.data, size, type);
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
  data.value = MEM_mallocN_aligned(type.size, type.alignment, __func__);
  type.copy_construct(value.get(), data.value);
  data.sharing_info = sharing_ptr_for_value(data.value, type);
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

}  // namespace blender::nodes
