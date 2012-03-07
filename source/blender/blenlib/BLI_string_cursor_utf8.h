/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_STRING_CURSOR_UTF8_H__
#define __BLI_STRING_CURSOR_UTF8_H__

/** \file BLI_string_utf8.h
 *  \ingroup bli
 */

typedef enum strCursorDelimType {
	STRCUR_DELIM_NONE,
	STRCUR_DELIM_ALPHA,
	STRCUR_DELIM_PUNCT,
	STRCUR_DELIM_BRACE,
	STRCUR_DELIM_OPERATOR,
	STRCUR_DELIM_QUOTE,
	STRCUR_DELIM_WHITESPACE,
	STRCUR_DELIM_OTHER
} strCursorDelimType;

typedef enum strCursorJumpType {
	STRCUR_JUMP_NONE,
	STRCUR_JUMP_DELIM,
	STRCUR_JUMP_ALL
} strCursorJumpType;

typedef enum strCursorJumpDirection {
	STRCUR_DIR_PREV,
	STRCUR_DIR_NEXT
} strCursorJumpDirection;

int BLI_str_cursor_step_next_utf8(const char *str, size_t maxlen, short *pos);
int BLI_str_cursor_step_prev_utf8(const char *str, size_t maxlen, short *pos);

void BLI_str_cursor_step_utf8(const char *str, size_t maxlen,
                              short *pos, strCursorJumpDirection direction,
                              strCursorJumpType jump);

#endif /* __BLI_STRING_CURSOR_UTF8_H__ */
