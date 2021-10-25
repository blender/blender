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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/sort_utils.c
 *  \ingroup bli
 *
 * Utility functions for sorting common types.
 */

#include "BLI_sort_utils.h"  /* own include */

struct SortAnyByFloat {
	float sort_value;
};

struct SortAnyByInt {
	int sort_value;
};

int BLI_sortutil_cmp_float(const void *a_, const void *b_)
{
	const struct SortAnyByFloat *a = a_;
	const struct SortAnyByFloat *b = b_;
	if      (a->sort_value > b->sort_value) return  1;
	else if (a->sort_value < b->sort_value) return -1;
	else                                    return  0;
}

int BLI_sortutil_cmp_float_reverse(const void *a_, const void *b_)
{
	const struct SortAnyByFloat *a = a_;
	const struct SortAnyByFloat *b = b_;
	if      (a->sort_value < b->sort_value) return  1;
	else if (a->sort_value > b->sort_value) return -1;
	else                                    return  0;
}

int BLI_sortutil_cmp_int(const void *a_, const void *b_)
{
	const struct SortAnyByInt *a = a_;
	const struct SortAnyByInt *b = b_;
	if      (a->sort_value > b->sort_value) return  1;
	else if (a->sort_value < b->sort_value) return -1;
	else                                    return  0;
}

int BLI_sortutil_cmp_int_reverse(const void *a_, const void *b_)
{
	const struct SortAnyByInt *a = a_;
	const struct SortAnyByInt *b = b_;
	if      (a->sort_value < b->sort_value) return  1;
	else if (a->sort_value > b->sort_value) return -1;
	else                                    return  0;
}
