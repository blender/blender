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
 * A `blender::StringRef` references a const char array owned by someone else. It is just a pointer
 * and a size. Since the memory is not owned, StringRef should not be used to transfer ownership of
 * the string. The data referenced by a StringRef cannot be mutated through it.
 *
 * A StringRef is NOT null-terminated. This makes it much more powerful within C++, because we can
 * also cut off parts of the end without creating a copy. When interfacing with C code that expects
 * null-terminated strings, `blender::StringRefNull` can be used. It is essentially the same as
 * StringRef, but with the restriction that the string has to be null-terminated.
 *
 * Whenever possible, string parameters should be of type StringRef and the string return type
 * should be StringRefNull. Don't forget that the StringRefNull does not own the string, so don't
 * return it when the string exists only in the scope of the function. This convention makes
 * functions usable in the most contexts.
 *
 * blender::StringRef vs. std::string_view:
 *   Both types are certainly very similar. The main benefit of using StringRef in Blender is that
 *   this allows us to add convenience methods at any time. Especially, when doing a lot of string
 *   manipulation, this helps to keep the code clean. Furthermore, we need StringRefNull anyway,
 *   because there is a lot of C code that expects null-terminated strings. Once we use C++17,
 *   implicit conversions to and from string_view can be added.
 */

#include <cstring>
#include <sstream>
#include <string>

#include "BLI_span.hh"
#include "BLI_utildefines.h"

namespace blender {

class StringRef;

/**
 * A common base class for StringRef and StringRefNull. This should never be used in other files.
 * It only exists to avoid some code duplication.
 */
class StringRefBase {
 protected:
  const char *data_;
  uint size_;

  StringRefBase(const char *data, const uint size) : data_(data), size_(size)
  {
  }

 public:
  /**
   * Return the (byte-)length of the referenced string, without any null-terminator.
   */
  uint size() const
  {
    return size_;
  }

  /**
   * Return a pointer to the start of the string.
   */
  const char *data() const
  {
    return data_;
  }

  operator Span<char>() const
  {
    return Span<char>(data_, size_);
  }

  /**
   * Implicitly convert to std::string. This is convenient in most cases, but you have to be a bit
   * careful not to convert to std::string accidentally.
   */
  operator std::string() const
  {
    return std::string(data_, size_);
  }

  const char *begin() const
  {
    return data_;
  }

  const char *end() const
  {
    return data_ + size_;
  }

  /**
   * Copy the string into a buffer. The buffer has to be one byte larger than the size of the
   * string, because the copied string will be null-terminated. Only use this when you are
   * absolutely sure that the buffer is large enough.
   */
  void unsafe_copy(char *dst) const
  {
    memcpy(dst, data_, size_);
    dst[size_] = '\0';
  }

  /**
   * Copy the string into a buffer. The copied string will be null-terminated. This invokes
   * undefined behavior when dst_size is too small. (Should we define the behavior?)
   */
  void copy(char *dst, const uint dst_size) const
  {
    if (size_ < dst_size) {
      this->unsafe_copy(dst);
    }
    else {
      BLI_assert(false);
      dst[0] = '\0';
    }
  }

  /**
   * Copy the string into a char array. The copied string will be null-terminated. This invokes
   * undefined behavior when dst is too small.
   */
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

  StringRef substr(uint start, const uint size) const;
};

/**
 * References a null-terminated const char array.
 */
class StringRefNull : public StringRefBase {

 public:
  StringRefNull() : StringRefBase("", 0)
  {
  }

  /**
   * Construct a StringRefNull from a null terminated c-string. The pointer must not point to NULL.
   */
  StringRefNull(const char *str) : StringRefBase(str, (uint)strlen(str))
  {
    BLI_assert(str != NULL);
    BLI_assert(data_[size_] == '\0');
  }

  /**
   * Construct a StringRefNull from a null terminated c-string. This invokes undefined behavior
   * when the given size is not the correct size of the string.
   */
  StringRefNull(const char *str, const uint size) : StringRefBase(str, size)
  {
    BLI_assert((uint)strlen(str) == size);
  }

  /**
   * Reference a std::string. Remember that when the std::string is destructed, the StringRefNull
   * will point to uninitialized memory.
   */
  StringRefNull(const std::string &str) : StringRefNull(str.data())
  {
  }

  /**
   * Get the char at the given index.
   */
  char operator[](const uint index) const
  {
    /* Use '<=' instead of just '<', so that the null character can be accessed as well. */
    BLI_assert(index <= size_);
    return data_[index];
  }
};

/**
 * References a const char array. It might not be null terminated.
 */
class StringRef : public StringRefBase {
 public:
  StringRef() : StringRefBase(nullptr, 0)
  {
  }

  /**
   * StringRefNull can be converted into StringRef, but not the other way around.
   */
  StringRef(StringRefNull other) : StringRefBase(other.data(), other.size())
  {
  }

  /**
   * Create a StringRef from a null-terminated c-string.
   */
  StringRef(const char *str) : StringRefBase(str, str ? (uint)strlen(str) : 0)
  {
  }

  StringRef(const char *str, const uint length) : StringRefBase(str, length)
  {
  }

  /**
   * Create a StringRef from a start and end pointer. This invokes undefined behavior when the
   * second point points to a smaller address than the first one.
   */
  StringRef(const char *begin, const char *one_after_end)
      : StringRefBase(begin, (uint)(one_after_end - begin))
  {
    BLI_assert(begin <= one_after_end);
  }

  /**
   * Reference a std::string. Remember that when the std::string is destructed, the StringRef
   * will point to uninitialized memory.
   */
  StringRef(const std::string &str) : StringRefBase(str.data(), (uint)str.size())
  {
  }

  /**
   * Return a new StringRef that does not contain the first n chars.
   */
  StringRef drop_prefix(const uint n) const
  {
    BLI_assert(n <= size_);
    return StringRef(data_ + n, size_ - n);
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

  /**
   * Get the char at the given index.
   */
  char operator[](uint index) const
  {
    BLI_assert(index < size_);
    return data_[index];
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

/**
 * Adding two #StringRefs will allocate an std::string.
 * This is not efficient, but convenient in most cases.
 */
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

/**
 * Return true when the string starts with the given prefix.
 */
inline bool StringRefBase::startswith(StringRef prefix) const
{
  if (size_ < prefix.size_) {
    return false;
  }
  for (uint i = 0; i < prefix.size_; i++) {
    if (data_[i] != prefix.data_[i]) {
      return false;
    }
  }
  return true;
}

/**
 * Return true when the string ends with the given suffix.
 */
inline bool StringRefBase::endswith(StringRef suffix) const
{
  if (size_ < suffix.size_) {
    return false;
  }
  const uint offset = size_ - suffix.size_;
  for (uint i = 0; i < suffix.size_; i++) {
    if (data_[offset + i] != suffix.data_[i]) {
      return false;
    }
  }
  return true;
}

/**
 * Return a new #StringRef containing only a sub-string of the original string.
 */
inline StringRef StringRefBase::substr(const uint start, const uint size) const
{
  BLI_assert(start + size <= size_);
  return StringRef(data_ + start, size);
}

}  // namespace blender

#endif /* __BLI_STRING_REF_HH__ */
