/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <atomic>

#include "BLI_fixed_string.hh"
#include "BLI_hash.hh"
#include "BLI_string_ref.hh"

namespace blender {

namespace detail {

/**
 * Unique storage for a string. Owned by a global table. Comparing pointers to the unique values
 * in the table is enough to test for equality. Do not access these fields directly.
 */
struct UStringEntry {
  std::string str;
  uint64_t hash;

  friend bool operator==(const UStringEntry &a, const UStringEntry &b)
  {
    return a.str == b.str;
  }

  friend bool operator==(const UStringEntry &a, const StringRef b)
  {
    return a.str == b;
  }
};

const blender::detail::UStringEntry &ustring_ensure_entry(StringRef str);
const blender::detail::UStringEntry &ustring_ensure_entry(const char *str);

}  // namespace detail

/**
 * This class is inspired by OpenImageIO's ustring class.
 *
 * See the OpenImageIO documentation for more details:
 * https://openimageio.readthedocs.io/en/stable/imageioapi.html#efficient-unique-strings-ustring
 */
class UString {
 public:
  static constexpr blender::detail::UStringEntry EMPTY_ENTRY = {"", hash_string("")};

 private:
  const blender::detail::UStringEntry *ptr_ = &EMPTY_ENTRY;

  explicit UString(const blender::detail::UStringEntry *ptr) : ptr_(ptr) {}

 public:
  UString() = default;
  explicit UString(const StringRef str) : ptr_(&blender::detail::ustring_ensure_entry(str)) {}

  template<FixedString FStr> friend UString operator""_ustr();

  /**
   * Access the underlying string as a #StringRefNull.
   *
   * Note: This is not an implicit conversion to work around ambiguous function calls.
   */
  StringRefNull ref() const
  {
    return StringRefNull(ptr_->str);
  }

  const std::string &string() const
  {
    return ptr_->str;
  }

  const char *c_str() const
  {
    return ptr_->str.c_str();
  }

  friend bool operator==(const UString &a, const UString &b)
  {
    return a.ptr_ == b.ptr_;
  }

  friend bool operator==(const UString &a, const StringRef b)
  {
    return a.ref() == b;
  }

  uint64_t hash() const
  {
    return ptr_->hash;
  }

  int64_t size() const
  {
    return ptr_->str.size();
  }

  bool is_empty() const
  {
    return ptr_->str.empty();
  }

  char operator[](const int64_t i) const
  {
    /* Accessing null char at end is allowed too. */
    BLI_assert(i >= 0 && i <= this->size());
    return ptr_->str[i];
  }
};

/**
 * Define DefaultHash for UString keys so that it uses the cached hash on ustrings but also
 * supports hashing arbitrary (non-unique) strings in the same way.
 *
 * Note: The string hashes produced here are different from e.g. DefaultHash<StringRef>. That is
 * fine though. The only requirement is that all hashes defined in this template specialization are
 * compatible with each other.
 */
template<> struct DefaultHash<UString> {
  uint64_t operator()(const UString &value) const
  {
    return value.hash();
  }

  constexpr uint64_t operator()(const StringRef value) const
  {
    return get_default_hash(value);
  }
};

/**
 * Like #DefaultHash<UString> above, but for the table entries themselves: uses the cached hash
 * for an existing entry, and supports hashing a #StringRef directly so the global string table
 * can be queried without needing to allocate a #blender::detail::UStringEntry first.
 */
template<> struct DefaultHash<blender::detail::UStringEntry> {
  uint64_t operator()(const blender::detail::UStringEntry &value) const
  {
    return value.hash;
  }

  constexpr uint64_t operator()(const StringRef value) const
  {
    return get_default_hash(value);
  }
};

/**
 * Create a UString from a string literal. This is a template function so that each string is only
 * made unique once and not every time the literal is used.
 *
 * Note: OpenImageIO defines a similar `_us` string literal operator. However, it newly constructs
 * the ustring in each invocation instead of caching it in a static variable. Caching it like here
 * likely only works in C++20.
 */
template<FixedString FStr> inline UString operator""_ustr()
{
  /* This is a more optimized variant of just doing this:
   * \code{.cc}
   *   static UString ustr(FStr.data);
   *   return ustr
   * \endcode
   *
   * The goal of the actual implementation is to improve upon performance and binary size compared
   * to the above. This is possible here we have two pieces of information the compiler can't have:
   *  - The cache is a raw #UStringEntry pointer, so the not-yet-cached check is a plain null
   *    comparison. It never has to dereference a pointer, unlike e.g. comparing hashes or calling
   *    #UString::is_empty.
   *  - It is valid to initialize the static variable more than once and the result will still be
   *    the same because the string does not change. So a double checked lock is not needed.
   */
  /* This is initialized to null by default. */
  static std::atomic<const blender::detail::UStringEntry *> static_entry;
  const blender::detail::UStringEntry *entry = static_entry.load(std::memory_order_acquire);
  if (entry == nullptr) [[unlikely]] {
    entry = &blender::detail::ustring_ensure_entry(FStr.data);
    static_entry.store(entry, std::memory_order_release);
  }
  return UString(entry);
}

/**
 * Support using the `fmt` library with #UString.
 */
inline std::string_view format_as(UString str)
{
  return std::string_view(str.ref());
}

}  // namespace blender
