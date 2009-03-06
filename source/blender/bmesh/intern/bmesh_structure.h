/**
 * bmesh_structure.h    jan 2007
 *
 * The lowest level of functionality for manipulating bmesh structures.
 * None of these functions should ever be exported to the rest of Blender.
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.	
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef BM_STRUCTURE_H
#define BM_STRUCTURE_H

/*ALLOCATION/DEALLOCATION*/
struct BMVert *bmesh_addvertlist(struct BMesh *bm, struct BMVert *example);
struct BMEdge *bmesh_addedgelist(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMEdge *example);
struct BMFace *bmesh_addpolylist(struct BMesh *bm, struct BMFace *example); 
struct BMLoop *bmesh_create_loop(struct BMesh *bm, struct BMVert *v, struct BMEdge *e, struct BMFace *f, struct BMLoop *example);

void bmesh_free_vert(struct BMesh *bm, struct BMVert *v);
void bmesh_free_edge(struct BMesh *bm, struct BMEdge *e);
void bmesh_free_poly(struct BMesh *bm, struct BMFace *f);
void bmesh_free_loop(struct BMesh *bm, struct BMLoop *l);

/*DOUBLE CIRCULAR LINKED LIST FUNCTIONS*/
void bmesh_cycle_append(void *h, void *nt);
int bmesh_cycle_remove(void *h, void *remn);
int bmesh_cycle_validate(int len, void *h);
int bmesh_cycle_length(void *h);

/*DISK CYCLE MANAGMENT*/
int bmesh_disk_append_edge(struct BMEdge *e, struct BMVert *v);
void bmesh_disk_remove_edge(struct BMEdge *e, struct BMVert *v);
struct BMEdge *bmesh_disk_nextedge(struct BMEdge *e, struct BMVert *v);
struct BMNode *bmesh_disk_getpointer(struct BMEdge *e, struct BMVert *v);
int bmesh_disk_count_facevert(struct BMVert *v);
struct BMEdge *bmesh_disk_find_first_faceedge(struct BMEdge *e, struct BMVert *v);
struct BMEdge *bmesh_disk_find_next_faceedge(struct BMEdge *e, struct BMVert *v);

/*RADIAL CYCLE MANAGMENT*/
void bmesh_radial_append(struct BMEdge *e, struct BMLoop *l);
void bmesh_radial_remove_loop(struct BMLoop *l, struct BMEdge *e);
int bmesh_radial_find_face(struct BMEdge *e, struct BMFace *f);
struct BMLoop *bmesh_radial_nextloop(struct BMLoop *l);
int bmesh_radial_count_facevert(struct BMLoop *l, struct BMVert *v);
struct BMLoop *bmesh_radial_find_first_facevert(struct BMLoop *l, struct BMVert *v);
struct BMLoop *bmesh_radial_find_next_facevert(struct BMLoop *l, struct BMVert *v);

/*EDGE UTILITIES*/
int bmesh_vert_in_edge(struct BMEdge *e, struct BMVert *v);
int bmesh_verts_in_edge(struct BMVert *v1, struct BMVert *v2, struct BMEdge *e);
int bmesh_edge_swapverts(struct BMEdge *e, struct BMVert *orig, struct BMVert *new); /*relink edge*/
struct BMVert *bmesh_edge_getothervert(struct BMEdge *e, struct BMVert *v);
int bmesh_disk_hasedge(struct BMVert *v, struct BMEdge *e);
struct BMEdge *bmesh_disk_existedge(BMVert *v1, BMVert *v2);
struct BMEdge *bmesh_disk_next_edgeflag(struct BMEdge *e, struct BMVert *v, int eflag, int tflag);
int bmesh_disk_count_edgeflag(struct BMVert *v, int eflag, int tflag);

/*EULER API - For modifying structure*/
struct BMVert *bmesh_mv(struct BMesh *bm, float *vec);
struct BMEdge *bmesh_me(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2);
struct BMFace *bmesh_mf(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMEdge **elist, int len);
int bmesh_kv(struct BMesh *bm, struct BMVert *v);
int bmesh_ke(struct BMesh *bm, struct BMEdge *e);
int bmesh_kf(struct BMesh *bm, struct BMFace *bply);
struct BMVert *bmesh_semv(struct BMesh *bm, struct BMVert *tv, struct BMEdge *e, struct BMEdge **re);
struct BMFace *bmesh_sfme(struct BMesh *bm, struct BMFace *f, struct BMVert *v1, struct BMVert *v2, struct BMLoop **rl);
int bmesh_jekv(struct BMesh *bm, struct BMEdge *ke, struct BMVert *kv);
int bmesh_loop_reverse(struct BMesh *bm, struct BMFace *f);
struct BMFace *bmesh_jfke(struct BMesh *bm, struct BMFace *f1, BMFace *f2, BMEdge *e);

struct BMVert *bmesh_urmv(struct BMesh *bm, struct BMFace *sf, struct BMVert *sv);
//int *bmesh_grkv(struct BMesh *bm, struct BMFace *sf, struct BMVert *kv);

#endif
