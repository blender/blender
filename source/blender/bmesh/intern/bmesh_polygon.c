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
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_polygon.c
 *  \ingroup bmesh
 *
 * This file contains code for dealing
 * with polygons (normal/area calculation,
 * tessellation, etc)
 *
 * BMESH_TODO:
 *  - Add in Tessellator frontend that creates
 *    BMTriangles from copied faces
 *
 *  - Add in Function that checks for and flags
 *    degenerate faces.
 */

#include "BLI_math.h"
#include "BLI_array.h"

#include "MEM_guardedalloc.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/**
 * \brief TEST EDGE SIDE and POINT IN TRIANGLE
 *
 * Point in triangle tests stolen from scanfill.c.
 * Used for tessellator
 */

static short testedgesidef(const float v1[2], const float v2[2], const float v3[2])
{
	/* is v3 to the right of v1 - v2 ? With exception: v3 == v1 || v3 == v2 */
	double inp;

	//inp = (v2[cox] - v1[cox]) * (v1[coy] - v3[coy]) + (v1[coy] - v2[coy]) * (v1[cox] - v3[cox]);
	inp = (v2[0] - v1[0]) * (v1[1] - v3[1]) + (v1[1] - v2[1]) * (v1[0] - v3[0]);

	if (inp < 0.0) {
		return FALSE;
	}
	else if (inp == 0) {
		if (v1[0] == v3[0] && v1[1] == v3[1]) return FALSE;
		if (v2[0] == v3[0] && v2[1] == v3[1]) return FALSE;
	}
	return TRUE;
}

/**
 * \brief COMPUTE POLY NORMAL
 *
 * Computes the normal of a planar
 * polygon See Graphics Gems for
 * computing newell normal.
 */
static void compute_poly_normal(float normal[3], float (*verts)[3], int nverts)
{

	float u[3], v[3], w[3]; /*, *w, v1[3], v2[3]; */
	float n[3] = {0.0f, 0.0f, 0.0f} /*, l, v1[3], v2[3] */;
	/* double l2; */
	int i /*, s = 0 */;

	/* this fixes some weird numerical erro */
	add_v3_fl(verts[0], 0.0001f);

	for (i = 0; i < nverts; i++) {
		copy_v3_v3(u, verts[i]);
		copy_v3_v3(v, verts[(i + 1) % nverts]);
		copy_v3_v3(w, verts[(i + 2) % nverts]);
		
#if 0
		sub_v3_v3v3(v1, w, v);
		sub_v3_v3v3(v2, v, u);
		normalize_v3(v1);
		normalize_v3(v2);

		l = dot_v3v3(v1, v2);
		if (fabsf(l - 1.0) < 0.1f) {
			continue;
		}
#endif
		/* newell's method

		so thats?:
		(a[1] - b[1]) * (a[2] + b[2]);
		a[1] * b[2] - b[1] * a[2] - b[1] * b[2] + a[1] * a[2]

		odd.  half of that is the cross product. . .what's the
		other half?

		also could be like a[1] * (b[2] + a[2]) - b[1] * (a[2] - b[2])
		*/

		n[0] += (u[1] - v[1]) * (u[2] + v[2]);
		n[1] += (u[2] - v[2]) * (u[0] + v[0]);
		n[2] += (u[0] - v[0]) * (u[1] + v[1]);
	}

	if (normalize_v3_v3(normal, n) == 0.0f) {
		normal[2] = 1.0f; /* other axis set to 0.0 */
	}

#if 0
	l = len_v3(n);
	/* fast square root, newton/babylonian method:
	l2 = l * 0.1;

	l2 = (l / l2 + l2) * 0.5;
	l2 = (l / l2 + l2) * 0.5;
	l2 = (l / l2 + l2) * 0.5;
	*/

	if (l == 0.0) {
		normal[0] = 0.0f;
		normal[1] = 0.0f;
		normal[2] = 1.0f;

		return;
	}
	else {
		l = 1.0f / l;
	}

	mul_v3_fl(n, l);

	copy_v3_v3(normal, n);
#endif
}

/**
 * \brief COMPUTE POLY CENTER
 *
 * Computes the centroid and
 * area of a polygon in the X/Y
 * plane.
 */
static int compute_poly_center(float center[3], float *r_area, float (*verts)[3], int nverts)
{
	int i, j;
	float atmp = 0.0f, xtmp = 0.0f, ytmp = 0.0f, ai;

	zero_v3(center);

	if (nverts < 3)
		return FALSE;

	i = nverts - 1;
	j = 0;
	
	while (j < nverts) {
		ai = verts[i][0] * verts[j][1] - verts[j][0] * verts[i][1];
		atmp += ai;
		xtmp += (verts[j][0] + verts[i][0]) * ai;
		ytmp += (verts[j][1] + verts[i][1]) * ai;
		i = j;
		j += 1;
	}

	if (r_area)
		*r_area = atmp / 2.0f;
	
	if (atmp != 0) {
		center[0] = xtmp /  (3.0f * atmp);
		center[1] = xtmp /  (3.0f * atmp);
		return TRUE;
	}
	return FALSE;
}

/**
 * get the area of the face
 */
float BM_face_area_calc(BMesh *bm, BMFace *f)
{
	BMLoop *l;
	BMIter iter;
	float (*verts)[3];
	float center[3];
	float area = 0.0f;
	int i;

	BLI_array_fixedstack_declare(verts, BM_NGON_STACK_SIZE, f->len, __func__);

	i = 0;
	BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
		copy_v3_v3(verts[i], l->v->co);
		i++;
	}

	compute_poly_center(center, &area, verts, f->len);

	BLI_array_fixedstack_free(verts);

	return area;
}

/**
 * computes center of face in 3d.  uses center of bounding box.
 */
void BM_face_center_bounds_calc(BMesh *UNUSED(bm), BMFace *f, float r_cent[3])
{
	BMLoop *l_iter;
	BMLoop *l_first;
	float min[3], max[3];

	INIT_MINMAX(min, max);

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		DO_MINMAX(l_iter->v->co, min, max);
	} while ((l_iter = l_iter->next) != l_first);

	mid_v3_v3v3(r_cent, min, max);
}

/**
 * computes the center of a face, using the mean average
 */
void BM_face_center_mean_calc(BMesh *UNUSED(bm), BMFace *f, float r_cent[3])
{
	BMLoop *l_iter;
	BMLoop *l_first;

	zero_v3(r_cent);

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		add_v3_v3(r_cent, l_iter->v->co);
	} while ((l_iter = l_iter->next) != l_first);

	if (f->len)
		mul_v3_fl(r_cent, 1.0f / (float) f->len);
}

/**
 * COMPUTE POLY PLANE
 *
 * Projects a set polygon's vertices to
 * a plane defined by the average
 * of its edges cross products
 */
void compute_poly_plane(float (*verts)[3], int nverts)
{
	
	float avgc[3], norm[3], mag, avgn[3];
	float *v1, *v2, *v3;
	int i;
	
	if (nverts < 3)
		return;

	zero_v3(avgn);
	zero_v3(avgc);

	for (i = 0; i < nverts; i++) {
		v1 = verts[i];
		v2 = verts[(i + 1) % nverts];
		v3 = verts[(i + 2) % nverts];
		normal_tri_v3(norm, v1, v2, v3);

		add_v3_v3(avgn, norm);
	}

	/* what was this bit for */
	if (is_zero_v3(avgn)) {
		avgn[0] = 0.0f;
		avgn[1] = 0.0f;
		avgn[2] = 1.0f;
	}
	else {
		normalize_v3(avgn);
	}
	
	for (i = 0; i < nverts; i++) {
		v1 = verts[i];
		mag = dot_v3v3(v1, avgn);
		madd_v3_v3fl(v1, avgn, -mag);
	}
}

/**
 * \brief BM LEGAL EDGES
 *
 * takes in a face and a list of edges, and sets to NULL any edge in
 * the list that bridges a concave region of the face or intersects
 * any of the faces's edges.
 */
static void shrink_edgef(float v1[3], float v2[3], const float fac)
{
	float mid[3];

	mid_v3_v3v3(mid, v1, v2);

	sub_v3_v3v3(v1, v1, mid);
	sub_v3_v3v3(v2, v2, mid);

	mul_v3_fl(v1, fac);
	mul_v3_fl(v2, fac);

	add_v3_v3v3(v1, v1, mid);
	add_v3_v3v3(v2, v2, mid);
}


/**
 * \brief POLY ROTATE PLANE
 *
 * Rotates a polygon so that it's
 * normal is pointing towards the mesh Z axis
 */
void poly_rotate_plane(const float normal[3], float (*verts)[3], const int nverts)
{

	float up[3] = {0.0f, 0.0f, 1.0f}, axis[3], q[4];
	float mat[3][3];
	double angle;
	int i;

	cross_v3_v3v3(axis, normal, up);

	angle = saacos(dot_v3v3(normal, up));

	if (angle == 0.0) return;

	axis_angle_to_quat(q, axis, (float)angle);
	quat_to_mat3(mat, q);

	for (i = 0;  i < nverts;  i++)
		mul_m3_v3(mat, verts[i]);
}

/**
 * \brief BMESH UPDATE FACE NORMAL
 *
 * Updates the stored normal for the
 * given face. Requires that a buffer
 * of sufficient length to store projected
 * coordinates for all of the face's vertices
 * is passed in as well.
 */
void BM_face_normal_update(BMesh *bm, BMFace *f)
{
	if (f->len >= 3) {
		float (*proj)[3];

		BLI_array_fixedstack_declare(proj, BM_NGON_STACK_SIZE, f->len, __func__);

		bmesh_face_normal_update(bm, f, f->no, proj);

		BLI_array_fixedstack_free(proj);
	}
}
/* same as BM_face_normal_update but takes vertex coords */
void BM_face_normal_update_vcos(BMesh *bm, BMFace *f, float no[3], float (*vertexCos)[3])
{
	if (f->len >= 3) {
		float (*proj)[3];

		BLI_array_fixedstack_declare(proj, BM_NGON_STACK_SIZE, f->len, __func__);

		bmesh_face_normal_update_vertex_cos(bm, f, no, proj, vertexCos);

		BLI_array_fixedstack_free(proj);
	}
}

/**
 * updates face and vertex normals incident on an edge
 */
void BM_edge_normals_update(BMesh *bm, BMEdge *e)
{
	BMIter iter;
	BMFace *f;
	
	f = BM_iter_new(&iter, bm, BM_FACES_OF_EDGE, e);
	for ( ; f; f = BM_iter_step(&iter)) {
		BM_face_normal_update(bm, f);
	}

	BM_vert_normal_update(bm, e->v1);
	BM_vert_normal_update(bm, e->v2);
}

/**
 * update a vert normal (but not the faces incident on it)
 */
void BM_vert_normal_update(BMesh *bm, BMVert *v)
{
	/* TODO, we can normalize each edge only once, then compare with previous edge */

	BMIter liter;
	BMLoop *l;
	float vec1[3], vec2[3], fac;
	int len = 0;

	zero_v3(v->no);

	BM_ITER(l, &liter, bm, BM_LOOPS_OF_VERT, v) {
		/* Same calculation used in BM_mesh_normals_update */
		sub_v3_v3v3(vec1, l->v->co, l->prev->v->co);
		sub_v3_v3v3(vec2, l->next->v->co, l->v->co);
		normalize_v3(vec1);
		normalize_v3(vec2);

		fac = saacos(-dot_v3v3(vec1, vec2));

		madd_v3_v3fl(v->no, l->f->no, fac);

		len++;
	}

	if (len) {
		normalize_v3(v->no);
	}
}

void BM_vert_normal_update_all(BMesh *bm, BMVert *v)
{
	BMIter iter;
	BMFace *f;

	BM_ITER(f, &iter, bm, BM_FACES_OF_VERT, v) {
		BM_face_normal_update(bm, f);
	}

	BM_vert_normal_update(bm, v);
}

void bmesh_face_normal_update(BMesh *bm, BMFace *f, float no[3],
                              float (*projectverts)[3])
{
	BMLoop *l;

	/* common cases first */
	switch (f->len) {
		case 4:
		{
			BMVert *v1 = (l = BM_FACE_FIRST_LOOP(f))->v;
			BMVert *v2 = (l = l->next)->v;
			BMVert *v3 = (l = l->next)->v;
			BMVert *v4 = (l->next)->v;
			normal_quad_v3(no, v1->co, v2->co, v3->co, v4->co);
			break;
		}
		case 3:
		{
			BMVert *v1 = (l = BM_FACE_FIRST_LOOP(f))->v;
			BMVert *v2 = (l = l->next)->v;
			BMVert *v3 = (l->next)->v;
			normal_tri_v3(no, v1->co, v2->co, v3->co);
			break;
		}
		case 0:
		{
			zero_v3(no);
			break;
		}
		default:
		{
			BMIter iter;
			int i = 0;
			BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
				copy_v3_v3(projectverts[i], l->v->co);
				i += 1;
			}
			compute_poly_normal(no, projectverts, f->len);
			break;
		}
	}
}
/* exact same as 'bmesh_face_normal_update' but accepts vertex coords */
void bmesh_face_normal_update_vertex_cos(BMesh *bm, BMFace *f, float no[3],
                                         float (*projectverts)[3], float (*vertexCos)[3])
{
	BMLoop *l;

	/* must have valid index data */
	BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);

	/* common cases first */
	switch (f->len) {
		case 4:
		{
			BMVert *v1 = (l = BM_FACE_FIRST_LOOP(f))->v;
			BMVert *v2 = (l = l->next)->v;
			BMVert *v3 = (l = l->next)->v;
			BMVert *v4 = (l->next)->v;
			normal_quad_v3(no,
			               vertexCos[BM_elem_index_get(v1)],
			               vertexCos[BM_elem_index_get(v2)],
			               vertexCos[BM_elem_index_get(v3)],
			               vertexCos[BM_elem_index_get(v4)]);
			break;
		}
		case 3:
		{
			BMVert *v1 = (l = BM_FACE_FIRST_LOOP(f))->v;
			BMVert *v2 = (l = l->next)->v;
			BMVert *v3 = (l->next)->v;
			normal_tri_v3(no,
			              vertexCos[BM_elem_index_get(v1)],
			              vertexCos[BM_elem_index_get(v2)],
			              vertexCos[BM_elem_index_get(v3)]);
			break;
		}
		case 0:
		{
			zero_v3(no);
			break;
		}
		default:
		{
			BMIter iter;
			int i = 0;
			BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
				copy_v3_v3(projectverts[i], vertexCos[BM_elem_index_get(l->v)]);
				i += 1;
			}
			compute_poly_normal(no, projectverts, f->len);
			break;
		}
	}
}

/**
 * \brief Face Flip Normal
 *
 * Reverses the winding of a face.
 * \note This updates the calculated normal.
 */
void BM_face_normal_flip(BMesh *bm, BMFace *f)
{
	bmesh_loop_reverse(bm, f);
	negate_v3(f->no);
}

/* detects if two line segments cross each other (intersects).
 * note, there could be more winding cases then there needs to be. */
static int linecrossesf(const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
	int w1, w2, w3, w4, w5 /*, re */;
	float mv1[2], mv2[2], mv3[2], mv4[2];
	
	/* now test windin */
	w1 = testedgesidef(v1, v3, v2);
	w2 = testedgesidef(v2, v4, v1);
	w3 = !testedgesidef(v1, v2, v3);
	w4 = testedgesidef(v3, v2, v4);
	w5 = !testedgesidef(v3, v1, v4);
	
	if (w1 == w2 && w2 == w3 && w3 == w4 && w4 == w5) {
		return TRUE;
	}
	
#define GETMIN2_AXIS(a, b, ma, mb, axis) ma[axis] = MIN2(a[axis], b[axis]), mb[axis] = MAX2(a[axis], b[axis])
#define GETMIN2(a, b, ma, mb) GETMIN2_AXIS(a, b, ma, mb, 0); GETMIN2_AXIS(a, b, ma, mb, 1);
	
	GETMIN2(v1, v2, mv1, mv2);
	GETMIN2(v3, v4, mv3, mv4);
	
	/* do an interval test on the x and y axe */
	/* first do x axi */
#define T FLT_EPSILON * 15
	if ( ABS(v1[1] - v2[1]) < T &&
	     ABS(v3[1] - v4[1]) < T &&
	     ABS(v1[1] - v3[1]) < T)
	{
		return (mv4[0] >= mv1[0] && mv3[0] <= mv2[0]);
	}

	/* now do y axi */
	if ( ABS(v1[0] - v2[0]) < T &&
	     ABS(v3[0] - v4[0]) < T &&
	     ABS(v1[0] - v3[0]) < T)
	{
		return (mv4[1] >= mv1[1] && mv3[1] <= mv2[1]);
	}

	return FALSE;
}

/**
 *  BM POINT IN FACE
 *
 * Projects co onto face f, and returns true if it is inside
 * the face bounds.
 *
 * \note this uses a best-axis projection test,
 * instead of projecting co directly into f's orientation space,
 * so there might be accuracy issues.
 */
int BM_face_point_inside_test(BMesh *bm, BMFace *f, const float co[3])
{
	int ax, ay;
	float co2[2], cent[2] = {0.0f, 0.0f}, out[2] = {FLT_MAX * 0.5f, FLT_MAX * 0.5f};
	BMLoop *l_iter;
	BMLoop *l_first;
	int crosses = 0;
	float onepluseps = 1.0f + (float)FLT_EPSILON * 150.0f;
	
	if (dot_v3v3(f->no, f->no) <= FLT_EPSILON * 10)
		BM_face_normal_update(bm, f);
	
	/* find best projection of face XY, XZ or YZ: barycentric weights of
	 * the 2d projected coords are the same and faster to compute
	 *
	 * this probably isn't all that accurate, but it has the advantage of
	 * being fast (especially compared to projecting into the face orientation)
	 */
	axis_dominant_v3(&ax, &ay, f->no);

	co2[0] = co[ax];
	co2[1] = co[ay];
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		cent[0] += l_iter->v->co[ax];
		cent[1] += l_iter->v->co[ay];
	} while ((l_iter = l_iter->next) != l_first);
	
	mul_v2_fl(cent, 1.0f / (float)f->len);
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		float v1[2], v2[2];
		
		v1[0] = (l_iter->prev->v->co[ax] - cent[ax]) * onepluseps + cent[ax];
		v1[1] = (l_iter->prev->v->co[ay] - cent[ay]) * onepluseps + cent[ay];
		
		v2[0] = (l_iter->v->co[ax] - cent[ax]) * onepluseps + cent[ax];
		v2[1] = (l_iter->v->co[ay] - cent[ay]) * onepluseps + cent[ay];
		
		crosses += linecrossesf(v1, v2, co2, out) != 0;
	} while ((l_iter = l_iter->next) != l_first);
	
	return crosses % 2 != 0;
}

static int goodline(float (*projectverts)[3], BMFace *f, int v1i,
                    int v2i, int v3i, int UNUSED(nvert))
{
	BMLoop *l_iter;
	BMLoop *l_first;
	float v1[3], v2[3], v3[3], pv1[3], pv2[3];
	int i;

	copy_v3_v3(v1, projectverts[v1i]);
	copy_v3_v3(v2, projectverts[v2i]);
	copy_v3_v3(v3, projectverts[v3i]);
	
	if (testedgesidef(v1, v2, v3)) {
		return FALSE;
	}

	//for (i = 0; i < nvert; i++) {
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		i = BM_elem_index_get(l_iter->v);
		if (i == v1i || i == v2i || i == v3i) {
			continue;
		}
		
		copy_v3_v3(pv1, projectverts[BM_elem_index_get(l_iter->v)]);
		copy_v3_v3(pv2, projectverts[BM_elem_index_get(l_iter->next->v)]);
		
		//if (linecrossesf(pv1, pv2, v1, v3)) return FALSE;

		if (isect_point_tri_v2(pv1, v1, v2, v3) ||
		    isect_point_tri_v2(pv1, v3, v2, v1))
		{
			return FALSE;
		}
	} while ((l_iter = l_iter->next) != l_first);
	return TRUE;
}

/**
 * \brief Find Ear
 *
 * Used by tessellator to find
 * the next triangle to 'clip off'
 * of a polygon while tessellating.
 *
 * \param use_beauty Currently only applies to quads, can be extended later on.
 */
static BMLoop *find_ear(BMesh *UNUSED(bm), BMFace *f, float (*verts)[3], const int nvert, const int use_beauty)
{
	BMLoop *bestear = NULL;

	BMLoop *l_iter;
	BMLoop *l_first;

	if (f->len == 4) {
		BMLoop *larr[4];
		int i = 0;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			larr[i] = l_iter;
			i++;
		} while ((l_iter = l_iter->next) != l_first);

		/* pick 0/1 based on best lenth */
		bestear = larr[(((len_squared_v3v3(larr[0]->v->co, larr[2]->v->co) >
		                  len_squared_v3v3(larr[1]->v->co, larr[3]->v->co))) != use_beauty)];

	}
	else {
		BMVert *v1, *v2, *v3;

		/* float angle, bestangle = 180.0f; */
		int isear /*, i = 0 */;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			isear = 1;

			v1 = l_iter->prev->v;
			v2 = l_iter->v;
			v3 = l_iter->next->v;

			if (BM_edge_exists(v1, v3)) {
				isear = 0;
			}
			else if (!goodline(verts, f, BM_elem_index_get(v1), BM_elem_index_get(v2), BM_elem_index_get(v3), nvert)) {
				isear = 0;
			}

			if (isear) {
	#if 0
				/* if this code comes back, it needs to be converted to radians */
				angle = angle_v3v3v3(verts[v1->head.eflag2], verts[v2->head.eflag2], verts[v3->head.eflag2]);
				if (!bestear || ABS(angle - 45.0f) < bestangle) {
					bestear = l;
					bestangle = ABS(45.0f - angle);
				}

				if (angle > 20 && angle < 90) break;
				if (angle < 100 && i > 5) break;
				i += 1;
	#endif

				bestear = l_iter;
				break;
			}
		} while ((l_iter = l_iter->next) != l_first);
	}

	return bestear;
}

/**
 * \brief BMESH TRIANGULATE FACE
 *
 * Triangulates a face using a simple 'ear clipping' algorithm that tries to
 * favor non-skinny triangles (angles less than 90 degrees).
 *
 * If the triangulator has bits left over (or cannot triangulate at all)
 * it uses a simple fan triangulation,
 *
 * newfaces, if non-null, must be an array of BMFace pointers,
 * with a length equal to f->len.  it will be filled with the new
 * triangles, and will be NULL-terminated.
 *
 * \note newedgeflag sets a flag layer flag, obviously not the header flag.
 */
void BM_face_triangulate(BMesh *bm, BMFace *f, float (*projectverts)[3],
                         const short newedge_oflag, const short newface_oflag, BMFace **newfaces,
                         const short use_beauty)
{
	int i, done, nvert, nf_i = 0;
	BMLoop *newl, *nextloop;
	BMLoop *l_iter;
	BMLoop *l_first;
	/* BMVert *v; */ /* UNUSED */

	/* copy vertex coordinates to vertspace arra */
	i = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		copy_v3_v3(projectverts[i], l_iter->v->co);
		BM_elem_index_set(l_iter->v, i); /* set dirty! */
		i++;
	} while ((l_iter = l_iter->next) != l_first);

	bm->elem_index_dirty |= BM_VERT; /* see above */

	///bmesh_face_normal_update(bm, f, f->no, projectverts);

	compute_poly_normal(f->no, projectverts, f->len);
	poly_rotate_plane(f->no, projectverts, i);

	nvert = f->len;

	//compute_poly_plane(projectverts, i);
	for (i = 0; i < nvert; i++) {
		projectverts[i][2] = 0.0f;
	}

	done = 0;
	while (!done && f->len > 3) {
		done = 1;
		l_iter = find_ear(bm, f, projectverts, nvert, use_beauty);
		if (l_iter) {
			done = 0;
			/* v = l->v; */ /* UNUSED */
			f = BM_face_split(bm, l_iter->f, l_iter->prev->v,
			                  l_iter->next->v,
			                  &newl, NULL, TRUE);

			if (UNLIKELY(!f)) {
				fprintf(stderr, "%s: triangulator failed to split face! (bmesh internal error)\n", __func__);
				break;
			}

			copy_v3_v3(f->no, l_iter->f->no);
			BMO_elem_flag_enable(bm, newl->e, newedge_oflag);
			BMO_elem_flag_enable(bm, f, newface_oflag);
			
			if (newfaces) newfaces[nf_i++] = f;

#if 0
			l = f->loopbase;
			do {
				if (l->v == v) {
					f->loopbase = l;
					break;
				}
				l = l->next;
			} while (l != f->loopbase);
#endif

		}
	}

	if (f->len > 3) {
		l_iter = BM_FACE_FIRST_LOOP(f);
		while (l_iter->f->len > 3) {
			nextloop = l_iter->next->next;
			f = BM_face_split(bm, l_iter->f, l_iter->v, nextloop->v,
			                  &newl, NULL, TRUE);
			if (!f) {
				printf("triangle fan step of triangulator failed.\n");

				/* NULL-terminate */
				if (newfaces) newfaces[nf_i] = NULL;
				return;
			}

			if (newfaces) newfaces[nf_i++] = f;
			
			BMO_elem_flag_enable(bm, newl->e, newedge_oflag);
			BMO_elem_flag_enable(bm, f, newface_oflag);
			l_iter = nextloop;
		}
	}
	
	/* NULL-terminate */
	if (newfaces) newfaces[nf_i] = NULL;
}

/**
 * each pair of loops defines a new edge, a split.  this function goes
 * through and sets pairs that are geometrically invalid to null.  a
 * split is invalid, if it forms a concave angle or it intersects other
 * edges in the face, or it intersects another split.  in the case of
 * intersecting splits, only the first of the set of intersecting
 * splits survives
 */
void BM_face_legal_splits(BMesh *bm, BMFace *f, BMLoop *(*loops)[2], int len)
{
	BMIter iter;
	BMLoop *l;
	float v1[3], v2[3], v3[3]/*, v4[3 */, no[3], mid[3], *p1, *p2, *p3, *p4;
	float out[3] = {-234324.0f, -234324.0f, 0.0f};
	float (*projverts)[3];
	float (*edgeverts)[3];
	float fac1 = 1.0000001f, fac2 = 0.9f; //9999f; //0.999f;
	int i, j, a = 0, clen;

	BLI_array_fixedstack_declare(projverts, BM_NGON_STACK_SIZE, f->len,      "projvertsb");
	BLI_array_fixedstack_declare(edgeverts, BM_NGON_STACK_SIZE * 2, len * 2, "edgevertsb");
	
	i = 0;
	l = BM_iter_new(&iter, bm, BM_LOOPS_OF_FACE, f);
	for ( ; l; l = BM_iter_step(&iter)) {
		BM_elem_index_set(l, i); /* set_loop */
		copy_v3_v3(projverts[i], l->v->co);
		i++;
	}
	
	for (i = 0; i < len; i++) {
		copy_v3_v3(v1, loops[i][0]->v->co);
		copy_v3_v3(v2, loops[i][1]->v->co);

		shrink_edgef(v1, v2, fac2);
		
		copy_v3_v3(edgeverts[a], v1);
		a++;
		copy_v3_v3(edgeverts[a], v2);
		a++;
	}
	
	compute_poly_normal(no, projverts, f->len);
	poly_rotate_plane(no, projverts, f->len);
	poly_rotate_plane(no, edgeverts, len * 2);

	for (i = 0, l = BM_FACE_FIRST_LOOP(f); i < f->len; i++, l = l->next) {
		p1 = projverts[i];
		out[0] = MAX2(out[0], p1[0]) + 0.01f;
		out[1] = MAX2(out[1], p1[1]) + 0.01f;
		out[2] = 0.0f;
		p1[2] = 0.0f;

		//copy_v3_v3(l->v->co, p1);
	}
	
	for (i = 0; i < len; i++) {
		edgeverts[i * 2][2] = 0.0f;
		edgeverts[i * 2 + 1][2] = 0.0f;
	}

	/* do convexity test */
	for (i = 0; i < len; i++) {
		copy_v3_v3(v2, edgeverts[i * 2]);
		copy_v3_v3(v3, edgeverts[i * 2 + 1]);

		mid_v3_v3v3(mid, v2, v3);
		
		clen = 0;
		for (j = 0; j < f->len; j++) {
			p1 = projverts[j];
			p2 = projverts[(j + 1) % f->len];
			
			copy_v3_v3(v1, p1);
			copy_v3_v3(v2, p2);

			shrink_edgef(v1, v2, fac1);

			if (linecrossesf(p1, p2, mid, out)) clen++;
		}
		
		if (clen % 2 == 0) {
			loops[i][0] = NULL;
		}
	}
	
	/* do line crossing test */
	for (i = 0; i < f->len; i++) {
		p1 = projverts[i];
		p2 = projverts[(i + 1) % f->len];
		
		copy_v3_v3(v1, p1);
		copy_v3_v3(v2, p2);

		shrink_edgef(v1, v2, fac1);

		for (j = 0; j < len; j++) {
			if (!loops[j][0]) {
				continue;
			}

			p3 = edgeverts[j * 2];
			p4 = edgeverts[j * 2 + 1];

			if (linecrossesf(v1, v2, p3, p4)) {
				loops[j][0] = NULL;
			}
		}
	}

	for (i = 0; i < len; i++) {
		for (j = 0; j < len; j++) {
			if (j != i && loops[i][0] && loops[j][0]) {
				p1 = edgeverts[i * 2];
				p2 = edgeverts[i * 2 + 1];
				p3 = edgeverts[j * 2];
				p4 = edgeverts[j * 2 + 1];

				copy_v3_v3(v1, p1);
				copy_v3_v3(v2, p2);

				shrink_edgef(v1, v2, fac1);

				if (linecrossesf(v1, v2, p3, p4)) {
					loops[i][0] = NULL;
				}
			}
		}
	}

	BLI_array_fixedstack_free(projverts);
	BLI_array_fixedstack_free(edgeverts);
}
