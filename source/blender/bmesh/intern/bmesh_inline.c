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
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_inline.c
 *  \ingroup bmesh
 *
 * BM Inline functions.
 */

#ifndef __BMESH_INLINE_C__
#define __BMESH_INLINE_C__

#include "bmesh.h"

BM_INLINE char BM_elem_flag_test(const void *element, const char hflag)
{
	return ((const BMHeader *)element)->hflag & hflag;
}

BM_INLINE void BM_elem_flag_enable(void *element, const char hflag)
{
	((BMHeader *)element)->hflag |= hflag;
}

BM_INLINE void BM_elem_flag_disable(void *element, const char hflag)
{
	((BMHeader *)element)->hflag &= ~hflag;
}

BM_INLINE void BM_elem_flag_set(void *element, const char hflag, const int val)
{
	if (val)  BM_elem_flag_enable(element,  hflag);
	else      BM_elem_flag_disable(element, hflag);
}

BM_INLINE void BM_elem_flag_toggle(void *element, const char hflag)
{
	((BMHeader *)element)->hflag ^= hflag;
}

BM_INLINE void BM_elem_flag_merge(void *element_a, void *element_b)
{
	((BMHeader *)element_a)->hflag =
	((BMHeader *)element_b)->hflag = (((BMHeader *)element_a)->hflag |
	                                  ((BMHeader *)element_b)->hflag);
}

BM_INLINE void BM_elem_index_set(void *element, const int index)
{
	((BMHeader *)element)->index = index;
}

BM_INLINE int BM_elem_index_get(const void *element)
{
	return ((BMHeader *)element)->index;
}

#endif /* __BMESH_INLINE_C__ */
