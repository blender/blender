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

#ifndef __BLI_STRING_REF_HH__
#define __BLI_STRING_REF_HH__

/** \file
 * \ingroup bli
 *
 * A StringRef is a pointer to a string somewhere in memory. It should not be used to transfer
 * ownership of that string. When a function gets a StringRef as input, it cannot expect, that
 * the string will still exist after the function ends.
 *
 * There are two types of string references: One that guarantees null termination and one that does
 * not.
 */

#include <cstring>
#include <sstream>
#include <string>

#include "BLI_array_ref.hh"
#include "BLI_utildefines.h"

namespace BLI {

class StringRef;

class StringRefBase {
 public:
  using size_type = size_t;

 protected:
  const char *m_data;
  size_type m_size;

  StringRefBase(const char *data, size_type size) : m_data(data), m_size(size)
  {
  }

 public:
  /**
   * Return the (byte-)length of the referenced string, without any null-terminator.
   */
  size_type size() const
  {
    return m_size;
  }

  /**
   * Return a pointer to the start of the string.
   */
  const char *data() const
  {
    return m_data;
  }

  char operator[](size_type index) const
  {
    BLI_assert(index <= m_size);
    return m_data[index];
  }

  operator ArrayRef<char>() const
  {
    return ArrayRef<char>(m_data, m_size);
  }

  operator std::string() const
  {
    return std::string(m_data, m_size);
  }

  const char *begin() const
  {
    return m_data;
  }

  const char *end() const
  {
    return m_data + m_size;
  }

  void unsafe_copy(char *dst) const
  {
    memcpy(dst, m_data, m_size);
    dst[m_size] = '\0';
  }

  void copy(char *dst, uint dst_size) const
  {
    if (m_size < dst_size) {
      this->unsafe_copy(dst);
    }
    else {
      BLI_assert(false);
      dst[0] = '\0';
    }
  }

  template<uint N> void copy(char (&dst)[N])
  {
    this->copy(dst, N);
  }

  /**
   * Returns true when the string begins with the given prefix. Otherwise false.
   */
  bool startswith(StringRef prefix) const;

  /**
   * Returns true when the string ends with the given suffix. Otherwise false.
   */
  bool endswith(StringRef suffix) const;

  StringRef substr(uint start, uint size) const;
};

/**
 * References a null-terminated char array.
 */
class StringRefNull : public StringRefBase {

 public:
  StringRefNull() : StringRefBase("", 0)
  {
  }

  StringRefNull(const char *str) : StringRefBase(str, strlen(str))
  {
    BLI_assert(str != NULL);
    BLI_assert(m_data[m_size] == '\0');
  }

  StringRefNull(const char *str, size_type size) : StringRefBase(str, size)
  {
    BLI_assert(str[size] == '\0');
  }

  StringRefNull(const std::string &str) : StringRefNull(str.data())
  {
  }
};

/**
 * References a char array. It might not be null terminated.
 */
class StringRef : public StringRefBase {
 public:
  StringRef() : StringRefBase(nullptr, 0)
  {
  }

  StringRef(StringRefNull other) : StringRefBase(other.data(), other.size())
  {
  }

  StringRef(const char *str) : StringRefBase(str, str ? strlen(str) : 0)
  {
  }

  StringRef(const char *str, size_type length) : StringRefBase(str, length)
  {
  }

  StringRef(const std::string &str) : StringRefBase(str.data(), str.size())
  {
  }

  /**
   * Return a new StringRef that does not contain the first n chars.
   */
  StringRef drop_prefix(uint n) const
  {
    BLI_assert(n <= m_size);
    return StringRef(m_data + n, m_size - n);
  }

  /**
   * Return a new StringRef that with the given prefix being skipped.
   * Asserts that the string begins with the given prefix.
   */
  StringRef drop_prefix(StringRef prefix) const
  {
    BLI_assert(this->startswith(prefix));
    return this->drop_prefix(prefix.size());
  }
};

/* More inline functions
 ***************************************/

inline std::ostream &operator<<(std::ostream &stream, StringRef ref)
{
  stream << std::string(ref);
  return stream;
}

inline std::ostream &operator<<(std::ostream &stream, StringRefNull ref)
{
  stream << std::string(ref.data(), ref.size());
  return stream;
}

inline std::string operator+(StringRef a, StringRef b)
{
  return std::string(a) + std::string(b);
}

inline bool operator==(StringRef a, StringRef b)
{
  if (a.size() != b.size()) {
    return false;
  }
  return STREQLEN(a.data(), b.data(), a.size());
}

inline bool operator!=(StringRef a, StringRef b)
{
  return !(a == b);
}

inline bool StringRefBase::startswith(StringRef prefix) const
{
  if (m_size < prefix.m_size) {
    return false;
  }
  for (uint i = 0; i < prefix.m_size; i++) {
    if (m_data[i] != prefix.m_data[i]) {
      return false;
    }
  }
  return true;
}

inline bool StringRefBase::endswith(StringRef suffix) const
{
  if (m_size < suffix.m_size) {
    return false;
  }
  uint offset = m_size - suffix.m_size;
  for (uint i = 0; i < suffix.m_size; i++) {
    if (m_data[offset + i] != suffix.m_data[i]) {
      return false;
    }
  }
  return true;
}

inline StringRef StringRefBase::substr(uint start, uint size) const
{
  BLI_assert(start + size <= m_size);
  return StringRef(m_data + start, size);
}

}  // namespace BLI

#endif /* __BLI_STRING_REF_HH__ */
