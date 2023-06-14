/* SPDX-FileCopyrightText: 2010 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "GHOST_PathUtils.hh"
#include "GHOST_Types.h"

/* Based on: https://stackoverflow.com/a/2766963/432509 */

using DecodeState_e = enum DecodeState_e {
  /** Searching for an ampersand to convert. */
  STATE_SEARCH = 0,
  /** Convert the two proceeding characters from hex. */
  STATE_CONVERTING
};

void GHOST_URL_decode(char *buf_dst, int buf_dst_size, const char *buf_src)
{
  const uint buf_src_len = strlen(buf_src);
  DecodeState_e state = STATE_SEARCH;
  uint ascii_character;
  char temp_num_buf[3] = {0};

  memset(buf_dst, 0, buf_dst_size);

  for (uint i = 0; i < buf_src_len; i++) {
    switch (state) {
      case STATE_SEARCH: {
        if (buf_src[i] != '%') {
          strncat(buf_dst, &buf_src[i], 1);
          assert(int(strlen(buf_dst)) < buf_dst_size);
          break;
        }

        /* We are now converting. */
        state = STATE_CONVERTING;
        break;
      }
      case STATE_CONVERTING: {
        bool both_digits = true;

        /* Create a buffer to hold the hex. For example, if `%20`,
         * this buffer would hold 20 (in ASCII). */
        memset(temp_num_buf, 0, sizeof(temp_num_buf));

        /* Conversion complete (i.e. don't convert again next iteration). */
        state = STATE_SEARCH;

        strncpy(temp_num_buf, &buf_src[i], 2);

        /* Ensure both characters are hexadecimal. */
        for (int j = 0; j < 2; j++) {
          if (!isxdigit(temp_num_buf[j])) {
            both_digits = false;
          }
        }

        if (!both_digits) {
          break;
        }
        /* Convert two hexadecimal characters into one character. */
        sscanf(temp_num_buf, "%x", &ascii_character);

        /* Ensure we aren't going to overflow. */
        assert(int(strlen(buf_dst)) < buf_dst_size);

        /* Concatenate this character onto the output. */
        strncat(buf_dst, (char *)&ascii_character, 1);

        /* Skip the next character. */
        i++;
        break;
      }
    }
  }
}

char *GHOST_URL_decode_alloc(const char *buf_src)
{
  /* Assume one character of encoded URL can be expanded to 4 chars max. */
  const size_t decoded_size_max = 4 * strlen(buf_src) + 1;
  char *buf_dst = (char *)malloc(decoded_size_max);
  GHOST_URL_decode(buf_dst, decoded_size_max, buf_src);
  const size_t decoded_size = strlen(buf_dst) + 1;
  if (decoded_size != decoded_size_max) {
    char *buf_dst_trim = (char *)malloc(decoded_size);
    memcpy(buf_dst_trim, buf_dst, decoded_size);
    free(buf_dst);
    buf_dst = buf_dst_trim;
  }
  return buf_dst;
}
