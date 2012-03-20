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

BLI_INLINE char _bm_elem_flag_test(const BMHeader *head, const char hflag)
{
	return head->hflag & hflag;
}

BLI_INLINE void _bm_elem_flag_enable(BMHeader *head, const char hflag)
{
	head->hflag |= hflag;
}

BLI_INLINE void _bm_elem_flag_disable(BMHeader *head, const char hflag)
{
	head->hflag &= ~hflag;
}

BLI_INLINE void _bm_elem_flag_set(BMHeader *head, const char hflag, const int val)
{
	if (val)  _bm_elem_flag_enable(head,  hflag);
	else      _bm_elem_flag_disable(head, hflag);
}

BLI_INLINE void _bm_elem_flag_toggle(BMHeader *head, const char hflag)
{
	head->hflag ^= hflag;
}

BLI_INLINE void _bm_elem_flag_merge(BMHeader *head_a, BMHeader *head_b)
{
	head_a->hflag = head_b->hflag = head_a->hflag | head_b->hflag;
}


BLI_INLINE void _bm_elem_index_set(BMHeader *head, const int index)
{
	head->index = index;
}

BLI_INLINE int _bm_elem_index_get(const BMHeader *head)
{
	return head->index;
}

#endif /* __BMESH_INLINE_C__ */
