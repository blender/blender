/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eStrCursorJumpType {
  STRCUR_JUMP_NONE,
  STRCUR_JUMP_DELIM,
  STRCUR_JUMP_ALL,
} eStrCursorJumpType;

typedef enum eStrCursorJumpDirection {
  STRCUR_DIR_PREV,
  STRCUR_DIR_NEXT,
} eStrCursorJumpDirection;

bool BLI_str_cursor_step_next_utf8(const char *str, size_t str_maxlen, int *pos);
bool BLI_str_cursor_step_prev_utf8(const char *str, size_t str_maxlen, int *pos);

bool BLI_str_cursor_step_next_utf32(const char32_t *str, size_t str_maxlen, int *pos);
bool BLI_str_cursor_step_prev_utf32(const char32_t *str, size_t str_maxlen, int *pos);

void BLI_str_cursor_step_utf8(const char *str,
                              size_t str_maxlen,
                              int *pos,
                              eStrCursorJumpDirection direction,
                              eStrCursorJumpType jump,
                              bool use_init_step);

void BLI_str_cursor_step_utf32(const char32_t *str,
                               size_t str_maxlen,
                               int *pos,
                               eStrCursorJumpDirection direction,
                               eStrCursorJumpType jump,
                               bool use_init_step);

/**
 * Word/Sequence Selection. Given a position within a string, return the start and end of the
 * closest sequence of delimited characters. Generally a word, but could be a sequence of spaces.
 *
 * \param str: The string with a cursor position
 * \param str_maxlen: The maximum characters to consider
 * \param pos: The starting cursor position (probably moved on completion)
 * \param r_start: returned start of word/sequence boundary (0-based)
 * \param r_end: returned end of word/sequence boundary (0-based)
 */
void BLI_str_cursor_step_bounds_utf8(
    const char *str, const size_t str_maxlen, int *pos, int *r_start, int *r_end);

/** A UTF32 version of #BLI_str_cursor_step_bounds_utf8 */
void BLI_str_cursor_step_bounds_utf32(
    const char32_t *str, const size_t str_maxlen, int *pos, int *r_start, int *r_end);

#ifdef __cplusplus
}
#endif
