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
 *   because there is a lot of C code that expects null-terminated strings. Conversion between
 *   StringRef and string_view is very cheap and can be done at api boundaries at essentially no
 *   cost. Another benefit of using StringRef is that it uses signed integers, thus developers
 *   have to deal less with issues resulting from unsigned integers.
 */

#include <cstring>
#include <sstream>
#include <string>
#include <string_view>

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
  int64_t size_;

  constexpr StringRefBase(const char *data, const int64_t size) : data_(data), size_(size)
  {
  }

 public:
  /* Similar to string_view::npos, but signed. */
  static constexpr int64_t not_found = -1;

  /**
   * Return the (byte-)length of the referenced string, without any null-terminator.
   */
  constexpr int64_t size() const
  {
    return size_;
  }

  constexpr bool is_empty() const
  {
    return size_ == 0;
  }

  /**
   * Return a pointer to the start of the string.
   */
  constexpr const char *data() const
  {
    return data_;
  }

  constexpr operator Span<char>() const
  {
    return Span<char>(data_, size_);
  }

  /**
   * Implicitly convert to std::string. This is convenient in most cases, but you have to be a bit
   * careful not to convert to std::string accidentally.
   */
  operator std::string() const
  {
    return std::string(data_, static_cast<size_t>(size_));
  }

  constexpr operator std::string_view() const
  {
    return std::string_view(data_, static_cast<size_t>(size_));
  }

  constexpr const char *begin() const
  {
    return data_;
  }

  constexpr const char *end() const
  {
    return data_ + size_;
  }

  constexpr IndexRange index_range() const
  {
    return IndexRange(size_);
  }

  /**
   * Copy the string into a buffer. The buffer has to be one byte larger than the size of the
   * string, because the copied string will be null-terminated. Only use this when you are
   * absolutely sure that the buffer is large enough.
   */
  void unsafe_copy(char *dst) const
  {
    memcpy(dst, data_, static_cast<size_t>(size_));
    dst[size_] = '\0';
  }

  /**
   * Copy the string into a buffer. The copied string will be null-terminated. This invokes
   * undefined behavior when dst_size is too small. (Should we define the behavior?)
   */
  void copy(char *dst, const int64_t dst_size) const
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
  template<size_t N> void copy(char (&dst)[N]) const
  {
    this->copy(dst, N);
  }

  /**
   * Returns true when the string begins with the given prefix. Otherwise false.
   */
  constexpr bool startswith(StringRef prefix) const;

  /**
   * Returns true when the string ends with the given suffix. Otherwise false.
   */
  constexpr bool endswith(StringRef suffix) const;

  constexpr StringRef substr(int64_t start, const int64_t size) const;

  /**
   * Get the first char in the string. This invokes undefined behavior when the string is empty.
   */
  constexpr const char &front() const
  {
    BLI_assert(size_ >= 1);
    return data_[0];
  }

  /**
   * Get the last char in the string. This invokes undefined behavior when the string is empty.
   */
  constexpr const char &back() const
  {
    BLI_assert(size_ >= 1);
    return data_[size_ - 1];
  }

  /**
   * The behavior of those functions matches the standard library implementation of
   * std::string_view.
   */
  constexpr int64_t find(char c, int64_t pos = 0) const;
  constexpr int64_t find(StringRef str, int64_t pos = 0) const;
  constexpr int64_t rfind(char c, int64_t pos = INT64_MAX) const;
  constexpr int64_t rfind(StringRef str, int64_t pos = INT64_MAX) const;
  constexpr int64_t find_first_of(StringRef chars, int64_t pos = 0) const;
  constexpr int64_t find_first_of(char c, int64_t pos = 0) const;
  constexpr int64_t find_last_of(StringRef chars, int64_t pos = INT64_MAX) const;
  constexpr int64_t find_last_of(char c, int64_t pos = INT64_MAX) const;
  constexpr int64_t find_first_not_of(StringRef chars, int64_t pos = 0) const;
  constexpr int64_t find_first_not_of(char c, int64_t pos = 0) const;
  constexpr int64_t find_last_not_of(StringRef chars, int64_t pos = INT64_MAX) const;
  constexpr int64_t find_last_not_of(char c, int64_t pos = INT64_MAX) const;
};

/**
 * References a null-terminated const char array.
 */
class StringRefNull : public StringRefBase {

 public:
  constexpr StringRefNull() : StringRefBase("", 0)
  {
  }

  /**
   * Construct a StringRefNull from a null terminated c-string. The pointer must not point to
   * NULL.
   */
  StringRefNull(const char *str) : StringRefBase(str, static_cast<int64_t>(strlen(str)))
  {
    BLI_assert(str != nullptr);
    BLI_assert(data_[size_] == '\0');
  }

  /**
   * Construct a StringRefNull from a null terminated c-string. This invokes undefined behavior
   * when the given size is not the correct size of the string.
   */
  constexpr StringRefNull(const char *str, const int64_t size) : StringRefBase(str, size)
  {
    BLI_assert(static_cast<int64_t>(strlen(str)) == size);
  }

  /**
   * Reference a std::string. Remember that when the std::string is destructed, the StringRefNull
   * will point to uninitialized memory.
   */
  StringRefNull(const std::string &str) : StringRefNull(str.c_str())
  {
  }

  /**
   * Get the char at the given index.
   */
  constexpr char operator[](const int64_t index) const
  {
    BLI_assert(index >= 0);
    /* Use '<=' instead of just '<', so that the null character can be accessed as well. */
    BLI_assert(index <= size_);
    return data_[index];
  }

  /**
   * Returns the beginning of a null-terminated char array.
   *
   * This is like ->data(), but can only be called on a StringRefNull.
   */
  constexpr const char *c_str() const
  {
    return data_;
  }
};

/**
 * References a const char array. It might not be null terminated.
 */
class StringRef : public StringRefBase {
 public:
  constexpr StringRef() : StringRefBase(nullptr, 0)
  {
  }

  /**
   * StringRefNull can be converted into StringRef, but not the other way around.
   */
  constexpr StringRef(StringRefNull other) : StringRefBase(other.data(), other.size())
  {
  }

  /**
   * Create a StringRef from a null-terminated c-string.
   */
  constexpr StringRef(const char *str)
      : StringRefBase(str, str ? static_cast<int64_t>(std::char_traits<char>::length(str)) : 0)
  {
  }

  constexpr StringRef(const char *str, const int64_t length) : StringRefBase(str, length)
  {
  }

  /**
   * Create a StringRef from a start and end pointer. This invokes undefined behavior when the
   * second point points to a smaller address than the first one.
   */
  constexpr StringRef(const char *begin, const char *one_after_end)
      : StringRefBase(begin, static_cast<int64_t>(one_after_end - begin))
  {
    BLI_assert(begin <= one_after_end);
  }

  /**
   * Reference a std::string. Remember that when the std::string is destructed, the StringRef
   * will point to uninitialized memory.
   */
  StringRef(const std::string &str) : StringRefBase(str.data(), static_cast<int64_t>(str.size()))
  {
  }

  constexpr StringRef(std::string_view view)
      : StringRefBase(view.data(), static_cast<int64_t>(view.size()))
  {
  }

  /**
   * Returns a new StringRef that does not contain the first n chars.
   *
   * This is similar to std::string_view::remove_prefix.
   */
  constexpr StringRef drop_prefix(const int64_t n) const
  {
    BLI_assert(n >= 0);
    BLI_assert(n <= size_);
    return StringRef(data_ + n, size_ - n);
  }

  /**
   * Return a new StringRef with the given prefix being skipped. This invokes undefined behavior if
   * the string does not begin with the given prefix.
   */
  constexpr StringRef drop_prefix(StringRef prefix) const
  {
    BLI_assert(this->startswith(prefix));
    return this->drop_prefix(prefix.size());
  }

  /**
   * Return a new StringRef that does not contain the last n chars.
   *
   * This is similar to std::string_view::remove_suffix.
   */
  constexpr StringRef drop_suffix(const int64_t n) const
  {
    BLI_assert(n >= 0);
    BLI_assert(n <= size_);
    return StringRef(data_, size_ - n);
  }

  /**
   * Get the char at the given index.
   */
  constexpr char operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
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
  stream << std::string(ref.data(), (size_t)ref.size());
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

/* This does not compare StringRef and std::string_view, because of ambiguous overloads. This is
 * not a problem when std::string_view is only used at api boundaries. To compare a StringRef and a
 * std::string_view, one should convert the std::string_view to StringRef (which is very cheap).
 * Ideally, we only use StringRef in our code to avoid this problem altogether. */
constexpr inline bool operator==(StringRef a, StringRef b)
{
  if (a.size() != b.size()) {
    return false;
  }
  return STREQLEN(a.data(), b.data(), (size_t)a.size());
}

constexpr inline bool operator!=(StringRef a, StringRef b)
{
  return !(a == b);
}

constexpr inline bool operator<(StringRef a, StringRef b)
{
  return std::string_view(a) < std::string_view(b);
}

constexpr inline bool operator>(StringRef a, StringRef b)
{
  return std::string_view(a) > std::string_view(b);
}

constexpr inline bool operator<=(StringRef a, StringRef b)
{
  return std::string_view(a) <= std::string_view(b);
}

constexpr inline bool operator>=(StringRef a, StringRef b)
{
  return std::string_view(a) >= std::string_view(b);
}

/**
 * Return true when the string starts with the given prefix.
 */
constexpr inline bool StringRefBase::startswith(StringRef prefix) const
{
  if (size_ < prefix.size_) {
    return false;
  }
  for (int64_t i = 0; i < prefix.size_; i++) {
    if (data_[i] != prefix.data_[i]) {
      return false;
    }
  }
  return true;
}

/**
 * Return true when the string ends with the given suffix.
 */
constexpr inline bool StringRefBase::endswith(StringRef suffix) const
{
  if (size_ < suffix.size_) {
    return false;
  }
  const int64_t offset = size_ - suffix.size_;
  for (int64_t i = 0; i < suffix.size_; i++) {
    if (data_[offset + i] != suffix.data_[i]) {
      return false;
    }
  }
  return true;
}

/**
 * Return a new #StringRef containing only a sub-string of the original string.
 */
constexpr inline StringRef StringRefBase::substr(const int64_t start,
                                                 const int64_t max_size = INT64_MAX) const
{
  BLI_assert(max_size >= 0);
  BLI_assert(start >= 0);
  const int64_t substr_size = std::min(max_size, size_ - start);
  return StringRef(data_ + start, substr_size);
}

constexpr inline int64_t index_or_npos_to_int64(size_t index)
{
  /* The compiler will probably optimize this check away. */
  if (index == std::string_view::npos) {
    return StringRef::not_found;
  }
  return static_cast<int64_t>(index);
}

constexpr inline int64_t StringRefBase::find(char c, int64_t pos) const
{
  BLI_assert(pos >= 0);
  return index_or_npos_to_int64(std::string_view(*this).find(c, static_cast<size_t>(pos)));
}

constexpr inline int64_t StringRefBase::find(StringRef str, int64_t pos) const
{
  BLI_assert(pos >= 0);
  return index_or_npos_to_int64(std::string_view(*this).find(str, static_cast<size_t>(pos)));
}

constexpr inline int64_t StringRefBase::find_first_of(StringRef chars, int64_t pos) const
{
  BLI_assert(pos >= 0);
  return index_or_npos_to_int64(
      std::string_view(*this).find_first_of(chars, static_cast<size_t>(pos)));
}

constexpr inline int64_t StringRefBase::find_first_of(char c, int64_t pos) const
{
  return this->find_first_of(StringRef(&c, 1), pos);
}

constexpr inline int64_t StringRefBase::find_last_of(StringRef chars, int64_t pos) const
{
  BLI_assert(pos >= 0);
  return index_or_npos_to_int64(
      std::string_view(*this).find_last_of(chars, static_cast<size_t>(pos)));
}

constexpr inline int64_t StringRefBase::find_last_of(char c, int64_t pos) const
{
  return this->find_last_of(StringRef(&c, 1), pos);
}

constexpr inline int64_t StringRefBase::find_first_not_of(StringRef chars, int64_t pos) const
{
  BLI_assert(pos >= 0);
  return index_or_npos_to_int64(
      std::string_view(*this).find_first_not_of(chars, static_cast<size_t>(pos)));
}

constexpr inline int64_t StringRefBase::find_first_not_of(char c, int64_t pos) const
{
  return this->find_first_not_of(StringRef(&c, 1), pos);
}

constexpr inline int64_t StringRefBase::find_last_not_of(StringRef chars, int64_t pos) const
{
  BLI_assert(pos >= 0);
  return index_or_npos_to_int64(
      std::string_view(*this).find_last_not_of(chars, static_cast<size_t>(pos)));
}

constexpr inline int64_t StringRefBase::find_last_not_of(char c, int64_t pos) const
{
  return this->find_last_not_of(StringRef(&c, 1), pos);
}

}  // namespace blender
