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
 * in the vast majority of cases thes should not be used directly.
 * if absolutely necassary, see function defitions in code for
 * descriptive comments.  but seriously, don't use this stuff.
 */

struct ListBase;

/* LOOP CYCLE MANAGEMENT */
int bmesh_loop_validate(BMFace *f);

/* DISK CYCLE MANAGMENT */
int     bmesh_disk_edge_append(BMEdge *e, BMVert *v);
void    bmesh_disk_edge_remove(BMEdge *e, BMVert *v);
BMEdge *bmesh_disk_edge_next(BMEdge *e, BMVert *v);
BMEdge *bmesh_disk_edge_prev(BMEdge *e, BMVert *v);
int     bmesh_disk_facevert_count(BMVert *v);
BMEdge *bmesh_disk_faceedge_find_first(BMEdge *e, BMVert *v);
BMEdge *bmesh_disk_faceedge_find_next(BMEdge *e, BMVert *v);

/* RADIAL CYCLE MANAGMENT */
void    bmesh_radial_append(BMEdge *e, BMLoop *l);
void    bmesh_radial_loop_remove(BMLoop *l, BMEdge *e);
BMLoop *bmesh_radial_loop_next(BMLoop *l);
int     bmesh_radial_face_find(BMEdge *e, BMFace *f);
int     bmesh_radial_facevert_count(BMLoop *l, BMVert *v);
BMLoop *bmesh_radial_faceloop_find_first(BMLoop *l, BMVert *v);
BMLoop *bmesh_radial_faceloop_find_next(BMLoop *l, BMVert *v);
int     bmesh_radial_validate(int radlen, BMLoop *l);

/* EDGE UTILITIES */
int     bmesh_vert_in_edge(BMEdge *e, BMVert *v);
int     bmesh_verts_in_edge(BMVert *v1, BMVert *v2, BMEdge *e);
int     bmesh_edge_swapverts(BMEdge *e, BMVert *orig, BMVert *newv); /*relink edge*/
BMVert *bmesh_edge_other_vert_get(BMEdge *e, BMVert *v);
BMEdge *bmesh_disk_edge_exists(BMVert *v1, BMVert *v2);
int     bmesh_disk_validate(int len, BMEdge *e, BMVert *v);

#endif /* __BMESH_STRUCTURE_H__ */
