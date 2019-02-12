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

#include "BLI_sys_types.h"

#include "dna_utils.h"

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
