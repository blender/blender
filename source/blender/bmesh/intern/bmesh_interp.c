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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_interp.c
 *  \ingroup bmesh
 *
 * Functions for interpolating data across the surface of a mesh.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_alloca.h"
#include "BLI_math.h"

#include "BKE_customdata.h"
#include "BKE_multires.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/* edge and vertex share, currently theres no need to have different logic */
static void bm_data_interp_from_elem(CustomData *data_layer, BMElem *ele1, BMElem *ele2, BMElem *ele_dst, const float fac)
{
	if (ele1->head.data && ele2->head.data) {
		/* first see if we can avoid interpolation */
		if (fac <= 0.0f) {
			if (ele1 == ele_dst) {
				/* do nothing */
			}
			else {
				CustomData_bmesh_free_block_data(data_layer, &ele_dst->head.data);
				CustomData_bmesh_copy_data(data_layer, data_layer, ele1->head.data, &ele_dst->head.data);
			}
		}
		else if (fac >= 1.0f) {
			if (ele2 == ele_dst) {
				/* do nothing */
			}
			else {
				CustomData_bmesh_free_block_data(data_layer, &ele_dst->head.data);
				CustomData_bmesh_copy_data(data_layer, data_layer, ele2->head.data, &ele_dst->head.data);
			}
		}
		else {
			void *src[2];
			float w[2];

			src[0] = ele1->head.data;
			src[1] = ele2->head.data;
			w[0] = 1.0f - fac;
			w[1] = fac;
			CustomData_bmesh_interp(data_layer, src, w, NULL, 2, ele_dst->head.data);
		}
	}
}

/**
 * \brief Data, Interp From Verts
 *
 * Interpolates per-vertex data from two sources to a target.
 *
 * \note This is an exact match to #BM_data_interp_from_edges
 */
void BM_data_interp_from_verts(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, const float fac)
{
	bm_data_interp_from_elem(&bm->vdata, (BMElem *)v1, (BMElem *)v2, (BMElem *)v, fac);
}

/**
 * \brief Data, Interp From Edges
 *
 * Interpolates per-edge data from two sources to a target.
 *
 * \note This is an exact match to #BM_data_interp_from_verts
 */
void BM_data_interp_from_edges(BMesh *bm, BMEdge *e1, BMEdge *e2, BMEdge *e, const float fac)
{
	bm_data_interp_from_elem(&bm->edata, (BMElem *)e1, (BMElem *)e2, (BMElem *)e, fac);
}

/**
 * \brief Data Vert Average
 *
 * Sets all the customdata (e.g. vert, loop) associated with a vert
 * to the average of the face regions surrounding it.
 */
static void UNUSED_FUNCTION(BM_Data_Vert_Average)(BMesh *UNUSED(bm), BMFace *UNUSED(f))
{
	// BMIter iter;
}

/**
 * \brief Data Face-Vert Edge Interp
 *
 * Walks around the faces of an edge and interpolates the per-face-edge
 * data between two sources to a target.
 */
void BM_data_interp_face_vert_edge(BMesh *bm, BMVert *v1, BMVert *UNUSED(v2), BMVert *v, BMEdge *e1, const float fac)
{
	void *src[2];
	float w[2];
	BMLoop *l_v1 = NULL, *l_v = NULL, *l_v2 = NULL;
	BMLoop *l_iter = NULL;

	if (!e1->l) {
		return;
	}

	w[1] = 1.0f - fac;
	w[0] = fac;

	l_iter = e1->l;
	do {
		if (l_iter->v == v1) {
			l_v1 = l_iter;
			l_v = l_v1->next;
			l_v2 = l_v->next;
		}
		else if (l_iter->v == v) {
			l_v1 = l_iter->next;
			l_v = l_iter;
			l_v2 = l_iter->prev;
		}
		
		if (!l_v1 || !l_v2)
			return;
		
		src[0] = l_v1->head.data;
		src[1] = l_v2->head.data;

		CustomData_bmesh_interp(&bm->ldata, src, w, NULL, 2, l_v->head.data);
	} while ((l_iter = l_iter->radial_next) != e1->l);
}

/**
 * \brief Data Interp From Face
 *
 * projects target onto source, and pulls interpolated customdata from
 * source.
 *
 * \note Only handles loop customdata. multires is handled.
 */
void BM_face_interp_from_face_ex(BMesh *bm, BMFace *target, BMFace *source, const bool do_vertex,
                                 void **blocks_l, void **blocks_v, float (*cos_2d)[2], float axis_mat[3][3])
{
	BMLoop *l_iter;
	BMLoop *l_first;

	float *w = BLI_array_alloca(w, source->len);
	float co[2];
	int i;

	if (source != target)
		BM_elem_attrs_copy(bm, bm, source, target);

	/* interpolate */
	i = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(target);
	do {
		mul_v2_m3v3(co, axis_mat, l_iter->v->co);
		interp_weights_poly_v2(w, cos_2d, source->len, co);
		CustomData_bmesh_interp(&bm->ldata, blocks_l, w, NULL, source->len, l_iter->head.data);
		if (do_vertex) {
			CustomData_bmesh_interp(&bm->vdata, blocks_v, w, NULL, source->len, l_iter->v->head.data);
		}
	} while (i++, (l_iter = l_iter->next) != l_first);
}

void BM_face_interp_from_face(BMesh *bm, BMFace *target, BMFace *source, const bool do_vertex)
{
	BMLoop *l_iter;
	BMLoop *l_first;

	void **blocks_l    = BLI_array_alloca(blocks_l, source->len);
	void **blocks_v    = do_vertex ? BLI_array_alloca(blocks_v, source->len) : NULL;
	float (*cos_2d)[2] = BLI_array_alloca(cos_2d, source->len);
	float axis_mat[3][3];  /* use normal to transform into 2d xy coords */
	int i;

	/* convert the 3d coords into 2d for projection */
	BLI_assert(BM_face_is_normal_valid(source));
	axis_dominant_v3_to_m3(axis_mat, source->no);

	i = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(source);
	do {
		mul_v2_m3v3(cos_2d[i], axis_mat, l_iter->v->co);
		blocks_l[i] = l_iter->head.data;
		if (do_vertex) blocks_v[i] = l_iter->v->head.data;
	} while (i++, (l_iter = l_iter->next) != l_first);

	BM_face_interp_from_face_ex(bm, target, source, do_vertex,
	                            blocks_l, blocks_v, cos_2d, axis_mat);
}

/**
 * \brief Multires Interpolation
 *
 * mdisps is a grid of displacements, ordered thus:
 * <pre>
 *      v1/center----v4/next -> x
 *          |           |
 *          |           |
 *       v2/prev------v3/cur
 *          |
 *          V
 *          y
 * </pre>
 */
static int compute_mdisp_quad(BMLoop *l, float v1[3], float v2[3], float v3[3], float v4[3],
                              float e1[3], float e2[3])
{
	float cent[3], n[3], p[3];

	/* computer center */
	BM_face_calc_center_mean(l->f, cent);

	mid_v3_v3v3(p, l->prev->v->co, l->v->co);
	mid_v3_v3v3(n, l->next->v->co, l->v->co);
	
	copy_v3_v3(v1, cent);
	copy_v3_v3(v2, p);
	copy_v3_v3(v3, l->v->co);
	copy_v3_v3(v4, n);
	
	sub_v3_v3v3(e1, v2, v1);
	sub_v3_v3v3(e2, v3, v4);
	
	return 1;
}

/* funnily enough, I think this is identical to face_to_crn_interp, heh */
static float quad_coord(const float aa[3], const float bb[3], const float cc[3], const float dd[3], int a1, int a2)
{
	float x, y, z, f1;
	float div;
	
	x = aa[a1] * cc[a2] - cc[a1] * aa[a2];
	y = aa[a1] * dd[a2] + bb[a1] * cc[a2] - cc[a1] * bb[a2] - dd[a1] * aa[a2];
	z = bb[a1] * dd[a2] - dd[a1] * bb[a2];

	div = 2.0f * (x - y + z);

	if (fabsf(div) > FLT_EPSILON * 10.0f) {
		const float f_tmp = sqrtf(y * y - 4.0f * x * z);

		f1 = min_ff(fabsf(( f_tmp - y + 2.0f * z) / div),
		            fabsf((-f_tmp - y + 2.0f * z) / div));

		CLAMP_MAX(f1, 1.0f + FLT_EPSILON);
	}
	else {
		f1 = -z / (y - 2 * z);
		CLAMP(f1, 0.0f, 1.0f + FLT_EPSILON);
		
		if (isnan(f1) || f1 > 1.0f || f1 < 0.0f) {
			int i;
			
			for (i = 0; i < 2; i++) {
				if (fabsf(aa[i]) < FLT_EPSILON * 100.0f)
					return aa[(i + 1) % 2] / fabsf(bb[(i + 1) % 2] - aa[(i + 1) % 2]);
				if (fabsf(cc[i]) < FLT_EPSILON * 100.0f)
					return cc[(i + 1) % 2] / fabsf(dd[(i + 1) % 2] - cc[(i + 1) % 2]);
			}
		}
	}

	return f1;
}

static int quad_co(float *x, float *y,
                   const float v1[3], const float v2[3], const float v3[3], const float v4[3],
                   const float p[3], const float n[3])
{
	float projverts[5][3], n2[3];
	float dprojverts[4][3], origin[3] = {0.0f, 0.0f, 0.0f};
	int i;

	/* project points into 2d along normal */
	copy_v3_v3(projverts[0], v1);
	copy_v3_v3(projverts[1], v2);
	copy_v3_v3(projverts[2], v3);
	copy_v3_v3(projverts[3], v4);
	copy_v3_v3(projverts[4], p);

	normal_quad_v3(n2, projverts[0], projverts[1], projverts[2], projverts[3]);

	if (dot_v3v3(n, n2) < -FLT_EPSILON) {
		return 0;
	}

	/* rotate */
	poly_rotate_plane(n, projverts, 5);

	/* subtract origin */
	for (i = 0; i < 4; i++) {
		sub_v3_v3(projverts[i], projverts[4]);
	}
	
	copy_v3_v3(dprojverts[0], projverts[0]);
	copy_v3_v3(dprojverts[1], projverts[1]);
	copy_v3_v3(dprojverts[2], projverts[2]);
	copy_v3_v3(dprojverts[3], projverts[3]);

	if (!isect_point_quad_v2(origin, dprojverts[0], dprojverts[1], dprojverts[2], dprojverts[3])) {
		return 0;
	}
	
	*y = quad_coord(dprojverts[1], dprojverts[0], dprojverts[2], dprojverts[3], 0, 1);
	*x = quad_coord(dprojverts[2], dprojverts[1], dprojverts[3], dprojverts[0], 0, 1);

	return 1;
}

static void mdisp_axis_from_quad(float v1[3], float v2[3], float UNUSED(v3[3]), float v4[3],
                                float axis_x[3], float axis_y[3])
{
	sub_v3_v3v3(axis_x, v4, v1);
	sub_v3_v3v3(axis_y, v2, v1);

	normalize_v3(axis_x);
	normalize_v3(axis_y);
}

/* tl is loop to project onto, l is loop whose internal displacement, co, is being
 * projected.  x and y are location in loop's mdisps grid of point co. */
static bool mdisp_in_mdispquad(BMLoop *l, BMLoop *tl, float p[3], float *x, float *y,
                               int res, float axis_x[3], float axis_y[3])
{
	float v1[3], v2[3], c[3], v3[3], v4[3], e1[3], e2[3];
	float eps = FLT_EPSILON * 4000;
	
	if (is_zero_v3(l->v->no))
		BM_vert_normal_update_all(l->v);
	if (is_zero_v3(tl->v->no))
		BM_vert_normal_update_all(tl->v);

	compute_mdisp_quad(tl, v1, v2, v3, v4, e1, e2);

	/* expand quad a bit */
	cent_quad_v3(c, v1, v2, v3, v4);
	
	sub_v3_v3(v1, c); sub_v3_v3(v2, c);
	sub_v3_v3(v3, c); sub_v3_v3(v4, c);
	mul_v3_fl(v1, 1.0f + eps); mul_v3_fl(v2, 1.0f + eps);
	mul_v3_fl(v3, 1.0f + eps); mul_v3_fl(v4, 1.0f + eps);
	add_v3_v3(v1, c); add_v3_v3(v2, c);
	add_v3_v3(v3, c); add_v3_v3(v4, c);
	
	if (!quad_co(x, y, v1, v2, v3, v4, p, l->v->no))
		return 0;
	
	*x *= res - 1;
	*y *= res - 1;

	mdisp_axis_from_quad(v1, v2, v3, v4, axis_x, axis_y);

	return 1;
}

static float bm_loop_flip_equotion(float mat[2][2], float b[2], const float target_axis_x[3], const float target_axis_y[3],
                                   const float coord[3], int i, int j)
{
	mat[0][0] = target_axis_x[i];
	mat[0][1] = target_axis_y[i];
	mat[1][0] = target_axis_x[j];
	mat[1][1] = target_axis_y[j];
	b[0] = coord[i];
	b[1] = coord[j];

	return mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0];
}

static void bm_loop_flip_disp(float source_axis_x[3], float source_axis_y[3],
                                 float target_axis_x[3], float target_axis_y[3], float disp[3])
{
	float vx[3], vy[3], coord[3];
	float n[3], vec[3];
	float b[2], mat[2][2], d;

	mul_v3_v3fl(vx, source_axis_x, disp[0]);
	mul_v3_v3fl(vy, source_axis_y, disp[1]);
	add_v3_v3v3(coord, vx, vy);

	/* project displacement from source grid plane onto target grid plane */
	cross_v3_v3v3(n, target_axis_x, target_axis_y);
	project_v3_v3v3(vec, coord, n);
	sub_v3_v3v3(coord, coord, vec);

	d = bm_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 0, 1);

	if (fabsf(d) < 1e-4f) {
		d = bm_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 0, 2);
		if (fabsf(d) < 1e-4f)
			d = bm_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 1, 2);
	}

	disp[0] = (b[0] * mat[1][1] - mat[0][1] * b[1]) / d;
	disp[1] = (mat[0][0] * b[1] - b[0] * mat[1][0]) / d;
}

static void bm_loop_interp_mdisps(BMesh *bm, BMLoop *l_dst, BMFace *f_src)
{
	const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
	MDisps *md_dst;
	float d, v1[3], v2[3], v3[3], v4[3] = {0.0f, 0.0f, 0.0f}, e1[3], e2[3];
	int ix, res;
	float axis_x[3], axis_y[3];

	if (cd_loop_mdisp_offset == -1)
		return;
	
	/* ignore 2-edged faces */
	if (UNLIKELY(l_dst->f->len < 3))
		return;

	md_dst = BM_ELEM_CD_GET_VOID_P(l_dst, cd_loop_mdisp_offset);
	compute_mdisp_quad(l_dst, v1, v2, v3, v4, e1, e2);
	
	/* if no disps data allocate a new grid, the size of the first grid in f_src. */
	if (!md_dst->totdisp) {
		MDisps *md_src = BM_ELEM_CD_GET_VOID_P(BM_FACE_FIRST_LOOP(f_src), cd_loop_mdisp_offset);
		
		md_dst->totdisp = md_src->totdisp;
		md_dst->level = md_src->level;
		if (md_dst->totdisp) {
			md_dst->disps = MEM_callocN(sizeof(float) * 3 * md_dst->totdisp, __func__);
		}
		else {
			return;
		}
	}
	
	mdisp_axis_from_quad(v1, v2, v3, v4, axis_x, axis_y);

	res = (int)sqrt(md_dst->totdisp);
	d = 1.0f / (float)(res - 1);
#pragma omp parallel for if (res > 3)
	for (ix = 0; ix < res; ix++) {
		float x = d * ix, y;
		int iy;
		for (y = 0.0f, iy = 0; iy < res; y += d, iy++) {
			BMLoop *l_iter;
			BMLoop *l_first;
			float co1[3], co2[3], co[3];

			copy_v3_v3(co1, e1);
			
			mul_v3_fl(co1, y);
			add_v3_v3(co1, v1);
			
			copy_v3_v3(co2, e2);
			mul_v3_fl(co2, y);
			add_v3_v3(co2, v4);
			
			sub_v3_v3v3(co, co2, co1);
			mul_v3_fl(co, x);
			add_v3_v3(co, co1);
			
			l_iter = l_first = BM_FACE_FIRST_LOOP(f_src);
			do {
				float x2, y2;
				MDisps *md_src;
				float src_axis_x[3], src_axis_y[3];

				md_src = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_mdisp_offset);
				
				if (mdisp_in_mdispquad(l_dst, l_iter, co, &x2, &y2, res, src_axis_x, src_axis_y)) {
					old_mdisps_bilinear(md_dst->disps[iy * res + ix], md_src->disps, res, (float)x2, (float)y2);
					bm_loop_flip_disp(src_axis_x, src_axis_y, axis_x, axis_y, md_dst->disps[iy * res + ix]);

					break;
				}
			} while ((l_iter = l_iter->next) != l_first);
		}
	}
}

/**
 * smooths boundaries between multires grids,
 * including some borders in adjacent faces
 */
void BM_face_multires_bounds_smooth(BMesh *bm, BMFace *f)
{
	const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
	BMLoop *l;
	BMIter liter;
	
	if (cd_loop_mdisp_offset == -1)
		return;
	
	BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
		MDisps *mdp = BM_ELEM_CD_GET_VOID_P(l->prev, cd_loop_mdisp_offset);
		MDisps *mdl = BM_ELEM_CD_GET_VOID_P(l, cd_loop_mdisp_offset);
		MDisps *mdn = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_mdisp_offset);
		float co1[3];
		int sides;
		int y;
		
		/*
		 *  mdisps is a grid of displacements, ordered thus:
		 *
		 *                     v4/next
		 *                       |
		 *   |      v1/cent-----mid2 ---> x
		 *   |         |         |
		 *   |         |         |
		 *  v2/prev---mid1-----v3/cur
		 *             |
		 *             V
		 *             y
		 */

		sides = (int)sqrt(mdp->totdisp);
		for (y = 0; y < sides; y++) {
			mid_v3_v3v3(co1, mdn->disps[y * sides], mdl->disps[y]);

			copy_v3_v3(mdn->disps[y * sides], co1);
			copy_v3_v3(mdl->disps[y], co1);
		}
	}
	
	BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
		MDisps *mdl1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_mdisp_offset);
		MDisps *mdl2;
		float co1[3], co2[3], co[3];
		int sides;
		int y;
		
		/*
		 *  mdisps is a grid of displacements, ordered thus:
		 *
		 *                     v4/next
		 *                       |
		 *   |      v1/cent-----mid2 ---> x
		 *   |         |         |
		 *   |         |         |
		 *  v2/prev---mid1-----v3/cur
		 *             |
		 *             V
		 *             y
		 */

		if (l->radial_next == l)
			continue;

		if (l->radial_next->v == l->v)
			mdl2 = BM_ELEM_CD_GET_VOID_P(l->radial_next, cd_loop_mdisp_offset);
		else
			mdl2 = BM_ELEM_CD_GET_VOID_P(l->radial_next->next, cd_loop_mdisp_offset);

		sides = (int)sqrt(mdl1->totdisp);
		for (y = 0; y < sides; y++) {
			int a1, a2, o1, o2;
			
			if (l->v != l->radial_next->v) {
				a1 = sides * y + sides - 2;
				a2 = (sides - 2) * sides + y;
				
				o1 = sides * y + sides - 1;
				o2 = (sides - 1) * sides + y;
			}
			else {
				a1 = sides * y + sides - 2;
				a2 = sides * y + sides - 2;
				o1 = sides * y + sides - 1;
				o2 = sides * y + sides - 1;
			}
			
			/* magic blending numbers, hardcoded! */
			add_v3_v3v3(co1, mdl1->disps[a1], mdl2->disps[a2]);
			mul_v3_fl(co1, 0.18);
			
			add_v3_v3v3(co2, mdl1->disps[o1], mdl2->disps[o2]);
			mul_v3_fl(co2, 0.32);
			
			add_v3_v3v3(co, co1, co2);
			
			copy_v3_v3(mdl1->disps[o1], co);
			copy_v3_v3(mdl2->disps[o2], co);
		}
	}
}

/**
 * project the multires grid in target onto source's set of multires grids
 */
void BM_loop_interp_multires(BMesh *bm, BMLoop *target, BMFace *source)
{
	bm_loop_interp_mdisps(bm, target, source);
}

/**
 * projects a single loop, target, onto source for customdata interpolation. multires is handled.
 * if do_vertex is true, target's vert data will also get interpolated.
 */
void BM_loop_interp_from_face(BMesh *bm, BMLoop *target, BMFace *source,
                              const bool do_vertex, const bool do_multires)
{
	BMLoop *l_iter;
	BMLoop *l_first;
	void **vblocks  = do_vertex ? BLI_array_alloca(vblocks, source->len) : NULL;
	void **blocks   = BLI_array_alloca(blocks, source->len);
	float (*cos_2d)[2] = BLI_array_alloca(cos_2d, source->len);
	float *w        = BLI_array_alloca(w, source->len);
	float axis_mat[3][3];  /* use normal to transform into 2d xy coords */
	float co[2];
	int i;

	/* convert the 3d coords into 2d for projection */
	BLI_assert(BM_face_is_normal_valid(source));
	axis_dominant_v3_to_m3(axis_mat, source->no);

	i = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(source);
	do {
		mul_v2_m3v3(cos_2d[i], axis_mat, l_iter->v->co);
		blocks[i] = l_iter->head.data;

		if (do_vertex) {
			vblocks[i] = l_iter->v->head.data;
		}
	} while (i++, (l_iter = l_iter->next) != l_first);

	mul_v2_m3v3(co, axis_mat, target->v->co);

	/* interpolate */
	interp_weights_poly_v2(w, cos_2d, source->len, co);
	CustomData_bmesh_interp(&bm->ldata, blocks, w, NULL, source->len, target->head.data);
	if (do_vertex) {
		CustomData_bmesh_interp(&bm->vdata, vblocks, w, NULL, source->len, target->v->head.data);
	}

	if (do_multires) {
		bm_loop_interp_mdisps(bm, target, source);
	}
}


void BM_vert_interp_from_face(BMesh *bm, BMVert *v, BMFace *source)
{
	BMLoop *l_iter;
	BMLoop *l_first;
	void **blocks   = BLI_array_alloca(blocks, source->len);
	float (*cos_2d)[2] = BLI_array_alloca(cos_2d, source->len);
	float *w        = BLI_array_alloca(w,      source->len);
	float axis_mat[3][3];  /* use normal to transform into 2d xy coords */
	float co[2];
	int i;

	/* convert the 3d coords into 2d for projection */
	BLI_assert(BM_face_is_normal_valid(source));
	axis_dominant_v3_to_m3(axis_mat, source->no);

	i = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(source);
	do {
		mul_v2_m3v3(cos_2d[i], axis_mat, l_iter->v->co);
		blocks[i] = l_iter->v->head.data;
	} while (i++, (l_iter = l_iter->next) != l_first);

	mul_v2_m3v3(co, axis_mat, v->co);

	/* interpolate */
	interp_weights_poly_v2(w, cos_2d, source->len, co);
	CustomData_bmesh_interp(&bm->vdata, blocks, w, NULL, source->len, v->head.data);
}

static void update_data_blocks(BMesh *bm, CustomData *olddata, CustomData *data)
{
	BMIter iter;
	BLI_mempool *oldpool = olddata->pool;
	void *block;

	if (data == &bm->vdata) {
		BMVert *eve;

		CustomData_bmesh_init_pool(data, bm->totvert, BM_VERT);

		BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
			block = NULL;
			CustomData_bmesh_set_default(data, &block);
			CustomData_bmesh_copy_data(olddata, data, eve->head.data, &block);
			CustomData_bmesh_free_block(olddata, &eve->head.data);
			eve->head.data = block;
		}
	}
	else if (data == &bm->edata) {
		BMEdge *eed;

		CustomData_bmesh_init_pool(data, bm->totedge, BM_EDGE);

		BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
			block = NULL;
			CustomData_bmesh_set_default(data, &block);
			CustomData_bmesh_copy_data(olddata, data, eed->head.data, &block);
			CustomData_bmesh_free_block(olddata, &eed->head.data);
			eed->head.data = block;
		}
	}
	else if (data == &bm->ldata) {
		BMIter liter;
		BMFace *efa;
		BMLoop *l;

		CustomData_bmesh_init_pool(data, bm->totloop, BM_LOOP);
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				block = NULL;
				CustomData_bmesh_set_default(data, &block);
				CustomData_bmesh_copy_data(olddata, data, l->head.data, &block);
				CustomData_bmesh_free_block(olddata, &l->head.data);
				l->head.data = block;
			}
		}
	}
	else if (data == &bm->pdata) {
		BMFace *efa;

		CustomData_bmesh_init_pool(data, bm->totface, BM_FACE);

		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			block = NULL;
			CustomData_bmesh_set_default(data, &block);
			CustomData_bmesh_copy_data(olddata, data, efa->head.data, &block);
			CustomData_bmesh_free_block(olddata, &efa->head.data);
			efa->head.data = block;
		}
	}
	else {
		/* should never reach this! */
		BLI_assert(0);
	}

	if (oldpool) {
		/* this should never happen but can when dissolve fails - [#28960] */
		BLI_assert(data->pool != oldpool);

		BLI_mempool_destroy(oldpool);
	}
}

void BM_data_layer_add(BMesh *bm, CustomData *data, int type)
{
	CustomData olddata;

	olddata = *data;
	olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers) : NULL;

	/* the pool is now owned by olddata and must not be shared */
	data->pool = NULL;

	CustomData_add_layer(data, type, CD_DEFAULT, NULL, 0);

	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BM_data_layer_add_named(BMesh *bm, CustomData *data, int type, const char *name)
{
	CustomData olddata;

	olddata = *data;
	olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers) : NULL;

	/* the pool is now owned by olddata and must not be shared */
	data->pool = NULL;

	CustomData_add_layer_named(data, type, CD_DEFAULT, NULL, 0, name);

	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BM_data_layer_free(BMesh *bm, CustomData *data, int type)
{
	CustomData olddata;
	bool has_layer;

	olddata = *data;
	olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers) : NULL;

	/* the pool is now owned by olddata and must not be shared */
	data->pool = NULL;

	has_layer = CustomData_free_layer_active(data, type, 0);
	/* assert because its expensive to realloc - better not do if layer isnt present */
	BLI_assert(has_layer != false);

	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BM_data_layer_free_n(BMesh *bm, CustomData *data, int type, int n)
{
	CustomData olddata;
	bool has_layer;

	olddata = *data;
	olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers) : NULL;

	/* the pool is now owned by olddata and must not be shared */
	data->pool = NULL;

	has_layer = CustomData_free_layer(data, type, 0, CustomData_get_layer_index_n(data, type, n));
	/* assert because its expensive to realloc - better not do if layer isnt present */
	BLI_assert(has_layer != false);
	
	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BM_data_layer_copy(BMesh *bm, CustomData *data, int type, int src_n, int dst_n)
{
	BMIter iter;

	if (&bm->vdata == data) {
		BMVert *eve;

		BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
			void *ptr = CustomData_bmesh_get_n(data, eve->head.data, type, src_n);
			CustomData_bmesh_set_n(data, eve->head.data, type, dst_n, ptr);
		}
	}
	else if (&bm->edata == data) {
		BMEdge *eed;

		BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
			void *ptr = CustomData_bmesh_get_n(data, eed->head.data, type, src_n);
			CustomData_bmesh_set_n(data, eed->head.data, type, dst_n, ptr);
		}
	}
	else if (&bm->pdata == data) {
		BMFace *efa;

		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			void *ptr = CustomData_bmesh_get_n(data, efa->head.data, type, src_n);
			CustomData_bmesh_set_n(data, efa->head.data, type, dst_n, ptr);
		}
	}
	else if (&bm->ldata == data) {
		BMIter liter;
		BMFace *efa;
		BMLoop *l;

		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				void *ptr = CustomData_bmesh_get_n(data, l->head.data, type, src_n);
				CustomData_bmesh_set_n(data, l->head.data, type, dst_n, ptr);
			}
		}
	}
	else {
		/* should never reach this! */
		BLI_assert(0);
	}
}

float BM_elem_float_data_get(CustomData *cd, void *element, int type)
{
	const float *f = CustomData_bmesh_get(cd, ((BMHeader *)element)->data, type);
	return f ? *f : 0.0f;
}

void BM_elem_float_data_set(CustomData *cd, void *element, int type, const float val)
{
	float *f = CustomData_bmesh_get(cd, ((BMHeader *)element)->data, type);
	if (f) *f = val;
}
