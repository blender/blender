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
 * Copyright (C) 2018 Blender Foundation.
 */

/** \file \ingroup DNA
 *
 * Utilities for stand-alone makesdna.c and Blender to share.
 */

#include <string.h>

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
#include "BLI_assert.h"

#include "BLI_memarena.h"

#include "dna_utils.h"

/* -------------------------------------------------------------------- */
/** \name Struct Member Evaluation
 * \{ */

/**
 * Parses the `[n1][n2]...` on the end of an array name
 * and returns the number of array elements `n1 * n2 ...`.
 */
int DNA_elem_array_size(const char *str)
{
	int result = 1;
	int current = 0;
	while (true) {
		char c = *str++;
		switch (c) {
			case '\0':
				return result;
			case '[':
				current = 0;
				break;
			case ']':
				result *= current;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				current = current * 10 + (c - '0');
				break;
			default:
				break;
		}
	}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Struct Member Manipulation
 * \{ */

static bool is_identifier(const char c)
{
	return ((c >= 'a' && c <= 'z') ||
	        (c >= 'A' && c <= 'Z') ||
	        (c >= '0' && c <= '9') ||
	        (c == '_'));
}

uint DNA_elem_id_offset_start(const char *elem_full)
{
	uint elem_full_offset = 0;
	while (!is_identifier(elem_full[elem_full_offset])) {
		elem_full_offset++;
	}
	return elem_full_offset;
}

uint DNA_elem_id_offset_end(const char *elem_full)
{
	uint elem_full_offset = 0;
	while (is_identifier(elem_full[elem_full_offset])) {
		elem_full_offset++;
	}
	return elem_full_offset;
}

/**
 * \a elem_dst must be at least the size of \a elem_src.
 */
void DNA_elem_id_strip(char *elem_dst, const char *elem_src)
{
	const uint elem_src_offset = DNA_elem_id_offset_start(elem_src);
	const char *elem_src_trim = elem_src + elem_src_offset;
	const uint elem_src_trim_len = DNA_elem_id_offset_end(elem_src_trim);
	memcpy(elem_dst, elem_src_trim, elem_src_trim_len);
	elem_dst[elem_src_trim_len] = '\0';
}

/**
 * Check if 'var' matches '*var[3]' for eg,
 * return true if it does, with start/end offsets.
 */
bool DNA_elem_id_match(
        const char *elem_search, const int elem_search_len,
        const char *elem_full,
        uint *r_elem_full_offset)
{
	BLI_assert(strlen(elem_search) == elem_search_len);
	const uint elem_full_offset = DNA_elem_id_offset_start(elem_full);
	const char *elem_full_trim = elem_full + elem_full_offset;
	if (strncmp(elem_search, elem_full_trim, elem_search_len) == 0) {
		const char c = elem_full_trim[elem_search_len];
		if (c == '\0' || !is_identifier(c)) {
			*r_elem_full_offset = elem_full_offset;
			return true;
		}
	}
	return false;
}

/**
 * Return a renamed dna name, allocated from \a mem_arena.
 */
char *DNA_elem_id_rename(
        struct MemArena *mem_arena,
        const char *elem_src, const int elem_src_len,
        const char *elem_dst, const int elem_dst_len,
        const char *elem_full_src, const int elem_full_src_len,
        const uint elem_full_offset_start)
{
	BLI_assert(strlen(elem_src) == elem_src_len);
	BLI_assert(strlen(elem_dst) == elem_dst_len);
	BLI_assert(strlen(elem_full_src) == elem_full_src_len);
	BLI_assert(DNA_elem_id_offset_start(elem_full_src) == elem_full_offset_start);
	UNUSED_VARS_NDEBUG(elem_src);

	const int elem_final_len = (elem_full_src_len - elem_src_len) + elem_dst_len;
	char *elem_full_dst = BLI_memarena_alloc(mem_arena, elem_final_len + 1);
	uint i = 0;
	if (elem_full_offset_start != 0) {
		memcpy(elem_full_dst, elem_full_src, elem_full_offset_start);
		i = elem_full_offset_start;
	}
	memcpy(&elem_full_dst[i], elem_dst, elem_dst_len + 1);
	i += elem_dst_len;
	uint elem_full_offset_end = elem_full_offset_start + elem_src_len;
	if (elem_full_src[elem_full_offset_end] != '\0') {
		const int elem_full_tail_len = (elem_full_src_len - elem_full_offset_end);
		memcpy(&elem_full_dst[i], &elem_full_src[elem_full_offset_end], elem_full_tail_len + 1);
		i += elem_full_tail_len;
	}
	BLI_assert((strlen(elem_full_dst) == elem_final_len) && (i == elem_final_len));
	return elem_full_dst;
}

/** \} */
