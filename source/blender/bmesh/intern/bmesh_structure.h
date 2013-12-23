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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_STRUCTURE_H__
#define __BMESH_STRUCTURE_H__

/** \file blender/bmesh/intern/bmesh_structure.h
 *  \ingroup bmesh
 *
 * The lowest level of functionality for manipulating bmesh structures.
 * None of these functions should ever be exported to the rest of Blender.
 *
 * in the vast majority of cases there shouldn't be used directly.
 * if absolutely necessary, see function definitions in code for
 * descriptive comments.  but seriously, don't use this stuff.
 */

struct ListBase;

/* LOOP CYCLE MANAGEMENT */
bool    bmesh_loop_validate(BMFace *f);

/* DISK CYCLE MANAGMENT */
void    bmesh_disk_edge_append(BMEdge *e, BMVert *v);
void    bmesh_disk_edge_remove(BMEdge *e, BMVert *v);
BLI_INLINE BMEdge *bmesh_disk_edge_next(const BMEdge *e, const BMVert *v);
BLI_INLINE BMEdge *bmesh_disk_edge_prev(const BMEdge *e, const BMVert *v);
int     bmesh_disk_facevert_count(const BMVert *v);
BMEdge *bmesh_disk_faceedge_find_first(const BMEdge *e, const BMVert *v);
BMEdge *bmesh_disk_faceedge_find_next(const BMEdge *e, const BMVert *v);

/* RADIAL CYCLE MANAGMENT */
void    bmesh_radial_append(BMEdge *e, BMLoop *l);
void    bmesh_radial_loop_remove(BMLoop *l, BMEdge *e);
/* note:
 *      bmesh_radial_loop_next(BMLoop *l) / prev.
 * just use member access l->radial_next, l->radial_prev now */

int     bmesh_radial_facevert_count(const BMLoop *l, const BMVert *v);
BMLoop *bmesh_radial_faceloop_find_first(const BMLoop *l, const BMVert *v);
BMLoop *bmesh_radial_faceloop_find_next(const BMLoop *l, const BMVert *v);
BMLoop *bmesh_radial_faceloop_find_vert(const BMFace *f, const BMVert *v);
bool    bmesh_radial_validate(int radlen, BMLoop *l);

/* EDGE UTILITIES */
bool    bmesh_edge_swapverts(BMEdge *e, BMVert *orig, BMVert *newv);  /* relink edge */
BMEdge *bmesh_disk_edge_exists(const BMVert *v1, const BMVert *v2);
bool    bmesh_disk_validate(int len, BMEdge *e, BMVert *v);

#include "intern/bmesh_structure_inline.h"

#endif /* __BMESH_STRUCTURE_H__ */
