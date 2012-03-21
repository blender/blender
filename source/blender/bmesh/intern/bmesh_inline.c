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

/* stuff for dealing with header flags */
#define BM_elem_flag_test(   ele, hflag)      _bm_elem_flag_test    (&(ele)->head, hflag)
#define BM_elem_flag_enable( ele, hflag)      _bm_elem_flag_enable  (&(ele)->head, hflag)
#define BM_elem_flag_disable(ele, hflag)      _bm_elem_flag_disable (&(ele)->head, hflag)
#define BM_elem_flag_set(    ele, hflag, val) _bm_elem_flag_set     (&(ele)->head, hflag, val)
#define BM_elem_flag_toggle( ele, hflag)      _bm_elem_flag_toggle  (&(ele)->head, hflag)
#define BM_elem_flag_merge(  ele_a, ele_b)    _bm_elem_flag_merge   (&(ele_a)->head, &(ele_b)->head)

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


/* notes on BM_elem_index_set(...) usage,
 * Set index is sometimes abused as temp storage, other times we cant be
 * sure if the index values are valid because certain operations have modified
 * the mesh structure.
 *
 * To set the elements to valid indices 'BM_mesh_elem_index_ensure' should be used
 * rather then adding inline loops, however there are cases where we still
 * set the index directly
 *
 * In an attempt to manage this, here are 3 tags Im adding to uses of
 * 'BM_elem_index_set'
 *
 * - 'set_inline'  -- since the data is already being looped over set to a
 *                    valid value inline.
 *
 * - 'set_dirty!'  -- intentionally sets the index to an invalid value,
 *                    flagging 'bm->elem_index_dirty' so we don't use it.
 *
 * - 'set_ok'      -- this is valid use since the part of the code is low level.
 *
 * - 'set_ok_invalid'  -- set to -1 on purpose since this should not be
 *                    used without a full array re-index, do this on
 *                    adding new vert/edge/faces since they may be added at
 *                    the end of the array.
 *
 * - 'set_loop'    -- currently loop index values are not used used much so
 *                    assume each case they are dirty.
 * - campbell */

#define BM_elem_index_get(ele)           _bm_elem_index_get(&(ele)->head)
#define BM_elem_index_set(ele, index)    _bm_elem_index_set(&(ele)->head, index)

BLI_INLINE void _bm_elem_index_set(BMHeader *head, const int index)
{
	head->index = index;
}

BLI_INLINE int _bm_elem_index_get(const BMHeader *head)
{
	return head->index;
}

#endif /* __BMESH_INLINE_C__ */
