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
 * Contributor(s): Geoffrey Bantle, Levi Schooley.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_INTERP_H__
#define __BMESH_INTERP_H__

/** \file blender/bmesh/intern/bmesh_interp.h
 *  \ingroup bmesh
 */

#ifdef __cplusplus
extern "C" {
#endif

/* project the multires grid in target onto source's set of multires grids */
void  BM_loop_interp_multires(BMesh *bm, BMLoop *target, BMFace *source);
void  BM_vert_interp_from_face(BMesh *bm, BMVert *v, BMFace *source);

void  BM_data_interp_from_verts(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, const float fac);
void  BM_data_interp_face_vert_edge(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, BMEdge *e1, const float fac);
void  BM_data_layer_add(BMesh *em, CustomData *data, int type);
void  BM_data_layer_add_named(BMesh *bm, CustomData *data, int type, const char *name);
void  BM_data_layer_free(BMesh *em, CustomData *data, int type);
void  BM_data_layer_free_n(BMesh *bm, CustomData *data, int type, int n);
float BM_elem_float_data_get(CustomData *cd, void *element, int type);
void  BM_elem_float_data_set(CustomData *cd, void *element, int type, const float val);


/**
 * projects target onto source for customdata interpolation.
 * \note only does loop customdata.  multires is handled.  */
void BM_face_interp_from_face(BMesh *bm, BMFace *target, BMFace *source);

/* projects a single loop, target, onto source for customdata interpolation. multires is handled.
 * if do_vertex is true, target's vert data will also get interpolated.*/
void BM_loop_interp_from_face(BMesh *bm, BMLoop *target, BMFace *source,
                              int do_vertex, int do_multires);

/* smoothes boundaries between multires grids, including some borders in adjacent faces */
void BM_face_multires_bounds_smooth(BMesh *bm, BMFace *f);

/* convert MLoop*** in a bmface to mtface and mcol in
 * an MFace*/
void BM_loops_to_corners(BMesh *bm, struct Mesh *me, int findex,
                         BMFace *f, int numTex, int numCol);

#ifdef __cplusplus
}
#endif

#endif /* __BMESH_INTERP_H__ */
