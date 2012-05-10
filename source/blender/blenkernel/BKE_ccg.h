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
 * The Original Code is Copyright (C) 2012 by Nicholas Bishop.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_CCG_H__
#define __BKE_CCG_H__

/* defines BLI_INLINE */
#include "BLI_utildefines.h"

/* declares fprintf(), needed for BLI_assert */
#include <stdio.h>

struct CCGSubSurf;

/* Each CCGElem is CCGSubSurf's representation of a subdivided
   vertex. All CCGElems in a particular CCGSubSurf have the same
   layout, but the layout can vary from one CCGSubSurf to another. For
   this reason, CCGElem is presented as an opaque pointer, and
   elements should always be accompanied by a CCGKey, which provides
   the necessary offsets to access components of a CCGElem.
*/
typedef struct CCGElem CCGElem;

typedef struct CCGKey {
	int level;

	/* number of bytes in each element (one float per layer, plus
	   three floats for normals if enabled) */
	int elem_size;

	/* number of elements along each side of grid */
	int grid_size;
	/* number of elements in the grid (grid size squared) */
	int grid_area;
	/* number of bytes in each grid (grid_area * elem_size) */
	int grid_bytes;

	/* currently always the last three floats, unless normals are
	   disabled */
	int normal_offset;

	/* offset in bytes of mask value; only valid if 'has_mask' is
	   true */
	int mask_offset;

	int num_layers;
	int has_normals;
	int has_mask;
} CCGKey;

/* initialize 'key' at the specified level */
void CCG_key(CCGKey *key, const struct CCGSubSurf *ss, int level);
void CCG_key_top_level(CCGKey *key, const struct CCGSubSurf *ss);

/* get a pointer to the coordinate, normal, or mask components */
BLI_INLINE float *CCG_elem_co(const CCGKey *key, CCGElem *elem);
BLI_INLINE float *CCG_elem_no(const CCGKey *key, CCGElem *elem);
BLI_INLINE float *CCG_elem_mask(const CCGKey *key, CCGElem *elem);

/* get the element at 'offset' in an array */
BLI_INLINE CCGElem *CCG_elem_offset(const CCGKey *key, CCGElem *elem, int offset);

/* get the element at coordinate (x,y) in a face-grid array */
BLI_INLINE CCGElem *CCG_grid_elem(const CCGKey *key, CCGElem *elem, int x, int y);

/* combinations of above functions */
BLI_INLINE float *CCG_grid_elem_co(const CCGKey *key, CCGElem *elem, int x, int y);
BLI_INLINE float *CCG_grid_elem_no(const CCGKey *key, CCGElem *elem, int x, int y);
BLI_INLINE float *CCG_grid_elem_mask(const CCGKey *key, CCGElem *elem, int x, int y);
BLI_INLINE float *CCG_elem_offset_co(const CCGKey *key, CCGElem *elem, int offset);
BLI_INLINE float *CCG_elem_offset_no(const CCGKey *key, CCGElem *elem, int offset);
BLI_INLINE float *CCG_elem_offset_mask(const CCGKey *key, CCGElem *elem, int offset);

/* for iteration, get a pointer to the next element in an array */
BLI_INLINE CCGElem *CCG_elem_next(const CCGKey *key, CCGElem *elem);


/* inline definitions follow */

BLI_INLINE float *CCG_elem_co(const CCGKey *UNUSED(key), CCGElem *elem)
{
	return (float*)elem;
}

BLI_INLINE float *CCG_elem_no(const CCGKey *key, CCGElem *elem)
{
	BLI_assert(key->has_normals);
	return (float*)((char*)elem + key->normal_offset);
}

BLI_INLINE float *CCG_elem_mask(const CCGKey *key, CCGElem *elem)
{
	BLI_assert(key->has_mask);
	return (float*)((char*)elem + (key->mask_offset));
}

BLI_INLINE CCGElem *CCG_elem_offset(const CCGKey *key, CCGElem *elem, int offset)
{
	return (CCGElem*)(((char*)elem) + key->elem_size * offset);
}

BLI_INLINE CCGElem *CCG_grid_elem(const CCGKey *key, CCGElem *elem, int x, int y)
{
	BLI_assert(x < key->grid_size && y < key->grid_size);
	return CCG_elem_offset(key, elem, (y * key->grid_size + x));
}

BLI_INLINE float *CCG_grid_elem_co(const CCGKey *key, CCGElem *elem, int x, int y)
{
	return CCG_elem_co(key, CCG_grid_elem(key, elem, x, y));
}

BLI_INLINE float *CCG_grid_elem_no(const CCGKey *key, CCGElem *elem, int x, int y)
{
	return CCG_elem_no(key, CCG_grid_elem(key, elem, x, y));
}

BLI_INLINE float *CCG_grid_elem_mask(const CCGKey *key, CCGElem *elem, int x, int y)
{
	return CCG_elem_mask(key, CCG_grid_elem(key, elem, x, y));
}

BLI_INLINE float *CCG_elem_offset_co(const CCGKey *key, CCGElem *elem, int offset)
{
	return CCG_elem_co(key, CCG_elem_offset(key, elem, offset));
}

BLI_INLINE float *CCG_elem_offset_no(const CCGKey *key, CCGElem *elem, int offset)
{
	return CCG_elem_no(key, CCG_elem_offset(key, elem, offset));
}

BLI_INLINE float *CCG_elem_offset_mask(const CCGKey *key, CCGElem *elem, int offset)
{
	return CCG_elem_mask(key, CCG_elem_offset(key, elem, offset));
}

BLI_INLINE CCGElem *CCG_elem_next(const CCGKey *key, CCGElem *elem)
{
	return CCG_elem_offset(key, elem, 1);
}

#endif
