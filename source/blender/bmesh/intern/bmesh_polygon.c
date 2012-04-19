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
static void compute_poly_normal(float normal[3], float verts[][3], int nverts)
{
	float const *v_prev = verts[nverts - 1];
	float const *v_curr = verts[0];
	float n[3] = {0.0f};
	int i;

	/* Newell's Method */
	for (i = 0; i < nverts; v_prev = v_curr, v_curr = verts[++i]) {
		add_newell_cross_v3_v3v3(n, v_prev, v_curr);
	}

	if (UNLIKELY(normalize_v3_v3(normal, n) == 0.0f)) {
		normal[2] = 1.0f; /* other axis set to 0.0 */
	}
}

/**
 * \brief COMPUTE POLY NORMAL (BMFace)
 *
 * Same as #compute_poly_normal but operates directly on a bmesh face.
 */
static void bm_face_compute_poly_normal(BMFace *f)
{
	BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
	BMLoop *l_iter  = l_first;
	float const *v_prev = l_first->prev->v->co;
	float const *v_curr = l_first->v->co;
	float n[3] = {0.0f};

	/* Newell's Method */
	do {
		add_newell_cross_v3_v3v3(n, v_prev, v_curr);

		l_iter = l_iter->next;
		v_prev = v_curr;
		v_curr = l_iter->v->co;

	} while (l_iter != l_first);

	if (UNLIKELY(normalize_v3_v3(f->no, n) == 0.0f)) {
		f->no[2] = 1.0f; /* other axis set to 0.0 */
	}
}

/**
 * \brief COMPUTE POLY NORMAL (BMFace)
 *
 * Same as #compute_poly_normal and #bm_face_compute_poly_normal
 * but takes an array of vertex locations.
 */
static void bm_face_compute_poly_normal_vertex_cos(BMFace *f, float n[3],
                                                   float const (*vertexCos)[3])
{
	BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
	BMLoop *l_iter  = l_first;
	float const *v_prev = vertexCos[BM_elem_index_get(l_first->prev->v)];
	float const *v_curr = vertexCos[BM_elem_index_get(l_first->v)];

	zero_v3(n);

	/* Newell's Method */
	do {
		add_newell_cross_v3_v3v3(n, v_prev, v_curr);

		l_iter = l_iter->next;
		v_prev = v_curr;
		v_curr = vertexCos[BM_elem_index_get(l_iter->v)];
	} while (l_iter != l_first);

	if (UNLIKELY(normalize_v3(n) == 0.0f)) {
		n[2] = 1.0f; /* other axis set to 0.0 */
	}
}

/**
 * get the area of the face
 */
float BM_face_area_calc(BMFace *f)
{
	BMLoop *l;
	BMIter iter;
	float (*verts)[3];
	float normal[3];
	float area;
	int i;

	BLI_array_fixedstack_declare(verts, BM_NGON_STACK_SIZE, f->len, __func__);

	BM_ITER_ELEM_INDEX (l, &iter, f, BM_LOOPS_OF_FACE, i) {
		copy_v3_v3(verts[i], l->v->co);
	}

	if (f->len == 3) {
		area = area_tri_v3(verts[0], verts[1], verts[2]);
	}
	else if (f->len == 4) {
		area = area_quad_v3(verts[0], verts[1], verts[2], verts[3]);
	}
	else {
		compute_poly_normal(normal, verts, f->len);
		area = area_poly_v3(f->len, verts, normal);
	}

	BLI_array_fixedstack_free(verts);

	return area;
}

/**
 * compute the perimeter of an ngon
 */
float BM_face_perimeter_calc(BMFace *f)
{
	BMLoop *l_iter, *l_first;
	float perimeter = 0.0f;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		perimeter += len_v3v3(l_iter->v->co, l_iter->next->v->co);
	} while ((l_iter = l_iter->next) != l_first);

	return perimeter;
}

/**
 * computes center of face in 3d.  uses center of bounding box.
 */
void BM_face_center_bounds_calc(BMFace *f, float r_cent[3])
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
void BM_face_center_mean_calc(BMFace *f, float r_cent[3])
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
void compute_poly_plane(float (*verts)[3], const int nverts)
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

	if (UNLIKELY(normalize_v3(avgn) == 0.0f)) {
		avgn[2] = 1.0f;
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

	for (i = 0; i < nverts; i++)
		mul_m3_v3(mat, verts[i]);
}

/**
 * updates face and vertex normals incident on an edge
 */
void BM_edge_normals_update(BMEdge *e)
{
	BMIter iter;
	BMFace *f;
	
	BM_ITER_ELEM (f, &iter, e, BM_FACES_OF_EDGE) {
		BM_face_normal_update(f);
	}

	BM_vert_normal_update(e->v1);
	BM_vert_normal_update(e->v2);
}

/**
 * update a vert normal (but not the faces incident on it)
 */
void BM_vert_normal_update(BMVert *v)
{
	/* TODO, we can normalize each edge only once, then compare with previous edge */

	BMIter liter;
	BMLoop *l;
	float vec1[3], vec2[3], fac;
	int len = 0;

	zero_v3(v->no);

	BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
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

void BM_vert_normal_update_all(BMVert *v)
{
	BMIter iter;
	BMFace *f;

	BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
		BM_face_normal_update(f);
	}

	BM_vert_normal_update(v);
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

void BM_face_normal_update(BMFace *f)
{
	BMLoop *l;

	/* common cases first */
	switch (f->len) {
		case 4:
		{
			const float *co1 = (l = BM_FACE_FIRST_LOOP(f))->v->co;
			const float *co2 = (l = l->next)->v->co;
			const float *co3 = (l = l->next)->v->co;
			const float *co4 = (l->next)->v->co;

			normal_quad_v3(f->no, co1, co2, co3, co4);
			break;
		}
		case 3:
		{
			const float *co1 = (l = BM_FACE_FIRST_LOOP(f))->v->co;
			const float *co2 = (l = l->next)->v->co;
			const float *co3 = (l->next)->v->co;

			normal_tri_v3(f->no, co1, co2, co3);
			break;
		}
		case 0:
		{
			zero_v3(f->no);
			break;
		}
		default:
		{
			bm_face_compute_poly_normal(f);
			break;
		}
	}
}
/* exact same as 'bmesh_face_normal_update' but accepts vertex coords */
void BM_face_normal_update_vcos(BMesh *bm, BMFace *f, float no[3],
                                float const (*vertexCos)[3])
{
	BMLoop *l;

	/* must have valid index data */
	BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);

	/* common cases first */
	switch (f->len) {
		case 4:
		{
			const float *co1 = vertexCos[BM_elem_index_get((l = BM_FACE_FIRST_LOOP(f))->v)];
			const float *co2 = vertexCos[BM_elem_index_get((l = l->next)->v)];
			const float *co3 = vertexCos[BM_elem_index_get((l = l->next)->v)];
			const float *co4 = vertexCos[BM_elem_index_get((l->next)->v)];

			normal_quad_v3(no, co1, co2, co3, co4);
			break;
		}
		case 3:
		{
			const float *co1 = vertexCos[BM_elem_index_get((l = BM_FACE_FIRST_LOOP(f))->v)];
			const float *co2 = vertexCos[BM_elem_index_get((l = l->next)->v)];
			const float *co3 = vertexCos[BM_elem_index_get((l->next)->v)];

			normal_tri_v3(no, co1, co2, co3);
			break;
		}
		case 0:
		{
			zero_v3(no);
			break;
		}
		default:
		{
			bm_face_compute_poly_normal_vertex_cos(f, no, vertexCos);
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

#define GETMIN2_AXIS(a, b, ma, mb, axis)   \
	{                                      \
		ma[axis] = MIN2(a[axis], b[axis]); \
		mb[axis] = MAX2(a[axis], b[axis]); \
	} (void)0

#define GETMIN2(a, b, ma, mb)          \
	{                                  \
		GETMIN2_AXIS(a, b, ma, mb, 0); \
		GETMIN2_AXIS(a, b, ma, mb, 1); \
	} (void)0

#define EPS (FLT_EPSILON * 15)

	int w1, w2, w3, w4, w5 /*, re */;
	float mv1[2], mv2[2], mv3[2], mv4[2];
	
	/* now test winding */
	w1 = testedgesidef(v1, v3, v2);
	w2 = testedgesidef(v2, v4, v1);
	w3 = !testedgesidef(v1, v2, v3);
	w4 = testedgesidef(v3, v2, v4);
	w5 = !testedgesidef(v3, v1, v4);
	
	if (w1 == w2 && w2 == w3 && w3 == w4 && w4 == w5) {
		return TRUE;
	}
	
	GETMIN2(v1, v2, mv1, mv2);
	GETMIN2(v3, v4, mv3, mv4);
	
	/* do an interval test on the x and y axes */
	/* first do x axis */
	if (ABS(v1[1] - v2[1]) < EPS &&
	    ABS(v3[1] - v4[1]) < EPS &&
	    ABS(v1[1] - v3[1]) < EPS)
	{
		return (mv4[0] >= mv1[0] && mv3[0] <= mv2[0]);
	}

	/* now do y axis */
	if (ABS(v1[0] - v2[0]) < EPS &&
	    ABS(v3[0] - v4[0]) < EPS &&
	    ABS(v1[0] - v3[0]) < EPS)
	{
		return (mv4[1] >= mv1[1] && mv3[1] <= mv2[1]);
	}

	return FALSE;

#undef GETMIN2_AXIS
#undef GETMIN2
#undef EPS

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
int BM_face_point_inside_test(BMFace *f, const float co[3])
{
	int ax, ay;
	float co2[2], cent[2] = {0.0f, 0.0f}, out[2] = {FLT_MAX * 0.5f, FLT_MAX * 0.5f};
	BMLoop *l_iter;
	BMLoop *l_first;
	int crosses = 0;
	float onepluseps = 1.0f + (float)FLT_EPSILON * 150.0f;
	
	if (dot_v3v3(f->no, f->no) <= FLT_EPSILON * 10)
		BM_face_normal_update(f);
	
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

static int goodline(float const (*projectverts)[3], BMFace *f,
                    int v1i, int v2i, int v3i,
                    int UNUSED(nvert))
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
static BMLoop *find_ear(BMFace *f, float (*verts)[3], const int nvert, const int use_beauty)
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
			isear = TRUE;

			v1 = l_iter->prev->v;
			v2 = l_iter->v;
			v3 = l_iter->next->v;

			if (BM_edge_exists(v1, v3)) {
				isear = FALSE;
			}
			else if (!goodline((float const (*)[3])verts, f,
			                   BM_elem_index_get(v1), BM_elem_index_get(v2), BM_elem_index_get(v3),
			                   nvert))
			{
				isear = FALSE;
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
		l_iter = find_ear(f, projectverts, nvert, use_beauty);
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
	float v1[3], v2[3], v3[3] /*, v4[3 */, no[3], mid[3], *p1, *p2, *p3, *p4;
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
