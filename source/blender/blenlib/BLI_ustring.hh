/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <OpenImageIO/ustring.h>

#include "BLI_fixed_string.hh"
#include "BLI_hash.hh"
#include "BLI_string_ref.hh"

namespace blender {

/**
 * This is a thin wrapper around OpenImageIO's ustring class. Additionally it also provides
 * conversions to our StringRef types.
 *
 * See the OpenImageIO documentation for more details:
 * https://openimageio.readthedocs.io/en/stable/imageioapi.html#efficient-unique-strings-ustring
 */
class UString {
 private:
  /**
   * Using a member instead of inheritance because it simplifies avoiding various ambiguities with
   * operator overloads (especially equality comparison between UString, StringRef, std::string,
   * std::string_view, OpenImageIO::string_view, etc.).
   */
  OpenImageIO::ustring ustr_;

 public:
  UString() = default;
  explicit UString(const StringRef str) : ustr_(std::string_view(str)) {}

  /**
   * Access the underlying string as a #StringRefNull.
   *
   * Note: This is not an implicit conversion to work around ambiguous function calls.
   */
  StringRefNull ref() const
  {
    return StringRefNull(ustr_.c_str(), ustr_.length());
  }

  const std::string &string() const
  {
    return ustr_.string();
  }

  const char *c_str() const
  {
    return ustr_.c_str();
  }

  friend bool operator==(const UString &a, const UString &b)
  {
    return a.ustr_ == b.ustr_;
  }

  friend bool operator==(const UString &a, const StringRef b)
  {
    return a.ref() == b;
  }

  uint64_t hash() const
  {
    return ustr_.hash();
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
    /* This is the hash function used by OpenImageIO::ustring::make_unique internally. */
    return OpenImageIO::Strutil::strhash64(value.size(), value.data());
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
  /* Cache the resulting UString in a static variable so that it only has to be made unique once.
   * A separate static variable is used for each different string because this entire function is
   * templated on the string. */
  static UString ustr(FStr.data);
  return ustr;
}

}  // namespace blender
