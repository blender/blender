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

#include "FN_generic_virtual_array.hh"

namespace blender::fn {

void GVArray::materialize_to_uninitialized(const IndexMask mask, void *dst) const
{
  for (const int64_t i : mask) {
    void *elem_dst = POINTER_OFFSET(dst, type_->size() * i);
    this->get_to_uninitialized(i, elem_dst);
  }
}

void GVArray::get_impl(const int64_t index, void *r_value) const
{
  type_->destruct(r_value);
  this->get_to_uninitialized_impl(index, r_value);
}

bool GVArray::is_span_impl() const
{
  return false;
}

GSpan GVArray::get_span_impl() const
{
  BLI_assert(false);
  return GSpan(*type_);
}

bool GVArray::is_single_impl() const
{
  return false;
}

void GVArray::get_single_impl(void *UNUSED(r_value)) const
{
  BLI_assert(false);
}

void GVArrayForGSpan::get_impl(const int64_t index, void *r_value) const
{
  type_->copy_to_initialized(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

void GVArrayForGSpan::get_to_uninitialized_impl(const int64_t index, void *r_value) const
{
  type_->copy_to_uninitialized(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

bool GVArrayForGSpan::is_span_impl() const
{
  return true;
}

GSpan GVArrayForGSpan::get_span_impl() const
{
  return GSpan(*type_, data_, size_);
}

void GVArrayForSingleValueRef::get_impl(const int64_t UNUSED(index), void *r_value) const
{
  type_->copy_to_initialized(value_, r_value);
}

void GVArrayForSingleValueRef::get_to_uninitialized_impl(const int64_t UNUSED(index),
                                                         void *r_value) const
{
  type_->copy_to_uninitialized(value_, r_value);
}

bool GVArrayForSingleValueRef::is_span_impl() const
{
  return size_ == 1;
}

GSpan GVArrayForSingleValueRef::get_span_impl() const
{
  return GSpan{*type_, value_, 1};
}

bool GVArrayForSingleValueRef::is_single_impl() const
{
  return true;
}

void GVArrayForSingleValueRef::get_single_impl(void *r_value) const
{
  type_->copy_to_initialized(value_, r_value);
}

}  // namespace blender::fn
