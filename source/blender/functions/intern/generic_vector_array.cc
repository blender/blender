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

#include "FN_generic_vector_array.hh"
#include "FN_multi_function_params.hh"
#include "FN_multi_function_signature.hh"

namespace blender::fn {

GVectorArray::GVectorArray(const CPPType &type, const int64_t array_size)
    : type_(type), element_size_(type.size()), items_(array_size)
{
}

GVectorArray::~GVectorArray()
{
  if (type_.is_trivially_destructible()) {
    return;
  }
  for (Item &item : items_) {
    type_.destruct_n(item.start, item.length);
  }
}

void GVectorArray::append(const int64_t index, const void *value)
{
  Item &item = items_[index];
  if (item.length == item.capacity) {
    this->realloc_to_at_least(item, item.capacity + 1);
  }

  void *dst = POINTER_OFFSET(item.start, element_size_ * item.length);
  type_.copy_to_uninitialized(value, dst);
  item.length++;
}

void GVectorArray::extend(const int64_t index, const GVArray &values)
{
  BLI_assert(values.type() == type_);
  for (const int i : IndexRange(values.size())) {
    BUFFER_FOR_CPP_TYPE_VALUE(type_, buffer);
    values.get(i, buffer);
    this->append(index, buffer);
    type_.destruct(buffer);
  }
}

void GVectorArray::extend(const int64_t index, const GSpan values)
{
  GVArrayForGSpan varray{values};
  this->extend(index, varray);
}

void GVectorArray::extend(IndexMask mask, const GVVectorArray &values)
{
  for (const int i : mask) {
    GVArrayForGVVectorArrayIndex array{values, i};
    this->extend(i, array);
  }
}

void GVectorArray::extend(IndexMask mask, const GVectorArray &values)
{
  GVVectorArrayForGVectorArray virtual_values{values};
  this->extend(mask, virtual_values);
}

GMutableSpan GVectorArray::operator[](const int64_t index)
{
  Item &item = items_[index];
  return GMutableSpan{type_, item.start, item.length};
}

GSpan GVectorArray::operator[](const int64_t index) const
{
  const Item &item = items_[index];
  return GSpan{type_, item.start, item.length};
}

void GVectorArray::realloc_to_at_least(Item &item, int64_t min_capacity)
{
  const int64_t new_capacity = std::max(min_capacity, item.length * 2);

  void *new_buffer = allocator_.allocate(element_size_ * new_capacity, type_.alignment());
  type_.relocate_to_initialized_n(item.start, new_buffer, item.length);

  item.start = new_buffer;
  item.capacity = new_capacity;
}

}  // namespace blender::fn
