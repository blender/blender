/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cctype>
#include <cstdio>

#include "uri_convert.hh" /* Own include. */

bool url_encode(const char *str, char *dst, size_t dst_size)
{
  size_t i = 0;

  while (*str && i < dst_size - 1) {
    char c = char(*str);

    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      dst[i++] = *str;
    }
    else if (c == ' ') {
      dst[i++] = '+';
    }
    else {
      if (i + 3 >= dst_size) {
        /* There is not enough space for %XX. */
        dst[i] = '\0';
        return false;
      }
      sprintf(&dst[i], "%%%02X", c);
      i += 3;
    }
    ++str;
  }

  dst[i] = '\0';

  if (*str != '\0') {
    /* Output buffer was too small. */
    return false;
  }
  return true;
}
