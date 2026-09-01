/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_ustring.hh"

#include "BLI_concurrent_set.hh"

namespace blender {

using StringTable = ConcurrentSet<blender::detail::UStringEntry>;
static StringTable &global_string_table()
{
  static StringTable table = []() {
    StringTable table;
    /* Slightly larger than the number used for a default startup. */
    table.reserve(16384);
    return table;
  }();
  return table;
}

namespace detail {

const blender::detail::UStringEntry &ustring_ensure_entry(const StringRef str)
{
  /* Use the same EMPTY_ENTRY used by the default constructor so that all empty #UString instances
   * compare equal, and so that we skip the table lookup and associated overhead for the default
   * constructor. */
  if (str.is_empty()) {
    return UString::EMPTY_ENTRY;
  }

  StringTable &table = global_string_table();

  /* Avoid constructing a #std::string when the string is already stored. The TBB set
   * implementation does not support creating the actual key type lazily. Though it isn't ideal,
   * the second set lookup and std::string allocation only happen once per string. */
  if (const UStringEntry *entry = table.lookup_ptr(str)) {
    return *entry;
  }
  return table.lookup_key_or_add({std::string(str), hash_string(str)});
}

const blender::detail::UStringEntry &ustring_ensure_entry(const char *str)
{
  return ustring_ensure_entry(StringRef(str));
}

}  // namespace detail

}  // namespace blender
