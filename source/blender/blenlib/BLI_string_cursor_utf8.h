/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

bool BLI_str_cursor_step_next_utf8(const char *str, int str_maxlen, int *pos);
bool BLI_str_cursor_step_prev_utf8(const char *str, int str_maxlen, int *pos);

bool BLI_str_cursor_step_next_utf32(const char32_t *str, int str_maxlen, int *pos);
bool BLI_str_cursor_step_prev_utf32(const char32_t *str, int str_maxlen, int *pos);

void BLI_str_cursor_step_utf8(const char *str,
                              int str_maxlen,
                              int *pos,
                              eStrCursorJumpDirection direction,
                              eStrCursorJumpType jump,
                              bool use_init_step);

void BLI_str_cursor_step_utf32(const char32_t *str,
                               int str_maxlen,
                               int *pos,
                               eStrCursorJumpDirection direction,
                               eStrCursorJumpType jump,
                               bool use_init_step);

/**
 * Given a position within a string,
 * return the start and end of the closest sequence of delimited characters.
 * Typically a word, but can be a sequence of characters (including spaces).
 *
 * \note When used for word-selection the caller should set the cursor to `r_end` (by convention).
 *
 * \param str: The string with a cursor position
 * \param str_maxlen: The maximum characters to consider
 * \param pos: The starting cursor position.
 * \param r_start: returned start of word/sequence boundary (0-based)
 * \param r_end: returned end of word/sequence boundary (0-based)
 */
void BLI_str_cursor_step_bounds_utf8(
    const char *str, int str_maxlen, int pos, int *r_start, int *r_end);

/** A UTF32 version of #BLI_str_cursor_step_bounds_utf8 */
void BLI_str_cursor_step_bounds_utf32(
    const char32_t *str, int str_maxlen, int pos, int *r_start, int *r_end);

#ifdef __cplusplus
}
#endif
