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

BM_INLINE char _bm_elem_flag_test(const BMHeader *ele, const char hflag)
{
	return ele->hflag & hflag;
}

BM_INLINE void _bm_elem_flag_enable(BMHeader *ele, const char hflag)
{
	ele->hflag |= hflag;
}

BM_INLINE void _bm_elem_flag_disable(BMHeader *ele, const char hflag)
{
	ele->hflag &= ~hflag;
}

BM_INLINE void _bm_elem_flag_set(BMHeader *ele, const char hflag, const int val)
{
	if (val)  _bm_elem_flag_enable(ele,  hflag);
	else      _bm_elem_flag_disable(ele, hflag);
}

BM_INLINE void _bm_elem_flag_toggle(BMHeader *ele, const char hflag)
{
	ele->hflag ^= hflag;
}

BM_INLINE void _bm_elem_flag_merge(BMHeader *ele_a, BMHeader *ele_b)
{
	ele_a->hflag = ele_b->hflag = ele_a->hflag | ele_b->hflag;
}


BM_INLINE void _bm_elem_index_set(BMHeader *ele, const int index)
{
	ele->index = index;
}

BM_INLINE int _bm_elem_index_get(const BMHeader *ele)
{
	return ele->index;
}

#endif /* __BMESH_INLINE_C__ */
