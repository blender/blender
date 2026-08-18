/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_socket_items_name_util.hh"

namespace blender::nodes::socket_items {

std::string variable_name_find_short(StringRef src_name, FunctionRef<bool(StringRef)> exists_fn)
{
  /* The goal is to find a single-letter name that is not used already. Ideally, it starts with the
   * same letter as the given name. */

  char initial = 'a';
  if (!src_name.is_empty()) {
    const char first_c = src_name[0];
    if (first_c >= 'a' && first_c <= 'z') {
      initial = first_c;
    }
    else if (first_c >= 'A' && first_c <= 'Z') {
      initial = first_c - 'A' + 'a';
    }
  }
  for (const int i : IndexRange('z' - 'a' + 1)) {
    char c = initial + i;
    if (c > 'z') {
      /* Start at 'a' again. */
      c = c - 'z' + 'a' - 1;
    }
    const std::string potential_name = std::string(1, c);
    if (!exists_fn(potential_name)) {
      return potential_name;
    }
  }
  return src_name;
}

std::string variable_name_validate(StringRef name)
{
  /* The name has to start with a letter or underscore. The remaining letters may additionally be
   * digits. */
  std::string result;
  if (name.is_empty()) {
    return result;
  }
  const char first_char = name[0];
  if (!std::isalpha(first_char) && first_char != '_') {
    result += '_';
  }
  for (const char c : name) {
    if (std::isalnum(c) || c == '_') {
      result += c;
    }
    if (ELEM(c, '-', '.', ' ', '\t')) {
      result += '_';
    }
  }

  return result;
}

}  // namespace blender::nodes::socket_items
