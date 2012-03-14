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

#include "BKE_customdata.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"
#include "bmesh_walkers_private.h"

/**
 * Shell Walker:
 *
 * Starts at a vertex on the mesh and walks over the 'shell' it belongs
 * to via visiting connected edges.
 *
 * \todo Add restriction flag/callback for wire edges.
 */
static void bmw_ShellWalker_visitEdge(BMWalker *walker, BMEdge *e)
{
	BMwShellWalker *shellWalk = NULL;

	if (BLI_ghash_haskey(walker->visithash, e)) {
		return;
	}

	if (walker->mask_edge && !BMO_elem_flag_test(walker->bm, e, walker->mask_edge)) {
		return;
	}

	shellWalk = BMW_state_add(walker);
	shellWalk->curedge = e;
	BLI_ghash_insert(walker->visithash, e, NULL);
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
			BM_ITER(e, &eiter, walker->bm, BM_EDGES_OF_VERT, v) {
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
	BMwShellWalker *swalk = BMW_current_state(walker);
	BMEdge *e, *e2;
	BMVert *v;
	BMIter iter;
	int i;

	e = swalk->curedge;
	BMW_state_remove(walker);

	for (i = 0; i < 2; i++) {
		v = i ? e->v2 : e->v1;
		BM_ITER(e2, &iter, walker->bm, BM_EDGES_OF_VERT, v) {
			bmw_ShellWalker_visitEdge(walker, e2);
		}
	}

	return e;
}

#if 0
static void *bmw_ShellWalker_step(BMWalker *walker)
{
	BMEdge *curedge, *next = NULL;
	BMVert *ov = NULL;
	int restrictpass = 1;
	BMwShellWalker shellWalk = *((BMwShellWalker *)BMW_current_state(walker));
	
	if (!BLI_ghash_haskey(walker->visithash, shellWalk.base)) {
		BLI_ghash_insert(walker->visithash, shellWalk.base, NULL);
	}

	BMW_state_remove(walker);


	/* find the next edge whose other vertex has not been visite */
	curedge = shellWalk.curedge;
	do {
		if (!BLI_ghash_haskey(walker->visithash, curedge)) {
			if (!walker->restrictflag ||
			    (walker->restrictflag && BMO_elem_flag_test(walker->bm, curedge, walker->restrictflag)))
			{
				BMwShellWalker *newstate;

				ov = BM_edge_other_vert(curedge, shellWalk.base);
				
				/* push a new state onto the stac */
				newState = BMW_state_add(walker);
				BLI_ghash_insert(walker->visithash, curedge, NULL);
				
				/* populate the new stat */

				newState->base = ov;
				newState->curedge = curedge;
			}
		}
		curedge = bmesh_disk_edge_next(curedge, shellWalk.base);
	} while (curedge != shellWalk.curedge);
	
	return shellWalk.curedge;
}
#endif

/**
 * Connected Vertex Walker:
 *
 * Similar to shell walker, but visits vertices instead of edges.
 */
static void bmw_ConnectedVertexWalker_visitVertex(BMWalker *walker, BMVert *v)
{
	BMwConnectedVertexWalker *vwalk;

	if (BLI_ghash_haskey(walker->visithash, v)) {
		/* already visited */
		return;
	}
	if (walker->mask_vert && !BMO_elem_flag_test(walker->bm, v, walker->mask_vert)) {
		/* not flagged for walk */
		return;
	}

	vwalk = BMW_state_add(walker);
	vwalk->curvert = v;
	BLI_ghash_insert(walker->visithash, v, NULL);
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
	BMwConnectedVertexWalker *vwalk = BMW_current_state(walker);
	BMVert *v, *v2;
	BMEdge *e;
	BMIter iter;

	v = vwalk->curvert;

	BMW_state_remove(walker);

	BM_ITER(e, &iter, walker->bm, BM_EDGES_OF_VERT, v) {
		v2 = BM_edge_other_vert(e, v);
		if (!BLI_ghash_haskey(walker->visithash, v2)) {
			bmw_ConnectedVertexWalker_visitVertex(walker, v2);
		}
	}

	return v;
}

/**
 * Island Boundary Walker:
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

	BLI_ghash_insert(walker->visithash, data, NULL);

}

static void *bmw_IslandboundWalker_yield(BMWalker *walker)
{
	BMwIslandboundWalker *iwalk = BMW_current_state(walker);

	return iwalk->curloop;
}

static void *bmw_IslandboundWalker_step(BMWalker *walker)
{
	BMwIslandboundWalker *iwalk = BMW_current_state(walker), owalk;
	BMVert *v;
	BMEdge *e = iwalk->curloop->e;
	BMFace *f;
	BMLoop *l = iwalk->curloop;
	/* int found = 0; */

	owalk = *iwalk;

	v = BM_edge_other_vert(e, iwalk->lastv);

	if (!BM_vert_is_manifold(walker->bm, v)) {
		BMW_reset(walker);
		BMO_error_raise(walker->bm, NULL, BMERR_WALKER_FAILED,
		                "Non-manifold vert "
		                "while searching region boundary");
		return NULL;
	}
	
	/* pop off current stat */
	BMW_state_remove(walker);
	
	f = l->f;
	
	while (1) {
		l = BM_face_other_edge_loop(f, e, v);
		if (l != l->radial_next) {
			l = l->radial_next;
			f = l->f;
			e = l->e;
			if (walker->mask_face && !BMO_elem_flag_test(walker->bm, f, walker->mask_face)) {
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
	else if (BLI_ghash_haskey(walker->visithash, l)) {
		return owalk.curloop;
	}

	BLI_ghash_insert(walker->visithash, l, NULL);
	iwalk = BMW_state_add(walker);
	iwalk->base = owalk.base;

	//if (!BMO_elem_flag_test(walker->bm, l->f, walker->restrictflag))
	//	iwalk->curloop = l->radial_next;
	iwalk->curloop = l; //else iwalk->curloop = l;
	iwalk->lastv = v;

	return owalk.curloop;
}


/**
 * Island Walker:
 *
 * Starts at a tool flagged-face and walks over the face region
 *
 * \todo Add restriction flag/callback for wire edges.
 */
static void bmw_IslandWalker_begin(BMWalker *walker, void *data)
{
	BMwIslandWalker *iwalk = NULL;

	if (walker->mask_face && !BMO_elem_flag_test(walker->bm, (BMElemF *)data, walker->mask_face)) {
		return;
	}

	iwalk = BMW_state_add(walker);
	BLI_ghash_insert(walker->visithash, data, NULL);

	iwalk->cur = data;
}

static void *bmw_IslandWalker_yield(BMWalker *walker)
{
	BMwIslandWalker *iwalk = BMW_current_state(walker);

	return iwalk->cur;
}

static void *bmw_IslandWalker_step(BMWalker *walker)
{
	BMwIslandWalker *iwalk = BMW_current_state(walker);
	/* BMwIslandWalker *owalk = iwalk; */ /* UNUSED */
	BMIter iter, liter;
	BMFace *f, *curf = iwalk->cur;
	BMLoop *l;
	
	BMW_state_remove(walker);

	l = BM_iter_new(&liter, walker->bm, BM_LOOPS_OF_FACE, iwalk->cur);
	for ( ; l; l = BM_iter_step(&liter)) {
		/* could skip loop here too, but dont add unless we need it */
		if (walker->mask_edge && !BMO_elem_flag_test(walker->bm, l->e, walker->mask_edge)) {
			continue;
		}

		f = BM_iter_new(&iter, walker->bm, BM_FACES_OF_EDGE, l->e);
		for ( ; f; f = BM_iter_step(&iter)) {
			if (walker->mask_face && !BMO_elem_flag_test(walker->bm, f, walker->mask_face)) {
				continue;
			}

			if (BLI_ghash_haskey(walker->visithash, f)) {
				continue;
			}
			
			iwalk = BMW_state_add(walker);
			iwalk->cur = f;
			BLI_ghash_insert(walker->visithash, f, NULL);
			break;
		}
	}
	
	return curf;
}


/**
 * Edge Loop Walker:
 *
 * Starts at a tool-flagged edge and walks over the edge loop
 */
static void bmw_LoopWalker_begin(BMWalker *walker, void *data)
{
	BMwLoopWalker *lwalk = NULL, owalk;
	BMEdge *e = data;
	BMVert *v;
	int vert_edge_count[2] = {BM_vert_edge_count_nonwire(e->v1),
	                          BM_vert_edge_count_nonwire(e->v2)};

	v = e->v1;

	lwalk = BMW_state_add(walker);
	BLI_ghash_insert(walker->visithash, e, NULL);

	lwalk->cur = lwalk->start = e;
	lwalk->lastv = lwalk->startv = v;
	lwalk->is_boundry = BM_edge_is_boundary(e);
	lwalk->is_single = (vert_edge_count[0] == 2 && vert_edge_count[1] == 2);

	/* could also check that vertex*/
	if ((lwalk->is_boundry == FALSE) &&
	    (vert_edge_count[0] == 3 || vert_edge_count[1] == 3))
	{
		BMIter iter;
		BMFace *f_iter;
		BMFace *f_best = NULL;

		BM_ITER(f_iter, &iter, walker->bm, BM_FACES_OF_EDGE, e) {
			if (f_best == NULL || f_best->len < f_iter->len) {
				f_best = f_iter;
			}
		}

		/* only use hub selection for 5+ sides else this could
		 * conflict with normal edge loop selection. */
		lwalk->f_hub = f_best->len > 4 ? f_best : NULL;
	}
	else {
		lwalk->f_hub = NULL;
	}

	/* rewind */
	while (BMW_current_state(walker)) {
		owalk = *((BMwLoopWalker *)BMW_current_state(walker));
		BMW_walk(walker);
	}

	lwalk = BMW_state_add(walker);
	*lwalk = owalk;

	lwalk->lastv = lwalk->startv = BM_edge_other_vert(owalk.cur, lwalk->lastv);

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 2");
	BLI_ghash_insert(walker->visithash, owalk.cur, NULL);
}

static void *bmw_LoopWalker_yield(BMWalker *walker)
{
	BMwLoopWalker *lwalk = BMW_current_state(walker);

	return lwalk->cur;
}

static void *bmw_LoopWalker_step(BMWalker *walker)
{
	BMwLoopWalker *lwalk = BMW_current_state(walker), owalk;
	BMEdge *e = lwalk->cur, *nexte = NULL;
	BMLoop *l;
	BMVert *v;
	int i;

	owalk = *lwalk;
	BMW_state_remove(walker);

	l = e->l;

	if (owalk.f_hub) { /* NGON EDGE */
		int vert_edge_tot;

		v = BM_edge_other_vert(e, lwalk->lastv);

		vert_edge_tot = BM_vert_edge_count_nonwire(v);

		if (vert_edge_tot == 3) {
			l = BM_face_other_vert_loop(owalk.f_hub, lwalk->lastv, v);
			nexte = BM_edge_exists(v, l->v);

            if(!BLI_ghash_haskey(walker->visithash, nexte)){
                lwalk = BMW_state_add(walker);
                lwalk->cur = nexte;
                lwalk->lastv = v;

                lwalk->is_boundry = owalk.is_boundry;
                lwalk->is_single = owalk.is_single;
                lwalk->f_hub = owalk.f_hub;

                BLI_ghash_insert(walker->visithash, nexte, NULL);
            }
		}
	}
	else if (l) { /* NORMAL EDGE WITH FACES */
		int vert_edge_tot;
		int stopi;

		v = BM_edge_other_vert(e, lwalk->lastv);

		vert_edge_tot = BM_vert_edge_count_nonwire(v);

		if (/* check if we should step, this is fairly involved */

			/* typical loopiong over edges in the middle of a mesh */
			/* however, why use 2 here at all? I guess for internal ngon loops it can be useful. Antony R. */
			((vert_edge_tot == 4 || vert_edge_tot == 2) && owalk.is_boundry == FALSE) ||

			/* walk over boundry of faces but stop at corners */
			(owalk.is_boundry == TRUE && owalk.is_single  == FALSE && vert_edge_tot > 2) ||

			/* initial edge was a boundry, so is this edge and vertex is only apart of this face
			 * this lets us walk over the the boundry of an ngon which is handy */
			(owalk.is_boundry == TRUE && owalk.is_single == TRUE && vert_edge_tot == 2 && BM_edge_is_boundary(e)))
		{
			i = 0;
			stopi = vert_edge_tot / 2;
			while (1) {
				if ((owalk.is_boundry == FALSE) && (i == stopi)) {
					break;
				}

				l = BM_face_other_edge_loop(l->f, l->e, v);

				if (l == NULL) {
					break;
				}
				else {
					BMLoop *l_next;

					l_next = l->radial_next;

					if ((l_next == l) || (l_next == NULL)) {
						break;
					}

					l = l_next;
					i++;
				}
			}
		}

		if (l != NULL) {
			if (l != e->l && !BLI_ghash_haskey(walker->visithash, l->e)) {
				if (!(owalk.is_boundry == FALSE && i != stopi)) {
					lwalk = BMW_state_add(walker);
					lwalk->cur = l->e;
					lwalk->lastv = v;

					lwalk->is_boundry = owalk.is_boundry;
					lwalk->is_single = owalk.is_single;
					lwalk->f_hub = owalk.f_hub;

					BLI_ghash_insert(walker->visithash, l->e, NULL);
				}
			}
		}
	}
	else { 	/* WIRE EDGE */
		BMIter eiter;

		/* match trunk: mark all connected wire edges */
		for (i = 0; i < 2; i++) {
			v = i ? e->v2 : e->v1;

			BM_ITER(nexte, &eiter, walker->bm, BM_EDGES_OF_VERT, v) {
				if ((nexte->l == NULL) && !BLI_ghash_haskey(walker->visithash, nexte)) {
					lwalk = BMW_state_add(walker);
					lwalk->cur = nexte;
					lwalk->lastv = v;

					lwalk->is_boundry = owalk.is_boundry;
					lwalk->is_single = owalk.is_single;
					lwalk->f_hub = owalk.f_hub;

					BLI_ghash_insert(walker->visithash, nexte, NULL);
				}
			}
		}
	}

	return owalk.cur;
}

/**
 * Face Loop Walker:
 *
 * Starts at a tool-flagged face and walks over the face loop
 * Conditions for starting and stepping the face loop have been
 * tuned in an attempt to match the face loops built by EditMesh
 */

/* Check whether the face loop should includes the face specified
 * by the given BMLoop */
static int bmw_FaceLoopWalker_include_face(BMWalker *walker, BMLoop *l)
{
	/* face must have degree 4 */
	if (l->f->len != 4) {
		return FALSE;
	}

	/* the face must not have been already visite */
	if (BLI_ghash_haskey(walker->visithash, l->f)) {
		return FALSE;
	}

	return TRUE;
}

/* Check whether the face loop can start from the given edge */
static int bmw_FaceLoopWalker_edge_begins_loop(BMWalker *walker, BMEdge *e)
{
	BMesh *bm = walker->bm;

	/* There is no face loop starting from a wire edge */
	if (BM_edge_is_wire(bm, e)) {
		return FALSE;
	}
	
	/* Don't start a loop from a boundary edge if it cannot
	 * be extended to cover any faces */
	if (BM_edge_face_count(e) == 1) {
		if (!bmw_FaceLoopWalker_include_face(walker, e->l)) {
			return FALSE;
		}
	}
	
	/* Don't start a face loop from non-manifold edges */
	if (!BM_edge_is_manifold(bm, e)) {
		return FALSE;
	}

	return TRUE;
}

static void bmw_FaceLoopWalker_begin(BMWalker *walker, void *data)
{
	BMwFaceLoopWalker *lwalk, owalk;
	BMEdge *e = data;
	/* BMesh *bm = walker->bm; */ /* UNUSED */
	/* int fcount = BM_edge_face_count(e); */ /* UNUSED */

	if (!bmw_FaceLoopWalker_edge_begins_loop(walker, e))
		return;

	lwalk = BMW_state_add(walker);
	lwalk->l = e->l;
	lwalk->nocalc = 0;
	BLI_ghash_insert(walker->visithash, lwalk->l->f, NULL);

	/* rewin */
	while (BMW_current_state(walker)) {
		owalk = *((BMwFaceLoopWalker *)BMW_current_state(walker));
		BMW_walk(walker);
	}

	lwalk = BMW_state_add(walker);
	*lwalk = owalk;
	lwalk->nocalc = 0;

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 3");
	BLI_ghash_insert(walker->visithash, lwalk->l->f, NULL);
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
	BMwFaceLoopWalker *lwalk = BMW_current_state(walker);
	BMFace *f = lwalk->l->f;
	BMLoop *l = lwalk->l, *origl = lwalk->l;

	BMW_state_remove(walker);

	l = l->radial_next;
	
	if (lwalk->nocalc) {
		return f;
	}

	if (!bmw_FaceLoopWalker_include_face(walker, l)) {
		l = lwalk->l;
		l = l->next->next;
		if (BM_edge_face_count(l->e) != 2) {
			l = l->prev->prev;
		}
		l = l->radial_next;
	}

	if (bmw_FaceLoopWalker_include_face(walker, l)) {
		lwalk = BMW_state_add(walker);
		lwalk->l = l;

		if (l->f->len != 4) {
			lwalk->nocalc = 1;
			lwalk->l = origl;
		}
		else {
			lwalk->nocalc = 0;
		}

		BLI_ghash_insert(walker->visithash, l->f, NULL);
	}

	return f;
}

// #define BMW_EDGERING_NGON

/**
 * Edge Ring Walker:
 *
 * Starts at a tool-flagged edge and walks over the edge ring
 * Conditions for starting and stepping the edge ring have been
 * tuned in an attempt to match the edge rings built by EditMesh
 */
static void bmw_EdgeringWalker_begin(BMWalker *walker, void *data)
{
	BMwEdgeringWalker *lwalk, owalk;
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

	BLI_ghash_insert(walker->visithash, lwalk->l->e, NULL);

	/* rewin */
	while (BMW_current_state(walker)) {
		owalk = *((BMwEdgeringWalker *)BMW_current_state(walker));
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

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 4");
	BLI_ghash_insert(walker->visithash, lwalk->l->e, NULL);
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
	BMwEdgeringWalker *lwalk = BMW_current_state(walker);
	BMEdge *e;
	BMLoop *l = lwalk->l /* , *origl = lwalk->l */;
	BMesh *bm = walker->bm;
#ifdef BMW_EDGERING_NGON
	int i, len;
#endif

	BMW_state_remove(walker);

	if (!l)
		return lwalk->wireedge;

	e = l->e;
	if (!BM_edge_is_manifold(bm, e)) {
		/* walker won't traverse to a non-manifold edge, but may
		 * be started on one, and should not traverse *away* from
		 * a non-manfold edge (non-manifold edges are never in an
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

	if ((len <= 0) || (len % 2 != 0) || !BM_edge_is_manifold(bm, l->e)) {
		l = lwalk->l;
		i = len;
		while (i > 0) {
			l = l->next;
			i -= 2;
		}
	}
	/* only walk to manifold edge */
	if ((l->f->len % 2 == 0) && BM_edge_is_manifold(bm, l->e) &&
	    !BLI_ghash_haskey(walker->visithash, l->e)) 

#else

	l = l->radial_next;
	l = l->next->next;
	
	if ((l->f->len != 4) || !BM_edge_is_manifold(bm, l->e)) {
		l = lwalk->l->next->next;
	}
	/* only walk to manifold edge */
	if ((l->f->len == 4) && BM_edge_is_manifold(bm, l->e) &&
	    !BLI_ghash_haskey(walker->visithash, l->e))
#endif
	{
		lwalk = BMW_state_add(walker);
		lwalk->l = l;
		lwalk->wireedge = NULL;

		BLI_ghash_insert(walker->visithash, l->e, NULL);
	}

	return e;
}

static void bmw_UVEdgeWalker_begin(BMWalker *walker, void *data)
{
	BMwUVEdgeWalker *lwalk;
	BMLoop *l = data;

	if (BLI_ghash_haskey(walker->visithash, l))
		return;

	lwalk = BMW_state_add(walker);
	lwalk->l = l;
	BLI_ghash_insert(walker->visithash, l, NULL);
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
	BMwUVEdgeWalker *lwalk = BMW_current_state(walker);
	BMLoop *l, *l2, *l3, *nl, *cl;
	BMIter liter;
	void *d1, *d2;
	int i, j, rlen, type;

	l = lwalk->l;
	nl = l->next;
	type = walker->bm->ldata.layers[walker->layer].type;

	BMW_state_remove(walker);
	
	if (walker->mask_edge && !BMO_elem_flag_test(walker->bm, l->e, walker->mask_edge))
		return l;

	/* go over loops around l->v and nl->v and see which ones share l and nl's
	 * mloopuv's coordinates. in addition, push on l->next if necessary */
	for (i = 0; i < 2; i++) {
		cl = i ? nl : l;
		BM_ITER(l2, &liter, walker->bm, BM_LOOPS_OF_VERT, cl->v) {
			d1 = CustomData_bmesh_get_layer_n(&walker->bm->ldata,
			                                  cl->head.data, walker->layer);
			
			rlen = BM_edge_face_count(l2->e);
			for (j = 0; j < rlen; j++) {
				if (BLI_ghash_haskey(walker->visithash, l2))
					continue;
				if (walker->mask_edge && !(BMO_elem_flag_test(walker->bm, l2->e, walker->mask_edge))) {
					if (l2->v != cl->v)
						continue;
				}
				
				l3 = l2->v != cl->v ? l2->next : l2;
				d2 = CustomData_bmesh_get_layer_n(&walker->bm->ldata,
				                                  l3->head.data, walker->layer);

				if (!CustomData_data_equals(type, d1, d2))
					continue;
				
				lwalk = BMW_state_add(walker);
				BLI_ghash_insert(walker->visithash, l2, NULL);

				lwalk->l = l2;

				l2 = l2->radial_next;
			}
		}
	}

	return l;
}

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
