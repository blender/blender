/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_generic_virtual_array.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl
 * \{ */

void GVArrayImpl::materialize(const IndexMask &mask, void *dst) const
{
  mask.foreach_index_optimized<int64_t>([&](const int64_t i) {
    void *elem_dst = POINTER_OFFSET(dst, type_->size() * i);
    this->get(i, elem_dst);
  });
}

void GVArrayImpl::materialize_to_uninitialized(const IndexMask &mask, void *dst) const
{
  mask.foreach_index_optimized<int64_t>([&](const int64_t i) {
    void *elem_dst = POINTER_OFFSET(dst, type_->size() * i);
    this->get_to_uninitialized(i, elem_dst);
  });
}

void GVArrayImpl::materialize_compressed(const IndexMask &mask, void *dst) const
{
  mask.foreach_index_optimized<int64_t>([&](const int64_t i, const int64_t pos) {
    void *elem_dst = POINTER_OFFSET(dst, type_->size() * pos);
    this->get(i, elem_dst);
  });
}

void GVArrayImpl::materialize_compressed_to_uninitialized(const IndexMask &mask, void *dst) const
{
  mask.foreach_index_optimized<int64_t>([&](const int64_t i, const int64_t pos) {
    void *elem_dst = POINTER_OFFSET(dst, type_->size() * pos);
    this->get_to_uninitialized(i, elem_dst);
  });
}

void GVArrayImpl::get(const int64_t index, void *r_value) const
{
  type_->destruct(r_value);
  this->get_to_uninitialized(index, r_value);
}

CommonVArrayInfo GVArrayImpl::common_info() const
{
  return {};
}

bool GVArrayImpl::try_assign_VArray(void * /*varray*/) const
{
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVMutableArrayImpl
 * \{ */

void GVMutableArrayImpl::set_by_copy(const int64_t index, const void *value)
{
  BUFFER_FOR_CPP_TYPE_VALUE(*type_, buffer);
  type_->copy_construct(value, buffer);
  this->set_by_move(index, buffer);
  type_->destruct(buffer);
}

void GVMutableArrayImpl::set_by_relocate(const int64_t index, void *value)
{
  this->set_by_move(index, value);
  type_->destruct(value);
}

void GVMutableArrayImpl::set_all(const void *src)
{
  const CommonVArrayInfo info = this->common_info();
  if (info.type == CommonVArrayInfo::Type::Span) {
    type_->copy_assign_n(src, const_cast<void *>(info.data), size_);
  }
  else {
    for (int64_t i : IndexRange(size_)) {
      this->set_by_copy(i, POINTER_OFFSET(src, type_->size() * i));
    }
  }
}

void GVMutableArray::fill(const void *value)
{
  const CommonVArrayInfo info = this->common_info();
  if (info.type == CommonVArrayInfo::Type::Span) {
    this->type().fill_assign_n(value, const_cast<void *>(info.data), this->size());
  }
  else {
    for (int64_t i : IndexRange(this->size())) {
      this->set_by_copy(i, value);
    }
  }
}

bool GVMutableArrayImpl::try_assign_VMutableArray(void * /*varray*/) const
{
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl_For_GSpan
 * \{ */

void GVArrayImpl_For_GSpan::get(const int64_t index, void *r_value) const
{
  type_->copy_assign(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

void GVArrayImpl_For_GSpan::get_to_uninitialized(const int64_t index, void *r_value) const
{
  type_->copy_construct(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

void GVArrayImpl_For_GSpan::set_by_copy(const int64_t index, const void *value)
{
  type_->copy_assign(value, POINTER_OFFSET(data_, element_size_ * index));
}

void GVArrayImpl_For_GSpan::set_by_move(const int64_t index, void *value)
{
  type_->move_construct(value, POINTER_OFFSET(data_, element_size_ * index));
}

void GVArrayImpl_For_GSpan::set_by_relocate(const int64_t index, void *value)
{
  type_->relocate_assign(value, POINTER_OFFSET(data_, element_size_ * index));
}

CommonVArrayInfo GVArrayImpl_For_GSpan::common_info() const
{
  return CommonVArrayInfo{CommonVArrayInfo::Type::Span, true, data_};
}

void GVArrayImpl_For_GSpan::materialize(const IndexMask &mask, void *dst) const
{
  type_->copy_assign_indices(data_, dst, mask);
}

void GVArrayImpl_For_GSpan::materialize_to_uninitialized(const IndexMask &mask, void *dst) const
{
  type_->copy_construct_indices(data_, dst, mask);
}

void GVArrayImpl_For_GSpan::materialize_compressed(const IndexMask &mask, void *dst) const
{
  type_->copy_assign_compressed(data_, dst, mask);
}

void GVArrayImpl_For_GSpan::materialize_compressed_to_uninitialized(const IndexMask &mask,
                                                                    void *dst) const
{
  type_->copy_construct_compressed(data_, dst, mask);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl_For_SingleValueRef
 * \{ */

/* Generic virtual array where each element has the same value. The value is not owned. */

void GVArrayImpl_For_SingleValueRef::get(const int64_t /*index*/, void *r_value) const
{
  type_->copy_assign(value_, r_value);
}
void GVArrayImpl_For_SingleValueRef::get_to_uninitialized(const int64_t /*index*/,
                                                          void *r_value) const
{
  type_->copy_construct(value_, r_value);
}

CommonVArrayInfo GVArrayImpl_For_SingleValueRef::common_info() const
{
  return CommonVArrayInfo{CommonVArrayInfo::Type::Single, true, value_};
}

void GVArrayImpl_For_SingleValueRef::materialize(const IndexMask &mask, void *dst) const
{
  type_->fill_assign_indices(value_, dst, mask);
}

void GVArrayImpl_For_SingleValueRef::materialize_to_uninitialized(const IndexMask &mask,
                                                                  void *dst) const
{
  type_->fill_construct_indices(value_, dst, mask);
}

void GVArrayImpl_For_SingleValueRef::materialize_compressed(const IndexMask &mask, void *dst) const
{
  type_->fill_assign_n(value_, dst, mask.size());
}

void GVArrayImpl_For_SingleValueRef::materialize_compressed_to_uninitialized(const IndexMask &mask,
                                                                             void *dst) const
{
  type_->fill_construct_n(value_, dst, mask.size());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl_For_SingleValue
 * \{ */

/* Same as GVArrayImpl_For_SingleValueRef, but the value is owned. */
class GVArrayImpl_For_SingleValue : public GVArrayImpl_For_SingleValueRef,
                                    NonCopyable,
                                    NonMovable {
 public:
  GVArrayImpl_For_SingleValue(const CPPType &type, const int64_t size, const void *value)
      : GVArrayImpl_For_SingleValueRef(type, size)
  {
    value_ = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
    type.copy_construct(value, (void *)value_);
  }

  ~GVArrayImpl_For_SingleValue() override
  {
    type_->destruct((void *)value_);
    MEM_freeN((void *)value_);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl_For_SmallTrivialSingleValue
 * \{ */

/**
 * Contains an inline buffer that can store a single value of a trivial type.
 * This avoids the allocation that would be done by #GVArrayImpl_For_SingleValue.
 */
template<int BufferSize> class GVArrayImpl_For_SmallTrivialSingleValue : public GVArrayImpl {
 private:
  AlignedBuffer<BufferSize, 8> buffer_;

 public:
  GVArrayImpl_For_SmallTrivialSingleValue(const CPPType &type,
                                          const int64_t size,
                                          const void *value)
      : GVArrayImpl(type, size)
  {
    BLI_assert(type.is_trivial());
    BLI_assert(type.alignment() <= 8);
    BLI_assert(type.size() <= BufferSize);
    type.copy_construct(value, &buffer_);
  }

 private:
  void get(const int64_t /*index*/, void *r_value) const override
  {
    this->copy_value_to(r_value);
  }
  void get_to_uninitialized(const int64_t /*index*/, void *r_value) const override
  {
    this->copy_value_to(r_value);
  }

  void copy_value_to(void *dst) const
  {
    memcpy(dst, &buffer_, type_->size());
  }

  CommonVArrayInfo common_info() const override
  {
    return CommonVArrayInfo{CommonVArrayInfo::Type::Single, true, &buffer_};
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArraySpan
 * \{ */

GVArraySpan::GVArraySpan() = default;

GVArraySpan::GVArraySpan(GVArray varray)
    : GSpan(varray ? &varray.type() : nullptr), varray_(std::move(varray))
{
  if (!varray_) {
    return;
  }

  size_ = varray_.size();
  const CommonVArrayInfo info = varray_.common_info();
  if (info.type == CommonVArrayInfo::Type::Span) {
    data_ = info.data;
  }
  else {
    owned_data_ = MEM_mallocN_aligned(type_->size() * size_, type_->alignment(), __func__);
    varray_.materialize_to_uninitialized(IndexRange(size_), owned_data_);
    data_ = owned_data_;
  }
}

GVArraySpan::GVArraySpan(GVArraySpan &&other)
    : GSpan(other.type_ptr()), varray_(std::move(other.varray_)), owned_data_(other.owned_data_)
{
  if (!varray_) {
    return;
  }

  size_ = varray_.size();
  const CommonVArrayInfo info = varray_.common_info();
  if (info.type == CommonVArrayInfo::Type::Span) {
    data_ = info.data;
  }
  else {
    data_ = owned_data_;
  }
  other.owned_data_ = nullptr;
  other.data_ = nullptr;
  other.size_ = 0;
}

GVArraySpan::~GVArraySpan()
{
  if (owned_data_ != nullptr) {
    type_->destruct_n(owned_data_, size_);
    MEM_freeN(owned_data_);
  }
}

GVArraySpan &GVArraySpan::operator=(GVArraySpan &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) GVArraySpan(std::move(other));
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GMutableVArraySpan
 * \{ */

GMutableVArraySpan::GMutableVArraySpan() = default;

GMutableVArraySpan::GMutableVArraySpan(GVMutableArray varray, const bool copy_values_to_span)
    : GMutableSpan(varray ? &varray.type() : nullptr), varray_(std::move(varray))
{
  if (!varray_) {
    return;
  }
  size_ = varray_.size();
  const CommonVArrayInfo info = varray_.common_info();
  if (info.type == CommonVArrayInfo::Type::Span) {
    data_ = const_cast<void *>(info.data);
  }
  else {
    owned_data_ = MEM_mallocN_aligned(type_->size() * size_, type_->alignment(), __func__);
    if (copy_values_to_span) {
      varray_.materialize_to_uninitialized(IndexRange(size_), owned_data_);
    }
    else {
      type_->default_construct_n(owned_data_, size_);
    }
    data_ = owned_data_;
  }
}

GMutableVArraySpan::GMutableVArraySpan(GMutableVArraySpan &&other)
    : GMutableSpan(other.type_ptr()),
      varray_(std::move(other.varray_)),
      owned_data_(other.owned_data_),
      show_not_saved_warning_(other.show_not_saved_warning_)
{
  if (!varray_) {
    return;
  }
  size_ = varray_.size();
  const CommonVArrayInfo info = varray_.common_info();
  if (info.type == CommonVArrayInfo::Type::Span) {
    data_ = const_cast<void *>(info.data);
  }
  else {
    data_ = owned_data_;
  }
  other.owned_data_ = nullptr;
  other.data_ = nullptr;
  other.size_ = 0;
}

GMutableVArraySpan::~GMutableVArraySpan()
{
  if (varray_) {
    if (show_not_saved_warning_) {
      if (!save_has_been_called_) {
        std::cout << "Warning: Call `save()` to make sure that changes persist in all cases.\n";
      }
    }
  }
  if (owned_data_ != nullptr) {
    type_->destruct_n(owned_data_, size_);
    MEM_freeN(owned_data_);
  }
}

GMutableVArraySpan &GMutableVArraySpan::operator=(GMutableVArraySpan &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) GMutableVArraySpan(std::move(other));
  return *this;
}

void GMutableVArraySpan::save()
{
  save_has_been_called_ = true;
  if (data_ != owned_data_) {
    return;
  }
  varray_.set_all(owned_data_);
}

void GMutableVArraySpan::disable_not_applied_warning()
{
  show_not_saved_warning_ = false;
}

const GVMutableArray &GMutableVArraySpan::varray() const
{
  return varray_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl_For_SlicedGVArray
 * \{ */

class GVArrayImpl_For_SlicedGVArray : public GVArrayImpl {
 protected:
  GVArray varray_;
  int64_t offset_;
  IndexRange slice_;

 public:
  GVArrayImpl_For_SlicedGVArray(GVArray varray, const IndexRange slice)
      : GVArrayImpl(varray.type(), slice.size()),
        varray_(std::move(varray)),
        offset_(slice.start()),
        slice_(slice)
  {
    BLI_assert(slice.one_after_last() <= varray_.size());
  }

  void get(const int64_t index, void *r_value) const override
  {
    varray_.get(index + offset_, r_value);
  }

  void get_to_uninitialized(const int64_t index, void *r_value) const override
  {
    varray_.get_to_uninitialized(index + offset_, r_value);
  }

  CommonVArrayInfo common_info() const override
  {
    const CommonVArrayInfo internal_info = varray_.common_info();
    switch (internal_info.type) {
      case CommonVArrayInfo::Type::Any: {
        return {};
      }
      case CommonVArrayInfo::Type::Span: {
        return CommonVArrayInfo(CommonVArrayInfo::Type::Span,
                                internal_info.may_have_ownership,
                                POINTER_OFFSET(internal_info.data, type_->size() * offset_));
      }
      case CommonVArrayInfo::Type::Single: {
        return internal_info;
      }
    }
    BLI_assert_unreachable();
    return {};
  }

  void materialize_compressed_to_uninitialized(const IndexMask &mask, void *dst) const override
  {
    IndexMaskFromSegment mask_from_segment;
    mask.foreach_segment([&](const IndexMaskSegment segment, const int64_t start) {
      const IndexMask &segment_mask = mask_from_segment.update(
          {segment.offset() + offset_, segment.base_span()});
      varray_.materialize_compressed_to_uninitialized(segment_mask,
                                                      POINTER_OFFSET(dst, type_->size() * start));
    });
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayCommon
 * \{ */

GVArrayCommon::GVArrayCommon(const GVArrayCommon &other) : storage_(other.storage_)
{
  impl_ = this->impl_from_storage();
}

GVArrayCommon::GVArrayCommon(GVArrayCommon &&other) noexcept : storage_(std::move(other.storage_))
{
  impl_ = this->impl_from_storage();
  other.storage_.reset();
  other.impl_ = nullptr;
}

GVArrayCommon::GVArrayCommon(const GVArrayImpl *impl) : impl_(impl)
{
  storage_ = impl_;
}

GVArrayCommon::GVArrayCommon(std::shared_ptr<const GVArrayImpl> impl) : impl_(impl.get())
{
  if (impl) {
    storage_ = std::move(impl);
  }
}

GVArrayCommon::~GVArrayCommon() = default;

void GVArrayCommon::materialize(void *dst) const
{
  this->materialize(IndexMask(impl_->size()), dst);
}

void GVArrayCommon::materialize(const IndexMask &mask, void *dst) const
{
  impl_->materialize(mask, dst);
}

void GVArrayCommon::materialize_to_uninitialized(void *dst) const
{
  this->materialize_to_uninitialized(IndexMask(impl_->size()), dst);
}

void GVArrayCommon::materialize_to_uninitialized(const IndexMask &mask, void *dst) const
{
  BLI_assert(mask.min_array_size() <= impl_->size());
  impl_->materialize_to_uninitialized(mask, dst);
}

void GVArrayCommon::materialize_compressed(const IndexMask &mask, void *dst) const
{
  impl_->materialize_compressed(mask, dst);
}

void GVArrayCommon::materialize_compressed_to_uninitialized(const IndexMask &mask, void *dst) const
{
  impl_->materialize_compressed_to_uninitialized(mask, dst);
}

void GVArrayCommon::copy_from(const GVArrayCommon &other)
{
  if (this == &other) {
    return;
  }
  storage_ = other.storage_;
  impl_ = this->impl_from_storage();
}

void GVArrayCommon::move_from(GVArrayCommon &&other) noexcept
{
  if (this == &other) {
    return;
  }
  storage_ = std::move(other.storage_);
  impl_ = this->impl_from_storage();
  other.storage_.reset();
  other.impl_ = nullptr;
}

bool GVArrayCommon::is_span() const
{
  const CommonVArrayInfo info = impl_->common_info();
  return info.type == CommonVArrayInfo::Type::Span;
}

GSpan GVArrayCommon::get_internal_span() const
{
  BLI_assert(this->is_span());
  const CommonVArrayInfo info = impl_->common_info();
  return GSpan(this->type(), info.data, this->size());
}

bool GVArrayCommon::is_single() const
{
  const CommonVArrayInfo info = impl_->common_info();
  return info.type == CommonVArrayInfo::Type::Single;
}

void GVArrayCommon::get_internal_single(void *r_value) const
{
  BLI_assert(this->is_single());
  const CommonVArrayInfo info = impl_->common_info();
  this->type().copy_assign(info.data, r_value);
}

void GVArrayCommon::get_internal_single_to_uninitialized(void *r_value) const
{
  impl_->type().default_construct(r_value);
  this->get_internal_single(r_value);
}

const GVArrayImpl *GVArrayCommon::impl_from_storage() const
{
  if (!storage_.has_value()) {
    return nullptr;
  }
  return storage_.extra_info().get_varray(storage_.get());
}

IndexRange GVArrayCommon::index_range() const
{
  return IndexRange(this->size());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArray
 * \{ */

GVArray::GVArray(const GVArray &other) = default;

GVArray::GVArray(GVArray &&other) noexcept = default;

GVArray::GVArray(const GVArrayImpl *impl) : GVArrayCommon(impl) {}

GVArray::GVArray(std::shared_ptr<const GVArrayImpl> impl) : GVArrayCommon(std::move(impl)) {}

GVArray::GVArray(varray_tag::single /* tag */,
                 const CPPType &type,
                 int64_t size,
                 const void *value)
{
  if (type.is_trivial() && type.size() <= 16 && type.alignment() <= 8) {
    this->emplace<GVArrayImpl_For_SmallTrivialSingleValue<16>>(type, size, value);
  }
  else {
    this->emplace<GVArrayImpl_For_SingleValue>(type, size, value);
  }
}

GVArray GVArray::ForSingle(const CPPType &type, const int64_t size, const void *value)
{
  return GVArray(varray_tag::single{}, type, size, value);
}

GVArray GVArray::ForSingleRef(const CPPType &type, const int64_t size, const void *value)
{
  return GVArray(varray_tag::single_ref{}, type, size, value);
}

GVArray GVArray::ForSingleDefault(const CPPType &type, const int64_t size)
{
  return GVArray::ForSingleRef(type, size, type.default_value());
}

GVArray GVArray::ForSpan(GSpan span)
{
  return GVArray(varray_tag::span{}, span);
}

class GVArrayImpl_For_GArray : public GVArrayImpl_For_GSpan {
 protected:
  GArray<> array_;

 public:
  GVArrayImpl_For_GArray(GArray<> array)
      : GVArrayImpl_For_GSpan(array.as_mutable_span()), array_(std::move(array))
  {
  }
};

GVArray GVArray::ForGArray(GArray<> array)
{
  return GVArray::For<GVArrayImpl_For_GArray>(array);
}

GVArray GVArray::ForEmpty(const CPPType &type)
{
  return GVArray::ForSpan(GSpan(type));
}

GVArray GVArray::slice(IndexRange slice) const
{
  const CommonVArrayInfo info = this->common_info();
  if (info.type == CommonVArrayInfo::Type::Single) {
    return GVArray::ForSingle(this->type(), slice.size(), info.data);
  }
  /* Need to check for ownership, because otherwise the referenced data can be destructed when
   * #this is destructed. */
  if (info.type == CommonVArrayInfo::Type::Span && !info.may_have_ownership) {
    return GVArray::ForSpan(GSpan(this->type(), info.data, this->size()).slice(slice));
  }
  return GVArray::For<GVArrayImpl_For_SlicedGVArray>(*this, slice);
}

GVArray &GVArray::operator=(const GVArray &other)
{
  this->copy_from(other);
  return *this;
}

GVArray &GVArray::operator=(GVArray &&other) noexcept
{
  this->move_from(std::move(other));
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVMutableArray
 * \{ */

GVMutableArray::GVMutableArray(const GVMutableArray &other) = default;
GVMutableArray::GVMutableArray(GVMutableArray &&other) noexcept = default;

GVMutableArray::GVMutableArray(GVMutableArrayImpl *impl) : GVArrayCommon(impl) {}

GVMutableArray::GVMutableArray(std::shared_ptr<GVMutableArrayImpl> impl)
    : GVArrayCommon(std::move(impl))
{
}

GVMutableArray GVMutableArray::ForSpan(GMutableSpan span)
{
  return GVMutableArray::For<GVArrayImpl_For_GSpan_final>(span);
}

GVMutableArray::operator GVArray() const &
{
  GVArray varray;
  varray.copy_from(*this);
  return varray;
}

GVMutableArray::operator GVArray() &&noexcept
{
  GVArray varray;
  varray.move_from(std::move(*this));
  return varray;
}

GVMutableArray &GVMutableArray::operator=(const GVMutableArray &other)
{
  this->copy_from(other);
  return *this;
}

GVMutableArray &GVMutableArray::operator=(GVMutableArray &&other) noexcept
{
  this->move_from(std::move(other));
  return *this;
}

GVMutableArrayImpl *GVMutableArray::get_implementation() const
{
  return this->get_impl();
}

void GVMutableArray::set_all(const void *src)
{
  this->get_impl()->set_all(src);
}

GMutableSpan GVMutableArray::get_internal_span() const
{
  BLI_assert(this->is_span());
  const CommonVArrayInfo info = impl_->common_info();
  return GMutableSpan(this->type(), const_cast<void *>(info.data), this->size());
}

/** \} */

CommonVArrayInfo GVArrayImpl_For_GSpan_final::common_info() const
{
  return CommonVArrayInfo(CommonVArrayInfo::Type::Span, false, data_);
}

CommonVArrayInfo GVArrayImpl_For_SingleValueRef_final::common_info() const
{
  return CommonVArrayInfo(CommonVArrayInfo::Type::Single, false, value_);
}

}  // namespace blender
