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

#pragma once

#include "FN_cpp_type.hh"

namespace blender::fn {

/**
 * A generic pointer whose type is only known at runtime.
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

  GMutablePointer(const CPPType &type, void *data = nullptr) : GMutablePointer(&type, data)
  {
  }

  template<typename T> GMutablePointer(T *data) : GMutablePointer(&CPPType::get<T>(), data)
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
    return reinterpret_cast<T *>(data_);
  }

  template<typename T> bool is_type() const
  {
    return type_ != nullptr && type_->is<T>();
  }

  void destruct()
  {
    BLI_assert(data_ != nullptr);
    type_->destruct(data_);
  }
};

}  // namespace blender::fn
