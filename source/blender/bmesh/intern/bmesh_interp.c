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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_multires.h"

#include "BLI_array.h"
#include "BLI_math.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/**
 * \brief Data, Interp From Verts
 *
 * Interpolates per-vertex data from two sources to a target.
 */
void BM_data_interp_from_verts(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, const float fac)
{
	if (v1->head.data && v2->head.data) {
		/* first see if we can avoid interpolation */
		if (fac <= 0.0f) {
			if (v1 == v) {
				/* do nothing */
			}
			else {
				CustomData_bmesh_free_block(&bm->vdata, &v->head.data);
				CustomData_bmesh_copy_data(&bm->vdata, &bm->vdata, v1->head.data, &v->head.data);
			}
		}
		else if (fac >= 1.0f) {
			if (v2 == v) {
				/* do nothing */
			}
			else {
				CustomData_bmesh_free_block(&bm->vdata, &v->head.data);
				CustomData_bmesh_copy_data(&bm->vdata, &bm->vdata, v2->head.data, &v->head.data);
			}
		}
		else {
			void *src[2];
			float w[2];

			src[0] = v1->head.data;
			src[1] = v2->head.data;
			w[0] = 1.0f-fac;
			w[1] = fac;
			CustomData_bmesh_interp(&bm->vdata, src, w, NULL, 2, v->head.data);
		}
	}
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
	BMLoop *v1loop = NULL, *vloop = NULL, *v2loop = NULL;
	BMLoop *l_iter = NULL;

	if (!e1->l) {
		return;
	}

	w[1] = 1.0f - fac;
	w[0] = fac;

	l_iter = e1->l;
	do {
		if (l_iter->v == v1) {
			v1loop = l_iter;
			vloop = v1loop->next;
			v2loop = vloop->next;
		}
		else if (l_iter->v == v) {
			v1loop = l_iter->next;
			vloop = l_iter;
			v2loop = l_iter->prev;
		}
		
		if (!v1loop || !v2loop)
			return;
		
		src[0] = v1loop->head.data;
		src[1] = v2loop->head.data;

		CustomData_bmesh_interp(&bm->ldata, src, w, NULL, 2, vloop->head.data);
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
void BM_face_interp_from_face(BMesh *bm, BMFace *target, BMFace *source)
{
	BMLoop *l_iter;
	BMLoop *l_first;

	void **blocks = NULL;
	float (*cos)[3] = NULL, *w = NULL;
	BLI_array_fixedstack_declare(cos,     BM_NGON_STACK_SIZE, source->len, __func__);
	BLI_array_fixedstack_declare(w,       BM_NGON_STACK_SIZE, source->len, __func__);
	BLI_array_fixedstack_declare(blocks,  BM_NGON_STACK_SIZE, source->len, __func__);
	int i;
	
	BM_elem_attrs_copy(bm, bm, source, target);

	i = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(source);
	do {
		copy_v3_v3(cos[i], l_iter->v->co);
		blocks[i] = l_iter->head.data;
		i++;
	} while ((l_iter = l_iter->next) != l_first);

	i = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(target);
	do {
		interp_weights_poly_v3(w, cos, source->len, l_iter->v->co);
		CustomData_bmesh_interp(&bm->ldata, blocks, w, NULL, source->len, l_iter->head.data);
		i++;
	} while ((l_iter = l_iter->next) != l_first);

	BLI_array_fixedstack_free(cos);
	BLI_array_fixedstack_free(w);
	BLI_array_fixedstack_free(blocks);
}

/**
 * \brief Multires Interpolation
 *
 * mdisps is a grid of displacements, ordered thus:
 *
 *      v1/center----v4/next -> x
 *          |           |
 *          |           |
 *       v2/prev------v3/cur
 *          |
 *          V
 *          y
 */
static int compute_mdisp_quad(BMLoop *l, float v1[3], float v2[3], float v3[3], float v4[3],
                              float e1[3], float e2[3])
{
	float cent[3] = {0.0f, 0.0f, 0.0f}, n[3], p[3];
	BMLoop *l_first;
	BMLoop *l_iter;
	
	/* computer center */
	l_iter = l_first = BM_FACE_FIRST_LOOP(l->f);
	do {
		cent[0] += (float)l_iter->v->co[0];
		cent[1] += (float)l_iter->v->co[1];
		cent[2] += (float)l_iter->v->co[2];
	} while ((l_iter = l_iter->next) != l_first);
	
	mul_v3_fl(cent, (1.0 / (float)l->f->len));
	
	add_v3_v3v3(p, l->prev->v->co, l->v->co);
	mul_v3_fl(p, 0.5);
	add_v3_v3v3(n, l->next->v->co, l->v->co);
	mul_v3_fl(n, 0.5);
	
	copy_v3_v3(v1, cent);
	copy_v3_v3(v2, p);
	copy_v3_v3(v3, l->v->co);
	copy_v3_v3(v4, n);
	
	sub_v3_v3v3(e1, v2, v1);
	sub_v3_v3v3(e2, v3, v4);
	
	return 1;
}

/* funnily enough, I think this is identical to face_to_crn_interp, heh */
static float quad_coord(float aa[3], float bb[3], float cc[3], float dd[3], int a1, int a2)
{
	float x, y, z, f1;
	
	x = aa[a1] * cc[a2] - cc[a1] * aa[a2];
	y = aa[a1] * dd[a2] + bb[a1] * cc[a2] - cc[a1] * bb[a2] - dd[a1] * aa[a2];
	z = bb[a1] * dd[a2] - dd[a1] * bb[a2];
	
	if (fabsf(2.0f * (x - y + z)) > FLT_EPSILON * 10.0f) {
		float f2;

		f1 = (sqrt(y * y - 4.0 * x * z) - y + 2.0 * z) / (2.0 * (x - y + z));
		f2 = (-sqrt(y * y - 4.0 * x * z) - y + 2.0 * z) / (2.0 * (x - y + z));

		f1 = fabsf(f1);
		f2 = fabsf(f2);
		f1 = MIN2(f1, f2);
		CLAMP(f1, 0.0f, 1.0f + FLT_EPSILON);
	}
	else {
		f1 = -z / (y - 2 * z);
		CLAMP(f1, 0.0f, 1.0f + FLT_EPSILON);
		
		if (isnan(f1) || f1 > 1.0 || f1 < 0.0f) {
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

static int quad_co(float *x, float *y, float v1[3], float v2[3], float v3[3], float v4[3],
                   float p[3], float n[3])
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
	
	/* flatten */
	for (i = 0; i < 5; i++) {
		projverts[i][2] = 0.0f;
	}
	
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
static int mdisp_in_mdispquad(BMesh *bm, BMLoop *l, BMLoop *tl, float p[3], float *x, float *y,
                              int res, float axis_x[3], float axis_y[3])
{
	float v1[3], v2[3], c[3], v3[3], v4[3], e1[3], e2[3];
	float eps = FLT_EPSILON * 4000;
	
	if (len_v3(l->v->no) == 0.0f)
		BM_vert_normal_update_all(bm, l->v);
	if (len_v3(tl->v->no) == 0.0f)
		BM_vert_normal_update_all(bm, tl->v);

	compute_mdisp_quad(tl, v1, v2, v3, v4, e1, e2);

	/* expand quad a bit */
	cent_quad_v3(c, v1, v2, v3, v4);
	
	sub_v3_v3(v1, c); sub_v3_v3(v2, c);
	sub_v3_v3(v3, c); sub_v3_v3(v4, c);
	mul_v3_fl(v1, 1.0 + eps); mul_v3_fl(v2, 1.0 + eps);
	mul_v3_fl(v3, 1.0 + eps); mul_v3_fl(v4, 1.0 + eps);
	add_v3_v3(v1, c); add_v3_v3(v2, c);
	add_v3_v3(v3, c); add_v3_v3(v4, c);
	
	if (!quad_co(x, y, v1, v2, v3, v4, p, l->v->no))
		return 0;
	
	*x *= res - 1;
	*y *= res - 1;

	mdisp_axis_from_quad(v1, v2, v3, v4, axis_x, axis_y);

	return 1;
}

static float bm_loop_flip_equotion(float mat[2][2], float b[2], float target_axis_x[3], float target_axis_y[3],
                                   float coord[3], int i, int j)
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

	if (fabsf(d) < 1e-4) {
		d = bm_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 0, 2);
		if (fabsf(d) < 1e-4)
			d = bm_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 1, 2);
	}

	disp[0] = (b[0] * mat[1][1] - mat[0][1] * b[1]) / d;
	disp[1] = (mat[0][0] * b[1] - b[0] * mat[1][0]) / d;
}

static void bm_loop_interp_mdisps(BMesh *bm, BMLoop *target, BMFace *source)
{
	MDisps *mdisps;
	BMLoop *l_iter;
	BMLoop *l_first;
	float x, y, d, v1[3], v2[3], v3[3], v4[3] = {0.0f, 0.0f, 0.0f}, e1[3], e2[3];
	int ix, iy, res;
	float axis_x[3], axis_y[3];
	
	/* ignore 2-edged faces */
	if (target->f->len < 3)
		return;
	
	if (!CustomData_has_layer(&bm->ldata, CD_MDISPS))
		return;
	
	mdisps = CustomData_bmesh_get(&bm->ldata, target->head.data, CD_MDISPS);
	compute_mdisp_quad(target, v1, v2, v3, v4, e1, e2);
	
	/* if no disps data allocate a new grid, the size of the first grid in source. */
	if (!mdisps->totdisp) {
		MDisps *md2 = CustomData_bmesh_get(&bm->ldata, BM_FACE_FIRST_LOOP(source)->head.data, CD_MDISPS);
		
		mdisps->totdisp = md2->totdisp;
		mdisps->level = md2->level;
		if (mdisps->totdisp) {
			mdisps->disps = MEM_callocN(sizeof(float) * 3 * mdisps->totdisp,
			                            "mdisp->disps in bmesh_loop_intern_mdisps");
		}
		else {
			return;
		}
	}
	
	mdisp_axis_from_quad(v1, v2, v3, v4, axis_x, axis_y);

	res = (int)sqrt(mdisps->totdisp);
	d = 1.0 / (float)(res - 1);
	for (x = 0.0f, ix = 0; ix < res; x += d, ix++) {
		for (y = 0.0f, iy = 0; iy < res; y += d, iy++) {
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
			
			l_iter = l_first = BM_FACE_FIRST_LOOP(source);
			do {
				float x2, y2;
				MDisps *md1, *md2;
				float src_axis_x[3], src_axis_y[3];

				md1 = CustomData_bmesh_get(&bm->ldata, target->head.data, CD_MDISPS);
				md2 = CustomData_bmesh_get(&bm->ldata, l_iter->head.data, CD_MDISPS);
				
				if (mdisp_in_mdispquad(bm, target, l_iter, co, &x2, &y2, res, src_axis_x, src_axis_y)) {
					old_mdisps_bilinear(md1->disps[iy * res + ix], md2->disps, res, (float)x2, (float)y2);
					bm_loop_flip_disp(src_axis_x, src_axis_y, axis_x, axis_y, md1->disps[iy * res + ix]);

					break;
				}
			} while ((l_iter = l_iter->next) != l_first);
		}
	}
}

/**
 * smoothes boundaries between multires grids,
 * including some borders in adjacent faces
 */
void BM_face_multires_bounds_smooth(BMesh *bm, BMFace *f)
{
	BMLoop *l;
	BMIter liter;
	
	if (!CustomData_has_layer(&bm->ldata, CD_MDISPS))
		return;
	
	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		MDisps *mdp = CustomData_bmesh_get(&bm->ldata, l->prev->head.data, CD_MDISPS);
		MDisps *mdl = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MDISPS);
		MDisps *mdn = CustomData_bmesh_get(&bm->ldata, l->next->head.data, CD_MDISPS);
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
			add_v3_v3v3(co1, mdn->disps[y * sides], mdl->disps[y]);
			mul_v3_fl(co1, 0.5);

			copy_v3_v3(mdn->disps[y * sides], co1);
			copy_v3_v3(mdl->disps[y], co1);
		}
	}
	
	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		MDisps *mdl1 = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MDISPS);
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
			mdl2 = CustomData_bmesh_get(&bm->ldata, l->radial_next->head.data, CD_MDISPS);
		else
			mdl2 = CustomData_bmesh_get(&bm->ldata, l->radial_next->next->head.data, CD_MDISPS);

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
                              int do_vertex, int do_multires)
{
	BMLoop *l_iter;
	BMLoop *l_first;
	void **blocks = NULL;
	void **vblocks = NULL;
	float (*cos)[3] = NULL, co[3], *w = NULL;
	float cent[3] = {0.0f, 0.0f, 0.0f};
	BLI_array_fixedstack_declare(cos,      BM_NGON_STACK_SIZE, source->len, __func__);
	BLI_array_fixedstack_declare(w,        BM_NGON_STACK_SIZE, source->len, __func__);
	BLI_array_fixedstack_declare(blocks,   BM_NGON_STACK_SIZE, source->len, __func__);
	BLI_array_fixedstack_declare(vblocks,  BM_NGON_STACK_SIZE, do_vertex ? source->len : 0, __func__);
	int i, ax, ay;

	BM_elem_attrs_copy(bm, bm, source, target->f);

	i = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(source);
	do {
		copy_v3_v3(cos[i], l_iter->v->co);
		add_v3_v3(cent, cos[i]);

		w[i] = 0.0f;
		blocks[i] = l_iter->head.data;

		if (do_vertex) {
			vblocks[i] = l_iter->v->head.data;
		}
		i++;

	} while ((l_iter = l_iter->next) != l_first);

	/* find best projection of face XY, XZ or YZ: barycentric weights of
	 * the 2d projected coords are the same and faster to compute */

	axis_dominant_v3(&ax, &ay, source->no);

	/* scale source face coordinates a bit, so points sitting directly on an
	 * edge will work. */
	mul_v3_fl(cent, 1.0f / (float)source->len);
	for (i = 0; i < source->len; i++) {
		float vec[3], tmp[3];
		sub_v3_v3v3(vec, cent, cos[i]);
		mul_v3_fl(vec, 0.001f);
		add_v3_v3(cos[i], vec);

		copy_v3_v3(tmp, cos[i]);
		cos[i][0] = tmp[ax];
		cos[i][1] = tmp[ay];
		cos[i][2] = 0.0f;
	}


	/* interpolate */
	co[0] = target->v->co[ax];
	co[1] = target->v->co[ay];
	co[2] = 0.0f;

	interp_weights_poly_v3(w, cos, source->len, co);
	CustomData_bmesh_interp(&bm->ldata, blocks, w, NULL, source->len, target->head.data);
	if (do_vertex) {
		CustomData_bmesh_interp(&bm->vdata, vblocks, w, NULL, source->len, target->v->head.data);
		BLI_array_fixedstack_free(vblocks);
	}

	BLI_array_fixedstack_free(cos);
	BLI_array_fixedstack_free(w);
	BLI_array_fixedstack_free(blocks);

	if (do_multires) {
		if (CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
			bm_loop_interp_mdisps(bm, target, source);
		}
	}
}


void BM_vert_interp_from_face(BMesh *bm, BMVert *v, BMFace *source)
{
	BMLoop *l_iter;
	BMLoop *l_first;
	void **blocks = NULL;
	float (*cos)[3] = NULL, *w = NULL;
	float cent[3] = {0.0f, 0.0f, 0.0f};
	BLI_array_fixedstack_declare(cos,      BM_NGON_STACK_SIZE, source->len, __func__);
	BLI_array_fixedstack_declare(w,        BM_NGON_STACK_SIZE, source->len, __func__);
	BLI_array_fixedstack_declare(blocks,   BM_NGON_STACK_SIZE, source->len, __func__);
	int i;

	i = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(source);
	do {
		copy_v3_v3(cos[i], l_iter->v->co);
		add_v3_v3(cent, cos[i]);

		w[i] = 0.0f;
		blocks[i] = l_iter->v->head.data;
		i++;
	} while ((l_iter = l_iter->next) != l_first);

	/* scale source face coordinates a bit, so points sitting directly on an
	 * edge will work. */
	mul_v3_fl(cent, 1.0f / (float)source->len);
	for (i = 0; i < source->len; i++) {
		float vec[3];
		sub_v3_v3v3(vec, cent, cos[i]);
		mul_v3_fl(vec, 0.01f);
		add_v3_v3(cos[i], vec);
	}

	/* interpolate */
	interp_weights_poly_v3(w, cos, source->len, v->co);
	CustomData_bmesh_interp(&bm->vdata, blocks, w, NULL, source->len, v->head.data);

	BLI_array_fixedstack_free(cos);
	BLI_array_fixedstack_free(w);
	BLI_array_fixedstack_free(blocks);
}

static void update_data_blocks(BMesh *bm, CustomData *olddata, CustomData *data)
{
	BMIter iter;
	BLI_mempool *oldpool = olddata->pool;
	void *block;

	if (data == &bm->vdata) {
		BMVert *eve;

		CustomData_bmesh_init_pool(data, bm->totvert, BM_VERT);

		BM_ITER(eve, &iter, bm, BM_VERTS_OF_MESH, NULL) {
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

		BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
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
		BM_ITER(efa, &iter, bm, BM_FACES_OF_MESH, NULL) {
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, efa) {
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

		BM_ITER(efa, &iter, bm, BM_FACES_OF_MESH, NULL) {
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
	olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers): NULL;

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
	olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers): NULL;

	/* the pool is now owned by olddata and must not be shared */
	data->pool = NULL;

	CustomData_add_layer_named(data, type, CD_DEFAULT, NULL, 0, name);

	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BM_data_layer_free(BMesh *bm, CustomData *data, int type)
{
	CustomData olddata;

	olddata = *data;
	olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers): NULL;

	/* the pool is now owned by olddata and must not be shared */
	data->pool = NULL;

	CustomData_free_layer_active(data, type, 0);

	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BM_data_layer_free_n(BMesh *bm, CustomData *data, int type, int n)
{
	CustomData olddata;

	olddata = *data;
	olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers): NULL;

	/* the pool is now owned by olddata and must not be shared */
	data->pool = NULL;

	CustomData_free_layer(data, type, 0, CustomData_get_layer_index_n(data, type, n));
	
	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

float BM_elem_float_data_get(CustomData *cd, void *element, int type)
{
	float *f = CustomData_bmesh_get(cd, ((BMHeader *)element)->data, type);
	return f ? *f : 0.0f;
}

void BM_elem_float_data_set(CustomData *cd, void *element, int type, const float val)
{
	float *f = CustomData_bmesh_get(cd, ((BMHeader *)element)->data, type);
	if (f) *f = val;
}
