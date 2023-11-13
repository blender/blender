/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_cpp_type.hh"
#include "BLI_span.hh"

namespace blender {

/**
 * A generic span. It behaves just like a blender::Span<T>, but the type is only known at run-time.
 */
class GSpan {
 protected:
  const CPPType *type_ = nullptr;
  const void *data_ = nullptr;
  int64_t size_ = 0;

 public:
  GSpan() = default;

  GSpan(const CPPType *type, const void *buffer, int64_t size)
      : type_(type), data_(buffer), size_(size)
  {
    BLI_assert(size >= 0);
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(size == 0 || type != nullptr);
    BLI_assert(type == nullptr || type->pointer_has_valid_alignment(buffer));
  }

  GSpan(const CPPType &type, const void *buffer, int64_t size) : GSpan(&type, buffer, size) {}

  GSpan(const CPPType &type) : type_(&type) {}

  GSpan(const CPPType *type) : type_(type) {}

  template<typename T>
  GSpan(Span<T> array)
      : GSpan(CPPType::get<T>(), static_cast<const void *>(array.data()), array.size())
  {
  }

  const CPPType &type() const
  {
    BLI_assert(type_ != nullptr);
    return *type_;
  }

  const CPPType *type_ptr() const
  {
    return type_;
  }

  bool is_empty() const
  {
    return size_ == 0;
  }

  int64_t size() const
  {
    return size_;
  }

  int64_t size_in_bytes() const
  {
    return type_->size() * size_;
  }

  const void *data() const
  {
    return data_;
  }

  const void *operator[](int64_t index) const
  {
    BLI_assert(index < size_);
    return POINTER_OFFSET(data_, type_->size() * index);
  }

  template<typename T> Span<T> typed() const
  {
    BLI_assert(type_->is<T>());
    return Span<T>(static_cast<const T *>(data_), size_);
  }

  GSpan slice(const int64_t start, int64_t size) const
  {
    BLI_assert(start >= 0);
    BLI_assert(size >= 0);
    BLI_assert(start + size <= size_ || size == 0);
    return GSpan(type_, POINTER_OFFSET(data_, type_->size() * start), size);
  }

  GSpan slice(const IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  GSpan drop_front(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::max<int64_t>(0, size_ - n);
    return GSpan(*type_, POINTER_OFFSET(data_, type_->size() * n), new_size);
  }

  GSpan drop_back(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::max<int64_t>(0, size_ - n);
    return GSpan(*type_, data_, new_size);
  }

  GSpan take_front(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::min<int64_t>(size_, n);
    return GSpan(*type_, data_, new_size);
  }

  GSpan take_back(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::min<int64_t>(size_, n);
    return GSpan(*type_, POINTER_OFFSET(data_, type_->size() * (size_ - new_size)), new_size);
  }
};

/**
 * A generic mutable span. It behaves just like a blender::MutableSpan<T>, but the type is only
 * known at run-time.
 */
class GMutableSpan {
 protected:
  const CPPType *type_ = nullptr;
  void *data_ = nullptr;
  int64_t size_ = 0;

 public:
  GMutableSpan() = default;

  GMutableSpan(const CPPType *type, void *buffer, int64_t size)
      : type_(type), data_(buffer), size_(size)
  {
    BLI_assert(size >= 0);
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(size == 0 || type != nullptr);
    BLI_assert(type == nullptr || type->pointer_has_valid_alignment(buffer));
  }

  GMutableSpan(const CPPType &type, void *buffer, int64_t size) : GMutableSpan(&type, buffer, size)
  {
  }

  GMutableSpan(const CPPType &type) : type_(&type) {}

  GMutableSpan(const CPPType *type) : type_(type) {}

  template<typename T>
  GMutableSpan(MutableSpan<T> array)
      : GMutableSpan(CPPType::get<T>(), static_cast<void *>(array.begin()), array.size())
  {
  }

  operator GSpan() const
  {
    return GSpan(type_, data_, size_);
  }

  const CPPType &type() const
  {
    BLI_assert(type_ != nullptr);
    return *type_;
  }

  const CPPType *type_ptr() const
  {
    return type_;
  }

  bool is_empty() const
  {
    return size_ == 0;
  }

  int64_t size() const
  {
    return size_;
  }

  int64_t size_in_bytes() const
  {
    return type_->size() * size_;
  }

  void *data() const
  {
    return data_;
  }

  void *operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return POINTER_OFFSET(data_, type_->size() * index);
  }

  template<typename T> MutableSpan<T> typed() const
  {
    BLI_assert(type_->is<T>());
    return MutableSpan<T>(static_cast<T *>(data_), size_);
  }

  GMutableSpan slice(const int64_t start, int64_t size) const
  {
    BLI_assert(start >= 0);
    BLI_assert(size >= 0);
    BLI_assert(start + size <= size_ || size == 0);
    return GMutableSpan(type_, POINTER_OFFSET(data_, type_->size() * start), size);
  }

  GMutableSpan slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  GMutableSpan drop_front(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::max<int64_t>(0, size_ - n);
    return GMutableSpan(*type_, POINTER_OFFSET(data_, type_->size() * n), new_size);
  }

  GMutableSpan drop_back(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::max<int64_t>(0, size_ - n);
    return GMutableSpan(*type_, data_, new_size);
  }

  GMutableSpan take_front(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::min<int64_t>(size_, n);
    return GMutableSpan(*type_, data_, new_size);
  }

  GMutableSpan take_back(const int64_t n) const
  {
    BLI_assert(n >= 0);
    const int64_t new_size = std::min<int64_t>(size_, n);
    return GMutableSpan(
        *type_, POINTER_OFFSET(data_, type_->size() * (size_ - new_size)), new_size);
  }

  /**
   * Copy all values from another span into this span. This invokes undefined behavior when the
   * destination contains uninitialized data and T is not trivially copy constructible.
   * The size of both spans is expected to be the same.
   */
  void copy_from(GSpan values)
  {
    BLI_assert(type_ == &values.type());
    BLI_assert(size_ == values.size());
    type_->copy_assign_n(values.data(), data_, size_);
  }
};

}  // namespace blender
