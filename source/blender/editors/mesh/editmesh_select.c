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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_select.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_bitmap_draw_2d.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_math_bits.h"
#include "BLI_rand.h"
#include "BLI_array.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_paint.h"
#include "BKE_editmesh.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "UI_resources.h"

#include "bmesh_tools.h"

#include "mesh_intern.h"  /* own include */

/* use bmesh operator flags for a few operators */
#define BMO_ELE_TAG 1

/* ****************************** MIRROR **************** */

void EDBM_select_mirrored(
        BMEditMesh *em, const int axis, const bool extend,
        int *r_totmirr, int *r_totfail)
{
	Mesh *me = (Mesh *)em->ob->data;
	BMesh *bm = em->bm;
	BMIter iter;
	int totmirr = 0;
	int totfail = 0;
	bool use_topology = (me && (me->editflag & ME_EDIT_MIRROR_TOPO));

	*r_totmirr = *r_totfail = 0;

	/* select -> tag */
	if (bm->selectmode & SCE_SELECT_VERTEX) {
		BMVert *v;
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			BM_elem_flag_set(v, BM_ELEM_TAG, BM_elem_flag_test(v, BM_ELEM_SELECT));
		}
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BMEdge *e;
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			BM_elem_flag_set(e, BM_ELEM_TAG, BM_elem_flag_test(e, BM_ELEM_SELECT));
		}
	}
	else {
		BMFace *f;
		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			BM_elem_flag_set(f, BM_ELEM_TAG, BM_elem_flag_test(f, BM_ELEM_SELECT));
		}
	}

	EDBM_verts_mirror_cache_begin(em, axis, true, true, use_topology);

	if (!extend)
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);


	if (bm->selectmode & SCE_SELECT_VERTEX) {
		BMVert *v;
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN) && BM_elem_flag_test(v, BM_ELEM_TAG)) {
				BMVert *v_mirr = EDBM_verts_mirror_get(em, v);
				if (v_mirr && !BM_elem_flag_test(v_mirr, BM_ELEM_HIDDEN)) {
					BM_vert_select_set(bm, v_mirr, true);
					totmirr++;
				}
				else {
					totfail++;
				}
			}
		}
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BMEdge *e;
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN) && BM_elem_flag_test(e, BM_ELEM_TAG)) {
				BMEdge *e_mirr = EDBM_verts_mirror_get_edge(em, e);
				if (e_mirr && !BM_elem_flag_test(e_mirr, BM_ELEM_HIDDEN)) {
					BM_edge_select_set(bm, e_mirr, true);
					totmirr++;
				}
				else {
					totfail++;
				}
			}
		}
	}
	else {
		BMFace *f;
		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN) && BM_elem_flag_test(f, BM_ELEM_TAG)) {
				BMFace *f_mirr = EDBM_verts_mirror_get_face(em, f);
				if (f_mirr && !BM_elem_flag_test(f_mirr, BM_ELEM_HIDDEN)) {
					BM_face_select_set(bm, f_mirr, true);
					totmirr++;
				}
				else {
					totfail++;
				}
			}
		}
	}

	EDBM_verts_mirror_cache_end(em);

	*r_totmirr = totmirr;
	*r_totfail = totfail;
}

void EDBM_automerge(Scene *scene, Object *obedit, bool update, const char hflag)
{
	bool ok;
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	ok = BMO_op_callf(em->bm, BMO_FLAG_DEFAULTS,
	                  "automerge verts=%hv dist=%f",
	                  hflag, scene->toolsettings->doublimit);

	if (LIKELY(ok) && update) {
		EDBM_update_generic(em, true, true);
	}
}

/* ****************************** SELECTION ROUTINES **************** */

unsigned int bm_solidoffs = 0, bm_wireoffs = 0, bm_vertoffs = 0;    /* set in drawobject.c ... for colorindices */

/* facilities for border select and circle select */
static BLI_bitmap *selbuf = NULL;

static BLI_bitmap *edbm_backbuf_alloc(const int size)
{
	return BLI_BITMAP_NEW(size, "selbuf");
}

/* reads rect, and builds selection array for quick lookup */
/* returns if all is OK */
bool EDBM_backbuf_border_init(ViewContext *vc, short xmin, short ymin, short xmax, short ymax)
{
	struct ImBuf *buf;
	unsigned int *dr;
	int a;
	
	if (vc->obedit == NULL || !V3D_IS_ZBUF(vc->v3d)) {
		return false;
	}
	
	buf = ED_view3d_backbuf_read(vc, xmin, ymin, xmax, ymax);
	if ((buf == NULL) || (bm_vertoffs == 0)) {
		return false;
	}

	dr = buf->rect;
	
	/* build selection lookup */
	selbuf = edbm_backbuf_alloc(bm_vertoffs + 1);
	
	a = (xmax - xmin + 1) * (ymax - ymin + 1);
	while (a--) {
		if (*dr > 0 && *dr <= bm_vertoffs) {
			BLI_BITMAP_ENABLE(selbuf, *dr);
		}
		dr++;
	}
	IMB_freeImBuf(buf);
	return true;
}

bool EDBM_backbuf_check(unsigned int index)
{
	/* odd logic, if selbuf is NULL we assume no zbuf-selection is enabled
	 * and just ignore the depth buffer, this is error prone since its possible
	 * code doesn't set the depth buffer by accident, but leave for now. - Campbell */
	if (selbuf == NULL)
		return true;

	if (index > 0 && index <= bm_vertoffs)
		return BLI_BITMAP_TEST_BOOL(selbuf, index);

	return false;
}

void EDBM_backbuf_free(void)
{
	if (selbuf) MEM_freeN(selbuf);
	selbuf = NULL;
}

struct LassoMaskData {
	unsigned int *px;
	int width;
};

static void edbm_mask_lasso_px_cb(int x, int x_end, int y, void *user_data)
{
	struct LassoMaskData *data = user_data;
	unsigned int *px = &data->px[(y * data->width) + x];
	do {
		*px = true;
		px++;
	} while (++x != x_end);
}


/* mcords is a polygon mask
 * - grab backbuffer,
 * - draw with black in backbuffer, 
 * - grab again and compare
 * returns 'OK' 
 */
bool EDBM_backbuf_border_mask_init(ViewContext *vc, const int mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax)
{
	unsigned int *dr, *dr_mask, *dr_mask_arr;
	struct ImBuf *buf;
	int a;
	struct LassoMaskData lasso_mask_data;
	
	/* method in use for face selecting too */
	if (vc->obedit == NULL) {
		if (!BKE_paint_select_elem_test(vc->obact)) {
			return false;
		}
	}
	else if (!V3D_IS_ZBUF(vc->v3d)) {
		return false;
	}

	buf = ED_view3d_backbuf_read(vc, xmin, ymin, xmax, ymax);
	if ((buf == NULL) || (bm_vertoffs == 0)) {
		return false;
	}

	dr = buf->rect;

	dr_mask = dr_mask_arr = MEM_callocN(sizeof(*dr_mask) * buf->x * buf->y, __func__);
	lasso_mask_data.px = dr_mask;
	lasso_mask_data.width = (xmax - xmin) + 1;

	BLI_bitmap_draw_2d_poly_v2i_n(
	       xmin, ymin, xmax + 1, ymax + 1,
	       mcords, tot,
	       edbm_mask_lasso_px_cb, &lasso_mask_data);

	/* build selection lookup */
	selbuf = edbm_backbuf_alloc(bm_vertoffs + 1);
	
	a = (xmax - xmin + 1) * (ymax - ymin + 1);
	while (a--) {
		if (*dr > 0 && *dr <= bm_vertoffs && *dr_mask == true) {
			BLI_BITMAP_ENABLE(selbuf, *dr);
		}
		dr++; dr_mask++;
	}
	IMB_freeImBuf(buf);
	MEM_freeN(dr_mask_arr);

	return true;
}

/* circle shaped sample area */
bool EDBM_backbuf_circle_init(ViewContext *vc, short xs, short ys, short rads)
{
	struct ImBuf *buf;
	unsigned int *dr;
	short xmin, ymin, xmax, ymax, xc, yc;
	int radsq;
	
	/* method in use for face selecting too */
	if (vc->obedit == NULL) {
		if (!BKE_paint_select_elem_test(vc->obact)) {
			return false;
		}
	}
	else if (!V3D_IS_ZBUF(vc->v3d)) {
		return false;
	}

	xmin = xs - rads; xmax = xs + rads;
	ymin = ys - rads; ymax = ys + rads;
	buf = ED_view3d_backbuf_read(vc, xmin, ymin, xmax, ymax);
	if ((buf == NULL) || (bm_vertoffs == 0)) {
		return false;
	}

	dr = buf->rect;
	
	/* build selection lookup */
	selbuf = edbm_backbuf_alloc(bm_vertoffs + 1);
	radsq = rads * rads;
	for (yc = -rads; yc <= rads; yc++) {
		for (xc = -rads; xc <= rads; xc++, dr++) {
			if (xc * xc + yc * yc < radsq) {
				if (*dr > 0 && *dr <= bm_vertoffs) {
					BLI_BITMAP_ENABLE(selbuf, *dr);
				}
			}
		}
	}

	IMB_freeImBuf(buf);
	return true;
	
}


/* -------------------------------------------------------------------- */

/** \name Find Nearest Vert/Edge/Face
 *
 * \note Screen-space manhatten distances are used here,
 * since its faster and good enough for the purpose of selection.
 *
 * \note \a dist_bias is used so we can bias against selected items.
 * when choosing between elements of a single type, but return the real distance
 * to avoid the bias interfering with distance comparisons when mixing types.
 * \{ */

#define FIND_NEAR_SELECT_BIAS 5
#define FIND_NEAR_CYCLE_THRESHOLD_MIN 3

struct NearestVertUserData_Hit {
	float   dist;
	float   dist_bias;
	int     index;
	BMVert *vert;
};

struct NearestVertUserData {
	float mval_fl[2];
	bool use_select_bias;
	bool use_cycle;
	int cycle_index_prev;

	struct NearestVertUserData_Hit hit;
	struct NearestVertUserData_Hit hit_cycle;
};

static void findnearestvert__doClosest(void *userData, BMVert *eve, const float screen_co[2], int index)
{
	struct NearestVertUserData *data = userData;
	float dist_test, dist_test_bias;

	dist_test = dist_test_bias = len_manhattan_v2v2(data->mval_fl, screen_co);

	if (data->use_select_bias && BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
		dist_test_bias += FIND_NEAR_SELECT_BIAS;
	}

	if (dist_test_bias < data->hit.dist_bias) {
		data->hit.dist_bias = dist_test_bias;
		data->hit.dist = dist_test;
		data->hit.index = index;
		data->hit.vert = eve;
	}

	if (data->use_cycle) {
		if ((data->hit_cycle.vert == NULL) &&
		    (index > data->cycle_index_prev) &&
		    (dist_test_bias < FIND_NEAR_CYCLE_THRESHOLD_MIN))
		{
			data->hit_cycle.dist_bias = dist_test_bias;
			data->hit_cycle.dist = dist_test;
			data->hit_cycle.index = index;
			data->hit_cycle.vert = eve;
		}
	}
}

/**
 * Nearest vertex under the cursor.
 *
 * \param r_dist (in/out), minimal distance to the nearest and at the end, actual distance
 * \param use_select_bias
 * - When true, selected vertice are given a 5 pixel bias to make them further than unselect verts.
 * - When false, unselected vertice are given the bias.
 * \param use_cycle Cycle over elements within #FIND_NEAR_CYCLE_THRESHOLD_MIN in order of index.
 */
BMVert *EDBM_vert_find_nearest_ex(
        ViewContext *vc, float *r_dist,
        const bool use_select_bias, bool use_cycle)
{
	BMesh *bm = vc->em->bm;

	if (V3D_IS_ZBUF(vc->v3d)) {
		const int dist_px = ED_view3d_backbuf_sample_size_clamp(vc->ar, *r_dist);
		float dist_test;
		unsigned int index;
		BMVert *eve;
		
		/* No afterqueue (yet), so we check it now, otherwise the bm_xxxofs indices are bad. */
		ED_view3d_backbuf_validate(vc);

		index = ED_view3d_backbuf_sample_rect(
		        vc, vc->mval, dist_px, bm_wireoffs, 0xFFFFFF, &dist_test);
		eve = index ? BM_vert_at_index_find_or_table(bm, index - 1) : NULL;
		
		if (eve) {
			if (dist_test < *r_dist) {
				*r_dist = dist_test;
				return eve;
			}
		}
		return NULL;
	}
	else {
		struct NearestVertUserData data = {{0}};
		const struct NearestVertUserData_Hit *hit;
		const eV3DProjTest clip_flag = V3D_PROJ_TEST_CLIP_DEFAULT;

		static int prev_select_index = 0;
		static const BMVert *prev_select_elem = NULL;

		if ((use_cycle == false) ||
		    (prev_select_elem && (prev_select_elem != BM_vert_at_index_find_or_table(bm, prev_select_index))))
		{
			prev_select_index = 0;
			prev_select_elem = NULL;
		}

		data.mval_fl[0] = vc->mval[0];
		data.mval_fl[1] = vc->mval[1];
		data.use_select_bias = use_select_bias;
		data.use_cycle = use_cycle;
		data.hit.dist      = data.hit_cycle.dist = \
		data.hit.dist_bias = data.hit_cycle.dist_bias = *r_dist;
		data.cycle_index_prev = prev_select_index;

		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
		mesh_foreachScreenVert(vc, findnearestvert__doClosest, &data, clip_flag);

		hit = (data.use_cycle && data.hit_cycle.vert) ? &data.hit_cycle : &data.hit;
		*r_dist = hit->dist;

		prev_select_elem = hit->vert;
		prev_select_index = hit->index;

		return hit->vert;
	}
}

BMVert *EDBM_vert_find_nearest(ViewContext *vc, float *r_dist)
{
	return EDBM_vert_find_nearest_ex(vc, r_dist, false, false);
}

/* find the distance to the edge we already have */
struct NearestEdgeUserData_ZBuf {
	float mval_fl[2];
	float dist;
	const BMEdge *edge_test;
};

static void find_nearest_edge_center__doZBuf(
        void *userData, BMEdge *eed,
        const float screen_co_a[2], const float screen_co_b[2],
        int UNUSED(index))
{
	struct NearestEdgeUserData_ZBuf *data = userData;

	if (eed == data->edge_test) {
		float dist_test;
		float screen_co_mid[2];

		mid_v2_v2v2(screen_co_mid, screen_co_a, screen_co_b);
		dist_test = len_manhattan_v2v2(data->mval_fl, screen_co_mid);

		if (dist_test < data->dist) {
			data->dist = dist_test;
		}
	}
}

struct NearestEdgeUserData_Hit {
	float   dist;
	float   dist_bias;
	int     index;
	BMEdge *edge;

	/* edges only, un-biased manhatten distance to which ever edge we pick
	 * (not used for choosing) */
	float   dist_center;
};

struct NearestEdgeUserData {
	ViewContext vc;
	float mval_fl[2];
	bool use_select_bias;
	bool use_cycle;
	int cycle_index_prev;

	struct NearestEdgeUserData_Hit hit;
	struct NearestEdgeUserData_Hit hit_cycle;
};

/* note; uses v3d, so needs active 3d window */
static void find_nearest_edge__doClosest(
        void *userData, BMEdge *eed,
        const float screen_co_a[2], const float screen_co_b[2],
        int index)
{
	struct NearestEdgeUserData *data = userData;
	float dist_test, dist_test_bias;

	float fac = line_point_factor_v2(data->mval_fl, screen_co_a, screen_co_b);
	float screen_co[2];

	if (fac <= 0.0f) {
		fac = 0.0f;
		copy_v2_v2(screen_co, screen_co_a);
	}
	else if (fac >= 1.0f) {
		fac = 1.0f;
		copy_v2_v2(screen_co, screen_co_b);
	}
	else {
		interp_v2_v2v2(screen_co, screen_co_a, screen_co_b, fac);
	}

	dist_test = dist_test_bias = len_manhattan_v2v2(data->mval_fl, screen_co);

	if (data->use_select_bias && BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
		dist_test_bias += FIND_NEAR_SELECT_BIAS;
	}

	if (data->vc.rv3d->rflag & RV3D_CLIPPING) {
		float vec[3];

		interp_v3_v3v3(vec, eed->v1->co, eed->v2->co, fac);
		if (ED_view3d_clipping_test(data->vc.rv3d, vec, true)) {
			return;
		}
	}

	if (dist_test_bias < data->hit.dist_bias) {
		float screen_co_mid[2];

		data->hit.dist_bias = dist_test_bias;
		data->hit.dist = dist_test;
		data->hit.index = index;
		data->hit.edge = eed;

		mid_v2_v2v2(screen_co_mid, screen_co_a, screen_co_b);
		data->hit.dist_center = len_manhattan_v2v2(data->mval_fl, screen_co_mid);
	}

	if (data->use_cycle) {
		if ((data->hit_cycle.edge == NULL) &&
		    (index > data->cycle_index_prev) &&
		    (dist_test_bias < FIND_NEAR_CYCLE_THRESHOLD_MIN))
		{
			float screen_co_mid[2];

			data->hit_cycle.dist_bias = dist_test_bias;
			data->hit_cycle.dist = dist_test;
			data->hit_cycle.index = index;
			data->hit_cycle.edge = eed;

			mid_v2_v2v2(screen_co_mid, screen_co_a, screen_co_b);
			data->hit_cycle.dist_center = len_manhattan_v2v2(data->mval_fl, screen_co_mid);
		}
	}
}

BMEdge *EDBM_edge_find_nearest_ex(
        ViewContext *vc, float *r_dist,
        float *r_dist_center,
        const bool use_select_bias, const bool use_cycle,
        BMEdge **r_eed_zbuf)
{
	BMesh *bm = vc->em->bm;

	if (V3D_IS_ZBUF(vc->v3d)) {
		const int dist_px = ED_view3d_backbuf_sample_size_clamp(vc->ar, *r_dist);
		float dist_test = 0.0f;
		unsigned int index;
		BMEdge *eed;

		/* No afterqueue (yet), so we check it now, otherwise the bm_xxxofs indices are bad. */
		ED_view3d_backbuf_validate(vc);

		index = ED_view3d_backbuf_sample_rect(vc, vc->mval, dist_px, bm_solidoffs, bm_wireoffs, &dist_test);
		eed = index ? BM_edge_at_index_find_or_table(bm, index - 1) : NULL;

		if (r_eed_zbuf) {
			*r_eed_zbuf = eed;
		}

		/* exception for faces (verts don't need this) */
		if (r_dist_center && eed) {
			struct NearestEdgeUserData_ZBuf data;

			data.mval_fl[0] = vc->mval[0];
			data.mval_fl[1] = vc->mval[1];
			data.dist = FLT_MAX;
			data.edge_test = eed;

			ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

			mesh_foreachScreenEdge(vc, find_nearest_edge_center__doZBuf, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

			*r_dist_center = data.dist;
		}
		/* end exception */

		if (eed) {
			if (dist_test < *r_dist) {
				*r_dist = dist_test;
				return eed;
			}
		}
		return NULL;
	}
	else {
		struct NearestEdgeUserData data = {{0}};
		const struct NearestEdgeUserData_Hit *hit;
		/* interpolate along the edge before doing a clipping plane test */
		const eV3DProjTest clip_flag = V3D_PROJ_TEST_CLIP_DEFAULT & ~V3D_PROJ_TEST_CLIP_BB;

		static int prev_select_index = 0;
		static const BMEdge *prev_select_elem = NULL;

		if ((use_cycle == false) ||
		    (prev_select_elem && (prev_select_elem != BM_edge_at_index_find_or_table(bm, prev_select_index))))
		{
			prev_select_index = 0;
			prev_select_elem = NULL;
		}

		data.vc = *vc;
		data.mval_fl[0] = vc->mval[0];
		data.mval_fl[1] = vc->mval[1];
		data.use_select_bias = use_select_bias;
		data.use_cycle = use_cycle;
		data.hit.dist      = data.hit_cycle.dist = \
		data.hit.dist_bias = data.hit_cycle.dist_bias = *r_dist;
		data.cycle_index_prev = prev_select_index;

		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
		mesh_foreachScreenEdge(vc, find_nearest_edge__doClosest, &data, clip_flag);

		hit = (data.use_cycle && data.hit_cycle.edge) ? &data.hit_cycle : &data.hit;
		*r_dist = hit->dist;
		if (r_dist_center) {
			*r_dist_center = hit->dist_center;
		}

		prev_select_elem = hit->edge;
		prev_select_index = hit->index;

		return hit->edge;
	}
}

BMEdge *EDBM_edge_find_nearest(
        ViewContext *vc, float *r_dist)
{
	return EDBM_edge_find_nearest_ex(vc, r_dist, NULL, false, false, NULL);
}

/* find the distance to the face we already have */
struct NearestFaceUserData_ZBuf {
	float mval_fl[2];
	float dist;
	const BMFace *face_test;
};

static void find_nearest_face_center__doZBuf(void *userData, BMFace *efa, const float screen_co[2], int UNUSED(index))
{
	struct NearestFaceUserData_ZBuf *data = userData;

	if (efa == data->face_test) {
		const float dist_test = len_manhattan_v2v2(data->mval_fl, screen_co);

		if (dist_test < data->dist) {
			data->dist = dist_test;
		}
	}
}


struct NearestFaceUserData_Hit {
	float   dist;
	float   dist_bias;
	int     index;
	BMFace *face;
};

struct NearestFaceUserData {
	float mval_fl[2];
	bool use_select_bias;
	bool use_cycle;
	int cycle_index_prev;

	struct NearestFaceUserData_Hit hit;
	struct NearestFaceUserData_Hit hit_cycle;
};

static void findnearestface__doClosest(void *userData, BMFace *efa, const float screen_co[2], int index)
{
	struct NearestFaceUserData *data = userData;
	float dist_test, dist_test_bias;

	dist_test = dist_test_bias = len_manhattan_v2v2(data->mval_fl, screen_co);

	if (data->use_select_bias && BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
		dist_test_bias += FIND_NEAR_SELECT_BIAS;
	}

	if (dist_test_bias < data->hit.dist_bias) {
		data->hit.dist_bias = dist_test_bias;
		data->hit.dist = dist_test;
		data->hit.index = index;
		data->hit.face = efa;
	}

	if (data->use_cycle) {
		if ((data->hit_cycle.face == NULL) &&
		    (index > data->cycle_index_prev) &&
		    (dist_test_bias < FIND_NEAR_CYCLE_THRESHOLD_MIN))
		{
			data->hit_cycle.dist_bias = dist_test_bias;
			data->hit_cycle.dist = dist_test;
			data->hit_cycle.index = index;
			data->hit_cycle.face = efa;
		}
	}
}


BMFace *EDBM_face_find_nearest_ex(
        ViewContext *vc, float *r_dist,
        float *r_dist_center,
        const bool use_select_bias, const bool use_cycle,
        BMFace **r_efa_zbuf)
{
	BMesh *bm = vc->em->bm;

	if (V3D_IS_ZBUF(vc->v3d)) {
		float dist_test = 0.0f;
		unsigned int index;
		BMFace *efa;

		ED_view3d_backbuf_validate(vc);

		index = ED_view3d_backbuf_sample(vc, vc->mval[0], vc->mval[1]);
		efa = index ? BM_face_at_index_find_or_table(bm, index - 1) : NULL;
		
		if (r_efa_zbuf) {
			*r_efa_zbuf = efa;
		}

		/* exception for faces (verts don't need this) */
		if (r_dist_center && efa) {
			struct NearestFaceUserData_ZBuf data;

			data.mval_fl[0] = vc->mval[0];
			data.mval_fl[1] = vc->mval[1];
			data.dist = FLT_MAX;
			data.face_test = efa;

			ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

			mesh_foreachScreenFace(vc, find_nearest_face_center__doZBuf, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

			*r_dist_center = data.dist;
		}
		/* end exception */

		if (efa) {
			if (dist_test < *r_dist) {
				*r_dist = dist_test;
				return efa;
			}
		}
		return NULL;
	}
	else {
		struct NearestFaceUserData data = {{0}};
		const struct NearestFaceUserData_Hit *hit;
		const eV3DProjTest clip_flag = V3D_PROJ_TEST_CLIP_DEFAULT;

		static int prev_select_index = 0;
		static const BMFace *prev_select_elem = NULL;

		if ((use_cycle == false) ||
		    (prev_select_elem && (prev_select_elem != BM_face_at_index_find_or_table(bm, prev_select_index))))
		{
			prev_select_index = 0;
			prev_select_elem = NULL;
		}

		data.mval_fl[0] = vc->mval[0];
		data.mval_fl[1] = vc->mval[1];
		data.use_select_bias = use_select_bias;
		data.use_cycle = use_cycle;
		data.hit.dist      = data.hit_cycle.dist = \
		data.hit.dist_bias = data.hit_cycle.dist_bias = *r_dist;
		data.cycle_index_prev = prev_select_index;

		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
		mesh_foreachScreenFace(vc, findnearestface__doClosest, &data, clip_flag);

		hit = (data.use_cycle && data.hit_cycle.face) ? &data.hit_cycle : &data.hit;
		*r_dist = hit->dist;
		if (r_dist_center) {
			*r_dist_center = hit->dist;
		}

		prev_select_elem = hit->face;
		prev_select_index = hit->index;

		return hit->face;
	}
}

BMFace *EDBM_face_find_nearest(ViewContext *vc, float *r_dist)
{
	return EDBM_face_find_nearest_ex(vc, r_dist, NULL, false, false, NULL);
}

#undef FIND_NEAR_SELECT_BIAS
#undef FIND_NEAR_CYCLE_THRESHOLD_MIN


/* best distance based on screen coords. 
 * use em->selectmode to define how to use 
 * selected vertices and edges get disadvantage
 * return 1 if found one
 */
static int unified_findnearest(ViewContext *vc, BMVert **r_eve, BMEdge **r_eed, BMFace **r_efa)
{
	BMEditMesh *em = vc->em;
	static short mval_prev[2] = {-1, -1};
	/* only cycle while the mouse remains still */
	const bool use_cycle = ((mval_prev[0] == vc->mval[0]) && (mval_prev[1] == vc->mval[1]));
	const float dist_init = ED_view3d_select_dist_px();
	/* since edges select lines, we give dots advantage of ~20 pix */
	const float dist_margin = (dist_init / 2);
	float dist = dist_init;
	BMFace *efa_zbuf = NULL;
	BMEdge *eed_zbuf = NULL;
	
	BMVert *eve = NULL;
	BMEdge *eed = NULL;
	BMFace *efa = NULL;


	/* no afterqueue (yet), so we check it now, otherwise the em_xxxofs indices are bad */
	ED_view3d_backbuf_validate(vc);

	if ((dist > 0.0f) && em->selectmode & SCE_SELECT_FACE) {
		float dist_center = 0.0f;
		float *dist_center_p = (em->selectmode & (SCE_SELECT_EDGE | SCE_SELECT_VERTEX)) ? &dist_center : NULL;
		efa = EDBM_face_find_nearest_ex(vc, &dist, dist_center_p, true, use_cycle, &efa_zbuf);
		if (efa && dist_center_p) {
			dist = min_ff(dist_margin, dist_center);
		}
	}

	if ((dist > 0.0f) && (em->selectmode & SCE_SELECT_EDGE)) {
		float dist_center = 0.0f;
		float *dist_center_p = (em->selectmode & SCE_SELECT_VERTEX) ? &dist_center : NULL;
		eed = EDBM_edge_find_nearest_ex(vc, &dist, dist_center_p, true, use_cycle, &eed_zbuf);
		if (eed && dist_center_p) {
			dist = min_ff(dist_margin, dist_center);
		}
	}

	if ((dist > 0.0f) && em->selectmode & SCE_SELECT_VERTEX) {
		eve = EDBM_vert_find_nearest_ex(vc, &dist, true, use_cycle);
	}

	/* return only one of 3 pointers, for frontbuffer redraws */
	if (eve) {
		efa = NULL; eed = NULL;
	}
	else if (eed) {
		efa = NULL;
	}

	/* there may be a face under the cursor, who's center if too far away
	 * use this if all else fails, it makes sense to select this */
	if ((eve || eed || efa) == 0) {
		if (eed_zbuf) {
			eed = eed_zbuf;
		}
		else if (efa_zbuf) {
			efa = efa_zbuf;
		}
	}

	mval_prev[0] = vc->mval[0];
	mval_prev[1] = vc->mval[1];

	*r_eve = eve;
	*r_eed = eed;
	*r_efa = efa;

	return (eve || eed || efa);
}

/** \} */


/* ****************  SIMILAR "group" SELECTS. FACE, EDGE AND VERTEX ************** */
static EnumPropertyItem prop_similar_compare_types[] = {
	{SIM_CMP_EQ, "EQUAL", 0, "Equal", ""},
	{SIM_CMP_GT, "GREATER", 0, "Greater", ""},
	{SIM_CMP_LT, "LESS", 0, "Less", ""},

	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem prop_similar_types[] = {
	{SIMVERT_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMVERT_FACE, "FACE", 0, "Amount of Adjacent Faces", ""},
	{SIMVERT_VGROUP, "VGROUP", 0, "Vertex Groups", ""},
	{SIMVERT_EDGE, "EDGE", 0, "Amount of connecting edges", ""},

	{SIMEDGE_LENGTH, "LENGTH", 0, "Length", ""},
	{SIMEDGE_DIR, "DIR", 0, "Direction", ""},
	{SIMEDGE_FACE, "FACE", 0, "Amount of Faces Around an Edge", ""},
	{SIMEDGE_FACE_ANGLE, "FACE_ANGLE", 0, "Face Angles", ""},
	{SIMEDGE_CREASE, "CREASE", 0, "Crease", ""},
	{SIMEDGE_BEVEL, "BEVEL", 0, "Bevel", ""},
	{SIMEDGE_SEAM, "SEAM", 0, "Seam", ""},
	{SIMEDGE_SHARP, "SHARP", 0, "Sharpness", ""},
#ifdef WITH_FREESTYLE
	{SIMEDGE_FREESTYLE, "FREESTYLE_EDGE", 0, "Freestyle Edge Marks", ""},
#endif

	{SIMFACE_MATERIAL, "MATERIAL", 0, "Material", ""},
	{SIMFACE_IMAGE, "IMAGE", 0, "Image", ""},
	{SIMFACE_AREA, "AREA", 0, "Area", ""},
	{SIMFACE_SIDES, "SIDES", 0, "Polygon Sides", ""},
	{SIMFACE_PERIMETER, "PERIMETER", 0, "Perimeter", ""},
	{SIMFACE_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMFACE_COPLANAR, "COPLANAR", 0, "Co-planar", ""},
	{SIMFACE_SMOOTH, "SMOOTH", 0, "Flat/Smooth", ""},
#ifdef WITH_FREESTYLE
	{SIMFACE_FREESTYLE, "FREESTYLE_FACE", 0, "Freestyle Face Marks", ""},
#endif

	{0, NULL, 0, NULL, NULL}
};

/* selects new faces/edges/verts based on the existing selection */

static int similar_face_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;

	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op,
	             "similar_faces faces=%hf type=%i thresh=%f compare=%i",
	             BM_ELEM_SELECT, type, thresh, compare);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}	

/* ***************************************************** */

/* EDGE GROUP */

/* wrap the above function but do selection flushing edge to face */
static int similar_edge_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;

	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op,
	             "similar_edges edges=%he type=%i thresh=%f compare=%i",
	             BM_ELEM_SELECT, type, thresh, compare);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "edges.out", BM_EDGE, BM_ELEM_SELECT, true);
	EDBM_selectmode_flush(em);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

/* ********************************* */

/*
 * VERT GROUP
 * mode 1: same normal
 * mode 2: same number of face users
 * mode 3: same vertex groups
 */
static int similar_vert_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;
	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op,
	             "similar_verts verts=%hv type=%i thresh=%f compare=%i",
	             BM_ELEM_SELECT, type, thresh, compare);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_selectmode_flush(em);

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

static int edbm_select_similar_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "threshold");

	const int type = RNA_enum_get(op->ptr, "type");

	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_set(op->ptr, prop, ts->select_thresh);
	}
	else {
		ts->select_thresh = RNA_property_float_get(op->ptr, prop);
	}

	if      (type < 100) return similar_vert_select_exec(C, op);
	else if (type < 200) return similar_edge_select_exec(C, op);
	else                 return similar_face_select_exec(C, op);
}

static EnumPropertyItem *select_similar_type_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop),
                                                   bool *r_free)
{
	Object *obedit;

	if (!C) /* needed for docs and i18n tools */
		return prop_similar_types;

	obedit = CTX_data_edit_object(C);

	if (obedit && obedit->type == OB_MESH) {
		EnumPropertyItem *item = NULL;
		int a, totitem = 0;
		BMEditMesh *em = BKE_editmesh_from_object(obedit);

		if (em->selectmode & SCE_SELECT_VERTEX) {
			for (a = SIMVERT_NORMAL; a < SIMEDGE_LENGTH; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		else if (em->selectmode & SCE_SELECT_EDGE) {
			for (a = SIMEDGE_LENGTH; a < SIMFACE_MATERIAL; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		else if (em->selectmode & SCE_SELECT_FACE) {
#ifdef WITH_FREESTYLE
			const int a_end = SIMFACE_FREESTYLE;
#else
			const int a_end = SIMFACE_SMOOTH;
#endif
			for (a = SIMFACE_MATERIAL; a <= a_end; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		RNA_enum_item_end(&item, &totitem);

		*r_free = true;

		return item;
	}

	return prop_similar_types;
}

void MESH_OT_select_similar(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Similar";
	ot->idname = "MESH_OT_select_similar";
	ot->description = "Select similar vertices, edges or faces by property types";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = edbm_select_similar_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	prop = ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, SIMVERT_NORMAL, "Type", "");
	RNA_def_enum_funcs(prop, select_similar_type_itemf);

	RNA_def_enum(ot->srna, "compare", prop_similar_compare_types, SIM_CMP_EQ, "Compare", "");

	RNA_def_float(ot->srna, "threshold", 0.0f, 0.0f, 1.0f, "Threshold", "", 0.0f, 1.0f);
}


/* -------------------------------------------------------------------- */
/* Select Similar Regions */

static int edbm_select_similar_region_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	bool changed = false;

	/* group vars */
	int *groups_array;
	int (*group_index)[2];
	int group_tot;
	int i;

	if (bm->totfacesel < 2) {
		BKE_report(op->reports, RPT_ERROR, "No face regions selected");
		return OPERATOR_CANCELLED;
	}

	groups_array = MEM_mallocN(sizeof(*groups_array) * bm->totfacesel, __func__);
	group_tot = BM_mesh_calc_face_groups(bm, groups_array, &group_index,
	                                     NULL, NULL,
	                                     BM_ELEM_SELECT, BM_VERT);

	BM_mesh_elem_table_ensure(bm, BM_FACE);

	for (i = 0; i < group_tot; i++) {
		ListBase faces_regions;
		int tot;

		const int fg_sta = group_index[i][0];
		const int fg_len = group_index[i][1];
		int j;
		BMFace **fg = MEM_mallocN(sizeof(*fg) * fg_len, __func__);


		for (j = 0; j < fg_len; j++) {
			fg[j] = BM_face_at_index(bm, groups_array[fg_sta + j]);
		}

		tot = BM_mesh_region_match(bm, fg, fg_len, &faces_regions);

		MEM_freeN(fg);

		if (tot) {
			LinkData *link;
			while ((link = BLI_pophead(&faces_regions))) {
				BMFace *f, **faces = link->data;
				while ((f = *(faces++))) {
					BM_face_select_set(bm, f, true);
				}
				MEM_freeN(link->data);
				MEM_freeN(link);

				changed = true;
			}
		}
	}

	MEM_freeN(groups_array);
	MEM_freeN(group_index);

	if (changed) {
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);
	}
	else {
		BKE_report(op->reports, RPT_WARNING, "No matching face regions found");
	}

	return OPERATOR_FINISHED;
}

void MESH_OT_select_similar_region(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Similar Regions";
	ot->idname = "MESH_OT_select_similar_region";
	ot->description = "Select similar face regions to the current selection";

	/* api callbacks */
	ot->exec = edbm_select_similar_region_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* ****************  Mode Select *************** */

static int edbm_select_mode_exec(bContext *C, wmOperator *op)
{
	const int type        = RNA_enum_get(op->ptr,    "type");
	const int action      = RNA_enum_get(op->ptr,    "action");
	const bool use_extend = RNA_boolean_get(op->ptr, "use_extend");
	const bool use_expand = RNA_boolean_get(op->ptr, "use_expand");

	if (EDBM_selectmode_toggle(C, type, action, use_extend, use_expand)) {
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static int edbm_select_mode_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	/* detecting these options based on shift/ctrl here is weak, but it's done
	 * to make this work when clicking buttons or menus */
	if (!RNA_struct_property_is_set(op->ptr, "use_extend"))
		RNA_boolean_set(op->ptr, "use_extend", event->shift);
	if (!RNA_struct_property_is_set(op->ptr, "use_expand"))
		RNA_boolean_set(op->ptr, "use_expand", event->ctrl);

	return edbm_select_mode_exec(C, op);
}

void MESH_OT_select_mode(wmOperatorType *ot)
{
	PropertyRNA *prop;

	static EnumPropertyItem elem_items[] = {
		{SCE_SELECT_VERTEX, "VERT", ICON_VERTEXSEL, "Vertices", ""},
		{SCE_SELECT_EDGE,   "EDGE", ICON_EDGESEL, "Edges", ""},
		{SCE_SELECT_FACE,   "FACE", ICON_FACESEL, "Faces", ""},
		{0, NULL, 0, NULL, NULL},
	};

	static EnumPropertyItem actions_items[] = {
		{0, "DISABLE", 0, "Disable", "Disable selected markers"},
		{1, "ENABLE", 0, "Enable", "Enable selected markers"},
		{2, "TOGGLE", 0, "Toggle", "Toggle disabled flag for selected markers"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select Mode";
	ot->idname = "MESH_OT_select_mode";
	ot->description = "Change selection mode";

	/* api callbacks */
	ot->invoke = edbm_select_mode_invoke;
	ot->exec = edbm_select_mode_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_boolean(ot->srna, "use_extend", false, "Extend", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "use_expand", false, "Expand", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	ot->prop = prop = RNA_def_enum(ot->srna, "type", elem_items, 0, "Type", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	RNA_def_enum(ot->srna, "action", actions_items, 2, "Action", "Selection action to execute");
}

/* ***************************************************** */

/* ****************  LOOP SELECTS *************** */

static void walker_select_count(BMEditMesh *em, int walkercode, void *start, const bool select, const bool select_mix,
                                int *r_totsel, int *r_totunsel)
{
	BMesh *bm = em->bm;
	BMElem *ele;
	BMWalker walker;
	int tot[2] = {0, 0};

	BMW_init(&walker, bm, walkercode,
	         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
	         BMW_FLAG_TEST_HIDDEN,
	         BMW_NIL_LAY);

	for (ele = BMW_begin(&walker, start); ele; ele = BMW_step(&walker)) {
		tot[(BM_elem_flag_test_bool(ele, BM_ELEM_SELECT) != select)] += 1;

		if (!select_mix && tot[0] && tot[1]) {
			tot[0] = tot[1] = -1;
			break;
		}
	}

	*r_totsel = tot[0];
	*r_totunsel = tot[1];

	BMW_end(&walker);
}

static void walker_select(BMEditMesh *em, int walkercode, void *start, const bool select)
{
	BMesh *bm = em->bm;
	BMElem *ele;
	BMWalker walker;

	BMW_init(&walker, bm, walkercode,
	         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
	         BMW_FLAG_TEST_HIDDEN,
	         BMW_NIL_LAY);

	for (ele = BMW_begin(&walker, start); ele; ele = BMW_step(&walker)) {
		if (!select) {
			BM_select_history_remove(bm, ele);
		}
		BM_elem_select_set(bm, ele, select);
	}
	BMW_end(&walker);
}

static int edbm_loop_multiselect_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMEdge *eed;
	BMEdge **edarray;
	int edindex;
	const bool is_ring = RNA_boolean_get(op->ptr, "ring");
	
	BMIter iter;
	int totedgesel = 0;

	BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
			totedgesel++;
		}
	}
	
	edarray = MEM_mallocN(sizeof(BMEdge *) * totedgesel, "edge array");
	edindex = 0;
	
	BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
			edarray[edindex] = eed;
			edindex++;
		}
	}
	
	if (is_ring) {
		for (edindex = 0; edindex < totedgesel; edindex += 1) {
			eed = edarray[edindex];
			walker_select(em, BMW_EDGERING, eed, true);
		}
		EDBM_selectmode_flush(em);
	}
	else {
		for (edindex = 0; edindex < totedgesel; edindex += 1) {
			eed = edarray[edindex];
			walker_select(em, BMW_EDGELOOP, eed, true);
		}
		EDBM_selectmode_flush(em);
	}
	MEM_freeN(edarray);
//	if (EM_texFaceCheck())
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_loop_multi_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Multi Select Loops";
	ot->idname = "MESH_OT_loop_multi_select";
	ot->description = "Select a loop of connected edges by connection type";
	
	/* api callbacks */
	ot->exec = edbm_loop_multiselect_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "ring", 0, "Ring", "");
}

		
/* ***************** MAIN MOUSE SELECTION ************** */


/* ***************** loop select (non modal) ************** */

static void mouse_mesh_loop_face(BMEditMesh *em, BMEdge *eed, bool select, bool select_clear)
{
	if (select_clear) {
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	}

	walker_select(em, BMW_FACELOOP, eed, select);
}

static void mouse_mesh_loop_edge_ring(BMEditMesh *em, BMEdge *eed, bool select, bool select_clear)
{
	if (select_clear) {
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	}

	walker_select(em, BMW_EDGERING, eed, select);
}

static void mouse_mesh_loop_edge(BMEditMesh *em, BMEdge *eed, bool select, bool select_clear, bool select_cycle)
{
	bool edge_boundary = false;

	/* cycle between BMW_EDGELOOP / BMW_EDGEBOUNDARY  */
	if (select_cycle && BM_edge_is_boundary(eed)) {
		int tot[2];

		/* if the loops selected toggle the boundaries */
		walker_select_count(em, BMW_EDGELOOP, eed, select, false,
		                    &tot[0], &tot[1]);
		if (tot[select] == 0) {
			edge_boundary = true;

			/* if the boundaries selected, toggle back to the loop */
			walker_select_count(em, BMW_EDGEBOUNDARY, eed, select, false,
			                    &tot[0], &tot[1]);
			if (tot[select] == 0) {
				edge_boundary = false;
			}
		}
	}

	if (select_clear) {
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	}

	if (edge_boundary) {
		walker_select(em, BMW_EDGEBOUNDARY, eed, select);
	}
	else {
		walker_select(em, BMW_EDGELOOP, eed, select);
	}
}


static bool mouse_mesh_loop(bContext *C, const int mval[2], bool extend, bool deselect, bool toggle, bool ring)
{
	ViewContext vc;
	BMEditMesh *em;
	BMEdge *eed;
	bool select = true;
	bool select_clear = false;
	bool select_cycle = true;
	float dist = ED_view3d_select_dist_px() * 0.6666f;
	float mvalf[2];

	em_setup_viewcontext(C, &vc);
	mvalf[0] = (float)(vc.mval[0] = mval[0]);
	mvalf[1] = (float)(vc.mval[1] = mval[1]);
	em = vc.em;

	/* no afterqueue (yet), so we check it now, otherwise the bm_xxxofs indices are bad */
	ED_view3d_backbuf_validate(&vc);

	eed = EDBM_edge_find_nearest_ex(&vc, &dist, NULL, true, true, NULL);
	if (eed == NULL) {
		return false;
	}

	if (extend == false && deselect == false && toggle == false) {
		select_clear = true;
	}

	if (extend) {
		select = true;
	}
	else if (deselect) {
		select = false;
	}
	else if (select_clear || (BM_elem_flag_test(eed, BM_ELEM_SELECT) == 0)) {
		select = true;
	}
	else if (toggle) {
		select = false;
		select_cycle = false;
	}

	if (em->selectmode & SCE_SELECT_FACE) {
		mouse_mesh_loop_face(em, eed, select, select_clear);
	}
	else {
		if (ring) {
			mouse_mesh_loop_edge_ring(em, eed, select, select_clear);
		}
		else {
			mouse_mesh_loop_edge(em, eed, select, select_clear, select_cycle);
		}
	}

	EDBM_selectmode_flush(em);

	/* sets as active, useful for other tools */
	if (select) {
		if (em->selectmode & SCE_SELECT_VERTEX) {
			/* Find nearest vert from mouse
			 * (initialize to large values incase only one vertex can be projected) */
			float v1_co[2], v2_co[2];
			float length_1 = FLT_MAX;
			float length_2 = FLT_MAX;

			/* We can't be sure this has already been set... */
			ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

			if (ED_view3d_project_float_object(vc.ar, eed->v1->co, v1_co, V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK) {
				length_1 = len_squared_v2v2(mvalf, v1_co);
			}

			if (ED_view3d_project_float_object(vc.ar, eed->v2->co, v2_co, V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK) {
				length_2 = len_squared_v2v2(mvalf, v2_co);
			}
#if 0
			printf("mouse to v1: %f\nmouse to v2: %f\n", len_squared_v2v2(mvalf, v1_co),
			       len_squared_v2v2(mvalf, v2_co));
#endif
			BM_select_history_store(em->bm, (length_1 < length_2) ? eed->v1 : eed->v2);
		}
		else if (em->selectmode & SCE_SELECT_EDGE) {
			BM_select_history_store(em->bm, eed);
		}
		else if (em->selectmode & SCE_SELECT_FACE) {
			/* Select the face of eed which is the nearest of mouse. */
			BMFace *f, *efa = NULL;
			BMIter iterf;
			float best_dist = FLT_MAX;

			/* We can't be sure this has already been set... */
			ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

			BM_ITER_ELEM (f, &iterf, eed, BM_FACES_OF_EDGE) {
				if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
					float cent[3];
					float co[2], tdist;

					BM_face_calc_center_mean(f, cent);
					if (ED_view3d_project_float_object(vc.ar, cent, co, V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK) {
						tdist = len_squared_v2v2(mvalf, co);
						if (tdist < best_dist) {
/*							printf("Best face: %p (%f)\n", f, tdist);*/
							best_dist = tdist;
							efa = f;
						}
					}
				}
			}
			if (efa) {
				BM_mesh_active_face_set(em->bm, efa);
				BM_select_history_store(em->bm, efa);
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit);

	return true;
}

static int edbm_select_loop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	
	view3d_operator_needs_opengl(C);
	
	if (mouse_mesh_loop(C, event->mval,
	                    RNA_boolean_get(op->ptr, "extend"),
	                    RNA_boolean_get(op->ptr, "deselect"),
	                    RNA_boolean_get(op->ptr, "toggle"),
	                    RNA_boolean_get(op->ptr, "ring")))
	{
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MESH_OT_loop_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Loop Select";
	ot->idname = "MESH_OT_loop_select";
	ot->description = "Select a loop of connected edges";
	
	/* api callbacks */
	ot->invoke = edbm_select_loop_invoke;
	ot->poll = ED_operator_editmesh_region_view3d;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "Extend the selection");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Remove from the selection");
	RNA_def_boolean(ot->srna, "toggle", 0, "Toggle Select", "Toggle the selection");
	RNA_def_boolean(ot->srna, "ring", 0, "Select Ring", "Select ring");
}

void MESH_OT_edgering_select(wmOperatorType *ot)
{
	/* description */
	ot->name = "Edge Ring Select";
	ot->idname = "MESH_OT_edgering_select";
	ot->description = "Select an edge ring";
	
	/* callbacks */
	ot->invoke = edbm_select_loop_invoke;
	ot->poll = ED_operator_editmesh_region_view3d;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Remove from the selection");
	RNA_def_boolean(ot->srna, "toggle", 0, "Toggle Select", "Toggle the selection");
	RNA_def_boolean(ot->srna, "ring", 1, "Select Ring", "Select ring");
}

/* ******************** (de)select all operator **************** */
static int edbm_select_all_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const int action = RNA_enum_get(op->ptr, "action");

	switch (action) {
		case SEL_TOGGLE:
			EDBM_select_toggle_all(em);
			break;
		case SEL_SELECT:
			EDBM_flag_enable_all(em, BM_ELEM_SELECT);
			break;
		case SEL_DESELECT:
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
			break;
		case SEL_INVERT:
			EDBM_select_swap(em);
			EDBM_selectmode_flush(em);
			break;
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->idname = "MESH_OT_select_all";
	ot->description = "(De)select all vertices, edges or faces";

	/* api callbacks */
	ot->exec = edbm_select_all_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

static int edbm_faces_select_interior_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	if (EDBM_select_interior_faces(em)) {
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}

}

void MESH_OT_select_interior_faces(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Interior Faces";
	ot->idname = "MESH_OT_select_interior_faces";
	ot->description = "Select faces where all edges have more than 2 face users";

	/* api callbacks */
	ot->exec = edbm_faces_select_interior_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* ************************************************** */
/* here actual select happens */
/* gets called via generic mouse select operator */
bool EDBM_select_pick(bContext *C, const int mval[2], bool extend, bool deselect, bool toggle)
{
	ViewContext vc;
	BMVert *eve = NULL;
	BMEdge *eed = NULL;
	BMFace *efa = NULL;

	/* setup view context for argument to callbacks */
	em_setup_viewcontext(C, &vc);
	vc.mval[0] = mval[0];
	vc.mval[1] = mval[1];

	if (unified_findnearest(&vc, &eve, &eed, &efa)) {

		/* Deselect everything */
		if (extend == false && deselect == false && toggle == false)
			EDBM_flag_disable_all(vc.em, BM_ELEM_SELECT);

		if (efa) {
			if (extend) {
				/* set the last selected face */
				BM_mesh_active_face_set(vc.em->bm, efa);

				/* Work-around: deselect first, so we can guarantee it will */
				/* be active even if it was already selected */
				BM_select_history_remove(vc.em->bm, efa);
				BM_face_select_set(vc.em->bm, efa, false);
				BM_select_history_store(vc.em->bm, efa);
				BM_face_select_set(vc.em->bm, efa, true);
			}
			else if (deselect) {
				BM_select_history_remove(vc.em->bm, efa);
				BM_face_select_set(vc.em->bm, efa, false);
			}
			else {
				/* set the last selected face */
				BM_mesh_active_face_set(vc.em->bm, efa);

				if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
					BM_select_history_store(vc.em->bm, efa);
					BM_face_select_set(vc.em->bm, efa, true);
				}
				else if (toggle) {
					BM_select_history_remove(vc.em->bm, efa);
					BM_face_select_set(vc.em->bm, efa, false);
				}
			}
		}
		else if (eed) {
			if (extend) {
				/* Work-around: deselect first, so we can guarantee it will */
				/* be active even if it was already selected */
				BM_select_history_remove(vc.em->bm, eed);
				BM_edge_select_set(vc.em->bm, eed, false);
				BM_select_history_store(vc.em->bm, eed);
				BM_edge_select_set(vc.em->bm, eed, true);
			}
			else if (deselect) {
				BM_select_history_remove(vc.em->bm, eed);
				BM_edge_select_set(vc.em->bm, eed, false);
			}
			else {
				if (!BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
					BM_select_history_store(vc.em->bm, eed);
					BM_edge_select_set(vc.em->bm, eed, true);
				}
				else if (toggle) {
					BM_select_history_remove(vc.em->bm, eed);
					BM_edge_select_set(vc.em->bm, eed, false);
				}
			}
		}
		else if (eve) {
			if (extend) {
				/* Work-around: deselect first, so we can guarantee it will */
				/* be active even if it was already selected */
				BM_select_history_remove(vc.em->bm, eve);
				BM_vert_select_set(vc.em->bm, eve, false);
				BM_select_history_store(vc.em->bm, eve);
				BM_vert_select_set(vc.em->bm, eve, true);
			}
			else if (deselect) {
				BM_select_history_remove(vc.em->bm, eve);
				BM_vert_select_set(vc.em->bm, eve, false);
			}
			else {
				if (!BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
					BM_select_history_store(vc.em->bm, eve);
					BM_vert_select_set(vc.em->bm, eve, true);
				}
				else if (toggle) {
					BM_select_history_remove(vc.em->bm, eve);
					BM_vert_select_set(vc.em->bm, eve, false);
				}
			}
		}

		EDBM_selectmode_flush(vc.em);

		/* change active material on object */
		if (efa && efa->mat_nr != vc.obedit->actcol - 1) {
			vc.obedit->actcol = efa->mat_nr + 1;
			vc.em->mat_nr = efa->mat_nr;

			WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, NULL);

		}

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit);
		return true;
	}

	return false;
}

static void edbm_strip_selections(BMEditMesh *em)
{
	BMEditSelection *ese, *nextese;

	if (!(em->selectmode & SCE_SELECT_VERTEX)) {
		ese = em->bm->selected.first;
		while (ese) {
			nextese = ese->next;
			if (ese->htype == BM_VERT) BLI_freelinkN(&(em->bm->selected), ese);
			ese = nextese;
		}
	}
	if (!(em->selectmode & SCE_SELECT_EDGE)) {
		ese = em->bm->selected.first;
		while (ese) {
			nextese = ese->next;
			if (ese->htype == BM_EDGE) BLI_freelinkN(&(em->bm->selected), ese);
			ese = nextese;
		}
	}
	if (!(em->selectmode & SCE_SELECT_FACE)) {
		ese = em->bm->selected.first;
		while (ese) {
			nextese = ese->next;
			if (ese->htype == BM_FACE) BLI_freelinkN(&(em->bm->selected), ese);
			ese = nextese;
		}
	}
}

/* when switching select mode, makes sure selection is consistent for editing */
/* also for paranoia checks to make sure edge or face mode works */
void EDBM_selectmode_set(BMEditMesh *em)
{
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	BMIter iter;
	
	em->bm->selectmode = em->selectmode;

	edbm_strip_selections(em); /* strip BMEditSelections from em->selected that are not relevant to new mode */
	
	if (em->bm->totvertsel == 0 &&
	    em->bm->totedgesel == 0 &&
	    em->bm->totfacesel == 0)
	{
		return;
	}

	if (em->selectmode & SCE_SELECT_VERTEX) {
		if (em->bm->totvertsel) {
			EDBM_select_flush(em);
		}
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		/* deselect vertices, and select again based on edge select */
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			BM_vert_select_set(em->bm, eve, false);
		}

		if (em->bm->totedgesel) {
			BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
				if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
					BM_edge_select_set(em->bm, eed, true);
				}
			}

			/* selects faces based on edge status */
			EDBM_selectmode_flush(em);
		}
	}
	else if (em->selectmode & SCE_SELECT_FACE) {
		/* deselect eges, and select again based on face select */
		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			BM_edge_select_set(em->bm, eed, false);
		}

		if (em->bm->totfacesel) {
			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
					BM_face_select_set(em->bm, efa, true);
				}
			}
		}
	}
}

/**
 * Expand & Contract the Selection
 * (used when changing modes and Ctrl key held)
 *
 * Flush the selection up:
 * - vert -> edge
 * - vert -> face
 * - edge -> face
 *
 * Flush the selection down:
 * - face -> edge
 * - face -> vert
 * - edge -> vert
 */
void EDBM_selectmode_convert(BMEditMesh *em, const short selectmode_old, const short selectmode_new)
{
	BMesh *bm = em->bm;

	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	BMIter iter;

	/* first tag-to-select, then select --- this avoids a feedback loop */

	/* have to find out what the selectionmode was previously */
	if (selectmode_old == SCE_SELECT_VERTEX) {
		if (bm->totvertsel == 0) {
			/* pass */
		}
		else if (selectmode_new == SCE_SELECT_EDGE) {
			/* flush up (vert -> edge) */

			/* select all edges associated with every selected vert */
			BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
				BM_elem_flag_set(eed, BM_ELEM_TAG, BM_edge_is_any_vert_flag_test(eed, BM_ELEM_SELECT));
			}

			BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
				if (BM_elem_flag_test(eed, BM_ELEM_TAG)) {
					BM_edge_select_set(bm, eed, true);
				}
			}
		}
		else if (selectmode_new == SCE_SELECT_FACE) {
			/* flush up (vert -> face) */

			/* select all faces associated with every selected vert */
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				BM_elem_flag_set(efa, BM_ELEM_TAG, BM_face_is_any_vert_flag_test(efa, BM_ELEM_SELECT));
			}

			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
					BM_face_select_set(bm, efa, true);
				}
			}
		}
	}
	else if (selectmode_old == SCE_SELECT_EDGE) {
		if (bm->totedgesel == 0) {
			/* pass */
		}
		else if (selectmode_new == SCE_SELECT_FACE) {
			/* flush up (edge -> face) */

			/* select all faces associated with every selected edge */
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				BM_elem_flag_set(efa, BM_ELEM_TAG, BM_face_is_any_edge_flag_test(efa, BM_ELEM_SELECT));
			}

			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
					BM_face_select_set(bm, efa, true);
				}
			}
		}
		else if (selectmode_new == SCE_SELECT_VERTEX) {
			/* flush down (edge -> vert) */

			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				if (!BM_vert_is_all_edge_flag_test(eve, BM_ELEM_SELECT, true)) {
					BM_vert_select_set(bm, eve, false);
				}
			}
			/* deselect edges without both verts selected */
			BM_mesh_deselect_flush(bm);
		}
	}
	else if (selectmode_old == SCE_SELECT_FACE) {
		if (bm->totfacesel == 0) {
			/* pass */
		}
		else if (selectmode_new == SCE_SELECT_EDGE) {
			/* flush down (face -> edge) */

			BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
				if (!BM_edge_is_all_face_flag_test(eed, BM_ELEM_SELECT, true)) {
					BM_edge_select_set(bm, eed, false);
				}
			}
		}
		else if (selectmode_new == SCE_SELECT_VERTEX) {
			/* flush down (face -> vert) */

			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				if (!BM_vert_is_all_face_flag_test(eve, BM_ELEM_SELECT, true)) {
					BM_vert_select_set(bm, eve, false);
				}
			}
			/* deselect faces without verts selected */
			BM_mesh_deselect_flush(bm);
		}
	}
}

/* user facing function, does notification */
bool EDBM_selectmode_toggle(bContext *C, const short selectmode_new,
                            const int action, const bool use_extend, const bool use_expand)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = NULL;
	bool ret = false;

	if (obedit && obedit->type == OB_MESH) {
		em = BKE_editmesh_from_object(obedit);
	}

	if (em == NULL) {
		return ret;
	}

	switch (action) {
		case -1:
			/* already set */
			break;
		case 0:  /* disable */
			/* check we have something to do */
			if ((em->selectmode & selectmode_new) == 0) {
				return false;
			}
			em->selectmode &= ~selectmode_new;
			break;
		case 1:  /* enable */
			/* check we have something to do */
			if ((em->selectmode & selectmode_new) != 0) {
				return false;
			}
			em->selectmode |= selectmode_new;
			break;
		case 2:  /* toggle */
			/* can't disable this flag if its the only one set */
			if (em->selectmode == selectmode_new) {
				return false;
			}
			em->selectmode ^= selectmode_new;
			break;
		default:
			BLI_assert(0);
			break;
	}

	if (use_extend == 0 || em->selectmode == 0) {
		if (use_expand) {
			const short selmode_max = highest_order_bit_s(ts->selectmode);
			EDBM_selectmode_convert(em, selmode_max, selectmode_new);
		}
	}

	switch (selectmode_new) {
		case SCE_SELECT_VERTEX:
			if (use_extend == 0 || em->selectmode == 0) {
				em->selectmode = SCE_SELECT_VERTEX;
			}
			ts->selectmode = em->selectmode;
			EDBM_selectmode_set(em);
			ret = true;
			break;
		case SCE_SELECT_EDGE:
			if (use_extend == 0 || em->selectmode == 0) {
				em->selectmode = SCE_SELECT_EDGE;
			}
			ts->selectmode = em->selectmode;
			EDBM_selectmode_set(em);
			ret = true;
			break;
		case SCE_SELECT_FACE:
			if (use_extend == 0 || em->selectmode == 0) {
				em->selectmode = SCE_SELECT_FACE;
			}
			ts->selectmode = em->selectmode;
			EDBM_selectmode_set(em);
			ret = true;
			break;
		default:
			BLI_assert(0);
			break;
	}

	if (ret == true) {
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
		WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);
	}

	return ret;
}

/**
 * Use to disable a selectmode if its enabled, Using another mode as a fallback
 * if the disabled mode is the only mode set.
 *
 * \return true if the mode is changed.
 */
bool EDBM_selectmode_disable(Scene *scene, BMEditMesh *em,
                             const short selectmode_disable,
                             const short selectmode_fallback)
{
	/* note essential, but switch out of vertex mode since the
	 * selected regions wont be nicely isolated after flushing */
	if (em->selectmode & selectmode_disable) {
		if (em->selectmode == selectmode_disable) {
			em->selectmode = selectmode_fallback;
		}
		else {
			em->selectmode &= ~selectmode_disable;
		}
		scene->toolsettings->selectmode = em->selectmode;
		EDBM_selectmode_set(em);

		WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, scene);

		return true;
	}
	else {
		return false;
	}
}

void EDBM_deselect_by_material(BMEditMesh *em, const short index, const bool select)
{
	BMIter iter;
	BMFace *efa;

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
			continue;
		if (efa->mat_nr == index) {
			BM_face_select_set(em->bm, efa, select);
		}
	}
}

void EDBM_select_toggle_all(BMEditMesh *em) /* exported for UV */
{
	if (em->bm->totvertsel || em->bm->totedgesel || em->bm->totfacesel)
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	else
		EDBM_flag_enable_all(em, BM_ELEM_SELECT);
}

void EDBM_select_swap(BMEditMesh *em) /* exported for UV */
{
	BMIter iter;
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	
	if (em->bm->selectmode & SCE_SELECT_VERTEX) {
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN))
				continue;
			BM_vert_select_set(em->bm, eve, !BM_elem_flag_test(eve, BM_ELEM_SELECT));
		}
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN))
				continue;
			BM_edge_select_set(em->bm, eed, !BM_elem_flag_test(eed, BM_ELEM_SELECT));
		}
	}
	else {
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
				continue;
			BM_face_select_set(em->bm, efa, !BM_elem_flag_test(efa, BM_ELEM_SELECT));
		}

	}
//	if (EM_texFaceCheck())
}

bool EDBM_select_interior_faces(BMEditMesh *em)
{
	BMesh *bm = em->bm;
	BMIter iter;
	BMIter eiter;
	BMFace *efa;
	BMEdge *eed;
	bool ok;
	bool changed = false;

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
			continue;


		ok = true;
		BM_ITER_ELEM (eed, &eiter, efa, BM_EDGES_OF_FACE) {
			if (!BM_edge_face_count_is_over(eed, 2)) {
				ok = false;
				break;
			}
		}

		if (ok) {
			BM_face_select_set(bm, efa, true);
			changed = true;
		}
	}

	return changed;
}


/************************ Select Linked Operator *************************/

/* so we can have last-used default depend on selection mode (rare exception!) */
#define USE_LINKED_SELECT_DEFAULT_HACK

struct DelimitData {
	int cd_loop_type;
	int cd_loop_offset;
};

static bool select_linked_delimit_test(
        BMEdge *e, int delimit,
        const struct DelimitData *delimit_data)
{
	BLI_assert(delimit);

	if (delimit & BMO_DELIM_SEAM) {
		if (BM_elem_flag_test(e, BM_ELEM_SEAM)) {
			return true;
		}
	}

	if (delimit & BMO_DELIM_SHARP) {
		if (BM_elem_flag_test(e, BM_ELEM_SMOOTH) == 0) {
			return true;
		}
	}

	if (delimit & BMO_DELIM_NORMAL) {
		if (!BM_edge_is_contiguous(e)) {
			return true;
		}
	}

	if (delimit & BMO_DELIM_MATERIAL) {
		if (e->l && e->l->radial_next != e->l) {
			const short mat_nr = e->l->f->mat_nr;
			BMLoop *l_iter = e->l->radial_next;
			do {
				if (l_iter->f->mat_nr != mat_nr) {
					return true;
				}
			} while ((l_iter = l_iter->radial_next) != e->l);
		}
	}

	if (delimit & BMO_DELIM_UV) {
		if (BM_edge_is_contiguous_loop_cd(e, delimit_data->cd_loop_type, delimit_data->cd_loop_offset) == 0) {
			return true;
		}
	}

	return false;
}

#ifdef USE_LINKED_SELECT_DEFAULT_HACK
/**
 * Gets the default from the operator fallback to own last-used value
 * (selected based on mode)
 */
static int select_linked_delimit_default_from_op(wmOperator *op, BMEditMesh *em)
{
	static char delimit_last_store[2] = {0, BMO_DELIM_SEAM};
	int delimit_last_index = (em->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) == 0;
	char *delimit_last = &delimit_last_store[delimit_last_index];
	PropertyRNA *prop_delimit = RNA_struct_find_property(op->ptr, "delimit");
	int delimit;

	if (RNA_property_is_set(op->ptr, prop_delimit)) {
		delimit = RNA_property_enum_get(op->ptr, prop_delimit);
		*delimit_last = delimit;
	}
	else {
		delimit = *delimit_last;
		RNA_property_enum_set(op->ptr, prop_delimit, delimit);
	}
	return delimit;
}
#endif

static void select_linked_delimit_validate(BMesh *bm, int *delimit)
{
	if ((*delimit) & BMO_DELIM_UV) {
		if (!CustomData_has_layer(&bm->ldata, CD_MLOOPUV)) {
			(*delimit) &= ~BMO_DELIM_UV;
		}
	}
}

static void select_linked_delimit_begin(BMesh *bm, int delimit)
{
	struct DelimitData delimit_data = {0};

	if (delimit & BMO_DELIM_UV) {
		delimit_data.cd_loop_type = CD_MLOOPUV;
		delimit_data.cd_loop_offset = CustomData_get_offset(&bm->ldata, delimit_data.cd_loop_type);
		if (delimit_data.cd_loop_offset == -1) {
			delimit &= ~BMO_DELIM_UV;
		}
	}

	/* grr, shouldn't need to alloc BMO flags here */
	BM_mesh_elem_toolflags_ensure(bm);

	{
		BMIter iter;
		BMEdge *e;

		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			const bool is_walk_ok = (
			        (select_linked_delimit_test(e, delimit, &delimit_data) == false));

			BMO_edge_flag_set(bm, e, BMO_ELE_TAG, is_walk_ok);
		}
	}
}

static void select_linked_delimit_end(BMEditMesh *em)
{
	BMesh *bm = em->bm;

	BM_mesh_elem_toolflags_clear(bm);
}

static int edbm_select_linked_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMIter iter;
	BMWalker walker;

#ifdef USE_LINKED_SELECT_DEFAULT_HACK
	int delimit = select_linked_delimit_default_from_op(op, em);
#else
	int delimit = RNA_enum_get(op->ptr, "delimit");
#endif

	select_linked_delimit_validate(bm, &delimit);

	if (delimit) {
		select_linked_delimit_begin(em->bm, delimit);
	}

	if (em->selectmode & SCE_SELECT_VERTEX) {
		BMVert *v;

		BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
			BM_elem_flag_set(v, BM_ELEM_TAG, BM_elem_flag_test(v, BM_ELEM_SELECT));
		}

		/* exclude all delimited verts */
		if (delimit) {
			BMEdge *e;
			BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
				if (!BMO_edge_flag_test(bm, e, BMO_ELE_TAG)) {
					BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
					BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
				}
			}
		}

		BMW_init(&walker, em->bm, delimit ? BMW_LOOP_SHELL_WIRE : BMW_VERT_SHELL,
		         BMW_MASK_NOP, delimit ? BMO_ELE_TAG : BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_TEST_HIDDEN,
		         BMW_NIL_LAY);

		if (delimit) {
			BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
				if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
					BMElem *ele_walk;
					BMW_ITER (ele_walk, &walker, v) {
						if (ele_walk->head.htype == BM_LOOP) {
							BMVert *v_step = ((BMLoop *)ele_walk)->v;
							BM_vert_select_set(em->bm, v_step, true);
							BM_elem_flag_disable(v_step, BM_ELEM_TAG);
						}
						else {
							BMEdge *e_step = (BMEdge *)ele_walk;
							BLI_assert(ele_walk->head.htype == BM_EDGE);
							BM_edge_select_set(em->bm, e_step, true);
							BM_elem_flag_disable(e_step->v1, BM_ELEM_TAG);
							BM_elem_flag_disable(e_step->v2, BM_ELEM_TAG);
						}
					}
				}
			}
		}
		else {
			BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
				if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
					BMEdge *e_walk;
					BMW_ITER (e_walk, &walker, v) {
						BM_edge_select_set(em->bm, e_walk, true);
						BM_elem_flag_disable(e_walk, BM_ELEM_TAG);
					}
				}
			}
		}

		BMW_end(&walker);

		EDBM_selectmode_flush(em);
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BMEdge *e;

		if (delimit) {
			BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
				BM_elem_flag_set(
				        e, BM_ELEM_TAG,
				        (BM_elem_flag_test(e, BM_ELEM_SELECT) && BMO_edge_flag_test(bm, e, BMO_ELE_TAG)));
			}
		}
		else {
			BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
				BM_elem_flag_set(e, BM_ELEM_TAG, BM_elem_flag_test(e, BM_ELEM_SELECT));
			}
		}

		BMW_init(&walker, em->bm, delimit ? BMW_LOOP_SHELL_WIRE : BMW_VERT_SHELL,
		         BMW_MASK_NOP, delimit ? BMO_ELE_TAG : BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_TEST_HIDDEN,
		         BMW_NIL_LAY);

		if (delimit) {
			BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
				if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
					BMElem *ele_walk;
					BMW_ITER (ele_walk, &walker, e) {
						if (ele_walk->head.htype == BM_LOOP) {
							BMLoop *l_step = (BMLoop *)ele_walk;
							BM_edge_select_set(em->bm, l_step->e, true);
							BM_edge_select_set(em->bm, l_step->prev->e, true);
							BM_elem_flag_disable(l_step->e, BM_ELEM_TAG);
						}
						else {
							BMEdge *e_step = (BMEdge *)ele_walk;
							BLI_assert(ele_walk->head.htype == BM_EDGE);
							BM_edge_select_set(em->bm, e_step, true);
							BM_elem_flag_disable(e_step, BM_ELEM_TAG);
						}
					}
				}
			}
		}
		else {
			BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
				if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
					BMEdge *e_walk;
					BMW_ITER (e_walk, &walker, e) {
						BM_edge_select_set(em->bm, e_walk, true);
						BM_elem_flag_disable(e_walk, BM_ELEM_TAG);
					}
				}
			}
		}

		BMW_end(&walker);

		EDBM_selectmode_flush(em);
	}
	else {
		BMFace *f;

		BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_elem_flag_set(f, BM_ELEM_TAG, BM_elem_flag_test(f, BM_ELEM_SELECT));
		}

		BMW_init(&walker, bm, BMW_ISLAND,
		         BMW_MASK_NOP, delimit ? BMO_ELE_TAG : BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_TEST_HIDDEN,
		         BMW_NIL_LAY);

		BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
				BMFace *f_walk;
				BMW_ITER (f_walk, &walker, f) {
					BM_face_select_set(bm, f_walk, true);
					BM_elem_flag_disable(f_walk, BM_ELEM_TAG);
				}
			}
		}

		BMW_end(&walker);
	}

	if (delimit) {
		select_linked_delimit_end(em);
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_linked(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Linked All";
	ot->idname = "MESH_OT_select_linked";
	ot->description = "Select all vertices connected to the current selection";

	/* api callbacks */
	ot->exec = edbm_select_linked_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_enum_flag(ot->srna, "delimit", rna_enum_mesh_delimit_mode_items, BMO_DELIM_SEAM, "Delimit",
	                         "Delimit selected region");
#ifdef USE_LINKED_SELECT_DEFAULT_HACK
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
#else
	UNUSED_VARS(prop);
#endif
}

static int edbm_select_linked_pick_exec(bContext *C, wmOperator *op);

static void edbm_select_linked_pick_ex(BMEditMesh *em, BMElem *ele, bool sel, int delimit)
{
	BMesh *bm = em->bm;
	BMWalker walker;

	select_linked_delimit_validate(bm, &delimit);

	if (delimit) {
		select_linked_delimit_begin(bm, delimit);
	}

	/* Note: logic closely matches 'edbm_select_linked_exec', keep in sync */

	if (ele->head.htype == BM_VERT) {
		BMVert *eve = (BMVert *)ele;

		BMW_init(&walker, bm, delimit ? BMW_LOOP_SHELL_WIRE : BMW_VERT_SHELL,
		         BMW_MASK_NOP, delimit ? BMO_ELE_TAG : BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_TEST_HIDDEN,
		         BMW_NIL_LAY);

		if (delimit) {
			BMElem *ele_walk;
			BMW_ITER (ele_walk, &walker, eve) {
				if (ele_walk->head.htype == BM_LOOP) {
					BMVert *v_step = ((BMLoop *)ele_walk)->v;
					BM_vert_select_set(bm, v_step, sel);
				}
				else {
					BMEdge *e_step = (BMEdge *)ele_walk;
					BLI_assert(ele_walk->head.htype == BM_EDGE);
					BM_edge_select_set(bm, e_step, sel);
				}
			}
		}
		else {
			BMEdge *e_walk;
			BMW_ITER (e_walk, &walker, eve) {
				BM_edge_select_set(bm, e_walk, sel);
			}
		}

		BMW_end(&walker);

		EDBM_selectmode_flush(em);
	}
	else if (ele->head.htype == BM_EDGE) {
		BMEdge *eed = (BMEdge *)ele;

		BMW_init(&walker, bm, delimit ? BMW_LOOP_SHELL_WIRE : BMW_VERT_SHELL,
		         BMW_MASK_NOP, delimit ? BMO_ELE_TAG : BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_TEST_HIDDEN,
		         BMW_NIL_LAY);

		if (delimit) {
			BMElem *ele_walk;
			BMW_ITER (ele_walk, &walker, eed) {
				if (ele_walk->head.htype == BM_LOOP) {
					BMEdge *e_step = ((BMLoop *)ele_walk)->e;
					BM_edge_select_set(bm, e_step, sel);
				}
				else {
					BMEdge *e_step = (BMEdge *)ele_walk;
					BLI_assert(ele_walk->head.htype == BM_EDGE);
					BM_edge_select_set(bm, e_step, sel);
				}
			}
		}
		else {
			BMEdge *e_walk;
			BMW_ITER (e_walk, &walker, eed) {
				BM_edge_select_set(bm, e_walk, sel);
			}
		}

		BMW_end(&walker);

		EDBM_selectmode_flush(em);
	}
	else if (ele->head.htype == BM_FACE) {
		BMFace *efa = (BMFace *)ele;

		BMW_init(&walker, bm, BMW_ISLAND,
		         BMW_MASK_NOP, delimit ? BMO_ELE_TAG : BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_TEST_HIDDEN,
		         BMW_NIL_LAY);

		{
			BMFace *f_walk;
			BMW_ITER (f_walk, &walker, efa) {
				BM_face_select_set(bm, f_walk, sel);
				BM_elem_flag_disable(f_walk, BM_ELEM_TAG);
			}
		}

		BMW_end(&walker);
	}

	if (delimit) {
		select_linked_delimit_end(em);
	}
}

static int edbm_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	ViewContext vc;
	BMEditMesh *em;
	BMesh *bm;
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	const bool sel = !RNA_boolean_get(op->ptr, "deselect");
	int index;

	if (RNA_struct_property_is_set(op->ptr, "index")) {
		return edbm_select_linked_pick_exec(C, op);
	}

	/* unified_finednearest needs ogl */
	view3d_operator_needs_opengl(C);

	/* setup view context for argument to callbacks */
	em_setup_viewcontext(C, &vc);
	em = vc.em;
	bm = em->bm;

	if (bm->totedge == 0) {
		return OPERATOR_CANCELLED;
	}

	vc.mval[0] = event->mval[0];
	vc.mval[1] = event->mval[1];

	/* return warning! */
	if (unified_findnearest(&vc, &eve, &eed, &efa) == 0) {
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);

		return OPERATOR_CANCELLED;
	}

#ifdef USE_LINKED_SELECT_DEFAULT_HACK
	int delimit = select_linked_delimit_default_from_op(op, em);
#else
	int delimit = RNA_enum_get(op->ptr, "delimit");
#endif

	BMElem *ele = EDBM_elem_from_selectmode(em, eve, eed, efa);

	edbm_select_linked_pick_ex(em, ele, sel, delimit);

	/* to support redo */
	BM_mesh_elem_index_ensure(bm, ele->head.htype);
	index = EDBM_elem_to_index_any(em, ele);

	RNA_int_set(op->ptr, "index", index);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);

	return OPERATOR_FINISHED;
}


static int edbm_select_linked_pick_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	int index;
	const bool sel = !RNA_boolean_get(op->ptr, "deselect");

	index = RNA_int_get(op->ptr, "index");
	if (index < 0 || index >= (bm->totvert + bm->totedge + bm->totface)) {
		return OPERATOR_CANCELLED;
	}

	BMElem *ele = EDBM_elem_from_index_any(em, index);

#ifdef USE_LINKED_SELECT_DEFAULT_HACK
	int delimit = select_linked_delimit_default_from_op(op, em);
#else
	int delimit = RNA_enum_get(op->ptr, "delimit");
#endif

	edbm_select_linked_pick_ex(em, ele, sel, delimit);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_linked_pick(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Linked";
	ot->idname = "MESH_OT_select_linked_pick";
	ot->description = "(De)select all vertices linked to the edge under the mouse cursor";
	
	/* api callbacks */
	ot->invoke = edbm_select_linked_pick_invoke;
	ot->exec = edbm_select_linked_pick_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "");
	prop = RNA_def_enum_flag(ot->srna, "delimit", rna_enum_mesh_delimit_mode_items, BMO_DELIM_SEAM, "Delimit",
	                         "Delimit selected region");
#ifdef USE_LINKED_SELECT_DEFAULT_HACK
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
#endif

	/* use for redo */
	prop = RNA_def_int(ot->srna, "index", -1, -1, INT_MAX, "", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}


static int edbm_select_face_by_sides_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMIter iter;
	const int numverts = RNA_int_get(op->ptr, "number");
	const int type = RNA_enum_get(op->ptr, "type");

	if (!RNA_boolean_get(op->ptr, "extend"))
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {

		bool select;

		switch (type) {
			case 0:
				select = (efa->len < numverts);
				break;
			case 1:
				select = (efa->len == numverts);
				break;
			case 2:
				select = (efa->len > numverts);
				break;
			case 3:
				select = (efa->len != numverts);
				break;
			default:
				BLI_assert(0);
				select = false;
				break;
		}

		if (select) {
			BM_face_select_set(em->bm, efa, true);
		}
	}

	EDBM_selectmode_flush(em);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_face_by_sides(wmOperatorType *ot)
{
	static const EnumPropertyItem type_items[] = {
		{0, "LESS", 0, "Less Than", ""},
		{1, "EQUAL", 0, "Equal To", ""},
		{2, "GREATER", 0, "Greater Than", ""},
		{3, "NOTEQUAL", 0, "Not Equal To", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select Faces by Sides";
	ot->description = "Select vertices or faces by the number of polygon sides";
	ot->idname = "MESH_OT_select_face_by_sides";

	/* api callbacks */
	ot->exec = edbm_select_face_by_sides_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "number", 4, 3, INT_MAX, "Number of Vertices", "", 3, INT_MAX);
	RNA_def_enum(ot->srna, "type", type_items, 1, "Type", "Type of comparison to make");
	RNA_def_boolean(ot->srna, "extend", true, "Extend", "Extend the selection");
}


static int edbm_select_loose_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMIter iter;

	if (!RNA_boolean_get(op->ptr, "extend"))
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	if (em->selectmode & SCE_SELECT_VERTEX) {
		BMVert *eve;
		BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
			if (!eve->e) {
				BM_vert_select_set(bm, eve, true);
			}
		}
	}

	if (em->selectmode & SCE_SELECT_EDGE) {
		BMEdge *eed;
		BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_edge_is_wire(eed)) {
				BM_edge_select_set(bm, eed, true);
			}
		}
	}

	if (em->selectmode & SCE_SELECT_FACE) {
		BMFace *efa;
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			BMIter liter;
			BMLoop *l;
			bool is_loose = true;
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (!BM_edge_is_boundary(l->e)) {
					is_loose = false;
					break;
				}
			}
			if (is_loose) {
				BM_face_select_set(bm, efa, true);
			}
		}
	}

	EDBM_selectmode_flush(em);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_loose(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Loose Geometry";
	ot->description = "Select loose geometry based on the selection mode";
	ot->idname = "MESH_OT_select_loose";

	/* api callbacks */
	ot->exec = edbm_select_loose_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}


static int edbm_select_mirror_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const int axis_flag = RNA_enum_get(op->ptr, "axis");
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	if (em->bm->totvert && em->bm->totvertsel) {
		int totmirr, totfail;

		for (int axis = 0; axis < 3; axis++) {
			if ((1 << axis) & axis_flag) {
				EDBM_select_mirrored(em, axis, extend, &totmirr, &totfail);
			}
		}

		if (totmirr) {
			EDBM_selectmode_flush(em);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
		}

		ED_mesh_report_mirror_ex(op, totmirr, totfail, em->bm->selectmode);
	}

	return OPERATOR_FINISHED;
}

void MESH_OT_select_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Mirror";
	ot->description = "Select mesh items at mirrored locations";
	ot->idname = "MESH_OT_select_mirror";

	/* api callbacks */
	ot->exec = edbm_select_mirror_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_enum_flag(ot->srna, "axis", rna_enum_axis_flag_xyz_items, (1 << 0), "Axis", "");

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the existing selection");
}

/* ******************** **************** */

static int edbm_select_more_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const bool use_face_step = RNA_boolean_get(op->ptr, "use_face_step");

	EDBM_select_more(em, use_face_step);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname = "MESH_OT_select_more";
	ot->description = "Select more vertices, edges or faces connected to initial selection";

	/* api callbacks */
	ot->exec = edbm_select_more_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "use_face_step", true, "Face Step", "Connected faces (instead of edges)");
}

static int edbm_select_less_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const bool use_face_step = RNA_boolean_get(op->ptr, "use_face_step");

	EDBM_select_less(em, use_face_step);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname = "MESH_OT_select_less";
	ot->description = "Deselect vertices, edges or faces at the boundary of each selection region";

	/* api callbacks */
	ot->exec = edbm_select_less_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "use_face_step", true, "Face Step", "Connected faces (instead of edges)");
}

/**
 * Check if we're connected to another selected efge.
 */
static bool bm_edge_is_select_isolated(BMEdge *e)
{
	BMIter viter;
	BMVert *v;

	BM_ITER_ELEM (v, &viter, e, BM_VERTS_OF_EDGE) {
		BMIter eiter;
		BMEdge *e_other;

		BM_ITER_ELEM (e_other, &eiter, v, BM_EDGES_OF_VERT) {
			if ((e_other != e) && BM_elem_flag_test(e_other, BM_ELEM_SELECT)) {
				return false;
			}
		}
	}
	return true;
}

/* Walk all reachable elements of the same type as h_act in breadth-first
 * order, starting from h_act. Deselects elements if the depth when they
 * are reached is not a multiple of "nth". */
static void walker_deselect_nth(
        BMEditMesh *em, const struct CheckerIntervalParams *op_params,
        BMHeader *h_act)
{
	BMElem *ele;
	BMesh *bm = em->bm;
	BMWalker walker;
	BMIter iter;
	int walktype = 0, itertype = 0, flushtype = 0;
	short mask_vert = 0, mask_edge = 0, mask_face = 0;

	/* No active element from which to start - nothing to do */
	if (h_act == NULL) {
		return;
	}

	/* Determine which type of iter, walker, and select flush to use
	 * based on type of the elements being deselected */
	switch (h_act->htype) {
		case BM_VERT:
			itertype = BM_VERTS_OF_MESH;
			walktype = BMW_CONNECTED_VERTEX;
			flushtype = SCE_SELECT_VERTEX;
			mask_vert = BMO_ELE_TAG;
			break;
		case BM_EDGE:
			/* When an edge has no connected-selected edges,
			 * use face-stepping (supports edge-rings) */
			itertype = BM_EDGES_OF_MESH;
			walktype = bm_edge_is_select_isolated((BMEdge *)h_act) ? BMW_FACE_SHELL : BMW_VERT_SHELL;
			flushtype = SCE_SELECT_EDGE;
			mask_edge = BMO_ELE_TAG;
			break;
		case BM_FACE:
			itertype = BM_FACES_OF_MESH;
			walktype = BMW_ISLAND;
			flushtype = SCE_SELECT_FACE;
			mask_face = BMO_ELE_TAG;
			break;
	}

	/* grr, shouldn't need to alloc BMO flags here */
	BM_mesh_elem_toolflags_ensure(bm);

	/* Walker restrictions uses BMO flags, not header flags,
	 * so transfer BM_ELEM_SELECT from HFlags onto a BMO flag layer. */
	BMO_push(bm, NULL);
	BM_ITER_MESH (ele, &iter, bm, itertype) {
		if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
			BMO_elem_flag_enable(bm, (BMElemF *)ele, BMO_ELE_TAG);
		}
	}

	/* Walk over selected elements starting at active */
	BMW_init(&walker, bm, walktype,
	         mask_vert, mask_edge, mask_face,
	         BMW_FLAG_NOP, /* don't use BMW_FLAG_TEST_HIDDEN here since we want to desel all */
	         BMW_NIL_LAY);

	/* use tag to avoid touching the same verts twice */
	BM_ITER_MESH (ele, &iter, bm, itertype) {
		BM_elem_flag_disable(ele, BM_ELEM_TAG);
	}

	BLI_assert(walker.order == BMW_BREADTH_FIRST);
	for (ele = BMW_begin(&walker, h_act); ele != NULL; ele = BMW_step(&walker)) {
		if (!BM_elem_flag_test(ele, BM_ELEM_TAG)) {
			/* Deselect elements that aren't at "nth" depth from active */
			const int depth = BMW_current_depth(&walker) - 1;
			if (WM_operator_properties_checker_interval_test(op_params, depth)) {
				BM_elem_select_set(bm, ele, false);
			}
			BM_elem_flag_enable(ele, BM_ELEM_TAG);
		}
	}
	BMW_end(&walker);

	BMO_pop(bm);

	/* Flush selection up */
	EDBM_selectmode_flush_ex(em, flushtype);
}

static void deselect_nth_active(BMEditMesh *em, BMVert **r_eve, BMEdge **r_eed, BMFace **r_efa)
{
	BMIter iter;
	BMElem *ele;

	*r_eve = NULL;
	*r_eed = NULL;
	*r_efa = NULL;

	EDBM_selectmode_flush(em);
	ele = BM_mesh_active_elem_get(em->bm);

	if (ele && BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
		switch (ele->head.htype) {
			case BM_VERT:
				*r_eve = (BMVert *)ele;
				return;
			case BM_EDGE:
				*r_eed = (BMEdge *)ele;
				return;
			case BM_FACE:
				*r_efa = (BMFace *)ele;
				return;
		}
	}

	if (em->selectmode & SCE_SELECT_VERTEX) {
		BMVert *v;
		BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				*r_eve = v;
				return;
			}
		}
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BMEdge *e;
		BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
				*r_eed = e;
				return;
			}
		}
	}
	else if (em->selectmode & SCE_SELECT_FACE) {
		BMFace *f = BM_mesh_active_face_get(em->bm, true, false);
		if (f && BM_elem_flag_test(f, BM_ELEM_SELECT)) {
			*r_efa = f;
			return;
		}
	}
}

static bool edbm_deselect_nth(BMEditMesh *em, const struct CheckerIntervalParams *op_params)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	deselect_nth_active(em, &v, &e, &f);

	if (v) {
		walker_deselect_nth(em, op_params, &v->head);
		return true;
	}
	else if (e) {
		walker_deselect_nth(em, op_params, &e->head);
		return true;
	}
	else if (f) {
		walker_deselect_nth(em, op_params, &f->head);
		return true;
	}

	return false;
}

static int edbm_select_nth_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	struct CheckerIntervalParams op_params;

	WM_operator_properties_checker_interval_from_op(op, &op_params);

	if (edbm_deselect_nth(em, &op_params) == false) {
		BKE_report(op->reports, RPT_ERROR, "Mesh has no active vert/edge/face");
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}


void MESH_OT_select_nth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Checker Deselect";
	ot->idname = "MESH_OT_select_nth";
	ot->description = "Deselect every Nth element starting from the active vertex, edge or face";

	/* api callbacks */
	ot->exec = edbm_select_nth_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_checker_interval(ot, false);
}

void em_setup_viewcontext(bContext *C, ViewContext *vc)
{
	view3d_set_viewcontext(C, vc);
	
	if (vc->obedit) {
		vc->em = BKE_editmesh_from_object(vc->obedit);
	}
}


static int edbm_select_sharp_edges_exec(bContext *C, wmOperator *op)
{
	/* Find edges that have exactly two neighboring faces,
	 * check the angle between those faces, and if angle is
	 * small enough, select the edge
	 */
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMIter iter;
	BMEdge *e;
	BMLoop *l1, *l2;
	const float angle_limit_cos = cosf(RNA_float_get(op->ptr, "sharpness"));

	BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_HIDDEN) == false &&
		    BM_edge_loop_pair(e, &l1, &l2))
		{
			/* edge has exactly two neighboring faces, check angle */
			const float angle_cos = dot_v3v3(l1->f->no, l2->f->no);

			if (angle_cos < angle_limit_cos) {
				BM_edge_select_set(em->bm, e, true);
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_edges_select_sharp(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Sharp Edges";
	ot->description = "Select all sharp-enough edges";
	ot->idname = "MESH_OT_edges_select_sharp";
	
	/* api callbacks */
	ot->exec = edbm_select_sharp_edges_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	prop = RNA_def_float_rotation(ot->srna, "sharpness", 0, NULL, DEG2RADF(0.01f), DEG2RADF(180.0f),
	                              "Sharpness", "", DEG2RADF(1.0f), DEG2RADF(180.0f));
	RNA_def_property_float_default(prop, DEG2RADF(30.0f));
}

static int edbm_select_linked_flat_faces_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;

	BLI_LINKSTACK_DECLARE(stack, BMFace *);

	BMIter iter, liter, liter2;
	BMFace *f;
	BMLoop *l, *l2;
	const float angle_limit_cos = cosf(RNA_float_get(op->ptr, "sharpness"));

	BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);

	BLI_LINKSTACK_INIT(stack);

	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if ((BM_elem_flag_test(f, BM_ELEM_HIDDEN) != 0) ||
		    (BM_elem_flag_test(f, BM_ELEM_TAG)    != 0) ||
		    (BM_elem_flag_test(f, BM_ELEM_SELECT) == 0))
		{
			continue;
		}

		BLI_assert(BLI_LINKSTACK_SIZE(stack) == 0);

		do {
			BM_face_select_set(bm, f, true);

			BM_elem_flag_enable(f, BM_ELEM_TAG);

			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				BM_ITER_ELEM (l2, &liter2, l, BM_LOOPS_OF_LOOP) {
					float angle_cos;

					if (BM_elem_flag_test(l2->f, BM_ELEM_TAG) ||
					    BM_elem_flag_test(l2->f, BM_ELEM_HIDDEN))
					{
						continue;
					}

					angle_cos = dot_v3v3(f->no, l2->f->no);

					if (angle_cos > angle_limit_cos) {
						BLI_LINKSTACK_PUSH(stack, l2->f);
					}
				}
			}
		} while ((f = BLI_LINKSTACK_POP(stack)));
	}

	BLI_LINKSTACK_FREE(stack);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_faces_select_linked_flat(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Linked Flat Faces";
	ot->description = "Select linked faces by angle";
	ot->idname = "MESH_OT_faces_select_linked_flat";
	
	/* api callbacks */
	ot->exec = edbm_select_linked_flat_faces_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	prop = RNA_def_float_rotation(ot->srna, "sharpness", 0, NULL, DEG2RADF(0.01f), DEG2RADF(180.0f),
	                              "Sharpness", "", DEG2RADF(1.0f), DEG2RADF(180.0f));
	RNA_def_property_float_default(prop, DEG2RADF(1.0f));
}

static int edbm_select_non_manifold_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMVert *v;
	BMEdge *e;
	BMIter iter;

	const bool use_wire = RNA_boolean_get(op->ptr, "use_wire");
	const bool use_boundary = RNA_boolean_get(op->ptr, "use_boundary");
	const bool use_multi_face = RNA_boolean_get(op->ptr, "use_multi_face");
	const bool use_non_contiguous = RNA_boolean_get(op->ptr, "use_non_contiguous");
	const bool use_verts = RNA_boolean_get(op->ptr, "use_verts");


	if (!RNA_boolean_get(op->ptr, "extend"))
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* Selects isolated verts, and edges that do not have 2 neighboring
	 * faces
	 */
	
	if (em->selectmode == SCE_SELECT_FACE) {
		BKE_report(op->reports, RPT_ERROR, "Does not work in face selection mode");
		return OPERATOR_CANCELLED;
	}
	
	if (use_verts) {
		BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
				if (!BM_vert_is_manifold(v)) {
					BM_vert_select_set(em->bm, v, true);
				}
			}
		}
	}
	
	if (use_wire || use_boundary || use_multi_face || use_non_contiguous) {
		BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
				if ((use_wire && BM_edge_is_wire(e)) ||
				    (use_boundary && BM_edge_is_boundary(e)) ||
				    (use_non_contiguous && (BM_edge_is_manifold(e) && !BM_edge_is_contiguous(e))) ||
				    (use_multi_face && (BM_edge_face_count_is_over(e, 2))))
				{
					/* check we never select perfect edge (in test above) */
					BLI_assert(!(BM_edge_is_manifold(e) && BM_edge_is_contiguous(e)));

					BM_edge_select_set(em->bm, e, true);
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	EDBM_selectmode_flush(em);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_non_manifold(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Non Manifold";
	ot->description = "Select all non-manifold vertices or edges";
	ot->idname = "MESH_OT_select_non_manifold";
	
	/* api callbacks */
	ot->exec = edbm_select_non_manifold_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "extend", true, "Extend", "Extend the selection");
	/* edges */
	RNA_def_boolean(ot->srna, "use_wire", true, "Wire",
	                "Wire edges");
	RNA_def_boolean(ot->srna, "use_boundary", true, "Boundaries",
	                "Boundary edges");
	RNA_def_boolean(ot->srna, "use_multi_face", true,
	                "Multiple Faces", "Edges shared by 3+ faces");
	RNA_def_boolean(ot->srna, "use_non_contiguous", true, "Non Contiguous",
	                "Edges between faces pointing in alternate directions");
	/* verts */
	RNA_def_boolean(ot->srna, "use_verts", true, "Vertices",
	                "Vertices connecting multiple face regions");
}

static int edbm_select_random_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);
	const float randfac =  RNA_float_get(op->ptr, "percent") / 100.0f;
	const int seed = WM_operator_properties_select_random_seed_increment_get(op);

	BMIter iter;

	RNG *rng = BLI_rng_new_srandom(seed);

	if (em->selectmode & SCE_SELECT_VERTEX) {
		BMVert *eve;
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN) && BLI_rng_get_float(rng) < randfac) {
				BM_vert_select_set(em->bm, eve, select);
			}
		}
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BMEdge *eed;
		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && BLI_rng_get_float(rng) < randfac) {
				BM_edge_select_set(em->bm, eed, select);
			}
		}
	}
	else {
		BMFace *efa;
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && BLI_rng_get_float(rng) < randfac) {
				BM_face_select_set(em->bm, efa, select);
			}
		}
	}

	BLI_rng_free(rng);

	if (select) {
		/* was EDBM_select_flush, but it over select in edge/face mode */
		EDBM_selectmode_flush(em);
	}
	else {
		EDBM_deselect_flush(em);
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_select_random(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Random";
	ot->description = "Randomly select vertices";
	ot->idname = "MESH_OT_select_random";

	/* api callbacks */
	ot->exec = edbm_select_random_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	WM_operator_properties_select_random(ot);
}

static int edbm_select_ungrouped_poll(bContext *C)
{
	if (ED_operator_editmesh(C)) {
		Object *obedit = CTX_data_edit_object(C);
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

		if ((em->selectmode & SCE_SELECT_VERTEX) == 0) {
			CTX_wm_operator_poll_msg_set(C, "Must be in vertex selection mode");
		}
		else if (BLI_listbase_is_empty(&obedit->defbase) || cd_dvert_offset == -1) {
			CTX_wm_operator_poll_msg_set(C, "No weights/vertex groups on object");
		}
		else {
			return true;
		}
	}
	return false;
}

static int edbm_select_ungrouped_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

	BMVert *eve;
	BMIter iter;

	if (!RNA_boolean_get(op->ptr, "extend")) {
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	}

	BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
			MDeformVert *dv = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
			/* no dv or dv set with no weight */
			if (ELEM(NULL, dv, dv->dw)) {
				BM_vert_select_set(em->bm, eve, true);
			}
		}
	}

	EDBM_selectmode_flush(em);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_ungrouped(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Ungrouped";
	ot->idname = "MESH_OT_select_ungrouped";
	ot->description = "Select vertices without a group";

	/* api callbacks */
	ot->exec = edbm_select_ungrouped_exec;
	ot->poll = edbm_select_ungrouped_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}


/* BMESH_TODO - some way to select on an arbitrary axis */
static int edbm_select_axis_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMVert *v_act = BM_mesh_active_vert_get(bm);
	const int axis = RNA_enum_get(op->ptr, "axis");
	const int mode = RNA_enum_get(op->ptr, "mode"); /* -1 == aligned, 0 == neg, 1 == pos */

	if (v_act == NULL) {
		BKE_report(op->reports, RPT_WARNING, "This operator requires an active vertex (last selected)");
		return OPERATOR_CANCELLED;
	}
	else {
		BMVert *v;
		BMIter iter;
		const float limit = RNA_float_get(op->ptr, "threshold");
		float value = v_act->co[axis];

		if (mode == 0)
			value -= limit;
		else if (mode == 1)
			value += limit;

		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
				switch (mode) {
					case -1: /* aligned */
						if (fabsf(v->co[axis] - value) < limit)
							BM_vert_select_set(bm, v, true);
						break;
					case 0: /* neg */
						if (v->co[axis] > value)
							BM_vert_select_set(bm, v, true);
						break;
					case 1: /* pos */
						if (v->co[axis] < value)
							BM_vert_select_set(bm, v, true);
						break;
				}
			}
		}
	}

	EDBM_selectmode_flush(em);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_axis(wmOperatorType *ot)
{
	static EnumPropertyItem axis_mode_items[] = {
		{0,  "POSITIVE", 0, "Positive Axis", ""},
		{1,  "NEGATIVE", 0, "Negative Axis", ""},
		{-1, "ALIGNED",  0, "Aligned Axis", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem axis_items_xyz[] = {
		{0, "X_AXIS", 0, "X Axis", ""},
		{1, "Y_AXIS", 0, "Y Axis", ""},
		{2, "Z_AXIS", 0, "Z Axis", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select Axis";
	ot->description = "Select all data in the mesh on a single axis";
	ot->idname = "MESH_OT_select_axis";

	/* api callbacks */
	ot->exec = edbm_select_axis_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "mode", axis_mode_items, 0, "Axis Mode", "Axis side to use when selecting");
	RNA_def_enum(ot->srna, "axis", axis_items_xyz, 0, "Axis", "Select the axis to compare each vertex on");
	RNA_def_float(ot->srna, "threshold", 0.0001f, 0.000001f, 50.0f,  "Threshold", "", 0.00001f, 10.0f);
}

static int edbm_region_to_loop_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *f;
	BMEdge *e;
	BMIter iter;

	BM_mesh_elem_hflag_disable_all(em->bm, BM_EDGE, BM_ELEM_TAG, false);

	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		BMLoop *l1, *l2;
		BMIter liter1, liter2;
		
		BM_ITER_ELEM (l1, &liter1, f, BM_LOOPS_OF_FACE) {
			int tot = 0, totsel = 0;
			
			BM_ITER_ELEM (l2, &liter2, l1->e, BM_LOOPS_OF_EDGE) {
				tot++;
				totsel += BM_elem_flag_test(l2->f, BM_ELEM_SELECT) != 0;
			}
			
			if ((tot != totsel && totsel > 0) || (totsel == 1 && tot == 1))
				BM_elem_flag_enable(l1->e, BM_ELEM_TAG);
		}
	}

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	
	BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			BM_edge_select_set(em->bm, e, true);
		}
	}

	/* If in face-only select mode, switch to edge select mode so that
	 * an edge-only selection is not inconsistent state */
	if (em->selectmode == SCE_SELECT_FACE) {
		em->selectmode = SCE_SELECT_EDGE;
		EDBM_selectmode_set(em);
		EDBM_selectmode_to_scene(C);
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_region_to_loop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Boundary Loop";
	ot->idname = "MESH_OT_region_to_loop";
	ot->description = "Select boundary edges around the selected faces";

	/* api callbacks */
	ot->exec = edbm_region_to_loop_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int loop_find_region(BMLoop *l, int flag,
                            GSet *visit_face_set, BMFace ***region_out)
{
	BMFace **region = NULL;
	BMFace **stack = NULL;
	BLI_array_declare(region);
	BLI_array_declare(stack);
	BMFace *f;
	
	BLI_array_append(stack, l->f);
	BLI_gset_insert(visit_face_set, l->f);
	
	while (BLI_array_count(stack) > 0) {
		BMIter liter1, liter2;
		BMLoop *l1, *l2;
		
		f = BLI_array_pop(stack);
		BLI_array_append(region, f);
		
		BM_ITER_ELEM (l1, &liter1, f, BM_LOOPS_OF_FACE) {
			if (BM_elem_flag_test(l1->e, flag))
				continue;
			
			BM_ITER_ELEM (l2, &liter2, l1->e, BM_LOOPS_OF_EDGE) {
				/* avoids finding same region twice
				 * (otherwise) the logic works fine without */
				if (BM_elem_flag_test(l2->f, BM_ELEM_TAG)) {
					continue;
				}

				if (BLI_gset_add(visit_face_set, l2->f)) {
					BLI_array_append(stack, l2->f);
				}
			}
		}
	}
	
	BLI_array_free(stack);
	
	*region_out = region;
	return BLI_array_count(region);
}

static int verg_radial(const void *va, const void *vb)
{
	const BMEdge *e_a = *((const BMEdge **)va);
	const BMEdge *e_b = *((const BMEdge **)vb);

	int a, b;
	a = BM_edge_face_count(e_a);
	b = BM_edge_face_count(e_b);
	
	if (a > b) return -1;
	if (a < b) return  1;
	return  0;
}

/**
 * This function leaves faces tagged which are apart of the new region.
 *
 * \note faces already tagged are ignored, to avoid finding the same regions twice:
 * important when we have regions with equal face counts, see: T40309
 */
static int loop_find_regions(BMEditMesh *em, const bool selbigger)
{
	GSet *visit_face_set;
	BMIter iter;
	const int edges_len = em->bm->totedgesel;
	BMEdge *e, **edges;
	int count = 0, i;
	
	visit_face_set = BLI_gset_ptr_new_ex(__func__, edges_len);
	edges = MEM_mallocN(sizeof(*edges) * edges_len, __func__);

	i = 0;
	BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
			edges[i++] = e;
			BM_elem_flag_enable(e, BM_ELEM_TAG);
		}
		else {
			BM_elem_flag_disable(e, BM_ELEM_TAG);
		}
	}
	
	/* sort edges by radial cycle length */
	qsort(edges, edges_len, sizeof(*edges), verg_radial);
	
	for (i = 0; i < edges_len; i++) {
		BMIter liter;
		BMLoop *l;
		BMFace **region = NULL, **region_out;
		int c, tot = 0;
		
		e = edges[i];
		
		if (!BM_elem_flag_test(e, BM_ELEM_TAG))
			continue;
		
		BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
			if (BLI_gset_haskey(visit_face_set, l->f))
				continue;

			c = loop_find_region(l, BM_ELEM_SELECT, visit_face_set, &region_out);

			if (!region || (selbigger ? c >= tot : c < tot)) {
				/* this region is the best seen so far */
				tot = c;
				if (region) {
					/* free the previous best */
					MEM_freeN(region);
				}
				/* track the current region as the new best */
				region = region_out;
			}
			else {
				/* this region is not as good as best so far, just free it */
				MEM_freeN(region_out);
			}
		}
		
		if (region) {
			int j;
			
			for (j = 0; j < tot; j++) {
				BM_elem_flag_enable(region[j], BM_ELEM_TAG);
				BM_ITER_ELEM (l, &liter, region[j], BM_LOOPS_OF_FACE) {
					BM_elem_flag_disable(l->e, BM_ELEM_TAG);
				}
			}
			
			count += tot;
			
			MEM_freeN(region);
		}
	}
	
	MEM_freeN(edges);
	BLI_gset_free(visit_face_set, NULL);
	
	return count;
}

static int edbm_loop_to_region_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMIter iter;
	BMFace *f;
	const bool select_bigger = RNA_boolean_get(op->ptr, "select_bigger");
	int a, b;


	/* find the set of regions with smallest number of total faces */
	BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);
	a = loop_find_regions(em, select_bigger);
	b = loop_find_regions(em, !select_bigger);

	BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);
	loop_find_regions(em, ((a <= b) != select_bigger) ? select_bigger : !select_bigger);
	
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	
	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_TAG) && !BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
			BM_face_select_set(em->bm, f, true);
		}
	}
	
	EDBM_selectmode_flush(em);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	return OPERATOR_FINISHED;
}

void MESH_OT_loop_to_region(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Loop Inner-Region";
	ot->idname = "MESH_OT_loop_to_region";
	ot->description = "Select region of faces inside of a selected loop of edges";

	/* api callbacks */
	ot->exec = edbm_loop_to_region_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "select_bigger", 0, "Select Bigger", "Select bigger regions instead of smaller ones");
}


/************************ Select Path Operator *************************/
