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
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Levi Schooley.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_walkers_impl.c
 *  \ingroup bmesh
 *
 * BMesh Walker Code.
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"
#include "intern/bmesh_walkers_private.h"

/* pop into stack memory (common operation) */
#define BMW_state_remove_r(walker, owalk) { \
	memcpy(owalk, BMW_current_state(walker), sizeof(*(owalk))); \
	BMW_state_remove(walker); \
} (void)0

/** \name Mask Flag Checks
 * \{ */

static bool bmw_mask_check_vert(BMWalker *walker, BMVert *v)
{
	if ((walker->flag & BMW_FLAG_TEST_HIDDEN) && BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
		return false;
	}
	else if (walker->mask_vert && !BMO_elem_flag_test(walker->bm, v, walker->mask_vert)) {
		return false;
	}
	else {
		return true;
	}
}

static bool bmw_mask_check_edge(BMWalker *walker, BMEdge *e)
{
	if ((walker->flag & BMW_FLAG_TEST_HIDDEN) && BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
		return false;
	}
	else if (walker->mask_edge && !BMO_elem_flag_test(walker->bm, e, walker->mask_edge)) {
		return false;
	}
	else {
		return true;
	}
}

static bool bmw_mask_check_face(BMWalker *walker, BMFace *f)
{
	if ((walker->flag & BMW_FLAG_TEST_HIDDEN) && BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
		return false;
	}
	else if (walker->mask_face && !BMO_elem_flag_test(walker->bm, f, walker->mask_face)) {
		return false;
	}
	else {
		return true;
	}
}

/** \} */


/** \name Shell Walker
 * \{
 *
 * Starts at a vertex on the mesh and walks over the 'shell' it belongs
 * to via visiting connected edges.
 *
 * \todo Add restriction flag/callback for wire edges.
 */
static void bmw_ShellWalker_visitEdge(BMWalker *walker, BMEdge *e)
{
	BMwShellWalker *shellWalk = NULL;

	if (BLI_gset_haskey(walker->visit_set, e)) {
		return;
	}

	if (!bmw_mask_check_edge(walker, e)) {
		return;
	}

	shellWalk = BMW_state_add(walker);
	shellWalk->curedge = e;
	BLI_gset_insert(walker->visit_set, e);
}

static void bmw_ShellWalker_begin(BMWalker *walker, void *data)
{
	BMIter eiter;
	BMHeader *h = data;
	BMEdge *e;
	BMVert *v;

	if (UNLIKELY(h == NULL)) {
		return;
	}

	switch (h->htype) {
		case BM_VERT:
		{
			/* starting the walk at a vert, add all the edges
			 * to the worklist */
			v = (BMVert *)h;
			BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
				bmw_ShellWalker_visitEdge(walker, e);
			}
			break;
		}

		case BM_EDGE:
		{
			/* starting the walk at an edge, add the single edge
			 * to the worklist */
			e = (BMEdge *)h;
			bmw_ShellWalker_visitEdge(walker, e);
			break;
		}
	}
}

static void *bmw_ShellWalker_yield(BMWalker *walker)
{
	BMwShellWalker *shellWalk = BMW_current_state(walker);
	return shellWalk->curedge;
}

static void *bmw_ShellWalker_step(BMWalker *walker)
{
	BMwShellWalker *swalk, owalk;
	BMEdge *e, *e2;
	BMVert *v;
	BMIter iter;
	int i;

	BMW_state_remove_r(walker, &owalk);
	swalk = &owalk;

	e = swalk->curedge;

	for (i = 0; i < 2; i++) {
		v = i ? e->v2 : e->v1;
		BM_ITER_ELEM (e2, &iter, v, BM_EDGES_OF_VERT) {
			bmw_ShellWalker_visitEdge(walker, e2);
		}
	}

	return e;
}

#if 0
static void *bmw_ShellWalker_step(BMWalker *walker)
{
	BMEdge *curedge, *next = NULL;
	BMVert *v_old = NULL;
	bool restrictpass = true;
	BMwShellWalker shellWalk = *((BMwShellWalker *)BMW_current_state(walker));
	
	if (!BLI_gset_haskey(walker->visit_set, shellWalk.base)) {
		BLI_gset_insert(walker->visit_set, shellWalk.base);
	}

	BMW_state_remove(walker);


	/* find the next edge whose other vertex has not been visite */
	curedge = shellWalk.curedge;
	do {
		if (!BLI_gset_haskey(walker->visit_set, curedge)) {
			if (!walker->restrictflag ||
			    (walker->restrictflag && BMO_elem_flag_test(walker->bm, curedge, walker->restrictflag)))
			{
				BMwShellWalker *newstate;

				v_old = BM_edge_other_vert(curedge, shellWalk.base);
				
				/* push a new state onto the stac */
				newState = BMW_state_add(walker);
				BLI_gset_insert(walker->visit_set, curedge);
				
				/* populate the new stat */

				newState->base = v_old;
				newState->curedge = curedge;
			}
		}
		curedge = bmesh_disk_edge_next(curedge, shellWalk.base);
	} while (curedge != shellWalk.curedge);
	
	return shellWalk.curedge;
}
#endif

/** \} */


/** \name Connected Vertex Walker
 * \{
 *
 * Similar to shell walker, but visits vertices instead of edges.
 */
static void bmw_ConnectedVertexWalker_visitVertex(BMWalker *walker, BMVert *v)
{
	BMwConnectedVertexWalker *vwalk;

	if (BLI_gset_haskey(walker->visit_set, v)) {
		/* already visited */
		return;
	}

	if (!bmw_mask_check_vert(walker, v)) {
		/* not flagged for walk */
		return;
	}

	vwalk = BMW_state_add(walker);
	vwalk->curvert = v;
	BLI_gset_insert(walker->visit_set, v);
}

static void bmw_ConnectedVertexWalker_begin(BMWalker *walker, void *data)
{
	BMVert *v = data;
	bmw_ConnectedVertexWalker_visitVertex(walker, v);
}

static void *bmw_ConnectedVertexWalker_yield(BMWalker *walker)
{
	BMwConnectedVertexWalker *vwalk = BMW_current_state(walker);
	return vwalk->curvert;
}

static void *bmw_ConnectedVertexWalker_step(BMWalker *walker)
{
	BMwConnectedVertexWalker *vwalk, owalk;
	BMVert *v, *v2;
	BMEdge *e;
	BMIter iter;

	BMW_state_remove_r(walker, &owalk);
	vwalk = &owalk;

	v = vwalk->curvert;

	BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
		v2 = BM_edge_other_vert(e, v);
		if (!BLI_gset_haskey(walker->visit_set, v2)) {
			bmw_ConnectedVertexWalker_visitVertex(walker, v2);
		}
	}

	return v;
}

/** \} */


/** \name Island Boundary Walker
 * \{
 *
 * Starts at a edge on the mesh and walks over the boundary of an island it belongs to.
 *
 * \todo Add restriction flag/callback for wire edges.
 */
static void bmw_IslandboundWalker_begin(BMWalker *walker, void *data)
{
	BMLoop *l = data;
	BMwIslandboundWalker *iwalk = NULL;

	iwalk = BMW_state_add(walker);

	iwalk->base = iwalk->curloop = l;
	iwalk->lastv = l->v;

	BLI_gset_insert(walker->visit_set, data);

}

static void *bmw_IslandboundWalker_yield(BMWalker *walker)
{
	BMwIslandboundWalker *iwalk = BMW_current_state(walker);

	return iwalk->curloop;
}

static void *bmw_IslandboundWalker_step(BMWalker *walker)
{
	BMwIslandboundWalker *iwalk, owalk;
	BMVert *v;
	BMEdge *e;
	BMFace *f;
	BMLoop *l;
	/* int found = 0; */

	memcpy(&owalk, BMW_current_state(walker), sizeof(owalk));
	/* normally we'd remove here, but delay until after error checking */
	iwalk = &owalk;

	l = iwalk->curloop;
	e = l->e;

	v = BM_edge_other_vert(e, iwalk->lastv);

	if (!BM_vert_is_manifold(v)) {
		BMW_reset(walker);
		BMO_error_raise(walker->bm, NULL, BMERR_WALKER_FAILED,
		                "Non-manifold vert while searching region boundary");
		return NULL;
	}
	
	/* pop off current state */
	BMW_state_remove(walker);
	
	f = l->f;
	
	while (1) {
		l = BM_loop_other_edge_loop(l, v);
		if (l != l->radial_next) {
			l = l->radial_next;
			f = l->f;
			e = l->e;

			if (!bmw_mask_check_face(walker, f)) {
				l = l->radial_next;
				break;
			}
		}
		else {
			f = l->f;
			e = l->e;
			break;
		}
	}
	
	if (l == owalk.curloop) {
		return NULL;
	}
	else if (BLI_gset_haskey(walker->visit_set, l)) {
		return owalk.curloop;
	}

	BLI_gset_insert(walker->visit_set, l);
	iwalk = BMW_state_add(walker);
	iwalk->base = owalk.base;

	//if (!BMO_elem_flag_test(walker->bm, l->f, walker->restrictflag))
	//	iwalk->curloop = l->radial_next;
	iwalk->curloop = l; //else iwalk->curloop = l;
	iwalk->lastv = v;

	return owalk.curloop;
}


/** \name Island Walker
 * \{
 *
 * Starts at a tool flagged-face and walks over the face region
 *
 * \todo Add restriction flag/callback for wire edges.
 */
static void bmw_IslandWalker_begin(BMWalker *walker, void *data)
{
	BMwIslandWalker *iwalk = NULL;

	if (!bmw_mask_check_face(walker, data)) {
		return;
	}

	iwalk = BMW_state_add(walker);
	BLI_gset_insert(walker->visit_set, data);

	iwalk->cur = data;
}

static void *bmw_IslandWalker_yield(BMWalker *walker)
{
	BMwIslandWalker *iwalk = BMW_current_state(walker);

	return iwalk->cur;
}

static void *bmw_IslandWalker_step(BMWalker *walker)
{
	BMwIslandWalker *iwalk, owalk;
	BMIter iter, liter;
	BMFace *f;
	BMLoop *l;
	
	BMW_state_remove_r(walker, &owalk);
	iwalk = &owalk;

	l = BM_iter_new(&liter, walker->bm, BM_LOOPS_OF_FACE, iwalk->cur);
	for ( ; l; l = BM_iter_step(&liter)) {
		/* could skip loop here too, but don't add unless we need it */
		if (!bmw_mask_check_edge(walker, l->e)) {
			continue;
		}

		f = BM_iter_new(&iter, walker->bm, BM_FACES_OF_EDGE, l->e);
		for ( ; f; f = BM_iter_step(&iter)) {

			if (!bmw_mask_check_face(walker, f)) {
				continue;
			}

			/* saves checking BLI_gset_haskey below (manifold edges theres a 50% chance) */
			if (f == iwalk->cur) {
				continue;
			}

			if (BLI_gset_haskey(walker->visit_set, f)) {
				continue;
			}
			
			iwalk = BMW_state_add(walker);
			iwalk->cur = f;
			BLI_gset_insert(walker->visit_set, f);
			break;
		}
	}
	
	return owalk.cur;
}

/** \} */


/** \name Edge Loop Walker
 * \{
 *
 * Starts at a tool-flagged edge and walks over the edge loop
 */

/* utility function to see if an edge is apart of an ngon boundary */
static bool bm_edge_is_single(BMEdge *e)
{
	return ((BM_edge_is_boundary(e)) &&
	        (e->l->f->len > 4) &&
	        (BM_edge_is_boundary(e->l->next->e) || BM_edge_is_boundary(e->l->prev->e)));
}

static void bmw_LoopWalker_begin(BMWalker *walker, void *data)
{
	BMwLoopWalker *lwalk = NULL, owalk, *owalk_pt;
	BMEdge *e = data;
	BMVert *v;
	int vert_edge_count[2] = {BM_vert_edge_count_nonwire(e->v1),
	                          BM_vert_edge_count_nonwire(e->v2)};

	v = e->v1;

	lwalk = BMW_state_add(walker);
	BLI_gset_insert(walker->visit_set, e);

	lwalk->cur = lwalk->start = e;
	lwalk->lastv = lwalk->startv = v;
	lwalk->is_boundary = BM_edge_is_boundary(e);
	lwalk->is_single = (lwalk->is_boundary && bm_edge_is_single(e));

	/* could also check that vertex*/
	if ((lwalk->is_boundary == false) &&
	    (vert_edge_count[0] == 3 || vert_edge_count[1] == 3))
	{
		BMIter iter;
		BMFace *f_iter;
		BMFace *f_best = NULL;

		BM_ITER_ELEM (f_iter, &iter, e, BM_FACES_OF_EDGE) {
			if (f_best == NULL || f_best->len < f_iter->len) {
				f_best = f_iter;
			}
		}

		if (f_best) {
			/* only use hub selection for 5+ sides else this could
			 * conflict with normal edge loop selection. */
			lwalk->f_hub = f_best->len > 4 ? f_best : NULL;
		}
		else {
			/* edge doesn't have any faces connected to it */
			lwalk->f_hub = NULL;
		}
	}
	else {
		lwalk->f_hub = NULL;
	}

	/* rewind */
	while ((owalk_pt = BMW_current_state(walker))) {
		owalk = *((BMwLoopWalker *)owalk_pt);
		BMW_walk(walker);
	}

	lwalk = BMW_state_add(walker);
	*lwalk = owalk;

	lwalk->lastv = lwalk->startv = BM_edge_other_vert(owalk.cur, lwalk->lastv);

	BLI_gset_clear(walker->visit_set, NULL);
	BLI_gset_insert(walker->visit_set, owalk.cur);
}

static void *bmw_LoopWalker_yield(BMWalker *walker)
{
	BMwLoopWalker *lwalk = BMW_current_state(walker);

	return lwalk->cur;
}

static void *bmw_LoopWalker_step(BMWalker *walker)
{
	BMwLoopWalker *lwalk, owalk;
	BMEdge *e, *nexte = NULL;
	BMLoop *l;
	BMVert *v;
	int i = 0;

	BMW_state_remove_r(walker, &owalk);
	lwalk = &owalk;

	e = lwalk->cur;
	l = e->l;

	if (owalk.f_hub) { /* NGON EDGE */
		int vert_edge_tot;

		v = BM_edge_other_vert(e, lwalk->lastv);

		vert_edge_tot = BM_vert_edge_count_nonwire(v);

		if (vert_edge_tot == 3) {
			l = BM_face_other_vert_loop(owalk.f_hub, lwalk->lastv, v);
			nexte = BM_edge_exists(v, l->v);

			if (bmw_mask_check_edge(walker, nexte) &&
			    !BLI_gset_haskey(walker->visit_set, nexte) &&
			    /* never step onto a boundary edge, this gives odd-results */
			    (BM_edge_is_boundary(nexte) == false))
			{
				lwalk = BMW_state_add(walker);
				lwalk->cur = nexte;
				lwalk->lastv = v;

				lwalk->is_boundary = owalk.is_boundary;
				lwalk->is_single = owalk.is_single;
				lwalk->f_hub = owalk.f_hub;

				BLI_gset_insert(walker->visit_set, nexte);
			}
		}
	}
	else if (l == NULL) {  /* WIRE EDGE */
		BMIter eiter;

		/* match trunk: mark all connected wire edges */
		for (i = 0; i < 2; i++) {
			v = i ? e->v2 : e->v1;

			BM_ITER_ELEM (nexte, &eiter, v, BM_EDGES_OF_VERT) {
				if ((nexte->l == NULL) &&
				    bmw_mask_check_edge(walker, nexte) &&
				    !BLI_gset_haskey(walker->visit_set, nexte))
				{
					lwalk = BMW_state_add(walker);
					lwalk->cur = nexte;
					lwalk->lastv = v;

					lwalk->is_boundary = owalk.is_boundary;
					lwalk->is_single = owalk.is_single;
					lwalk->f_hub = owalk.f_hub;

					BLI_gset_insert(walker->visit_set, nexte);
				}
			}
		}
	}
	else if (owalk.is_boundary == false) {  /* NORMAL EDGE WITH FACES */
		int vert_edge_tot;

		v = BM_edge_other_vert(e, lwalk->lastv);

		vert_edge_tot = BM_vert_edge_count_nonwire(v);

		/* typical loopiong over edges in the middle of a mesh */
		/* however, why use 2 here at all? I guess for internal ngon loops it can be useful. Antony R. */
		if (vert_edge_tot == 4 || vert_edge_tot == 2) {
			int i_opposite = vert_edge_tot / 2;
			int i = 0;
			do {
				l = BM_loop_other_edge_loop(l, v);
				if (BM_edge_is_manifold(l->e)) {
					l = l->radial_next;
				}
				else {
					l = NULL;
					break;
				}
			} while ((++i != i_opposite));
		}
		else {
			l = NULL;
		}

		if (l != NULL) {
			if (l != e->l &&
			    bmw_mask_check_edge(walker, l->e) &&
			    !BLI_gset_haskey(walker->visit_set, l->e))
			{
				lwalk = BMW_state_add(walker);
				lwalk->cur = l->e;
				lwalk->lastv = v;

				lwalk->is_boundary = owalk.is_boundary;
				lwalk->is_single = owalk.is_single;
				lwalk->f_hub = owalk.f_hub;

				BLI_gset_insert(walker->visit_set, l->e);
			}
		}
	}
	else if (owalk.is_boundary == true) {  /* BOUNDARY EDGE WITH FACES */
		int vert_edge_tot;

		v = BM_edge_other_vert(e, lwalk->lastv);

		vert_edge_tot = BM_vert_edge_count_nonwire(v);

		/* check if we should step, this is fairly involved */
		if (
		    /* walk over boundary of faces but stop at corners */
		    (owalk.is_single == false && vert_edge_tot > 2) ||

		    /* initial edge was a boundary, so is this edge and vertex is only apart of this face
		    * this lets us walk over the the boundary of an ngon which is handy */
		    (owalk.is_single == true && vert_edge_tot == 2 && BM_edge_is_boundary(e)))
		{
			/* find next boundary edge in the fan */
			do {
				l = BM_loop_other_edge_loop(l, v);
				if (BM_edge_is_manifold(l->e)) {
					l = l->radial_next;
				}
				else if (BM_edge_is_boundary(l->e)) {
					break;
				}
				else {
					l = NULL;
					break;
				}
			} while (true);
		}

		if (owalk.is_single == false && l && bm_edge_is_single(l->e)) {
			l = NULL;
		}

		if (l != NULL) {
			if (l != e->l &&
			    bmw_mask_check_edge(walker, l->e) &&
			    !BLI_gset_haskey(walker->visit_set, l->e))
			{
				lwalk = BMW_state_add(walker);
				lwalk->cur = l->e;
				lwalk->lastv = v;

				lwalk->is_boundary = owalk.is_boundary;
				lwalk->is_single = owalk.is_single;
				lwalk->f_hub = owalk.f_hub;

				BLI_gset_insert(walker->visit_set, l->e);
			}
		}
	}

	return owalk.cur;
}

/** \} */


/** \name Face Loop Walker
 * \{
 *
 * Starts at a tool-flagged face and walks over the face loop
 * Conditions for starting and stepping the face loop have been
 * tuned in an attempt to match the face loops built by EditMesh
 */

/* Check whether the face loop should includes the face specified
 * by the given BMLoop */
static bool bmw_FaceLoopWalker_include_face(BMWalker *walker, BMLoop *l)
{
	/* face must have degree 4 */
	if (l->f->len != 4) {
		return false;
	}

	if (!bmw_mask_check_face(walker, l->f)) {
		return false;
	}

	/* the face must not have been already visited */
	if (BLI_gset_haskey(walker->visit_set, l->f) && BLI_gset_haskey(walker->visit_set_alt, l->e)) {
		return false;
	}

	return true;
}

/* Check whether the face loop can start from the given edge */
static bool bmw_FaceLoopWalker_edge_begins_loop(BMWalker *walker, BMEdge *e)
{
	/* There is no face loop starting from a wire edge */
	if (BM_edge_is_wire(e)) {
		return false;
	}
	
	/* Don't start a loop from a boundary edge if it cannot
	 * be extended to cover any faces */
	if (BM_edge_is_boundary(e)) {
		if (!bmw_FaceLoopWalker_include_face(walker, e->l)) {
			return false;
		}
	}
	
	/* Don't start a face loop from non-manifold edges */
	if (!BM_edge_is_manifold(e)) {
		return false;
	}

	return true;
}

static void bmw_FaceLoopWalker_begin(BMWalker *walker, void *data)
{
	BMwFaceLoopWalker *lwalk, owalk, *owalk_pt;
	BMEdge *e = data;
	/* BMesh *bm = walker->bm; */ /* UNUSED */
	/* int fcount = BM_edge_face_count(e); */ /* UNUSED */

	if (!bmw_FaceLoopWalker_edge_begins_loop(walker, e))
		return;

	lwalk = BMW_state_add(walker);
	lwalk->l = e->l;
	lwalk->no_calc = false;
	BLI_gset_insert(walker->visit_set, lwalk->l->f);

	/* rewind */
	while ((owalk_pt = BMW_current_state(walker))) {
		owalk = *((BMwFaceLoopWalker *)owalk_pt);
		BMW_walk(walker);
	}

	lwalk = BMW_state_add(walker);
	*lwalk = owalk;
	lwalk->no_calc = false;

	BLI_gset_clear(walker->visit_set_alt, NULL);
	BLI_gset_insert(walker->visit_set_alt, lwalk->l->e);

	BLI_gset_clear(walker->visit_set, NULL);
	BLI_gset_insert(walker->visit_set, lwalk->l->f);
}

static void *bmw_FaceLoopWalker_yield(BMWalker *walker)
{
	BMwFaceLoopWalker *lwalk = BMW_current_state(walker);
	
	if (!lwalk) {
		return NULL;
	}

	return lwalk->l->f;
}

static void *bmw_FaceLoopWalker_step(BMWalker *walker)
{
	BMwFaceLoopWalker *lwalk, owalk;
	BMFace *f;
	BMLoop *l;

	BMW_state_remove_r(walker, &owalk);
	lwalk = &owalk;

	f = lwalk->l->f;
	l = lwalk->l->radial_next;
	
	if (lwalk->no_calc) {
		return f;
	}

	if (!bmw_FaceLoopWalker_include_face(walker, l)) {
		l = lwalk->l;
		l = l->next->next;
		if (!BM_edge_is_manifold(l->e)) {
			l = l->prev->prev;
		}
		l = l->radial_next;
	}

	if (bmw_FaceLoopWalker_include_face(walker, l)) {
		lwalk = BMW_state_add(walker);
		lwalk->l = l;

		if (l->f->len != 4) {
			lwalk->no_calc = true;
			lwalk->l = owalk.l;
		}
		else {
			lwalk->no_calc = false;
		}

		/* both may already exist */
		BLI_gset_reinsert(walker->visit_set_alt, l->e, NULL);
		BLI_gset_reinsert(walker->visit_set, l->f, NULL);
	}

	return f;
}

/** \} */


// #define BMW_EDGERING_NGON

/** \name Edge Ring Walker
 * \{
 *
 * Starts at a tool-flagged edge and walks over the edge ring
 * Conditions for starting and stepping the edge ring have been
 * tuned in an attempt to match the edge rings built by EditMesh
 */
static void bmw_EdgeringWalker_begin(BMWalker *walker, void *data)
{
	BMwEdgeringWalker *lwalk, owalk, *owalk_pt;
	BMEdge *e = data;

	lwalk = BMW_state_add(walker);
	lwalk->l = e->l;

	if (!lwalk->l) {
		lwalk->wireedge = e;
		return;
	}
	else {
		lwalk->wireedge = NULL;
	}

	BLI_gset_insert(walker->visit_set, lwalk->l->e);

	/* rewind */
	while ((owalk_pt = BMW_current_state(walker))) {
		owalk = *((BMwEdgeringWalker *)owalk_pt);
		BMW_walk(walker);
	}

	lwalk = BMW_state_add(walker);
	*lwalk = owalk;

#ifdef BMW_EDGERING_NGON
	if (lwalk->l->f->len % 2 != 0)
#else
	if (lwalk->l->f->len != 4)
#endif
	{
		lwalk->l = lwalk->l->radial_next;
	}

	BLI_gset_clear(walker->visit_set, NULL);
	BLI_gset_insert(walker->visit_set, lwalk->l->e);
}

static void *bmw_EdgeringWalker_yield(BMWalker *walker)
{
	BMwEdgeringWalker *lwalk = BMW_current_state(walker);
	
	if (!lwalk) {
		return NULL;
	}

	if (lwalk->l) {
		return lwalk->l->e;
	}
	else {
		return lwalk->wireedge;
	}
}

static void *bmw_EdgeringWalker_step(BMWalker *walker)
{
	BMwEdgeringWalker *lwalk, owalk;
	BMEdge *e;
	BMLoop *l;
#ifdef BMW_EDGERING_NGON
	int i, len;
#endif

#define EDGE_CHECK(e) (bmw_mask_check_edge(walker, e) && (BM_edge_is_boundary(e) || BM_edge_is_manifold(e)))

	BMW_state_remove_r(walker, &owalk);
	lwalk = &owalk;

	l = lwalk->l;
	if (!l)
		return lwalk->wireedge;

	e = l->e;
	if (!EDGE_CHECK(e)) {
		/* walker won't traverse to a non-manifold edge, but may
		 * be started on one, and should not traverse *away* from
		 * a non-manifold edge (non-manifold edges are never in an
		 * edge ring with manifold edges */
		return e;
	}

#ifdef BMW_EDGERING_NGON
	l = l->radial_next;

	i = len = l->f->len;
	while (i > 0) {
		l = l->next;
		i -= 2;
	}

	if ((len <= 0) || (len % 2 != 0) || !EDGE_CHECK(l->e) ||
	    !bmw_mask_check_face(walker, l->f))
	{
		l = owalk.l;
		i = len;
		while (i > 0) {
			l = l->next;
			i -= 2;
		}
	}
	/* only walk to manifold edge */
	if ((l->f->len % 2 == 0) && EDGE_CHECK(l->e) &&
	    !BLI_gset_haskey(walker->visit_set, l->e))

#else

	l = l->radial_next;
	l = l->next->next;
	
	if ((l->f->len != 4) || !EDGE_CHECK(l->e) || !bmw_mask_check_face(walker, l->f)) {
		l = owalk.l->next->next;
	}
	/* only walk to manifold edge */
	if ((l->f->len == 4) && EDGE_CHECK(l->e) &&
	    !BLI_gset_haskey(walker->visit_set, l->e))
#endif
	{
		lwalk = BMW_state_add(walker);
		lwalk->l = l;
		lwalk->wireedge = NULL;

		BLI_gset_insert(walker->visit_set, l->e);
	}

	return e;

#undef EDGE_CHECK
}

/** \} */


/** \name UV Edge Walker
 * \{ */

static void bmw_UVEdgeWalker_begin(BMWalker *walker, void *data)
{
	BMwUVEdgeWalker *lwalk;
	BMLoop *l = data;

	if (BLI_gset_haskey(walker->visit_set, l))
		return;

	lwalk = BMW_state_add(walker);
	lwalk->l = l;
	BLI_gset_insert(walker->visit_set, l);
}

static void *bmw_UVEdgeWalker_yield(BMWalker *walker)
{
	BMwUVEdgeWalker *lwalk = BMW_current_state(walker);
	
	if (!lwalk) {
		return NULL;
	}

	return lwalk->l;
}

static void *bmw_UVEdgeWalker_step(BMWalker *walker)
{
	const int type = walker->bm->ldata.layers[walker->layer].type;
	BMwUVEdgeWalker *lwalk, owalk;
	BMLoop *l, *l2, *l3, *nl, *cl;
	BMIter liter;
	void *d1, *d2;
	int i, j, rlen;

	BMW_state_remove_r(walker, &owalk);
	lwalk = &owalk;

	l = lwalk->l;
	nl = l->next;

	if (!bmw_mask_check_edge(walker, l->e)) {
		return l;
	}

	/* go over loops around l->v and nl->v and see which ones share l and nl's
	 * mloopuv's coordinates. in addition, push on l->next if necessary */
	for (i = 0; i < 2; i++) {
		cl = i ? nl : l;
		BM_ITER_ELEM (l2, &liter, cl->v, BM_LOOPS_OF_VERT) {
			d1 = CustomData_bmesh_get_layer_n(&walker->bm->ldata,
			                                  cl->head.data, walker->layer);
			
			rlen = BM_edge_face_count(l2->e);
			for (j = 0; j < rlen; j++) {
				if (BLI_gset_haskey(walker->visit_set, l2)) {
					continue;
				}

				if (!bmw_mask_check_edge(walker, l2->e)) {
					if (l2->v != cl->v) {
						continue;
					}
				}

				l3 = l2->v != cl->v ? l2->next : l2;
				d2 = CustomData_bmesh_get_layer_n(&walker->bm->ldata,
				                                  l3->head.data, walker->layer);

				if (!CustomData_data_equals(type, d1, d2))
					continue;
				
				lwalk = BMW_state_add(walker);
				BLI_gset_insert(walker->visit_set, l2);

				lwalk->l = l2;

				l2 = l2->radial_next;
			}
		}
	}

	return l;
}

/** \} */


static BMWalker bmw_ShellWalker_Type = {
	bmw_ShellWalker_begin,
	bmw_ShellWalker_step,
	bmw_ShellWalker_yield,
	sizeof(BMwShellWalker),
	BMW_BREADTH_FIRST,
	BM_EDGE, /* valid restrict masks */
};

static BMWalker bmw_IslandboundWalker_Type = {
	bmw_IslandboundWalker_begin,
	bmw_IslandboundWalker_step,
	bmw_IslandboundWalker_yield,
	sizeof(BMwIslandboundWalker),
	BMW_DEPTH_FIRST,
	BM_FACE, /* valid restrict masks */
};

static BMWalker bmw_IslandWalker_Type = {
	bmw_IslandWalker_begin,
	bmw_IslandWalker_step,
	bmw_IslandWalker_yield,
	sizeof(BMwIslandWalker),
	BMW_BREADTH_FIRST,
	BM_EDGE | BM_FACE, /* valid restrict masks */
};

static BMWalker bmw_LoopWalker_Type = {
	bmw_LoopWalker_begin,
	bmw_LoopWalker_step,
	bmw_LoopWalker_yield,
	sizeof(BMwLoopWalker),
	BMW_DEPTH_FIRST,
	0, /* valid restrict masks */ /* could add flags here but so far none are used */
};

static BMWalker bmw_FaceLoopWalker_Type = {
	bmw_FaceLoopWalker_begin,
	bmw_FaceLoopWalker_step,
	bmw_FaceLoopWalker_yield,
	sizeof(BMwFaceLoopWalker),
	BMW_DEPTH_FIRST,
	0, /* valid restrict masks */ /* could add flags here but so far none are used */
};

static BMWalker bmw_EdgeringWalker_Type = {
	bmw_EdgeringWalker_begin,
	bmw_EdgeringWalker_step,
	bmw_EdgeringWalker_yield,
	sizeof(BMwEdgeringWalker),
	BMW_DEPTH_FIRST,
	0, /* valid restrict masks */ /* could add flags here but so far none are used */
};

static BMWalker bmw_UVEdgeWalker_Type = {
	bmw_UVEdgeWalker_begin,
	bmw_UVEdgeWalker_step,
	bmw_UVEdgeWalker_yield,
	sizeof(BMwUVEdgeWalker),
	BMW_DEPTH_FIRST,
	BM_EDGE, /* valid restrict masks */
};

static BMWalker bmw_ConnectedVertexWalker_Type = {
	bmw_ConnectedVertexWalker_begin,
	bmw_ConnectedVertexWalker_step,
	bmw_ConnectedVertexWalker_yield,
	sizeof(BMwConnectedVertexWalker),
	BMW_BREADTH_FIRST,
	BM_VERT, /* valid restrict masks */
};

BMWalker *bm_walker_types[] = {
	&bmw_ShellWalker_Type,              /* BMW_SHELL */
	&bmw_LoopWalker_Type,               /* BMW_LOOP */
	&bmw_FaceLoopWalker_Type,           /* BMW_FACELOOP */
	&bmw_EdgeringWalker_Type,           /* BMW_EDGERING */
	&bmw_UVEdgeWalker_Type,             /* BMW_LOOPDATA_ISLAND */
	&bmw_IslandboundWalker_Type,        /* BMW_ISLANDBOUND */
	&bmw_IslandWalker_Type,             /* BMW_ISLAND */
	&bmw_ConnectedVertexWalker_Type,    /* BMW_CONNECTED_VERTEX */
};

const int bm_totwalkers = sizeof(bm_walker_types) / sizeof(*bm_walker_types);
