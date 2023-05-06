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

bool BLI_str_cursor_step_next_utf8(const char *str, size_t maxlen, int *pos);
bool BLI_str_cursor_step_prev_utf8(const char *str, size_t maxlen, int *pos);

bool BLI_str_cursor_step_next_utf32(const char32_t *str, size_t maxlen, int *pos);
bool BLI_str_cursor_step_prev_utf32(const char32_t *str, size_t maxlen, int *pos);

void BLI_str_cursor_step_utf8(const char *str,
                              size_t maxlen,
                              int *pos,
                              eStrCursorJumpDirection direction,
                              eStrCursorJumpType jump,
                              bool use_init_step);

void BLI_str_cursor_step_utf32(const char32_t *str,
                               size_t maxlen,
                               int *pos,
                               eStrCursorJumpDirection direction,
                               eStrCursorJumpType jump,
                               bool use_init_step);

#ifdef __cplusplus
}
#endif
