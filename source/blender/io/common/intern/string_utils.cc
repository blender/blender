/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "IO_string_utils.hh"

/* Note: we could use C++17 <charconv> from_chars to parse
 * floats, but even if some compilers claim full support,
 * their standard libraries are not quite there yet.
 * LLVM/libc++ only has a float parser since LLVM 14,
 * and gcc/libstdc++ since 11.1. So until at least these are
 * the mininum spec, use an external library. */
#include "fast_float.h"
#include <charconv>

namespace blender::io {

StringRef read_next_line(StringRef &buffer)
{
  const char *start = buffer.begin();
  const char *end = buffer.end();
  size_t len = 0;
  char prev = 0;
  const char *ptr = start;
  while (ptr < end) {
    char c = *ptr++;
    if (c == '\n' && prev != '\\') {
      break;
    }
    prev = c;
    ++len;
  }

  buffer = StringRef(ptr, end);
  return StringRef(start, len);
}

static bool is_whitespace(char c)
{
  return c <= ' ' || c == '\\';
}

StringRef drop_whitespace(StringRef str)
{
  while (!str.is_empty() && is_whitespace(str[0])) {
    str = str.drop_prefix(1);
  }
  return str;
}

StringRef drop_non_whitespace(StringRef str)
{
  while (!str.is_empty() && !is_whitespace(str[0])) {
    str = str.drop_prefix(1);
  }
  return str;
}

static StringRef drop_plus(StringRef str)
{
  if (!str.is_empty() && str[0] == '+') {
    str = str.drop_prefix(1);
  }
  return str;
}

StringRef parse_float(StringRef str, float fallback, float &dst, bool skip_space)
{
  if (skip_space) {
    str = drop_whitespace(str);
  }
  str = drop_plus(str);
  fast_float::from_chars_result res = fast_float::from_chars(str.begin(), str.end(), dst);
  if (res.ec == std::errc::invalid_argument || res.ec == std::errc::result_out_of_range) {
    dst = fallback;
  }
  return StringRef(res.ptr, str.end());
}

StringRef parse_floats(StringRef str, float fallback, float *dst, int count)
{
  for (int i = 0; i < count; ++i) {
    str = parse_float(str, fallback, dst[i]);
  }
  return str;
}

StringRef parse_int(StringRef str, int fallback, int &dst, bool skip_space)
{
  if (skip_space) {
    str = drop_whitespace(str);
  }
  str = drop_plus(str);
  std::from_chars_result res = std::from_chars(str.begin(), str.end(), dst);
  if (res.ec == std::errc::invalid_argument || res.ec == std::errc::result_out_of_range) {
    dst = fallback;
  }
  return StringRef(res.ptr, str.end());
}

}  // namespace blender::io
