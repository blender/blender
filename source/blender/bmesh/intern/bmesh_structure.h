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

/* DOUBLE CIRCULAR LINKED LIST FUNCTIONS */
int bmesh_cycle_length(void *h);

/* LOOP CYCLE MANAGEMENT */
int bmesh_loop_validate(BMFace *f);

/* DISK CYCLE MANAGMENT */
int bmesh_disk_append_edge(struct BMEdge *e, struct BMVert *v);
void bmesh_disk_remove_edge(struct BMEdge *e, struct BMVert *v);
struct BMEdge *bmesh_disk_nextedge(struct BMEdge *e, struct BMVert *v);
struct BMNode *bmesh_disk_getpointer(struct BMEdge *e, struct BMVert *v);
int bmesh_disk_count_facevert(struct BMVert *v);
struct BMEdge *bmesh_disk_find_first_faceedge(struct BMEdge *e, struct BMVert *v);
struct BMEdge *bmesh_disk_find_next_faceedge(struct BMEdge *e, struct BMVert *v);

/* RADIAL CYCLE MANAGMENT */
void bmesh_radial_append(struct BMEdge *e, struct BMLoop *l);
void bmesh_radial_remove_loop(struct BMLoop *l, struct BMEdge *e);
int bmesh_radial_find_face(struct BMEdge *e, struct BMFace *f);
struct BMLoop *bmesh_radial_nextloop(struct BMLoop *l);
int bmesh_radial_count_facevert(struct BMLoop *l, struct BMVert *v);
struct BMLoop *bmesh_radial_find_first_faceloop(struct BMLoop *l, struct BMVert *v);
struct BMLoop *bmesh_radial_find_next_faceloop(struct BMLoop *l, struct BMVert *v);
int bmesh_radial_validate(int radlen, struct BMLoop *l);

/* EDGE UTILITIES */
int bmesh_vert_in_edge(struct BMEdge *e, struct BMVert *v);
int bmesh_verts_in_edge(struct BMVert *v1, struct BMVert *v2, struct BMEdge *e);
int bmesh_edge_swapverts(struct BMEdge *e, struct BMVert *orig, struct BMVert *newv); /*relink edge*/
struct BMVert *bmesh_edge_getothervert(struct BMEdge *e, struct BMVert *v);
struct BMEdge *bmesh_disk_existedge(BMVert *v1, BMVert *v2);
struct BMEdge *bmesh_disk_next_edgeflag(struct BMEdge *e, struct BMVert *v, int eflag, int tflag);
int bmesh_disk_validate(int len, struct BMEdge *e, struct BMVert *v);

/*EULER API - For modifying structure*/
struct BMVert *bmesh_mv(struct BMesh *bm, float *vec);
struct BMEdge *bmesh_me(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2);
struct BMFace *bmesh_mf(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMEdge **elist, int len);
int bmesh_kv(struct BMesh *bm, struct BMVert *v);
int bmesh_ke(struct BMesh *bm, struct BMEdge *e);
int bmesh_kf(struct BMesh *bm, struct BMFace *bply);
struct BMVert *bmesh_semv(struct BMesh *bm, struct BMVert *tv, struct BMEdge *e, struct BMEdge **re);
struct BMFace *bmesh_sfme(struct BMesh *bm, struct BMFace *f, struct BMVert *v1,
                          struct BMVert *v2, struct BMLoop **rl,
#ifdef USE_BMESH_HOLES
                          ListBase *holes,
#endif
                          BMEdge *example
                          );
int bmesh_jekv(struct BMesh *bm, struct BMEdge *ke, struct BMVert *kv);
int bmesh_loop_reverse(struct BMesh *bm, struct BMFace *f);
struct BMFace *bmesh_jfke(struct BMesh *bm, struct BMFace *f1, struct BMFace *f2, struct BMEdge *e);

struct BMVert *bmesh_urmv(struct BMesh *bm, struct BMFace *sf, struct BMVert *sv);
//int *bmesh_grkv(struct BMesh *bm, struct BMFace *sf, struct BMVert *kv);

#endif /* __BMESH_STRUCTURE_H__ */
