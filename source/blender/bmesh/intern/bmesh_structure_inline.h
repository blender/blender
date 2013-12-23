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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_structure_inline.h
 *  \ingroup bmesh
 *
 * BMesh inline operator functions.
 */

#ifndef __BMESH_STRUCTURE_INLINE_H__
#define __BMESH_STRUCTURE_INLINE_H__

/**
 * \brief Next Disk Edge
 *
 * Find the next edge in a disk cycle
 *
 * \return Pointer to the next edge in the disk cycle for the vertex v.
 */
BLI_INLINE BMEdge *bmesh_disk_edge_next(const BMEdge *e, const BMVert *v)
{
	if (v == e->v1)
		return e->v1_disk_link.next;
	if (v == e->v2)
		return e->v2_disk_link.next;
	return NULL;
}

BLI_INLINE BMEdge *bmesh_disk_edge_prev(const BMEdge *e, const BMVert *v)
{
	if (v == e->v1)
		return e->v1_disk_link.prev;
	if (v == e->v2)
		return e->v2_disk_link.prev;
	return NULL;
}

#endif /* __BMESH_STRUCTURE_INLINE_H__ */
