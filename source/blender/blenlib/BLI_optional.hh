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

/** \file
 * \ingroup bli
 *
 * Simple version of std::optional, which is only available since C++17.
 */

#ifndef __BLI_OPTIONAL_HH__
#define __BLI_OPTIONAL_HH__

#include "BLI_memory_utils.hh"
#include "BLI_utildefines.h"

#include <algorithm>
#include <memory>

namespace BLI {

template<typename T> class Optional {
 private:
  AlignedBuffer<sizeof(T), alignof(T)> m_storage;
  bool m_set;

 public:
  Optional() : m_set(false)
  {
  }

  ~Optional()
  {
    this->reset();
  }

  Optional(const T &value) : Optional()
  {
    this->set(value);
  }

  Optional(T &&value) : Optional()
  {
    this->set(std::forward<T>(value));
  }

  Optional(const Optional &other) : Optional()
  {
    if (other.has_value()) {
      this->set(other.value());
    }
  }

  Optional(Optional &&other) : Optional()
  {
    if (other.has_value()) {
      this->set(std::move(other.value()));
    }
  }

  Optional &operator=(const Optional &other)
  {
    if (this == &other) {
      return *this;
    }
    if (other.has_value()) {
      this->set(other.value());
    }
    else {
      this->reset();
    }
    return *this;
  }

  Optional &operator=(Optional &&other)
  {
    if (this == &other) {
      return *this;
    }
    if (other.has_value()) {
      this->set(std::move(other.value()));
    }
    else {
      this->reset();
    }
    return *this;
  }

  bool has_value() const
  {
    return m_set;
  }

  const T &value() const
  {
    BLI_assert(m_set);
    return *this->value_ptr();
  }

  T &value()
  {
    BLI_assert(m_set);
    return *this->value_ptr();
  }

  void set(const T &value)
  {
    if (m_set) {
      this->value() = value;
    }
    else {
      new (this->value_ptr()) T(value);
      m_set = true;
    }
  }

  void set(T &&value)
  {
    if (m_set) {
      this->value() = std::move(value);
    }
    else {
      new (this->value_ptr()) T(std::move(value));
      m_set = true;
    }
  }

  void set_new(const T &value)
  {
    BLI_assert(!m_set);
    new (this->value_ptr()) T(value);
    m_set = true;
  }

  void set_new(T &&value)
  {
    BLI_assert(!m_set);
    new (this->value_ptr()) T(std::move(value));
    m_set = true;
  }

  void reset()
  {
    if (m_set) {
      this->value_ptr()->~T();
      m_set = false;
    }
  }

  T extract()
  {
    BLI_assert(m_set);
    T value = std::move(this->value());
    this->reset();
    return value;
  }

  T *operator->()
  {
    return this->value_ptr();
  }

  T &operator*()
  {
    return *this->value_ptr();
  }

 private:
  T *value_ptr() const
  {
    return (T *)m_storage.ptr();
  }
};

} /* namespace BLI */

#endif /* __BLI_OPTIONAL_HH__ */
