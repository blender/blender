/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

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
