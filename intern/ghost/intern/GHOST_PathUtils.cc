/* SPDX-FileCopyrightText: 2010 Blender Authors
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

#include "GHOST_Debug.hh"
#include "GHOST_PathUtils.hh"
#include "GHOST_Types.h"

/* Based on: https://stackoverflow.com/a/2766963/432509 */

using DecodeState_e = enum DecodeState_e {
  /** Searching for an ampersand to convert. */
  STATE_SEARCH = 0,
  /** Convert the two proceeding characters from hex. */
  STATE_CONVERTING
};

void GHOST_URL_decode(char *buf_dst, int buf_dst_size, const char *buf_src, const int buf_src_len)
{
  GHOST_ASSERT(strnlen(buf_src, buf_src_len) == buf_src_len, "Incorrect length");

  DecodeState_e state = STATE_SEARCH;
  uint ascii_character;

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
        /* Create a buffer to hold the hex. For example, if `%20`,
         * this buffer would hold 20 (in ASCII). */
        char temp_num_buf[3];

        /* Conversion complete (i.e. don't convert again next iteration). */
        state = STATE_SEARCH;

        /* Ensure both characters are hexadecimal. */
        bool both_digits = true;
        for (int j = 0; j < 2; j++) {
          /* `isxdigit` serves to early null terminate the string too. */
          const char hex_char = buf_src[i + j];
          if (!isxdigit(hex_char)) {
            both_digits = false;
            break;
          }
          temp_num_buf[j] = hex_char;
        }
        if (!both_digits) {
          break;
        }
        /* Convert two hexadecimal characters into one character. */
        temp_num_buf[2] = '\0';
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

char *GHOST_URL_decode_alloc(const char *buf_src, const int buf_src_len)
{
  /* Assume one character of encoded URL can be expanded to 4 chars max. */
  const size_t decoded_size_max = 4 * buf_src_len + 1;
  char *buf_dst = (char *)malloc(decoded_size_max);
  GHOST_URL_decode(buf_dst, decoded_size_max, buf_src, buf_src_len);
  const size_t decoded_size = strlen(buf_dst) + 1;
  if (decoded_size != decoded_size_max) {
    char *buf_dst_trim = (char *)malloc(decoded_size);
    memcpy(buf_dst_trim, buf_dst, decoded_size);
    free(buf_dst);
    buf_dst = buf_dst_trim;
  }
  return buf_dst;
}
