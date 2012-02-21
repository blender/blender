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
#include "bmesh_private.h"

/**
 *			bmesh_data_interp_from_verts
 *
 *  Interpolates per-vertex data from two sources to a target.
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

/*
 *  BM Data Vert Average
 *
 * Sets all the customdata (e.g. vert, loop) associated with a vert
 * to the average of the face regions surrounding it.
 */


static void UNUSED_FUNCTION(BM_Data_Vert_Average)(BMesh *UNUSED(bm), BMFace *UNUSED(f))
{
	// BMIter iter;
}

/**
 *			bmesh_data_facevert_edgeinterp
 *
 *  Walks around the faces of an edge and interpolates the per-face-edge
 *  data between two sources to a target.
 *
 *  Returns -
 *	Nothing
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

void BM_loops_to_corners(BMesh *bm, Mesh *me, int findex,
                         BMFace *f, int numTex, int numCol)
{
	BMLoop *l;
	BMIter iter;
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;
	int i, j;

	for (i = 0; i < numTex; i++) {
		texface = CustomData_get_n(&me->fdata, CD_MTFACE, findex, i);
		texpoly = CustomData_bmesh_get_n(&bm->pdata, f->head.data, CD_MTEXPOLY, i);
		
		ME_MTEXFACE_CPY(texface, texpoly);

		j = 0;
		BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
			mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPUV, i);
			copy_v2_v2(texface->uv[j], mloopuv->uv);

			j++;
		}

	}

	for (i = 0; i < numCol; i++) {
		mcol = CustomData_get_n(&me->fdata, CD_MCOL, findex, i);

		j = 0;
		BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
			mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPCOL, i);
			mcol[j].r = mloopcol->r;
			mcol[j].g = mloopcol->g;
			mcol[j].b = mloopcol->b;
			mcol[j].a = mloopcol->a;

			j++;
		}
	}
}

/**
 *			BM_data_interp_from_face
 *
 *  projects target onto source, and pulls interpolated customdata from
 *  source.
 *
 *  Returns -
 *	Nothing
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

/* some math stuff for dealing with doubles, put here to
 * avoid merge errors - joeedh */

#define VECMUL(a, b) (((a)[0] = (a)[0] * (b)), ((a)[1] = (a)[1] * (b)), ((a)[2] = (a)[2] * (b)))
#define VECADD2(a, b) (((a)[0] = (a)[0] + (b)[0]), ((a)[1] = (a)[1] + (b)[1]), ((a)[2] = (a)[2] + (b)[2]))
#define VECSUB2(a, b) (((a)[0] = (a)[0] - (b)[0]), ((a)[1] = (a)[1] - (b)[1]), ((a)[2] = (a)[2] - (b)[2]))

/* find closest point to p on line through l1, l2 and return lambda,
 * where (0 <= lambda <= 1) when cp is in the line segement l1, l2
 */
static double closest_to_line_v3_d(double cp[3], const double p[3], const double l1[3], const double l2[3])
{
	double h[3], u[3], lambda;
	VECSUB(u, l2, l1);
	VECSUB(h, p, l1);
	lambda = INPR(u, h) / INPR(u, u);
	cp[0] = l1[0] + u[0] * lambda;
	cp[1] = l1[1] + u[1] * lambda;
	cp[2] = l1[2] + u[2] * lambda;
	return lambda;
}

/* point closest to v1 on line v2-v3 in 3D */
static void UNUSED_FUNCTION(closest_to_line_segment_v3_d)(double *closest, double v1[3], double v2[3], double v3[3])
{
	double lambda, cp[3];

	lambda = closest_to_line_v3_d(cp, v1, v2, v3);

	if (lambda <= 0.0) {
		VECCOPY(closest, v2);
	}
	else if (lambda >= 1.0) {
		VECCOPY(closest, v3);
	}
	else {
		VECCOPY(closest, cp);
	}
}

static double UNUSED_FUNCTION(len_v3_d)(const double a[3])
{
	return sqrt(INPR(a, a));
}

static double UNUSED_FUNCTION(len_v3v3_d)(const double a[3], const double b[3])
{
	double d[3];

	VECSUB(d, b, a);
	return sqrt(INPR(d, d));
}

static void cent_quad_v3_d(double *cent, double *v1, double *v2, double *v3, double *v4)
{
	cent[0] = 0.25 * (v1[0] + v2[0] + v3[0] + v4[0]);
	cent[1] = 0.25 * (v1[1] + v2[1] + v3[1] + v4[1]);
	cent[2] = 0.25 * (v1[2] + v2[2] + v3[2] + v4[2]);
}

static void UNUSED_FUNCTION(cent_tri_v3_d)(double *cent, double *v1, double *v2, double *v3)
{
	cent[0] = (1.0 / 3.0) * (v1[0] + v2[0] + v3[0]);
	cent[1] = (1.0 / 3.0) * (v1[1] + v2[1] + v3[1]);
	cent[2] = (1.0 / 3.0) * (v1[2] + v2[2] + v3[2]);
}

static void UNUSED_FUNCTION(cross_v3_v3v3_d)(double r[3], const double a[3], const double b[3])
{
	r[0] = a[1] * b[2] - a[2] * b[1];
	r[1] = a[2] * b[0] - a[0] * b[2];
	r[2] = a[0] * b[1] - a[1] * b[0];
}

/* distance v1 to line-piece v2-v3 */
static double UNUSED_FUNCTION(dist_to_line_segment_v2_d)(double v1[3], double v2[3], double v3[3])
{
	double labda, rc[2], pt[2], len;
	
	rc[0] = v3[0] - v2[0];
	rc[1] = v3[1] - v2[1];
	len = rc[0] * rc[0] + rc[1] * rc[1];
	if (len == 0.0) {
		rc[0] = v1[0] - v2[0];
		rc[1] = v1[1] - v2[1];
		return sqrt(rc[0] * rc[0] + rc[1] * rc[1]);
	}
	
	labda = (rc[0] * (v1[0] - v2[0]) + rc[1] * (v1[1] - v2[1])) / len;
	if (labda <= 0.0) {
		pt[0] = v2[0];
		pt[1] = v2[1];
	}
	else if (labda >= 1.0) {
		pt[0] = v3[0];
		pt[1] = v3[1];
	}
	else {
		pt[0] = labda * rc[0] + v2[0];
		pt[1] = labda * rc[1] + v2[1];
	}

	rc[0] = pt[0] - v1[0];
	rc[1] = pt[1] - v1[1];
	return sqrt(rc[0] * rc[0] + rc[1] * rc[1]);
}


MINLINE double line_point_side_v2_d(const double *l1, const double *l2, const double *pt)
{
	return	((l1[0] - pt[0]) * (l2[1] - pt[1])) -
	        ((l2[0] - pt[0]) * (l1[1] - pt[1]));
}

/* point in quad - only convex quads */
static int isect_point_quad_v2_d(double pt[2], double v1[2], double v2[2], double v3[2], double v4[2])
{
	if (line_point_side_v2_d(v1, v2, pt) >= 0.0) {
		if (line_point_side_v2_d(v2, v3, pt) >= 0.0) {
			if (line_point_side_v2_d(v3, v4, pt) >= 0.0) {
				if (line_point_side_v2_d(v4, v1, pt) >= 0.0) {
					return 1;
				}
			}
		}
	}
	else {
		if (! (line_point_side_v2_d(v2, v3, pt) >= 0.0)) {
			if (! (line_point_side_v2_d(v3, v4, pt) >= 0.0)) {
				if (! (line_point_side_v2_d(v4, v1, pt) >= 0.0)) {
					return -1;
				}
			}
		}
	}
	
	return 0;
}

/***** multires interpolation*****
 *
 * mdisps is a grid of displacements, ordered thus:
 *
 *  v1/center----v4/next -> x
 *      |           |
 *      |           |
 *   v2/prev------v3/cur
 *      |
 *      V
 *      y
 */

static int compute_mdisp_quad(BMLoop *l, double v1[3], double v2[3], double v3[3], double v4[3],
                              double e1[3], double e2[3])
{
	double cent[3] = {0.0, 0.0, 0.0}, n[3], p[3];
	BMLoop *l_first;
	BMLoop *l_iter;
	
	/* computer center */
	l_iter = l_first = BM_FACE_FIRST_LOOP(l->f);
	do {
		cent[0] += (double)l_iter->v->co[0];
		cent[1] += (double)l_iter->v->co[1];
		cent[2] += (double)l_iter->v->co[2];
	} while ((l_iter = l_iter->next) != l_first);
	
	VECMUL(cent, (1.0 / (double)l->f->len));
	
	VECADD(p, l->prev->v->co, l->v->co);
	VECMUL(p, 0.5);
	VECADD(n, l->next->v->co, l->v->co);
	VECMUL(n, 0.5);
	
	VECCOPY(v1, cent);
	VECCOPY(v2, p);
	VECCOPY(v3, l->v->co);
	VECCOPY(v4, n);
	
	VECSUB(e1, v2, v1);
	VECSUB(e2, v3, v4);
	
	return 1;
}

/* funnily enough, I think this is identical to face_to_crn_interp, heh */
static double quad_coord(double aa[3], double bb[3], double cc[3], double dd[3], int a1, int a2)
{
	double x, y, z, f1;
	
	x = aa[a1] * cc[a2] - cc[a1] * aa[a2];
	y = aa[a1] * dd[a2] + bb[a1] * cc[a2] - cc[a1] * bb[a2] - dd[a1] * aa[a2];
	z = bb[a1] * dd[a2] - dd[a1] * bb[a2];
	
	if (fabs(2 * (x - y + z)) > DBL_EPSILON * 10.0) {
		double f2;

		f1 = (sqrt(y * y - 4.0 * x * z) - y + 2.0 * z) / (2.0 * (x - y + z));
		f2 = (-sqrt(y * y - 4.0 * x * z) - y + 2.0 * z) / (2.0 * (x - y + z));

		f1 = fabs(f1);
		f2 = fabs(f2);
		f1 = MIN2(f1, f2);
		CLAMP(f1, 0.0, 1.0 + DBL_EPSILON);
	}
	else {
		f1 = -z / (y - 2 * z);
		CLAMP(f1, 0.0, 1.0 + DBL_EPSILON);
		
		if (isnan(f1) || f1 > 1.0 || f1 < 0.0) {
			int i;
			
			for (i = 0; i < 2; i++) {
				if (fabsf(aa[i]) < FLT_EPSILON * 100)
					return aa[(i + 1) % 2] / fabs(bb[(i + 1) % 2] - aa[(i + 1) % 2]);
				if (fabsf(cc[i]) < FLT_EPSILON * 100)
					return cc[(i + 1) % 2] / fabs(dd[(i + 1) % 2] - cc[(i + 1) % 2]);
			}
		}
	}

	return f1;
}

static int quad_co(double *x, double *y, double v1[3], double v2[3], double v3[3], double v4[3],
                   double p[3], float n[3])
{
	float projverts[5][3], n2[3];
	double dprojverts[4][3], origin[3] = {0.0f, 0.0f, 0.0f};
	int i;

	/* project points into 2d along normal */
	VECCOPY(projverts[0], v1);
	VECCOPY(projverts[1], v2);
	VECCOPY(projverts[2], v3);
	VECCOPY(projverts[3], v4);
	VECCOPY(projverts[4], p);

	normal_quad_v3(n2, projverts[0], projverts[1], projverts[2], projverts[3]);

	if (INPR(n, n2) < -FLT_EPSILON) {
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
		VECSUB2(projverts[i], projverts[4]);
	}
	
	VECCOPY(dprojverts[0], projverts[0]);
	VECCOPY(dprojverts[1], projverts[1]);
	VECCOPY(dprojverts[2], projverts[2]);
	VECCOPY(dprojverts[3], projverts[3]);

	if (!isect_point_quad_v2_d(origin, dprojverts[0], dprojverts[1], dprojverts[2], dprojverts[3])) {
		return 0;
	}
	
	*y = quad_coord(dprojverts[1], dprojverts[0], dprojverts[2], dprojverts[3], 0, 1);
	*x = quad_coord(dprojverts[2], dprojverts[1], dprojverts[3], dprojverts[0], 0, 1);

	return 1;
}

static void mdisp_axis_from_quad(double v1[3], double v2[3], double UNUSED(v3[3]), double v4[3],
                                float axis_x[3], float axis_y[3])
{
	VECSUB(axis_x, v4, v1);
	VECSUB(axis_y, v2, v1);

	normalize_v3(axis_x);
	normalize_v3(axis_y);
}

/* tl is loop to project onto, l is loop whose internal displacement, co, is being
 * projected.  x and y are location in loop's mdisps grid of point co. */
static int mdisp_in_mdispquad(BMesh *bm, BMLoop *l, BMLoop *tl, double p[3], double *x, double *y,
                              int res, float axis_x[3], float axis_y[3])
{
	double v1[3], v2[3], c[3], v3[3], v4[3], e1[3], e2[3];
	double eps = FLT_EPSILON * 4000;
	
	if (len_v3(l->v->no) == 0.0f)
		BM_vert_normal_update_all(bm, l->v);
	if (len_v3(tl->v->no) == 0.0f)
		BM_vert_normal_update_all(bm, tl->v);

	compute_mdisp_quad(tl, v1, v2, v3, v4, e1, e2);

	/* expand quad a bit */
	cent_quad_v3_d(c, v1, v2, v3, v4);
	
	VECSUB2(v1, c); VECSUB2(v2, c);
	VECSUB2(v3, c); VECSUB2(v4, c);
	VECMUL(v1, 1.0 + eps); VECMUL(v2, 1.0 + eps);
	VECMUL(v3, 1.0 + eps); VECMUL(v4, 1.0 + eps);
	VECADD2(v1, c); VECADD2(v2, c);
	VECADD2(v3, c); VECADD2(v4, c);
	
	if (!quad_co(x, y, v1, v2, v3, v4, p, l->v->no))
		return 0;
	
	*x *= res - 1;
	*y *= res - 1;

	mdisp_axis_from_quad(v1, v2, v3, v4, axis_x, axis_y);

	return 1;
}

static float bmesh_loop_flip_equotion(float mat[2][2], float b[2], float target_axis_x[3], float target_axis_y[3],
                                      float coord[3], int i, int j)
{
	mat[0][0] = target_axis_x[i];
	mat[0][1] = target_axis_y[i];
	mat[1][0] = target_axis_x[j];
	mat[1][1] = target_axis_y[j];
	b[0] = coord[i];
	b[1] = coord[j];

	return mat[0][0]*mat[1][1] - mat[0][1]*mat[1][0];
}

static void bmesh_loop_flip_disp(float source_axis_x[3], float source_axis_y[3],
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

	d = bmesh_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 0, 1);

	if (fabsf(d) < 1e-4) {
		d = bmesh_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 0, 2);
		if (fabsf(d) < 1e-4)
			d = bmesh_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 1, 2);
	}

	disp[0] = (b[0]*mat[1][1] - mat[0][1]*b[1]) / d;
	disp[1] = (mat[0][0]*b[1] - b[0]*mat[1][0]) / d;
}

static void bmesh_loop_interp_mdisps(BMesh *bm, BMLoop *target, BMFace *source)
{
	MDisps *mdisps;
	BMLoop *l_iter;
	BMLoop *l_first;
	double x, y, d, v1[3], v2[3], v3[3], v4[3] = {0.0f, 0.0f, 0.0f}, e1[3], e2[3];
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
	d = 1.0 / (double)(res - 1);
	for (x = 0.0f, ix = 0; ix < res; x += d, ix++) {
		for (y = 0.0f, iy = 0; iy < res; y += d, iy++) {
			double co1[3], co2[3], co[3];
			/* double xx, yy; */ /* UNUSED */
			
			VECCOPY(co1, e1);
			
			/* if (!iy) yy = y + (double)FLT_EPSILON * 20; */
			/* else yy = y - (double)FLT_EPSILON * 20; */
			
			VECMUL(co1, y);
			VECADD2(co1, v1);
			
			VECCOPY(co2, e2);
			VECMUL(co2, y);
			VECADD2(co2, v4);
			
			/* if (!ix) xx = x + (double)FLT_EPSILON * 20; */ /* UNUSED */
			/* else xx = x - (double)FLT_EPSILON * 20; */ /* UNUSED */
			
			VECSUB(co, co2, co1);
			VECMUL(co, x);
			VECADD2(co, co1);
			
			l_iter = l_first = BM_FACE_FIRST_LOOP(source);
			do {
				double x2, y2;
				MDisps *md1, *md2;
				float src_axis_x[3], src_axis_y[3];

				md1 = CustomData_bmesh_get(&bm->ldata, target->head.data, CD_MDISPS);
				md2 = CustomData_bmesh_get(&bm->ldata, l_iter->head.data, CD_MDISPS);
				
				if (mdisp_in_mdispquad(bm, target, l_iter, co, &x2, &y2, res, src_axis_x, src_axis_y)) {
					/* int ix2 = (int)x2; */ /* UNUSED */
					/* int iy2 = (int)y2; */ /* UNUSED */
					
					old_mdisps_bilinear(md1->disps[iy * res + ix], md2->disps, res, (float)x2, (float)y2);
					bmesh_loop_flip_disp(src_axis_x, src_axis_y, axis_x, axis_y, md1->disps[iy * res + ix]);

					break;
				}
			} while ((l_iter = l_iter->next) != l_first);
		}
	}
}

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

#if 0
static void print_loop(BMLoop *loop)
{
	BMLoop *cur = loop;
	do {
		print_v3("\t\tco", cur->v->co);
	} while ((cur = cur->next) != loop);
}
#endif

void BM_loop_interp_multires(BMesh *bm, BMLoop *target, BMFace *source)
{
#if 0
	{
		static int count = 0;
		count++;
		printf("%s: counter=%d\n", __func__, count);
		printf("\ttarget=%p:\n", target);
		print_loop(target);
		printf("\tsource=%p:\n", source);
		print_loop(source->l_first);
		/*if(count!=5) {
			return;
		}*/
	}
#endif

	bmesh_loop_interp_mdisps(bm, target, source);
}

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

	/* scale source face coordinates a bit, so points sitting directonly on an
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
		cos[i][2] = 0.0;
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
			bmesh_loop_interp_mdisps(bm, target, source);
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

	/* scale source face coordinates a bit, so points sitting directonly on an
	 * edge will work. */
	mul_v3_fl(cent, 1.0f / (float)source->len);
	for (i = 0; i < source->len; i++) {
		float vec[3];
		sub_v3_v3v3(vec, cent, cos[i]);
		mul_v3_fl(vec, 0.01);
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

	CustomData_bmesh_init_pool(data, data == &bm->ldata ? 2048 : 512);

	if (data == &bm->vdata) {
		BMVert *eve;
		
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

		BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			block = NULL;
			CustomData_bmesh_set_default(data, &block);
			CustomData_bmesh_copy_data(olddata, data, eed->head.data, &block);
			CustomData_bmesh_free_block(olddata, &eed->head.data);
			eed->head.data = block;
		}
	}
	else if (data == &bm->pdata || data == &bm->ldata) {
		BMIter liter;
		BMFace *efa;
		BMLoop *l;

		BM_ITER(efa, &iter, bm, BM_FACES_OF_MESH, NULL) {
			if (data == &bm->pdata) {
				block = NULL;
				CustomData_bmesh_set_default(data, &block);
				CustomData_bmesh_copy_data(olddata, data, efa->head.data, &block);
				CustomData_bmesh_free_block(olddata, &efa->head.data);
				efa->head.data = block;
			}

			if (data == &bm->ldata) {
				BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, efa) {
					block = NULL;
					CustomData_bmesh_set_default(data, &block);
					CustomData_bmesh_copy_data(olddata, data, l->head.data, &block);
					CustomData_bmesh_free_block(olddata, &l->head.data);
					l->head.data = block;
				}
			}
		}
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
