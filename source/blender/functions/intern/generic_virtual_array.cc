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

/* --------------------------------------------------------------------
 * GVArray_For_ShallowCopy.
 */

class GVArray_For_ShallowCopy : public GVArray {
 private:
  const GVArray &varray_;

 public:
  GVArray_For_ShallowCopy(const GVArray &varray)
      : GVArray(varray.type(), varray.size()), varray_(varray)
  {
  }

 private:
  void get_impl(const int64_t index, void *r_value) const override
  {
    varray_.get(index, r_value);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    varray_.get_to_uninitialized(index, r_value);
  }

  void materialize_to_uninitialized_impl(const IndexMask mask, void *dst) const override
  {
    varray_.materialize_to_uninitialized(mask, dst);
  }
};

/* --------------------------------------------------------------------
 * GVArray.
 */

void GVArray::materialize(void *dst) const
{
  this->materialize(IndexMask(size_), dst);
}

void GVArray::materialize(const IndexMask mask, void *dst) const
{
  this->materialize_impl(mask, dst);
}

void GVArray::materialize_impl(const IndexMask mask, void *dst) const
{
  for (const int64_t i : mask) {
    void *elem_dst = POINTER_OFFSET(dst, type_->size() * i);
    this->get(i, elem_dst);
  }
}

void GVArray::materialize_to_uninitialized(void *dst) const
{
  this->materialize_to_uninitialized(IndexMask(size_), dst);
}

void GVArray::materialize_to_uninitialized(const IndexMask mask, void *dst) const
{
  BLI_assert(mask.min_array_size() <= size_);
  this->materialize_to_uninitialized_impl(mask, dst);
}

void GVArray::materialize_to_uninitialized_impl(const IndexMask mask, void *dst) const
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

GSpan GVArray::get_internal_span_impl() const
{
  BLI_assert(false);
  return GSpan(*type_);
}

bool GVArray::is_single_impl() const
{
  return false;
}

void GVArray::get_internal_single_impl(void *UNUSED(r_value)) const
{
  BLI_assert(false);
}

const void *GVArray::try_get_internal_varray_impl() const
{
  return nullptr;
}

/**
 * Creates a new `std::unique_ptr<GVArray>` based on this `GVArray`.
 * The lifetime of the returned virtual array must not be longer than the lifetime of this virtual
 * array.
 */
GVArrayPtr GVArray::shallow_copy() const
{
  if (this->is_span()) {
    return std::make_unique<GVArray_For_GSpan>(this->get_internal_span());
  }
  if (this->is_single()) {
    BUFFER_FOR_CPP_TYPE_VALUE(*type_, buffer);
    this->get_internal_single(buffer);
    std::unique_ptr new_varray = std::make_unique<GVArray_For_SingleValue>(*type_, size_, buffer);
    type_->destruct(buffer);
    return new_varray;
  }
  return std::make_unique<GVArray_For_ShallowCopy>(*this);
}

/* --------------------------------------------------------------------
 * GVMutableArray.
 */

void GVMutableArray::set_by_copy_impl(const int64_t index, const void *value)
{
  BUFFER_FOR_CPP_TYPE_VALUE(*type_, buffer);
  type_->copy_to_uninitialized(value, buffer);
  this->set_by_move_impl(index, buffer);
  type_->destruct(buffer);
}

void GVMutableArray::set_by_relocate_impl(const int64_t index, void *value)
{
  this->set_by_move_impl(index, value);
  type_->destruct(value);
}

void *GVMutableArray::try_get_internal_mutable_varray_impl()
{
  return nullptr;
}

void GVMutableArray::fill(const void *value)
{
  if (this->is_span()) {
    const GMutableSpan span = this->get_internal_span();
    type_->fill_initialized(value, span.data(), size_);
  }
  else {
    for (int64_t i : IndexRange(size_)) {
      this->set_by_copy(i, value);
    }
  }
}

/* --------------------------------------------------------------------
 * GVArray_For_GSpan.
 */

void GVArray_For_GSpan::get_impl(const int64_t index, void *r_value) const
{
  type_->copy_to_initialized(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

void GVArray_For_GSpan::get_to_uninitialized_impl(const int64_t index, void *r_value) const
{
  type_->copy_to_uninitialized(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

bool GVArray_For_GSpan::is_span_impl() const
{
  return true;
}

GSpan GVArray_For_GSpan::get_internal_span_impl() const
{
  return GSpan(*type_, data_, size_);
}

/* --------------------------------------------------------------------
 * GVMutableArray_For_GMutableSpan.
 */

void GVMutableArray_For_GMutableSpan::get_impl(const int64_t index, void *r_value) const
{
  type_->copy_to_initialized(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

void GVMutableArray_For_GMutableSpan::get_to_uninitialized_impl(const int64_t index,
                                                                void *r_value) const
{
  type_->copy_to_uninitialized(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

void GVMutableArray_For_GMutableSpan::set_by_copy_impl(const int64_t index, const void *value)
{
  type_->copy_to_initialized(value, POINTER_OFFSET(data_, element_size_ * index));
}

void GVMutableArray_For_GMutableSpan::set_by_move_impl(const int64_t index, void *value)
{
  type_->move_to_initialized(value, POINTER_OFFSET(data_, element_size_ * index));
}

void GVMutableArray_For_GMutableSpan::set_by_relocate_impl(const int64_t index, void *value)
{
  type_->relocate_to_initialized(value, POINTER_OFFSET(data_, element_size_ * index));
}

bool GVMutableArray_For_GMutableSpan::is_span_impl() const
{
  return true;
}

GSpan GVMutableArray_For_GMutableSpan::get_internal_span_impl() const
{
  return GSpan(*type_, data_, size_);
}

/* --------------------------------------------------------------------
 * GVArray_For_SingleValueRef.
 */

void GVArray_For_SingleValueRef::get_impl(const int64_t UNUSED(index), void *r_value) const
{
  type_->copy_to_initialized(value_, r_value);
}

void GVArray_For_SingleValueRef::get_to_uninitialized_impl(const int64_t UNUSED(index),
                                                           void *r_value) const
{
  type_->copy_to_uninitialized(value_, r_value);
}

bool GVArray_For_SingleValueRef::is_span_impl() const
{
  return size_ == 1;
}

GSpan GVArray_For_SingleValueRef::get_internal_span_impl() const
{
  return GSpan{*type_, value_, 1};
}

bool GVArray_For_SingleValueRef::is_single_impl() const
{
  return true;
}

void GVArray_For_SingleValueRef::get_internal_single_impl(void *r_value) const
{
  type_->copy_to_initialized(value_, r_value);
}

/* --------------------------------------------------------------------
 * GVArray_For_SingleValue.
 */

GVArray_For_SingleValue::GVArray_For_SingleValue(const CPPType &type,
                                                 const int64_t size,
                                                 const void *value)
    : GVArray_For_SingleValueRef(type, size)
{
  value_ = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
  type.copy_to_uninitialized(value, (void *)value_);
}

GVArray_For_SingleValue::~GVArray_For_SingleValue()
{
  type_->destruct((void *)value_);
  MEM_freeN((void *)value_);
}

/* --------------------------------------------------------------------
 * GVArray_GSpan.
 */

GVArray_GSpan::GVArray_GSpan(const GVArray &varray) : GSpan(varray.type()), varray_(varray)
{
  size_ = varray_.size();
  if (varray_.is_span()) {
    data_ = varray_.get_internal_span().data();
  }
  else {
    owned_data_ = MEM_mallocN_aligned(type_->size() * size_, type_->alignment(), __func__);
    varray_.materialize_to_uninitialized(IndexRange(size_), owned_data_);
    data_ = owned_data_;
  }
}

GVArray_GSpan::~GVArray_GSpan()
{
  if (owned_data_ != nullptr) {
    type_->destruct_n(owned_data_, size_);
    MEM_freeN(owned_data_);
  }
}

/* --------------------------------------------------------------------
 * GVMutableArray_GSpan.
 */

GVMutableArray_GSpan::GVMutableArray_GSpan(GVMutableArray &varray, const bool copy_values_to_span)
    : GMutableSpan(varray.type()), varray_(varray)
{
  size_ = varray_.size();
  if (varray_.is_span()) {
    data_ = varray_.get_internal_span().data();
  }
  else {
    owned_data_ = MEM_mallocN_aligned(type_->size() * size_, type_->alignment(), __func__);
    if (copy_values_to_span) {
      varray_.materialize_to_uninitialized(IndexRange(size_), owned_data_);
    }
    else {
      type_->construct_default_n(owned_data_, size_);
    }
    data_ = owned_data_;
  }
}

GVMutableArray_GSpan::~GVMutableArray_GSpan()
{
  if (show_not_saved_warning_) {
    if (!save_has_been_called_) {
      std::cout << "Warning: Call `apply()` to make sure that changes persist in all cases.\n";
    }
  }
  if (owned_data_ != nullptr) {
    type_->destruct_n(owned_data_, size_);
    MEM_freeN(owned_data_);
  }
}

void GVMutableArray_GSpan::save()
{
  save_has_been_called_ = true;
  if (data_ != owned_data_) {
    return;
  }
  const int64_t element_size = type_->size();
  for (int64_t i : IndexRange(size_)) {
    varray_.set_by_copy(i, POINTER_OFFSET(owned_data_, element_size * i));
  }
}

void GVMutableArray_GSpan::disable_not_applied_warning()
{
  show_not_saved_warning_ = false;
}

}  // namespace blender::fn
