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

/** \file blender/bmesh/intern/bmesh_marking.c
 *  \ingroup bmesh
 *
 * Selection routines for bmesh structures.
 * This is actually all old code ripped from
 * editmesh_lib.c and slightly modified to work
 * for bmesh's. This also means that it has some
 * of the same problems.... something that
 * that should be addressed eventually.
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"

#include "bmesh.h"

static void recount_totsels(BMesh *bm)
{
	BMIter iter;
	BMElem *ele;
	const char iter_types[3] = {BM_VERTS_OF_MESH,
	                            BM_EDGES_OF_MESH,
	                            BM_FACES_OF_MESH};
	int *tots[3];
	int i;

	/* recount (tot * sel) variables */
	bm->totvertsel = bm->totedgesel = bm->totfacesel = 0;
	tots[0] = &bm->totvertsel;
	tots[1] = &bm->totedgesel;
	tots[2] = &bm->totfacesel;

	for (i = 0; i < 3; i++) {
		ele = BM_iter_new(&iter, bm, iter_types[i], NULL);
		for ( ; ele; ele = BM_iter_step(&iter)) {
			if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) *tots[i] += 1;
		}
	}
}

/**
 * \brief Select Mode Flush
 *
 * Makes sure to flush selections 'upwards'
 * (ie: all verts of an edge selects the edge and so on).
 * This should only be called by system and not tool authors.
 */
void BM_mesh_select_mode_flush(BMesh *bm)
{
	BMEdge *e;
	BMLoop *l_iter;
	BMLoop *l_first;
	BMFace *f;

	BMIter eiter;
	BMIter fiter;

	int ok;

	if (bm->selectmode & SCE_SELECT_VERTEX) {
		BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e->v1, BM_ELEM_SELECT) &&
			    BM_elem_flag_test(e->v2, BM_ELEM_SELECT) &&
			    !BM_elem_flag_test(e, BM_ELEM_HIDDEN))
			{
				BM_elem_flag_enable(e, BM_ELEM_SELECT);
			}
			else {
				BM_elem_flag_disable(e, BM_ELEM_SELECT);
			}
		}
		BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
			ok = TRUE;
			if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
				l_iter = l_first = BM_FACE_FIRST_LOOP(f);
				do {
					if (!BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
						ok = FALSE;
						break;
					}
				} while ((l_iter = l_iter->next) != l_first);
			}
			else {
				ok = FALSE;
			}

			BM_elem_flag_set(f, BM_ELEM_SELECT, ok);
		}
	}
	else if (bm->selectmode & SCE_SELECT_EDGE) {
		BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
			ok = TRUE;
			if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
				l_iter = l_first = BM_FACE_FIRST_LOOP(f);
				do {
					if (!BM_elem_flag_test(l_iter->e, BM_ELEM_SELECT)) {
						ok = FALSE;
						break;
					}
				} while ((l_iter = l_iter->next) != l_first);
			}
			else {
				ok = FALSE;
			}

			BM_elem_flag_set(f, BM_ELEM_SELECT, ok);
		}
	}

	/* Remove any deselected elements from the BMEditSelection */
	BM_select_history_validate(bm);

	recount_totsels(bm);
}

/**
 * mode independent flushing up/down
 */
void BM_mesh_deselect_flush(BMesh *bm)
{
	BMEdge *e;
	BMLoop *l_iter;
	BMLoop *l_first;
	BMFace *f;

	BMIter eiter;
	BMIter fiter;

	int ok;

	BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
		if (!(BM_elem_flag_test(e->v1, BM_ELEM_SELECT) &&
		      BM_elem_flag_test(e->v2, BM_ELEM_SELECT) &&
		      !BM_elem_flag_test(e, BM_ELEM_HIDDEN)))
		{
			BM_elem_flag_disable(e, BM_ELEM_SELECT);
		}
	}

	BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
		ok = TRUE;
		if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
			l_iter = l_first = BM_FACE_FIRST_LOOP(f);
			do {
				if (!BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
					ok = FALSE;
					break;
				}
			} while ((l_iter = l_iter->next) != l_first);
		}
		else {
			ok = FALSE;
		}

		if (ok == FALSE) {
			BM_elem_flag_disable(f, BM_ELEM_SELECT);
		}
	}

	/* Remove any deselected elements from the BMEditSelection */
	BM_select_history_validate(bm);

	recount_totsels(bm);
}


/**
 * mode independent flushing up/down
 */
void BM_mesh_select_flush(BMesh *bm)
{
	BMEdge *e;
	BMLoop *l_iter;
	BMLoop *l_first;
	BMFace *f;

	BMIter eiter;
	BMIter fiter;

	int ok;

	BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e->v1, BM_ELEM_SELECT) &&
		    BM_elem_flag_test(e->v2, BM_ELEM_SELECT) &&
		    !BM_elem_flag_test(e, BM_ELEM_HIDDEN))
		{
			BM_elem_flag_enable(e, BM_ELEM_SELECT);
		}
	}

	BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
		ok = TRUE;
		if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
			l_iter = l_first = BM_FACE_FIRST_LOOP(f);
			do {
				if (!BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
					ok = FALSE;
					break;
				}
			} while ((l_iter = l_iter->next) != l_first);
		}
		else {
			ok = FALSE;
		}

		if (ok) {
			BM_elem_flag_enable(f, BM_ELEM_SELECT);
		}
	}

	recount_totsels(bm);
}

/**
 * \brief Select Vert
 *
 * Changes selection state of a single vertex
 * in a mesh
 */
void BM_vert_select_set(BMesh *bm, BMVert *v, int select)
{
	BLI_assert(v->head.htype == BM_VERT);

	if (BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
		return;
	}

	if (select) {
		if (!BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			bm->totvertsel += 1;
			BM_elem_flag_enable(v, BM_ELEM_SELECT);
		}
	}
	else {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			bm->totvertsel -= 1;
			BM_elem_flag_disable(v, BM_ELEM_SELECT);
		}
	}
}

/**
 * \brief Select Edge
 *
 * Changes selection state of a single edge in a mesh.
 */
void BM_edge_select_set(BMesh *bm, BMEdge *e, int select)
{
	BLI_assert(e->head.htype == BM_EDGE);

	if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
		return;
	}

	if (select) {
		if (!BM_elem_flag_test(e, BM_ELEM_SELECT)) bm->totedgesel += 1;

		BM_elem_flag_enable(e, BM_ELEM_SELECT);
		BM_vert_select_set(bm, e->v1, TRUE);
		BM_vert_select_set(bm, e->v2, TRUE);
	}
	else {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) bm->totedgesel -= 1;
		BM_elem_flag_disable(e, BM_ELEM_SELECT);

		if (bm->selectmode == SCE_SELECT_EDGE ||
		    bm->selectmode == SCE_SELECT_FACE ||
		    bm->selectmode == (SCE_SELECT_EDGE | SCE_SELECT_FACE))
		{

			BMIter iter;
			BMVert *verts[2] = {e->v1, e->v2};
			BMEdge *e2;
			int i;

			for (i = 0; i < 2; i++) {
				int deselect = 1;

				for (e2 = BM_iter_new(&iter, bm, BM_EDGES_OF_VERT, verts[i]); e2; e2 = BM_iter_step(&iter)) {
					if (e2 == e) {
						continue;
					}

					if (BM_elem_flag_test(e2, BM_ELEM_SELECT)) {
						deselect = 0;
						break;
					}
				}

				if (deselect) {
					BM_vert_select_set(bm, verts[i], FALSE);
				}
			}
		}
		else {
			BM_vert_select_set(bm, e->v1, FALSE);
			BM_vert_select_set(bm, e->v2, FALSE);
		}

	}
}

/**
 * \brief Select Face
 *
 * Changes selection state of a single
 * face in a mesh.
 */
void BM_face_select_set(BMesh *bm, BMFace *f, int select)
{
	BMLoop *l_iter;
	BMLoop *l_first;

	BLI_assert(f->head.htype == BM_FACE);

	if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
		return;
	}

	if (select) {
		if (!BM_elem_flag_test(f, BM_ELEM_SELECT)) {
			bm->totfacesel++;
		}

		BM_elem_flag_enable(f, BM_ELEM_SELECT);
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			BM_vert_select_set(bm, l_iter->v, TRUE);
			BM_edge_select_set(bm, l_iter->e, TRUE);
		} while ((l_iter = l_iter->next) != l_first);
	}
	else {
		BMIter liter;
		BMLoop *l;

		if (BM_elem_flag_test(f, BM_ELEM_SELECT)) bm->totfacesel -= 1;
		BM_elem_flag_disable(f, BM_ELEM_SELECT);

		/* flush down to edges */
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			BMIter fiter;
			BMFace *f2;
			BM_ITER_ELEM (f2, &fiter, l->e, BM_FACES_OF_EDGE) {
				if (BM_elem_flag_test(f2, BM_ELEM_SELECT))
					break;
			}

			if (!f2) {
				BM_edge_select_set(bm, l->e, FALSE);
			}
		}

		/* flush down to verts */
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			BMIter eiter;
			BMEdge *e;
			BM_ITER_ELEM (e, &eiter, l->v, BM_EDGES_OF_VERT) {
				if (BM_elem_flag_test(e, BM_ELEM_SELECT))
					break;
			}

			if (!e) {
				BM_vert_select_set(bm, l->v, FALSE);
			}
		}
	}
}

/**
 * Select Mode Set
 *
 * Sets the selection mode for the bmesh,
 * updating the selection state.
 */
void BM_mesh_select_mode_set(BMesh *bm, int selectmode)
{
	BMIter iter;
	BMElem *ele;
	
	bm->selectmode = selectmode;

	if (bm->selectmode & SCE_SELECT_VERTEX) {
		/* disabled because selection flushing handles these */
#if 0
		BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
			BM_elem_flag_disable(ele, BM_ELEM_SELECT);
		}
		BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
			BM_elem_flag_disable(ele, BM_ELEM_SELECT);
		}
#endif
		BM_mesh_select_mode_flush(bm);
	}
	else if (bm->selectmode & SCE_SELECT_EDGE) {
		/* disabled because selection flushing handles these */
#if 0
		BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
			BM_elem_flag_disable(ele, BM_ELEM_SELECT);
		}
#endif

		BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
				BM_edge_select_set(bm, (BMEdge *)ele, TRUE);
			}
		}
		BM_mesh_select_mode_flush(bm);
	}
	else if (bm->selectmode & SCE_SELECT_FACE) {
		/* disabled because selection flushing handles these */
#if 0
		BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
			BM_elem_flag_disable(ele, BM_ELEM_SELECT);
		}
#endif
		BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
				BM_face_select_set(bm, (BMFace *)ele, TRUE);
			}
		}
		BM_mesh_select_mode_flush(bm);
	}
}

/**
 * counts number of elements with flag enabled/disabled
 */
static int bm_mesh_flag_count(BMesh *bm, const char htype, const char hflag,
                              const short respecthide, const short test_for_enabled)
{
	BMElem *ele;
	BMIter iter;
	int tot = 0;

	BLI_assert(ELEM(TRUE, FALSE, test_for_enabled));

	if (htype & BM_VERT) {
		for (ele = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, NULL); ele; ele = BM_iter_step(&iter)) {
			if (respecthide && BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) continue;
			if (BM_elem_flag_test_bool(ele, hflag) == test_for_enabled) tot++;
		}
	}
	if (htype & BM_EDGE) {
		for (ele = BM_iter_new(&iter, bm, BM_EDGES_OF_MESH, NULL); ele; ele = BM_iter_step(&iter)) {
			if (respecthide && BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) continue;
			if (BM_elem_flag_test_bool(ele, hflag) == test_for_enabled) tot++;
		}
	}
	if (htype & BM_FACE) {
		for (ele = BM_iter_new(&iter, bm, BM_FACES_OF_MESH, NULL); ele; ele = BM_iter_step(&iter)) {
			if (respecthide && BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) continue;
			if (BM_elem_flag_test_bool(ele, hflag) == test_for_enabled) tot++;
		}
	}

	return tot;
}

int BM_mesh_elem_hflag_count_enabled(BMesh *bm, const char htype, const char hflag, int respecthide)
{
	return bm_mesh_flag_count(bm, htype, hflag, respecthide, TRUE);
}

int BM_mesh_elem_hflag_count_disabled(BMesh *bm, const char htype, const char hflag, int respecthide)
{
	return bm_mesh_flag_count(bm, htype, hflag, respecthide, FALSE);
}

/**
 * \note use BM_elem_flag_test(ele, BM_ELEM_SELECT) to test selection
 * \note by design, this will not touch the editselection history stuff
 */
void _bm_elem_select_set(BMesh *bm, BMHeader *head, int select)
{
	switch (head->htype) {
		case BM_VERT:
			BM_vert_select_set(bm, (BMVert *)head, select);
			break;
		case BM_EDGE:
			BM_edge_select_set(bm, (BMEdge *)head, select);
			break;
		case BM_FACE:
			BM_face_select_set(bm, (BMFace *)head, select);
			break;
		default:
			BLI_assert(0);
			break;
	}
}

/* this replaces the active flag used in uv/face mode */
void BM_active_face_set(BMesh *bm, BMFace *efa)
{
	bm->act_face = efa;
}

BMFace *BM_active_face_get(BMesh *bm, int sloppy)
{
	if (bm->act_face) {
		return bm->act_face;
	}
	else if (sloppy) {
		BMIter iter;
		BMFace *f = NULL;
		BMEditSelection *ese;
		
		/* Find the latest non-hidden face from the BMEditSelection */
		ese = bm->selected.last;
		for ( ; ese; ese = ese->prev) {
			if (ese->htype == BM_FACE) {
				f = (BMFace *)ese->ele;
				
				if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
					f = NULL;
				}
				else {
					break;
				}
			}
		}
		/* Last attempt: try to find any selected face */
		if (f == NULL) {
			BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
				if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
					break;
				}
			}
		}
		return f; /* can still be null */
	}
	return NULL;
}

/**
 * Generic way to get data from an EditSelection type
 * These functions were written to be used by the Modifier widget
 * when in Rotate about active mode, but can be used anywhere.
 *
 * - #EM_editselection_center
 * - #EM_editselection_normal
 * - #EM_editselection_plane
 */
void BM_editselection_center(float r_center[3], BMEditSelection *ese)
{
	if (ese->htype == BM_VERT) {
		BMVert *eve = (BMVert *)ese->ele;
		copy_v3_v3(r_center, eve->co);
	}
	else if (ese->htype == BM_EDGE) {
		BMEdge *eed = (BMEdge *)ese->ele;
		add_v3_v3v3(r_center, eed->v1->co, eed->v2->co);
		mul_v3_fl(r_center, 0.5);
	}
	else if (ese->htype == BM_FACE) {
		BMFace *efa = (BMFace *)ese->ele;
		BM_face_center_bounds_calc(efa, r_center);
	}
}

void BM_editselection_normal(float r_normal[3], BMEditSelection *ese)
{
	if (ese->htype == BM_VERT) {
		BMVert *eve = (BMVert *)ese->ele;
		copy_v3_v3(r_normal, eve->no);
	}
	else if (ese->htype == BM_EDGE) {
		BMEdge *eed = (BMEdge *)ese->ele;
		float plane[3]; /* need a plane to correct the normal */
		float vec[3]; /* temp vec storage */
		
		add_v3_v3v3(r_normal, eed->v1->no, eed->v2->no);
		sub_v3_v3v3(plane, eed->v2->co, eed->v1->co);
		
		/* the 2 vertex normals will be close but not at rightangles to the edge
		 * for rotate about edge we want them to be at right angles, so we need to
		 * do some extra colculation to correct the vert normals,
		 * we need the plane for this */
		cross_v3_v3v3(vec, r_normal, plane);
		cross_v3_v3v3(r_normal, plane, vec);
		normalize_v3(r_normal);
		
	}
	else if (ese->htype == BM_FACE) {
		BMFace *efa = (BMFace *)ese->ele;
		copy_v3_v3(r_normal, efa->no);
	}
}

/* ref - editmesh_lib.cL:EM_editselection_plane() */

/* Calculate a plane that is rightangles to the edge/vert/faces normal
 * also make the plane run along an axis that is related to the geometry,
 * because this is used for the manipulators Y axis. */
void BM_editselection_plane(BMesh *bm, float r_plane[3], BMEditSelection *ese)
{
	if (ese->htype == BM_VERT) {
		BMVert *eve = (BMVert *)ese->ele;
		float vec[3] = {0.0f, 0.0f, 0.0f};
		
		if (ese->prev) { /* use previously selected data to make a useful vertex plane */
			BM_editselection_center(vec, ese->prev);
			sub_v3_v3v3(r_plane, vec, eve->co);
		}
		else {
			/* make a fake  plane thats at rightangles to the normal
			 * we cant make a crossvec from a vec thats the same as the vec
			 * unlikely but possible, so make sure if the normal is (0, 0, 1)
			 * that vec isn't the same or in the same direction even. */
			if      (eve->no[0] < 0.5f) vec[0] = 1.0f;
			else if (eve->no[1] < 0.5f) vec[1] = 1.0f;
			else                        vec[2] = 1.0f;
			cross_v3_v3v3(r_plane, eve->no, vec);
		}
	}
	else if (ese->htype == BM_EDGE) {
		BMEdge *eed = (BMEdge *)ese->ele;

		/* the plane is simple, it runs along the edge
		 * however selecting different edges can swap the direction of the y axis.
		 * this makes it less likely for the y axis of the manipulator
		 * (running along the edge).. to flip less often.
		 * at least its more predictable */
		if (eed->v2->co[1] > eed->v1->co[1]) {  /* check which to do first */
			sub_v3_v3v3(r_plane, eed->v2->co, eed->v1->co);
		}
		else {
			sub_v3_v3v3(r_plane, eed->v1->co, eed->v2->co);
		}
		
	}
	else if (ese->htype == BM_FACE) {
		BMFace *efa = (BMFace *)ese->ele;
		float vec[3] = {0.0f, 0.0f, 0.0f};
		
		/* for now, use face normal */

		/* make a fake plane thats at rightangles to the normal
		 * we cant make a crossvec from a vec thats the same as the vec
		 * unlikely but possible, so make sure if the normal is (0, 0, 1)
		 * that vec isn't the same or in the same direction even. */
		if (efa->len < 3) {
			/* crappy fallback method */
			if      (efa->no[0] < 0.5f)	vec[0] = 1.0f;
			else if (efa->no[1] < 0.5f)	vec[1] = 1.0f;
			else                        vec[2] = 1.0f;
			cross_v3_v3v3(r_plane, efa->no, vec);
		}
		else {
			BMVert *verts[4] = {NULL};

			BM_iter_as_array(bm, BM_VERTS_OF_FACE, efa, (void **)verts, 4);

			if (efa->len == 4) {
				float vecA[3], vecB[3];
				sub_v3_v3v3(vecA, verts[3]->co, verts[2]->co);
				sub_v3_v3v3(vecB, verts[0]->co, verts[1]->co);
				add_v3_v3v3(r_plane, vecA, vecB);

				sub_v3_v3v3(vecA, verts[0]->co, verts[3]->co);
				sub_v3_v3v3(vecB, verts[1]->co, verts[2]->co);
				add_v3_v3v3(vec, vecA, vecB);
				/* use the biggest edge length */
				if (dot_v3v3(r_plane, r_plane) < dot_v3v3(vec, vec)) {
					copy_v3_v3(r_plane, vec);
				}
			}
			else {
				/* BMESH_TODO (not urgent, use longest ngon edge for alignment) */

				/* start with v1-2 */
				sub_v3_v3v3(r_plane, verts[0]->co, verts[1]->co);

				/* test the edge between v2-3, use if longer */
				sub_v3_v3v3(vec, verts[1]->co, verts[2]->co);
				if (dot_v3v3(r_plane, r_plane) < dot_v3v3(vec, vec))
					copy_v3_v3(r_plane, vec);

				/* test the edge between v1-3, use if longer */
				sub_v3_v3v3(vec, verts[2]->co, verts[0]->co);
				if (dot_v3v3(r_plane, r_plane) < dot_v3v3(vec, vec)) {
					copy_v3_v3(r_plane, vec);
				}
			}

		}
	}
	normalize_v3(r_plane);
}

int BM_select_history_check(BMesh *bm, const BMElem *ele)
{
	BMEditSelection *ese;
	
	for (ese = bm->selected.first; ese; ese = ese->next) {
		if (ese->ele == ele) {
			return TRUE;
		}
	}
	
	return FALSE;
}

int BM_select_history_remove(BMesh *bm, BMElem *ele)
{
	BMEditSelection *ese;
	for (ese = bm->selected.first; ese; ese = ese->next) {
		if (ese->ele == ele) {
			BLI_freelinkN(&(bm->selected), ese);
			return TRUE;
		}
	}

	return FALSE;
}

void BM_select_history_clear(BMesh *bm)
{
	BLI_freelistN(&bm->selected);
	bm->selected.first = bm->selected.last = NULL;
}

void BM_select_history_store_notest(BMesh *bm, BMElem *ele)
{
	BMEditSelection *ese = (BMEditSelection *) MEM_callocN(sizeof(BMEditSelection), "BMEdit Selection");
	ese->htype = ((BMHeader *)ele)->htype;
	ese->ele = ele;
	BLI_addtail(&(bm->selected), ese);
}

void BM_select_history_store(BMesh *bm, BMElem *ele)
{
	if (!BM_select_history_check(bm, ele)) {
		BM_select_history_store_notest(bm, ele);
	}
}

void BM_select_history_validate(BMesh *bm)
{
	BMEditSelection *ese, *nextese;

	ese = bm->selected.first;

	while (ese) {
		nextese = ese->next;
		if (!BM_elem_flag_test(ese->ele, BM_ELEM_SELECT)) {
			BLI_freelinkN(&(bm->selected), ese);
		}
		ese = nextese;
	}
}

void BM_mesh_elem_hflag_disable_test(BMesh *bm, const char htype, const char hflag,
                                     int respecthide, const char hflag_test)
{
	const char iter_types[3] = {BM_VERTS_OF_MESH,
	                            BM_EDGES_OF_MESH,
	                            BM_FACES_OF_MESH};

	const char flag_types[3] = {BM_VERT, BM_EDGE, BM_FACE};

	BMIter iter;
	BMElem *ele;
	int i;

	if (hflag & BM_ELEM_SELECT) {
		BM_select_history_clear(bm);
	}

	if ((htype == (BM_VERT | BM_EDGE | BM_FACE)) &&
	    (hflag == BM_ELEM_SELECT) &&
	    (respecthide == FALSE) &&
	    (hflag_test == 0))
	{
		/* fast path for deselect all, avoid topology loops
		 * since we know all will be de-selected anyway. */
		for (i = 0; i < 3; i++) {
			ele = BM_iter_new(&iter, bm, iter_types[i], NULL);
			for ( ; ele; ele = BM_iter_step(&iter)) {
				BM_elem_flag_disable(ele, BM_ELEM_SELECT);
			}
		}
		bm->totvertsel = bm->totedgesel = bm->totfacesel = 0;
	}
	else {
		for (i = 0; i < 3; i++) {
			if (htype & flag_types[i]) {
				ele = BM_iter_new(&iter, bm, iter_types[i], NULL);
				for ( ; ele; ele = BM_iter_step(&iter)) {

					if (respecthide && BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) {
						continue;
					}
					if (hflag_test && !BM_elem_flag_test(ele, hflag_test)) {
						continue;
					}

					if (hflag & BM_ELEM_SELECT) {
						BM_elem_select_set(bm, ele, FALSE);
					}
					BM_elem_flag_disable(ele, hflag);
				}
			}
		}
	}
}

void BM_mesh_elem_hflag_enable_test(BMesh *bm, const char htype, const char hflag,
                                    int respecthide, const char hflag_test)
{
	const char iter_types[3] = {BM_VERTS_OF_MESH,
	                            BM_EDGES_OF_MESH,
	                            BM_FACES_OF_MESH};

	const char flag_types[3] = {BM_VERT, BM_EDGE, BM_FACE};

	/* use the nosel version when setting so under no
	 * condition may a hidden face become selected.
	 * Applying other flags to hidden faces is OK. */
	const char hflag_nosel = hflag & ~BM_ELEM_SELECT;

	BMIter iter;
	BMElem *ele;
	int i;

	if (hflag & BM_ELEM_SELECT) {
		BM_select_history_clear(bm);
	}

	/* note, better not attempt a fast path for selection as done with de-select
	 * because hidden geometry and different selection modes can give different results,
	 * we could of course check for no hiddent faces and then use quicker method but its not worth it. */

	for (i = 0; i < 3; i++) {
		if (htype & flag_types[i]) {
			ele = BM_iter_new(&iter, bm, iter_types[i], NULL);
			for ( ; ele; ele = BM_iter_step(&iter)) {

				if (respecthide && BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) {
					continue;
				}
				if (hflag_test && !BM_elem_flag_test(ele, hflag_test)) {
					continue;
				}

				if (hflag & BM_ELEM_SELECT) {
					BM_elem_select_set(bm, ele, TRUE);
				}
				BM_elem_flag_enable(ele, hflag_nosel);
			}
		}
	}
}

void BM_mesh_elem_hflag_disable_all(BMesh *bm, const char htype, const char hflag,
                                    int respecthide)
{
	/* call with 0 hflag_test */
	BM_mesh_elem_hflag_disable_test(bm, htype, hflag, respecthide, 0);
}

void BM_mesh_elem_hflag_enable_all(BMesh *bm, const char htype, const char hflag,
                                   int respecthide)
{
	/* call with 0 hflag_test */
	BM_mesh_elem_hflag_enable_test(bm, htype, hflag, respecthide, 0);
}

/***************** Mesh Hiding stuff *********** */

static void vert_flush_hide_set(BMVert *v)
{
	BMIter iter;
	BMEdge *e;
	int hide = TRUE;

	BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
		hide = hide && BM_elem_flag_test(e, BM_ELEM_HIDDEN);
	}

	BM_elem_flag_set(v, BM_ELEM_HIDDEN, hide);
}

static void edge_flush_hide(BMEdge *e)
{
	BMIter iter;
	BMFace *f;
	int hide = TRUE;

	BM_ITER_ELEM (f, &iter, e, BM_FACES_OF_EDGE) {
		hide = hide && BM_elem_flag_test(f, BM_ELEM_HIDDEN);
	}

	BM_elem_flag_set(e, BM_ELEM_HIDDEN, hide);
}

void BM_vert_hide_set(BMVert *v, int hide)
{
	/* vert hiding: vert + surrounding edges and faces */
	BMIter iter, fiter;
	BMEdge *e;
	BMFace *f;

	BM_elem_flag_set(v, BM_ELEM_HIDDEN, hide);

	BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
		BM_elem_flag_set(e, BM_ELEM_HIDDEN, hide);

		BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
			BM_elem_flag_set(f, BM_ELEM_HIDDEN, hide);
		}
	}
}

void BM_edge_hide_set(BMEdge *e, int hide)
{
	BMIter iter;
	BMFace *f;
	/* BMVert *v; */

	/* edge hiding: faces around the edge */
	BM_ITER_ELEM (f, &iter, e, BM_FACES_OF_EDGE) {
		BM_elem_flag_set(f, BM_ELEM_HIDDEN, hide);
	}
	
	BM_elem_flag_set(e, BM_ELEM_HIDDEN, hide);

	/* hide vertices if necessary */
	vert_flush_hide_set(e->v1);
	vert_flush_hide_set(e->v2);
}

void BM_face_hide_set(BMFace *f, int hide)
{
	BMIter iter;
	BMLoop *l;

	BM_elem_flag_set(f, BM_ELEM_HIDDEN, hide);

	BM_ITER_ELEM (l, &iter, f, BM_LOOPS_OF_FACE) {
		edge_flush_hide(l->e);
	}

	BM_ITER_ELEM (l, &iter, f, BM_LOOPS_OF_FACE) {
		vert_flush_hide_set(l->v);
	}
}

void _bm_elem_hide_set(BMesh *bm, BMHeader *head, int hide)
{
	/* Follow convention of always deselecting before
	 * hiding an element */
	switch (head->htype) {
		case BM_VERT:
			if (hide) BM_vert_select_set(bm, (BMVert *)head, FALSE);
			BM_vert_hide_set((BMVert *)head, hide);
			break;
		case BM_EDGE:
			if (hide) BM_edge_select_set(bm, (BMEdge *)head, FALSE);
			BM_edge_hide_set((BMEdge *)head, hide);
			break;
		case BM_FACE:
			if (hide) BM_face_select_set(bm, (BMFace *)head, FALSE);
			BM_face_hide_set((BMFace *)head, hide);
			break;
		default:
			BMESH_ASSERT(0);
			break;
	}
}
