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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_subdivide.c
 *  \ingroup bmesh
 *
 * Edge based subdivision with various subdivision patterns.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_array.h"
#include "BLI_noise.h"

#include "BKE_customdata.h"


#include "bmesh.h"
#include "intern/bmesh_private.h"
#include "intern/bmesh_operators_private.h"

typedef struct SubDParams {
	int numcuts;
	float smooth;
	int   smooth_falloff;
	float fractal;
	float along_normal;
	//int beauty;
	bool use_smooth;
	bool use_smooth_even;
	bool use_sphere;
	bool use_fractal;
	int seed;
	BMOperator *op;
	BMOpSlot *slot_edge_percents;  /* BMO_slot_get(params->op->slots_in, "edge_percents"); */
	BMOpSlot *slot_custom_patterns;  /* BMO_slot_get(params->op->slots_in, "custom_patterns"); */
	float fractal_ofs[3];

	/* rumtime storage for shape key */
	struct {
		int cd_vert_shape_offset;
		int cd_vert_shape_offset_tmp;
		int totlayer;

		/* shapekey holding displaced vertex coordinates for current geometry */
		int tmpkey;
	} shape_info;

} SubDParams;

static void bmo_subd_init_shape_info(BMesh *bm, SubDParams *params)
{
	const int skey = CustomData_number_of_layers(&bm->vdata, CD_SHAPEKEY) - 1;
	params->shape_info.tmpkey = skey;
	params->shape_info.cd_vert_shape_offset = CustomData_get_offset(&bm->vdata, CD_SHAPEKEY);
	params->shape_info.cd_vert_shape_offset_tmp = CustomData_get_n_offset(&bm->vdata, CD_SHAPEKEY, skey);
	params->shape_info.totlayer = CustomData_number_of_layers(&bm->vdata, CD_SHAPEKEY);

}

typedef void (*subd_pattern_fill_fp)(BMesh *bm, BMFace *face, BMVert **verts,
                                     const SubDParams *params);

/*
 * note: this is a pattern-based edge subdivider.
 * it tries to match a pattern to edge selections on faces,
 * then executes functions to cut them.
 */
typedef struct SubDPattern {
	int seledges[20]; /* selected edges mask, for splitting */

	/* verts starts at the first new vert cut, not the first vert in the face */
	subd_pattern_fill_fp connectexec;
	int len; /* total number of verts, before any subdivision */
} SubDPattern;

/* generic subdivision rules:
 *
 * - two selected edges in a face should make a link
 *   between them.
 *
 * - one edge should do, what? make pretty topology, or just
 *   split the edge only?
 */

/* flags for all elements share a common bitfield space */
#define SUBD_SPLIT	1

#define EDGE_PERCENT	2

/* I don't think new faces are flagged, currently, but
 * better safe than sorry. */
#define FACE_CUSTOMFILL	4
#define ELE_INNER	8
#define ELE_SPLIT	16

/* see bug [#32665], 0.00005 means a we get face splits at a little under 1.0 degrees */
#define FLT_FACE_SPLIT_EPSILON 0.00005f

/*
 * NOTE: beauty has been renamed to flag!
 */

/* generic subdivision rules:
 *
 * - two selected edges in a face should make a link
 *   between them.
 *
 * - one edge should do, what? make pretty topology, or just
 *   split the edge only?
 */

/* connects face with smallest len, which I think should always be correct for
 * edge subdivision */
static BMEdge *connect_smallest_face(BMesh *bm, BMVert *v_a, BMVert *v_b, BMFace **r_f_new)
{
	BMLoop *l_a, *l_b;
	BMFace *f;

	/* this isn't the best thing in the world.  it doesn't handle cases where there's
	 * multiple faces yet.  that might require a convexity test to figure out which
	 * face is "best" and who knows what for non-manifold conditions. */
	f = BM_vert_pair_share_face(v_a, v_b, &l_a, &l_b);

	if (f) {
		BMFace *f_new;
		BMLoop *l_new;

		f_new = BM_face_split(bm, f, l_a, l_b, &l_new, NULL, false);
		
		if (r_f_new) *r_f_new = f_new;
		return l_new ? l_new->e : NULL;
	}

	return NULL;
}
/* calculates offset for co, based on fractal, sphere or smooth settings  */
static void alter_co(BMVert *v, BMEdge *UNUSED(origed), const SubDParams *params, float perc,
                     BMVert *vsta, BMVert *vend)
{
	float tvec[3], prev_co[3], fac;
	float *co = BM_ELEM_CD_GET_VOID_P(v, params->shape_info.cd_vert_shape_offset_tmp);
	int i;
	
	BM_vert_normal_update_all(v);

	copy_v3_v3(co, v->co);
	copy_v3_v3(prev_co, co);

	if (UNLIKELY(params->use_sphere)) { /* subdivide sphere */
		normalize_v3(co);
		mul_v3_fl(co, params->smooth);
	}
	else if (params->use_smooth) {
		/* we calculate an offset vector vec1[], to be added to *co */
		float len, nor[3], nor1[3], nor2[3], val;

		sub_v3_v3v3(nor, vsta->co, vend->co);
		len = 0.5f * normalize_v3(nor);

		copy_v3_v3(nor1, vsta->no);
		copy_v3_v3(nor2, vend->no);

		/* cosine angle */
		fac = dot_v3v3(nor, nor1);
		mul_v3_v3fl(tvec, nor1, fac);

		/* cosine angle */
		fac = -dot_v3v3(nor, nor2);
		madd_v3_v3fl(tvec, nor2, fac);

		/* falloff for multi subdivide */
		val = fabsf(1.0f - 2.0f * fabsf(0.5f - perc));
		val = bmesh_subd_falloff_calc(params->smooth_falloff, val);

		if (params->use_smooth_even) {
			val *= BM_vert_calc_shell_factor(v);
		}

		mul_v3_fl(tvec, params->smooth * val * len);

		add_v3_v3(co, tvec);
	}

	if (params->use_fractal) {
		const float len = len_v3v3(vsta->co, vend->co);
		float normal[3], co2[3], base1[3], base2[3];

		fac = params->fractal * len;

		mid_v3_v3v3(normal, vsta->no, vend->no);
		ortho_basis_v3v3_v3(base1, base2, normal);

		add_v3_v3v3(co2, v->co, params->fractal_ofs);
		mul_v3_fl(co2, 10.0f);

		tvec[0] = fac * (BLI_gTurbulence(1.0, co2[0], co2[1], co2[2], 15, 0, 2) - 0.5f);
		tvec[1] = fac * (BLI_gTurbulence(1.0, co2[1], co2[0], co2[2], 15, 0, 2) - 0.5f);
		tvec[2] = fac * (BLI_gTurbulence(1.0, co2[1], co2[2], co2[0], 15, 0, 2) - 0.5f);

		/* add displacement */
		madd_v3_v3fl(co, normal, tvec[0]);
		madd_v3_v3fl(co, base1, tvec[1] * (1.0f - params->along_normal));
		madd_v3_v3fl(co, base2, tvec[2] * (1.0f - params->along_normal));
	}

	/* apply the new difference to the rest of the shape keys,
	 * note that this doesn't take rotations into account, we _could_ support
	 * this by getting the normals and coords for each shape key and
	 * re-calculate the smooth value for each but this is quite involved.
	 * for now its ok to simply apply the difference IMHO - campbell */
	sub_v3_v3v3(tvec, prev_co, co);

	if (params->shape_info.totlayer > 1) {
		/* skip the last layer since its the temp */
		i = params->shape_info.totlayer - 1;
		co = BM_ELEM_CD_GET_VOID_P(v, params->shape_info.cd_vert_shape_offset);
		while (i--) {
			BLI_assert(co != BM_ELEM_CD_GET_VOID_P(v, params->shape_info.cd_vert_shape_offset_tmp));
			sub_v3_v3(co += 3, tvec);
		}
	}
}

/* assumes in the edge is the correct interpolated vertices already */
/* percent defines the interpolation, rad and flag are for special options */
/* results in new vertex with correct coordinate, vertex normal and weight group info */
static BMVert *bm_subdivide_edge_addvert(BMesh *bm, BMEdge *edge, BMEdge *oedge,
                                         const SubDParams *params, float percent,
                                         float percent2,
                                         BMEdge **out, BMVert *vsta, BMVert *vend)
{
	BMVert *ev;
	
	ev = BM_edge_split(bm, edge, edge->v1, out, percent);

	BMO_elem_flag_enable(bm, ev, ELE_INNER);

	/* offset for smooth or sphere or fractal */
	alter_co(ev, oedge, params, percent2, vsta, vend);

#if 0 //BMESH_TODO
	/* clip if needed by mirror modifier */
	if (edge->v1->f2) {
		if (edge->v1->f2 & edge->v2->f2 & 1) {
			co[0] = 0.0f;
		}
		if (edge->v1->f2 & edge->v2->f2 & 2) {
			co[1] = 0.0f;
		}
		if (edge->v1->f2 & edge->v2->f2 & 4) {
			co[2] = 0.0f;
		}
	}
#endif
	
	interp_v3_v3v3(ev->no, vsta->no, vend->no, percent2);
	normalize_v3(ev->no);

	return ev;
}

static BMVert *subdivideedgenum(BMesh *bm, BMEdge *edge, BMEdge *oedge,
                                int curpoint, int totpoint, const SubDParams *params,
                                BMEdge **newe, BMVert *vsta, BMVert *vend)
{
	BMVert *ev;
	float percent, percent2 = 0.0f;

	if (BMO_elem_flag_test(bm, edge, EDGE_PERCENT) && totpoint == 1) {
		percent = BMO_slot_map_float_get(params->slot_edge_percents, edge);
	}
	else {
		percent = 1.0f / (float)(totpoint + 1 - curpoint);
		percent2 = (float)(curpoint + 1) / (float)(totpoint + 1);

	}
	
	ev = bm_subdivide_edge_addvert(bm, edge, oedge, params, percent,
	                               percent2, newe, vsta, vend);
	return ev;
}

static void bm_subdivide_multicut(BMesh *bm, BMEdge *edge, const SubDParams *params,
                                  BMVert *vsta, BMVert *vend)
{
	BMEdge *eed = edge, *e_new, e_tmp = *edge;
	BMVert *v, v1_tmp = *edge->v1, v2_tmp = *edge->v2, *v1 = edge->v1, *v2 = edge->v2;
	int i, numcuts = params->numcuts;

	e_tmp.v1 = &v1_tmp;
	e_tmp.v2 = &v2_tmp;
	
	for (i = 0; i < numcuts; i++) {
		v = subdivideedgenum(bm, eed, &e_tmp, i, params->numcuts, params, &e_new, vsta, vend);

		BMO_elem_flag_enable(bm, v, SUBD_SPLIT);
		BMO_elem_flag_enable(bm, eed, SUBD_SPLIT);
		BMO_elem_flag_enable(bm, e_new, SUBD_SPLIT);

		BMO_elem_flag_enable(bm, v, ELE_SPLIT);
		BMO_elem_flag_enable(bm, eed, ELE_SPLIT);
		BMO_elem_flag_enable(bm, e_new, SUBD_SPLIT);

		BM_CHECK_ELEMENT(v);
		if (v->e) BM_CHECK_ELEMENT(v->e);
		if (v->e && v->e->l) BM_CHECK_ELEMENT(v->e->l->f);
	}
	
	alter_co(v1, &e_tmp, params, 0, &v1_tmp, &v2_tmp);
	alter_co(v2, &e_tmp, params, 1.0, &v1_tmp, &v2_tmp);
}

/* note: the patterns are rotated as necessary to
 * match the input geometry.  they're based on the
 * pre-split state of the  face */

/**
 * <pre>
 *  v3---------v2
 *  |          |
 *  |          |
 *  |          |
 *  |          |
 *  v4---v0---v1
 * </pre>
 */
static void quad_1edge_split(BMesh *bm, BMFace *UNUSED(face),
                             BMVert **verts, const SubDParams *params)
{
	BMFace *f_new;
	int i, add, numcuts = params->numcuts;

	/* if it's odd, the middle face is a quad, otherwise it's a triangle */
	if ((numcuts % 2) == 0) {
		add = 2;
		for (i = 0; i < numcuts; i++) {
			if (i == numcuts / 2) {
				add -= 1;
			}
			connect_smallest_face(bm, verts[i], verts[numcuts + add], &f_new);
		}
	}
	else {
		add = 2;
		for (i = 0; i < numcuts; i++) {
			connect_smallest_face(bm, verts[i], verts[numcuts + add], &f_new);
			if (i == numcuts / 2) {
				add -= 1;
				connect_smallest_face(bm, verts[i], verts[numcuts + add], &f_new);
			}
		}

	}
}

static const SubDPattern quad_1edge = {
	{1, 0, 0, 0},
	quad_1edge_split,
	4,
};


/**
 * <pre>
 *  v6--------v5
 *  |          |
 *  |          |v4s
 *  |          |v3s
 *  |   s  s   |
 *  v7-v0--v1-v2
 * </pre>
 */
static void quad_2edge_split_path(BMesh *bm, BMFace *UNUSED(face), BMVert **verts,
                                  const SubDParams *params)
{
	BMFace *f_new;
	int i, numcuts = params->numcuts;
	
	for (i = 0; i < numcuts; i++) {
		connect_smallest_face(bm, verts[i], verts[numcuts + (numcuts - i)], &f_new);
	}
	connect_smallest_face(bm, verts[numcuts * 2 + 3], verts[numcuts * 2 + 1], &f_new);
}

static const SubDPattern quad_2edge_path = {
	{1, 1, 0, 0},
	quad_2edge_split_path,
	4,
};

/**
 * <pre>
 *  v6--------v5
 *  |          |
 *  |          |v4s
 *  |          |v3s
 *  |   s  s   |
 *  v7-v0--v1-v2
 * </pre>
 */
static void quad_2edge_split_innervert(BMesh *bm, BMFace *UNUSED(face), BMVert **verts,
                                       const SubDParams *params)
{
	BMFace *f_new;
	BMVert *v, *v_last;
	BMEdge *e, *e_new, e_tmp;
	int i, numcuts = params->numcuts;
	
	v_last = verts[numcuts];

	for (i = numcuts - 1; i >= 0; i--) {
		e = connect_smallest_face(bm, verts[i], verts[numcuts + (numcuts - i)], &f_new);

		e_tmp = *e;
		v = bm_subdivide_edge_addvert(bm, e, &e_tmp, params, 0.5f, 0.5f, &e_new, e->v1, e->v2);

		if (i != numcuts - 1) {
			connect_smallest_face(bm, v_last, v, &f_new);
		}

		v_last = v;
	}

	connect_smallest_face(bm, v_last, verts[numcuts * 2 + 2], &f_new);
}

static const SubDPattern quad_2edge_innervert = {
	{1, 1, 0, 0},
	quad_2edge_split_innervert,
	4,
};

/**
 * <pre>
 *  v6--------v5
 *  |          |
 *  |          |v4s
 *  |          |v3s
 *  |   s  s   |
 *  v7-v0--v1-v2
 * </pre>
 */
static void quad_2edge_split_fan(BMesh *bm, BMFace *UNUSED(face), BMVert **verts,
                                 const SubDParams *params)
{
	BMFace *f_new;
	/* BMVert *v; */               /* UNUSED */
	/* BMVert *v_last = verts[2]; */ /* UNUSED */
	/* BMEdge *e, *e_new; */          /* UNUSED */
	int i, numcuts = params->numcuts;

	for (i = 0; i < numcuts; i++) {
		connect_smallest_face(bm, verts[i], verts[numcuts * 2 + 2], &f_new);
		connect_smallest_face(bm, verts[numcuts + (numcuts - i)], verts[numcuts * 2 + 2], &f_new);
	}
}

static const SubDPattern quad_2edge_fan = {
	{1, 1, 0, 0},
	quad_2edge_split_fan,
	4,
};

/**
 * <pre>
 *      s   s
 *  v8--v7--v6-v5
 *  |          |
 *  |          v4 s
 *  |          |
 *  |          v3 s
 *  |   s  s   |
 *  v9-v0--v1-v2
 * </pre>
 */
static void quad_3edge_split(BMesh *bm, BMFace *UNUSED(face), BMVert **verts,
                             const SubDParams *params)
{
	BMFace *f_new;
	int i, add = 0, numcuts = params->numcuts;
	
	for (i = 0; i < numcuts; i++) {
		if (i == numcuts / 2) {
			if (numcuts % 2 != 0) {
				connect_smallest_face(bm, verts[numcuts - i - 1 + add], verts[i + numcuts + 1], &f_new);
			}
			add = numcuts * 2 + 2;
		}
		connect_smallest_face(bm, verts[numcuts - i - 1 + add], verts[i + numcuts + 1], &f_new);
	}

	for (i = 0; i < numcuts / 2 + 1; i++) {
		connect_smallest_face(bm, verts[i], verts[(numcuts - i) + numcuts * 2 + 1], &f_new);
	}
}

static const SubDPattern quad_3edge = {
	{1, 1, 1, 0},
	quad_3edge_split,
	4,
};

/**
 * <pre>
 *            v8--v7-v6--v5
 *            |     s    |
 *            |v9 s     s|v4
 * first line |          |   last line
 *            |v10s s   s|v3
 *            v11-v0--v1-v2
 *
 *            it goes from bottom up
 * </pre>
 */
static void quad_4edge_subdivide(BMesh *bm, BMFace *UNUSED(face), BMVert **verts,
                                 const SubDParams *params)
{
	BMFace *f_new;
	BMVert *v, *v1, *v2;
	BMEdge *e, *e_new, e_tmp;
	BMVert **lines;
	int numcuts = params->numcuts;
	int i, j, a, b, s = numcuts + 2 /* , totv = numcuts * 4 + 4 */;

	lines = MEM_callocN(sizeof(BMVert *) * (numcuts + 2) * (numcuts + 2), "q_4edge_split");
	/* build a 2-dimensional array of verts,
	 * containing every vert (and all new ones)
	 * in the face */

	/* first line */
	for (i = 0; i < numcuts + 2; i++) {
		lines[i] = verts[numcuts * 3 + 2 + (numcuts - i + 1)];
	}

	/* last line */
	for (i = 0; i < numcuts + 2; i++) {
		lines[(s - 1) * s + i] = verts[numcuts + i];
	}
	
	/* first and last members of middle lines */
	for (i = 0; i < numcuts; i++) {
		a = i;
		b = numcuts + 1 + numcuts + 1 + (numcuts - i - 1);
		
		e = connect_smallest_face(bm, verts[a], verts[b], &f_new);
		if (!e)
			continue;

		BMO_elem_flag_enable(bm, e, ELE_INNER);
		BMO_elem_flag_enable(bm, f_new, ELE_INNER);

		
		v1 = lines[(i + 1) * s] = verts[a];
		v2 = lines[(i + 1) * s + s - 1] = verts[b];
		
		e_tmp = *e;
		for (a = 0; a < numcuts; a++) {
			v = subdivideedgenum(bm, e, &e_tmp, a, numcuts, params, &e_new,
			                     v1, v2);

			BMESH_ASSERT(v != NULL);

			BMO_elem_flag_enable(bm, e_new, ELE_INNER);
			lines[(i + 1) * s + a + 1] = v;
		}
	}

	for (i = 1; i < numcuts + 2; i++) {
		for (j = 1; j <= numcuts; j++) {
			a = i * s + j;
			b = (i - 1) * s + j;
			e = connect_smallest_face(bm, lines[a], lines[b], &f_new);
			if (!e)
				continue;

			BMO_elem_flag_enable(bm, e, ELE_INNER);
			BMO_elem_flag_enable(bm, f_new, ELE_INNER);
		}
	}

	MEM_freeN(lines);
}

/**
 * <pre>
 *        v3
 *       / \
 *      /   \
 *     /     \
 *    /       \
 *   /         \
 *  v4--v0--v1--v2
 *      s    s
 * </pre>
 */
static void tri_1edge_split(BMesh *bm, BMFace *UNUSED(face), BMVert **verts,
                            const SubDParams *params)
{
	BMFace *f_new;
	int i, numcuts = params->numcuts;
	
	for (i = 0; i < numcuts; i++) {
		connect_smallest_face(bm, verts[i], verts[numcuts + 1], &f_new);
	}
}

static const SubDPattern tri_1edge = {
	{1, 0, 0},
	tri_1edge_split,
	3,
};

/**
 * <pre>
 *         v5
 *        / \
 *   s v6/---\ v4 s
 *      / \ / \
 *  sv7/---v---\ v3 s
 *    /  \/  \/ \
 *   v8--v0--v1--v2
 *      s    s
 * </pre>
 */
static void tri_3edge_subdivide(BMesh *bm, BMFace *UNUSED(face), BMVert **verts,
                                const SubDParams *params)
{
	BMFace *f_new;
	BMEdge *e, *e_new, e_tmp;
	BMVert ***lines, *v, v1_tmp, v2_tmp;
	void *stackarr[1];
	int i, j, a, b, numcuts = params->numcuts;
	
	/* number of verts in each lin */
	lines = MEM_callocN(sizeof(void *) * (numcuts + 2), "triangle vert table");
	
	lines[0] = (BMVert **) stackarr;
	lines[0][0] = verts[numcuts * 2 + 1];
	
	lines[numcuts + 1] = MEM_callocN(sizeof(void *) * (numcuts + 2), "triangle vert table 2");
	for (i = 0; i < numcuts; i++) {
		lines[numcuts + 1][i + 1] = verts[i];
	}
	lines[numcuts + 1][0] = verts[numcuts * 3 + 2];
	lines[numcuts + 1][numcuts + 1] = verts[numcuts];

	for (i = 0; i < numcuts; i++) {
		lines[i + 1] = MEM_callocN(sizeof(void *) * (2 + i), "triangle vert table row");
		a = numcuts * 2 + 2 + i;
		b = numcuts + numcuts - i;
		e = connect_smallest_face(bm, verts[a], verts[b], &f_new);
		if (!e) goto cleanup;

		BMO_elem_flag_enable(bm, e, ELE_INNER);
		BMO_elem_flag_enable(bm, f_new, ELE_INNER);

		lines[i + 1][0] = verts[a];
		lines[i + 1][i + 1] = verts[b];
		
		e_tmp = *e;
		v1_tmp = *verts[a];
		v2_tmp = *verts[b];
		e_tmp.v1 = &v1_tmp;
		e_tmp.v2 = &v2_tmp;
		for (j = 0; j < i; j++) {
			v = subdivideedgenum(bm, e, &e_tmp, j, i, params, &e_new,
			                     verts[a], verts[b]);
			lines[i + 1][j + 1] = v;

			BMO_elem_flag_enable(bm, e_new, ELE_INNER);
		}
	}
	
	/**
	 * <pre>
	 *         v5
	 *        / \
	 *   s v6/---\ v4 s
	 *      / \ / \
	 *  sv7/---v---\ v3 s
	 *    /  \/  \/ \
	 *   v8--v0--v1--v2
	 *      s    s
	 * </pre>
	 */
	for (i = 1; i <= numcuts; i++) {
		for (j = 0; j < i; j++) {
			e = connect_smallest_face(bm, lines[i][j], lines[i + 1][j + 1], &f_new);

			BMO_elem_flag_enable(bm, e, ELE_INNER);
			BMO_elem_flag_enable(bm, f_new, ELE_INNER);

			e = connect_smallest_face(bm, lines[i][j + 1], lines[i + 1][j + 1], &f_new);

			BMO_elem_flag_enable(bm, e, ELE_INNER);
			BMO_elem_flag_enable(bm, f_new, ELE_INNER);
		}
	}

cleanup:
	for (i = 1; i < numcuts + 2; i++) {
		if (lines[i]) MEM_freeN(lines[i]);
	}

	MEM_freeN(lines);
}

static const SubDPattern tri_3edge = {
	{1, 1, 1},
	tri_3edge_subdivide,
	3,
};


static const SubDPattern quad_4edge = {
	{1, 1, 1, 1},
	quad_4edge_subdivide,
	4,
};

static const SubDPattern *patterns[] = {
	NULL,  /* quad single edge pattern is inserted here */
	NULL,  /* quad corner vert pattern is inserted here */
	NULL,  /* tri single edge pattern is inserted here */
	NULL,
	&quad_3edge,
	NULL,
};

#define PATTERNS_TOT  (sizeof(patterns) / sizeof(void *))

typedef struct SubDFaceData {
	BMVert *start;
	const SubDPattern *pat;
	int totedgesel;  /* only used if pat was NULL, e.g. no pattern was found */
	BMFace *face;
} SubDFaceData;

void bmo_subdivide_edges_exec(BMesh *bm, BMOperator *op)
{
	BMOpSlot *einput;
	const SubDPattern *pat;
	SubDParams params;
	SubDFaceData *facedata = NULL;
	BLI_array_declare(facedata);
	BMIter viter, fiter, liter;
	BMVert *v, **verts = NULL;
	BMEdge *edge;
	BMEdge **edges = NULL;
	BLI_array_declare(edges);
	BMLoop *(*loops_split)[2] = NULL;
	BLI_array_declare(loops_split);
	BMLoop **loops = NULL;
	BLI_array_declare(loops);
	BMLoop *l_new, *l;
	BMFace *face;
	BLI_array_declare(verts);
	float smooth, fractal, along_normal;
	bool use_sphere, use_single_edge, use_grid_fill, use_only_quads;
	int cornertype, seed, i, j, matched, a, b, numcuts, totesel, smooth_falloff;
	
	BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, SUBD_SPLIT);
	
	numcuts = BMO_slot_int_get(op->slots_in, "cuts");
	seed = BMO_slot_int_get(op->slots_in, "seed");
	smooth = BMO_slot_float_get(op->slots_in, "smooth");
	smooth_falloff = BMO_slot_int_get(op->slots_in, "smooth_falloff");
	fractal = BMO_slot_float_get(op->slots_in, "fractal");
	along_normal = BMO_slot_float_get(op->slots_in, "along_normal");
	cornertype = BMO_slot_int_get(op->slots_in, "quad_corner_type");

	use_single_edge = BMO_slot_bool_get(op->slots_in, "use_single_edge");
	use_grid_fill = BMO_slot_bool_get(op->slots_in, "use_grid_fill");
	use_only_quads = BMO_slot_bool_get(op->slots_in, "use_only_quads");
	use_sphere = BMO_slot_bool_get(op->slots_in, "use_sphere");

	patterns[1] = NULL;
	/* straight cut is patterns[1] == NULL */
	switch (cornertype) {
		case SUBD_PATH:
			patterns[1] = &quad_2edge_path;
			break;
		case SUBD_INNERVERT:
			patterns[1] = &quad_2edge_innervert;
			break;
		case SUBD_FAN:
			patterns[1] = &quad_2edge_fan;
			break;
	}
	
	if (use_single_edge) {
		patterns[0] = &quad_1edge;
		patterns[2] = &tri_1edge;
	}
	else {
		patterns[0] = NULL;
		patterns[2] = NULL;
	}

	if (use_grid_fill) {
		patterns[3] = &quad_4edge;
		patterns[5] = &tri_3edge;
	}
	else {
		patterns[3] = NULL;
		patterns[5] = NULL;
	}
	
	/* add a temporary shapekey layer to store displacements on current geometry */
	BM_data_layer_add(bm, &bm->vdata, CD_SHAPEKEY);

	bmo_subd_init_shape_info(bm, &params);
	
	BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
		float *co = BM_ELEM_CD_GET_VOID_P(v, params.shape_info.cd_vert_shape_offset_tmp);
		copy_v3_v3(co, v->co);
	}

	/* first go through and tag edges */
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_in, "edges", BM_EDGE, SUBD_SPLIT);

	params.numcuts = numcuts;
	params.op = op;
	params.slot_edge_percents   = BMO_slot_get(op->slots_in, "edge_percents");
	params.slot_custom_patterns = BMO_slot_get(op->slots_in, "custom_patterns");
	params.smooth = smooth;
	params.smooth_falloff = smooth_falloff;
	params.seed = seed;
	params.fractal = fractal;
	params.along_normal = along_normal;
	params.use_smooth  = (smooth  != 0.0f);
	params.use_smooth_even = BMO_slot_bool_get(op->slots_in, "use_smooth_even");
	params.use_fractal = (fractal != 0.0f);
	params.use_sphere  = use_sphere;

	if (params.use_fractal) {
		RNG *rng = BLI_rng_new_srandom(seed);

		params.fractal_ofs[0] = BLI_rng_get_float(rng) * 200.0f;
		params.fractal_ofs[1] = BLI_rng_get_float(rng) * 200.0f;
		params.fractal_ofs[2] = BLI_rng_get_float(rng) * 200.0f;

		BLI_rng_free(rng);
	}
	
	BMO_slot_map_to_flag(bm, op->slots_in, "custom_patterns",
	                     BM_FACE, FACE_CUSTOMFILL);

	BMO_slot_map_to_flag(bm, op->slots_in, "edge_percents",
	                     BM_EDGE, EDGE_PERCENT);


	BM_ITER_MESH (face, &fiter, bm, BM_FACES_OF_MESH) {
		BMEdge *e1 = NULL, *e2 = NULL;
		float vec1[3], vec2[3];

		/* skip non-quads if requested */
		if (use_only_quads && face->len != 4)
			continue;

		/* figure out which pattern to use */

		BLI_array_empty(edges);
		BLI_array_empty(verts);

		BLI_array_grow_items(edges, face->len);
		BLI_array_grow_items(verts, face->len);

		matched = 0;

		totesel = 0;
		BM_ITER_ELEM_INDEX (l_new, &liter, face, BM_LOOPS_OF_FACE, i) {
			edges[i] = l_new->e;
			verts[i] = l_new->v;

			if (BMO_elem_flag_test(bm, edges[i], SUBD_SPLIT)) {
				if (!e1) e1 = edges[i];
				else     e2 = edges[i];

				totesel++;
			}
		}

		/* make sure the two edges have a valid angle to each other */
		if (totesel == 2 && BM_edge_share_vert_check(e1, e2)) {
			sub_v3_v3v3(vec1, e1->v2->co, e1->v1->co);
			sub_v3_v3v3(vec2, e2->v2->co, e2->v1->co);
			normalize_v3(vec1);
			normalize_v3(vec2);

			if (fabsf(dot_v3v3(vec1, vec2)) > 1.0f - FLT_FACE_SPLIT_EPSILON) {
				totesel = 0;
			}
		}

		if (BMO_elem_flag_test(bm, face, FACE_CUSTOMFILL)) {
			pat = *BMO_slot_map_data_get(params.slot_custom_patterns, face);
			for (i = 0; i < pat->len; i++) {
				matched = 1;
				for (j = 0; j < pat->len; j++) {
					a = (j + i) % pat->len;
					if ((!!BMO_elem_flag_test(bm, edges[a], SUBD_SPLIT)) != (!!pat->seledges[j])) {
						matched = 0;
						break;
					}
				}
				if (matched) {
					BLI_array_grow_one(facedata);
					b = BLI_array_count(facedata) - 1;
					facedata[b].pat = pat;
					facedata[b].start = verts[i];
					facedata[b].face = face;
					facedata[b].totedgesel = totesel;
					BMO_elem_flag_enable(bm, face, SUBD_SPLIT);
					break;
				}
			}

			/* obvously don't test for other patterns matching */
			continue;
		}

		for (i = 0; i < PATTERNS_TOT; i++) {
			pat = patterns[i];
			if (!pat) {
				continue;
			}

			if (pat->len == face->len) {
				for (a = 0; a < pat->len; a++) {
					matched = 1;
					for (b = 0; b < pat->len; b++) {
						j = (b + a) % pat->len;
						if ((!!BMO_elem_flag_test(bm, edges[j], SUBD_SPLIT)) != (!!pat->seledges[b])) {
							matched = 0;
							break;
						}
					}
					if (matched) {
						break;
					}
				}
				if (matched) {
					BLI_array_grow_one(facedata);
					j = BLI_array_count(facedata) - 1;

					BMO_elem_flag_enable(bm, face, SUBD_SPLIT);

					facedata[j].pat = pat;
					facedata[j].start = verts[a];
					facedata[j].face = face;
					facedata[j].totedgesel = totesel;
					break;
				}
			}

		}
		
		if (!matched && totesel) {
			BLI_array_grow_one(facedata);
			j = BLI_array_count(facedata) - 1;
			
			BMO_elem_flag_enable(bm, face, SUBD_SPLIT);

			/* must initialize all members here */
			facedata[j].start = NULL;
			facedata[j].pat = NULL;
			facedata[j].totedgesel = totesel;
			facedata[j].face = face;
		}
	}

	einput = BMO_slot_get(op->slots_in, "edges");

	/* go through and split edges */
	for (i = 0; i < einput->len; i++) {
		edge = einput->data.buf[i];
		bm_subdivide_multicut(bm, edge, &params, edge->v1, edge->v2);
	}

	/* copy original-geometry displacements to current coordinates */
	BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
		float *co = BM_ELEM_CD_GET_VOID_P(v, params.shape_info.cd_vert_shape_offset_tmp);
		copy_v3_v3(v->co, co);
	}

	i = 0;
	for (i = 0; i < BLI_array_count(facedata); i++) {
		face = facedata[i].face;

		/* figure out which pattern to use */
		BLI_array_empty(verts);

		pat = facedata[i].pat;

		if (!pat && facedata[i].totedgesel == 2) {
			int vlen;
			
			/* ok, no pattern.  we still may be able to do something */
			BLI_array_empty(loops);
			BLI_array_empty(loops_split);

			/* for case of two edges, connecting them shouldn't be too hard */
			BLI_array_grow_items(loops, face->len);
			BM_ITER_ELEM_INDEX (l, &liter, face, BM_LOOPS_OF_FACE, a) {
				loops[a] = l;
			}
			
			vlen = BLI_array_count(loops);

			/* find the boundary of one of the split edges */
			for (a = 1; a < vlen; a++) {
				if (!BMO_elem_flag_test(bm, loops[a - 1]->v, ELE_INNER) &&
				    BMO_elem_flag_test(bm, loops[a]->v, ELE_INNER))
				{
					break;
				}
			}
			
			if (BMO_elem_flag_test(bm, loops[(a + numcuts + 1) % vlen]->v, ELE_INNER)) {
				b = (a + numcuts + 1) % vlen;
			}
			else {
				/* find the boundary of the other edge. */
				for (j = 0; j < vlen; j++) {
					b = (j + a + numcuts + 1) % vlen;
					if (!BMO_elem_flag_test(bm, loops[b == 0 ? vlen - 1 : b - 1]->v, ELE_INNER) &&
					    BMO_elem_flag_test(bm, loops[b]->v, ELE_INNER))
					{
						break;
					}
				}
			}
			
			b += numcuts - 1;

			BLI_array_grow_items(loops_split, numcuts);
			for (j = 0; j < numcuts; j++) {
				bool ok = true;

				/* Check for special case: [#32500]
				 * This edge pair could be used by more than one face,
				 * in this case it used to (2.63), split both faces along the same verts
				 * while it could be calculated which face should do the split,
				 * it's ambiguous, so in this case we're better off to skip them as exceptional cases
				 * and not try to be clever guessing which face to cut up.
				 *
				 * To avoid this case we need to check:
				 * Do the verts of each share a face besides the one we are subdividing,
				 *  (but not connect to make an edge of that face).
				 */
				{
					BMLoop *other_loop;
					BMIter other_fiter;
					BM_ITER_ELEM (other_loop, &other_fiter, loops[a]->v, BM_LOOPS_OF_VERT) {
						if (other_loop->f != face) {
							if (BM_vert_in_face(other_loop->f, loops[b]->v)) {
								/* we assume that these verts are not making an edge in the face */
								BLI_assert(other_loop->prev->v != loops[a]->v);
								BLI_assert(other_loop->next->v != loops[a]->v);

								ok = false;
								break;
							}
						}
					}
				}


				if (ok == true) {
					loops_split[j][0] = loops[a];
					loops_split[j][1] = loops[b];
				}
				else {
					loops_split[j][0] = NULL;
					loops_split[j][1] = NULL;
				}

				b = (b - 1) % vlen;
				a = (a + 1) % vlen;
			}
			
			/* Since these are newly created vertices, we don't need to worry about them being legal,
			 * ... though there are some cases we _should_ check for
			 * - concave corner of an ngon.
			 * - 2 edges being used in 2+ ngons.
			 */
//			BM_face_legal_splits(face, loops_split, BLI_array_count(loops_split));

			for (j = 0; j < BLI_array_count(loops_split); j++) {
				if (loops_split[j][0]) {
					BMFace *f_new;
					BLI_assert(BM_edge_exists(loops_split[j][0]->v, loops_split[j][1]->v) == NULL);
					f_new = BM_face_split(bm, face, loops_split[j][0], loops_split[j][1], &l_new, NULL, false);
					if (f_new) {
						BMO_elem_flag_enable(bm, l_new->e, ELE_INNER);
					}
				}
			}

			continue;
		}
		else if (!pat) {
			continue;
		}

		a = 0;
		BM_ITER_ELEM_INDEX (l_new, &liter, face, BM_LOOPS_OF_FACE, j) {
			if (l_new->v == facedata[i].start) {
				a = j + 1;
				break;
			}
		}

		BLI_array_grow_items(verts, face->len);

		BM_ITER_ELEM_INDEX (l_new, &liter, face, BM_LOOPS_OF_FACE, j) {
			b = (j - a + face->len) % face->len;
			verts[b] = l_new->v;
		}

		BM_CHECK_ELEMENT(face);
		pat->connectexec(bm, face, verts, &params);
	}

	/* copy original-geometry displacements to current coordinates */
	BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
		float *co = BM_ELEM_CD_GET_VOID_P(v, params.shape_info.cd_vert_shape_offset_tmp);
		copy_v3_v3(v->co, co);
	}

	BM_data_layer_free_n(bm, &bm->vdata, CD_SHAPEKEY, params.shape_info.tmpkey);
	
	if (facedata) BLI_array_free(facedata);
	if (edges) BLI_array_free(edges);
	if (verts) BLI_array_free(verts);
	BLI_array_free(loops_split);
	BLI_array_free(loops);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom_inner.out", BM_ALL_NOLOOP, ELE_INNER);
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom_split.out", BM_ALL_NOLOOP, ELE_SPLIT);
	
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom.out", BM_ALL_NOLOOP, ELE_INNER | ELE_SPLIT | SUBD_SPLIT);
}

/* editmesh-emulating function */
void BM_mesh_esubdivide(BMesh *bm, const char edge_hflag,
                        const float smooth, const short smooth_falloff, const bool use_smooth_even,
                        const float fractal, const float along_normal,
                        const int numcuts,
                        const int seltype, const int cornertype,
                        const short use_single_edge, const short use_grid_fill,
                        const short use_only_quads,
                        const int seed)
{
	BMOperator op;
	
	/* use_sphere isnt exposed here since its only used for new primitives */
	BMO_op_initf(bm, &op, BMO_FLAG_DEFAULTS,
	             "subdivide_edges edges=%he "
	             "smooth=%f smooth_falloff=%i use_smooth_even=%b "
	             "fractal=%f along_normal=%f "
	             "cuts=%i "
	             "quad_corner_type=%i "
	             "use_single_edge=%b use_grid_fill=%b "
	             "use_only_quads=%b "
	             "seed=%i",
	             edge_hflag,
	             smooth, smooth_falloff, use_smooth_even,
	             fractal, along_normal,
	             numcuts,
	             cornertype,
	             use_single_edge, use_grid_fill,
	             use_only_quads,
	             seed);
	
	BMO_op_exec(bm, &op);
	
	switch (seltype) {
		case SUBDIV_SELECT_NONE:
			break;
		case SUBDIV_SELECT_ORIG:
			/* set the newly created data to be selected */
			BMO_slot_buffer_hflag_enable(bm, op.slots_out, "geom_inner.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);
			BM_mesh_select_flush(bm);
			break;
		case SUBDIV_SELECT_INNER:
			BMO_slot_buffer_hflag_enable(bm, op.slots_out, "geom_inner.out", BM_EDGE | BM_VERT, BM_ELEM_SELECT, true);
			break;
		case SUBDIV_SELECT_LOOPCUT:
			/* deselect input */
			BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);
			BMO_slot_buffer_hflag_enable(bm, op.slots_out, "geom_inner.out", BM_EDGE, BM_ELEM_SELECT, true);
			break;
	}

	BMO_op_finish(bm, &op);
}

void bmo_bisect_edges_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e;
	SubDParams params = {0};
	
	params.numcuts = BMO_slot_int_get(op->slots_in, "cuts");
	params.op = op;
	params.slot_edge_percents = BMO_slot_get(op->slots_in, "edge_percents");
	
	BM_data_layer_add(bm, &bm->vdata, CD_SHAPEKEY);

	bmo_subd_init_shape_info(bm, &params);

	/* go through and split edges */
	BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {
		bm_subdivide_multicut(bm, e, &params, e->v1, e->v2);
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom_split.out", BM_ALL_NOLOOP, ELE_SPLIT);

	BM_data_layer_free_n(bm, &bm->vdata, CD_SHAPEKEY, params.shape_info.tmpkey);
}
