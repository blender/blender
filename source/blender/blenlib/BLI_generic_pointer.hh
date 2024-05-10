/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_cpp_type.hh"

namespace blender {

/**
 * A generic non-const pointer whose type is only known at runtime.
 */
class GMutablePointer {
 private:
  const CPPType *type_ = nullptr;
  void *data_ = nullptr;

 public:
  GMutablePointer() = default;

  GMutablePointer(const CPPType *type, void *data = nullptr) : type_(type), data_(data)
  {
    /* If there is data, there has to be a type. */
    BLI_assert(data_ == nullptr || type_ != nullptr);
  }

  GMutablePointer(const CPPType &type, void *data = nullptr) : GMutablePointer(&type, data) {}

  template<typename T, BLI_ENABLE_IF(!std::is_void_v<T>)>
  GMutablePointer(T *data) : GMutablePointer(&CPPType::get<T>(), data)
  {
  }

  void *get() const
  {
    return data_;
  }

  const CPPType *type() const
  {
    return type_;
  }

  template<typename T> T *get() const
  {
    BLI_assert(this->is_type<T>());
    return static_cast<T *>(data_);
  }

  template<typename T> bool is_type() const
  {
    return type_ != nullptr && type_->is<T>();
  }

  template<typename T> T relocate_out()
  {
    BLI_assert(this->is_type<T>());
    T value;
    type_->relocate_assign(data_, &value);
    data_ = nullptr;
    type_ = nullptr;
    return value;
  }

  void destruct()
  {
    BLI_assert(data_ != nullptr);
    type_->destruct(data_);
  }
};

/**
 * A generic const pointer whose type is only known at runtime.
 */
class GPointer {
 private:
  const CPPType *type_ = nullptr;
  const void *data_ = nullptr;

 public:
  GPointer() = default;

  GPointer(GMutablePointer ptr) : type_(ptr.type()), data_(ptr.get()) {}

  GPointer(const CPPType *type, const void *data = nullptr) : type_(type), data_(data)
  {
    /* If there is data, there has to be a type. */
    BLI_assert(data_ == nullptr || type_ != nullptr);
  }

  GPointer(const CPPType &type, const void *data = nullptr) : type_(&type), data_(data) {}

  template<typename T> GPointer(T *data) : GPointer(&CPPType::get<T>(), data) {}

  const void *get() const
  {
    return data_;
  }

  const CPPType *type() const
  {
    return type_;
  }

  template<typename T> const T *get() const
  {
    BLI_assert(this->is_type<T>());
    return static_cast<const T *>(data_);
  }

  template<typename T> bool is_type() const
  {
    return type_ != nullptr && type_->is<T>();
  }
};

}  // namespace blender
