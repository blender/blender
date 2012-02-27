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
#include "bmesh_private.h"
#include "bmesh_walkers_private.h"

/*	Shell Walker:
 *
 *	Starts at a vertex on the mesh and walks over the 'shell' it belongs
 *	to via visiting connected edges.
 *
 *	TODO:
 *
 *  Add restriction flag/callback for wire edges.
 *
 */

static void shellWalker_visitEdge(BMWalker *walker, BMEdge *e)
{
	shellWalker *shellWalk = NULL;

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

static void shellWalker_begin(BMWalker *walker, void *data)
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
				shellWalker_visitEdge(walker, e);
			}
			break;
		}

		case BM_EDGE:
		{
			/* starting the walk at an edge, add the single edge
			 * to the worklist */
			e = (BMEdge *)h;
			shellWalker_visitEdge(walker, e);
			break;
		}
	}
}

static void *shellWalker_yield(BMWalker *walker)
{
	shellWalker *shellWalk = BMW_current_state(walker);
	return shellWalk->curedge;
}

static void *shellWalker_step(BMWalker *walker)
{
	shellWalker *swalk = BMW_current_state(walker);
	BMEdge *e, *e2;
	BMVert *v;
	BMIter iter;
	int i;

	e = swalk->curedge;
	BMW_state_remove(walker);

	for (i = 0; i < 2; i++) {
		v = i ? e->v2 : e->v1;
		BM_ITER(e2, &iter, walker->bm, BM_EDGES_OF_VERT, v) {
			shellWalker_visitEdge(walker, e2);
		}
	}

	return e;
}

#if 0
static void *shellWalker_step(BMWalker *walker)
{
	BMEdge *curedge, *next = NULL;
	BMVert *ov = NULL;
	int restrictpass = 1;
	shellWalker shellWalk = *((shellWalker *)BMW_current_state(walker));
	
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
				shellWalker *newstate;

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

/*	Connected Vertex Walker:
 *
 *	Similar to shell walker, but visits vertices instead of edges.
 *
 */

static void connectedVertexWalker_visitVertex(BMWalker *walker, BMVert *v)
{
	connectedVertexWalker *vwalk;

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

static void connectedVertexWalker_begin(BMWalker *walker, void *data)
{
	BMVert *v = data;
	connectedVertexWalker_visitVertex(walker, v);
}

static void *connectedVertexWalker_yield(BMWalker *walker)
{
	connectedVertexWalker *vwalk = BMW_current_state(walker);
	return vwalk->curvert;
}

static void *connectedVertexWalker_step(BMWalker *walker)
{
	connectedVertexWalker *vwalk = BMW_current_state(walker);
	BMVert *v, *v2;
	BMEdge *e;
	BMIter iter;

	v = vwalk->curvert;

	BMW_state_remove(walker);

	BM_ITER(e, &iter, walker->bm, BM_EDGES_OF_VERT, v) {
		v2 = BM_edge_other_vert(e, v);
		if (!BLI_ghash_haskey(walker->visithash, v2)) {
			connectedVertexWalker_visitVertex(walker, v2);
		}
	}

	return v;
}

/*	Island Boundary Walker:
 *
 *	Starts at a edge on the mesh and walks over the boundary of an
 *      island it belongs to.
 *
 *	TODO:
 *
 *  Add restriction flag/callback for wire edges.
 *
 */

static void islandboundWalker_begin(BMWalker *walker, void *data)
{
	BMLoop *l = data;
	islandboundWalker *iwalk = NULL;

	iwalk = BMW_state_add(walker);

	iwalk->base = iwalk->curloop = l;
	iwalk->lastv = l->v;

	BLI_ghash_insert(walker->visithash, data, NULL);

}

static void *islandboundWalker_yield(BMWalker *walker)
{
	islandboundWalker *iwalk = BMW_current_state(walker);

	return iwalk->curloop;
}

static void *islandboundWalker_step(BMWalker *walker)
{
	islandboundWalker *iwalk = BMW_current_state(walker), owalk;
	BMVert *v;
	BMEdge *e = iwalk->curloop->e;
	BMFace *f;
	BMLoop *l = iwalk->curloop;
	/* int found = 0; */

	owalk = *iwalk;

	if (iwalk->lastv == e->v1) v = e->v2;
	else v = e->v1;

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
		l = BM_face_other_loop(e, f, v);
		if (bmesh_radial_loop_next(l) != l) {
			l = bmesh_radial_loop_next(l);
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


/*	Island Walker:
 *
 *	Starts at a tool flagged-face and walks over the face region
 *
 *	TODO:
 *
 *  Add restriction flag/callback for wire edges.
 *
 */

static void islandWalker_begin(BMWalker *walker, void *data)
{
	islandWalker *iwalk = NULL;

	if (walker->mask_face && !BMO_elem_flag_test(walker->bm, (BMElemF *)data, walker->mask_face)) {
		return;
	}

	iwalk = BMW_state_add(walker);
	BLI_ghash_insert(walker->visithash, data, NULL);

	iwalk->cur = data;
}

static void *islandWalker_yield(BMWalker *walker)
{
	islandWalker *iwalk = BMW_current_state(walker);

	return iwalk->cur;
}

static void *islandWalker_step(BMWalker *walker)
{
	islandWalker *iwalk = BMW_current_state(walker);
	/* islandWalker *owalk = iwalk; */ /* UNUSED */
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
			if (BLI_ghash_haskey(walker->visithash, f)) continue;
			
			iwalk = BMW_state_add(walker);
			iwalk->cur = f;
			BLI_ghash_insert(walker->visithash, f, NULL);
			break;
		}
	}
	
	return curf;
}


/*	Edge Loop Walker:
 *
 *	Starts at a tool-flagged edge and walks over the edge loop
 *
 */

static void loopWalker_begin(BMWalker *walker, void *data)
{
	loopWalker *lwalk = NULL, owalk;
	BMEdge *e = data;
	BMVert *v;
	/* int found = 1, val; */ /* UNUSED */

	v = e->v1;

	/* val = BM_vert_edge_count(v); */ /* UNUSED */

	lwalk = BMW_state_add(walker);
	BLI_ghash_insert(walker->visithash, e, NULL);

	lwalk->cur = lwalk->start = e;
	lwalk->lastv = lwalk->startv = v;
	lwalk->stage2 = 0;
	lwalk->startrad = BM_edge_face_count(e);

	/* rewin */
	while (BMW_current_state(walker)) {
		owalk = *((loopWalker *)BMW_current_state(walker));
		BMW_walk(walker);
	}

	lwalk = BMW_state_add(walker);
	*lwalk = owalk;

	if (lwalk->lastv == owalk.cur->v1) lwalk->lastv = owalk.cur->v2;
	else lwalk->lastv = owalk.cur->v1;

	lwalk->startv = lwalk->lastv;

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 2");
	BLI_ghash_insert(walker->visithash, owalk.cur, NULL);
}

static void *loopWalker_yield(BMWalker *walker)
{
	loopWalker *lwalk = BMW_current_state(walker);

	return lwalk->cur;
}

static void *loopWalker_step(BMWalker *walker)
{
	loopWalker *lwalk = BMW_current_state(walker), owalk;
	BMIter eiter;
	BMEdge *e = lwalk->cur, *nexte = NULL;
	BMLoop *l, *l2;
	BMVert *v;
	int val, rlen /* , found = 0 */, i = 0, stopi;

	owalk = *lwalk;
	BMW_state_remove(walker);

	l = e->l;

	/* handle wire edge case */
	if (!l) {

		/* match trunk: mark all connected wire edges */
		for (i = 0; i < 2; i++) {
			v = i ? e->v2 : e->v1;

			BM_ITER(nexte, &eiter, walker->bm, BM_EDGES_OF_VERT, v) {
				if ((nexte->l == NULL) && !BLI_ghash_haskey(walker->visithash, nexte)) {
					lwalk = BMW_state_add(walker);
					lwalk->cur = nexte;
					lwalk->lastv = v;
					lwalk->startrad = owalk.startrad;

					BLI_ghash_insert(walker->visithash, nexte, NULL);
				}
			}
		}

		return owalk.cur;
	}

	v = (e->v1 == lwalk->lastv) ? e->v2 : e->v1;

	val = BM_vert_edge_count(v);

	rlen = owalk.startrad;

	if (val == 4 || val == 2 || rlen == 1) {
		i = 0;
		stopi = val / 2;
		while (1) {
			if (rlen != 1 && i == stopi) break;

			l = BM_face_other_loop(l->e, l->f, v);

			if (!l)
				break;

			l2 = bmesh_radial_loop_next(l);

			if (l2 == l) {
				break;
			}

			l = l2;
			i += 1;
		}
	}

	if (!l) {
		return owalk.cur;
	}

	if (l != e->l && !BLI_ghash_haskey(walker->visithash, l->e)) {
		if (!(rlen != 1 && i != stopi)) {
			lwalk = BMW_state_add(walker);
			lwalk->cur = l->e;
			lwalk->lastv = v;
			lwalk->startrad = owalk.startrad;
			BLI_ghash_insert(walker->visithash, l->e, NULL);
		}
	}

	return owalk.cur;
}

/*	Face Loop Walker:
 *
 *	Starts at a tool-flagged face and walks over the face loop
 * Conditions for starting and stepping the face loop have been
 * tuned in an attempt to match the face loops built by EditMesh
 *
 */

/* Check whether the face loop should includes the face specified
 * by the given BMLoop */
static int faceloopWalker_include_face(BMWalker *walker, BMLoop *l)
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
static int faceloopWalker_edge_begins_loop(BMWalker *walker, BMEdge *e)
{
	BMesh *bm = walker->bm;

	/* There is no face loop starting from a wire edge */
	if (BM_edge_is_wire(bm, e)) {
		return FALSE;
	}
	
	/* Don't start a loop from a boundary edge if it cannot
	 * be extended to cover any faces */
	if (BM_edge_face_count(e) == 1) {
		if (!faceloopWalker_include_face(walker, e->l)) {
			return FALSE;
		}
	}
	
	/* Don't start a face loop from non-manifold edges */
	if (!BM_edge_is_manifold(bm, e)) {
		return FALSE;
	}

	return TRUE;
}

static void faceloopWalker_begin(BMWalker *walker, void *data)
{
	faceloopWalker *lwalk, owalk;
	BMEdge *e = data;
	/* BMesh *bm = walker->bm; */ /* UNUSED */
	/* int fcount = BM_edge_face_count(e); */ /* UNUSED */

	if (!faceloopWalker_edge_begins_loop(walker, e))
		return;

	lwalk = BMW_state_add(walker);
	lwalk->l = e->l;
	lwalk->nocalc = 0;
	BLI_ghash_insert(walker->visithash, lwalk->l->f, NULL);

	/* rewin */
	while (BMW_current_state(walker)) {
		owalk = *((faceloopWalker *)BMW_current_state(walker));
		BMW_walk(walker);
	}

	lwalk = BMW_state_add(walker);
	*lwalk = owalk;
	lwalk->nocalc = 0;

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 3");
	BLI_ghash_insert(walker->visithash, lwalk->l->f, NULL);
}

static void *faceloopWalker_yield(BMWalker *walker)
{
	faceloopWalker *lwalk = BMW_current_state(walker);
	
	if (!lwalk) {
		return NULL;
	}

	return lwalk->l->f;
}

static void *faceloopWalker_step(BMWalker *walker)
{
	faceloopWalker *lwalk = BMW_current_state(walker);
	BMFace *f = lwalk->l->f;
	BMLoop *l = lwalk->l, *origl = lwalk->l;

	BMW_state_remove(walker);

	l = l->radial_next;
	
	if (lwalk->nocalc)
		return f;

	if (!faceloopWalker_include_face(walker, l)) {
		l = lwalk->l;
		l = l->next->next;
		if (BM_edge_face_count(l->e) != 2) {
			l = l->prev->prev;
		}
		l = l->radial_next;
	}

	if (faceloopWalker_include_face(walker, l)) {
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

/*	Edge Ring Walker:
 *
 *	Starts at a tool-flagged edge and walks over the edge ring
 * Conditions for starting and stepping the edge ring have been
 * tuned in an attempt to match the edge rings built by EditMesh
 *
 */

static void edgeringWalker_begin(BMWalker *walker, void *data)
{
	edgeringWalker *lwalk, owalk;
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
		owalk = *((edgeringWalker *)BMW_current_state(walker));
		BMW_walk(walker);
	}

	lwalk = BMW_state_add(walker);
	*lwalk = owalk;

	if (lwalk->l->f->len != 4)
		lwalk->l = lwalk->l->radial_next;

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 4");
	BLI_ghash_insert(walker->visithash, lwalk->l->e, NULL);
}

static void *edgeringWalker_yield(BMWalker *walker)
{
	edgeringWalker *lwalk = BMW_current_state(walker);
	
	if (!lwalk) {
		return NULL;
	}

	if (lwalk->l)
		return lwalk->l->e;
	else
		return lwalk->wireedge;
}

static void *edgeringWalker_step(BMWalker *walker)
{
	edgeringWalker *lwalk = BMW_current_state(walker);
	BMEdge *e;
	BMLoop *l = lwalk->l /* , *origl = lwalk->l */;
	BMesh *bm = walker->bm;

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

	l = l->radial_next;
	l = l->next->next;
	
	if ((l->f->len != 4) || !BM_edge_is_manifold(bm, l->e)) {
		l = lwalk->l->next->next;
	}

	/* only walk to manifold edge */
	if ((l->f->len == 4) && BM_edge_is_manifold(bm, l->e) &&
	    !BLI_ghash_haskey(walker->visithash, l->e)) {
		lwalk = BMW_state_add(walker);
		lwalk->l = l;
		lwalk->wireedge = NULL;

		BLI_ghash_insert(walker->visithash, l->e, NULL);
	}

	return e;
}

static void uvedgeWalker_begin(BMWalker *walker, void *data)
{
	uvedgeWalker *lwalk;
	BMLoop *l = data;

	if (BLI_ghash_haskey(walker->visithash, l))
		return;

	lwalk = BMW_state_add(walker);
	lwalk->l = l;
	BLI_ghash_insert(walker->visithash, l, NULL);
}

static void *uvedgeWalker_yield(BMWalker *walker)
{
	uvedgeWalker *lwalk = BMW_current_state(walker);
	
	if (!lwalk) {
		return NULL;
	}

	return lwalk->l;
}

static void *uvedgeWalker_step(BMWalker *walker)
{
	uvedgeWalker *lwalk = BMW_current_state(walker);
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
	 * mloopuv's coordinates. in addition, push on l->next if necassary */
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

static BMWalker shell_walker_type = {
	shellWalker_begin,
	shellWalker_step,
	shellWalker_yield,
	sizeof(shellWalker),
	BMW_BREADTH_FIRST,
	BM_EDGE, /* valid restrict masks */
};

static BMWalker islandbound_walker_type = {
	islandboundWalker_begin,
	islandboundWalker_step,
	islandboundWalker_yield,
	sizeof(islandboundWalker),
	BMW_DEPTH_FIRST,
	BM_FACE, /* valid restrict masks */
};

static BMWalker island_walker_type = {
	islandWalker_begin,
	islandWalker_step,
	islandWalker_yield,
	sizeof(islandWalker),
	BMW_BREADTH_FIRST,
	BM_EDGE | BM_FACE, /* valid restrict masks */
};

static BMWalker loop_walker_type = {
	loopWalker_begin,
	loopWalker_step,
	loopWalker_yield,
	sizeof(loopWalker),
	BMW_DEPTH_FIRST,
	0, /* valid restrict masks */ /* could add flags here but so far none are used */
};

static BMWalker faceloop_walker_type = {
	faceloopWalker_begin,
	faceloopWalker_step,
	faceloopWalker_yield,
	sizeof(faceloopWalker),
	BMW_DEPTH_FIRST,
	0, /* valid restrict masks */ /* could add flags here but so far none are used */
};

static BMWalker edgering_walker_type = {
	edgeringWalker_begin,
	edgeringWalker_step,
	edgeringWalker_yield,
	sizeof(edgeringWalker),
	BMW_DEPTH_FIRST,
	0, /* valid restrict masks */ /* could add flags here but so far none are used */
};

static BMWalker loopdata_region_walker_type = {
	uvedgeWalker_begin,
	uvedgeWalker_step,
	uvedgeWalker_yield,
	sizeof(uvedgeWalker),
	BMW_DEPTH_FIRST,
	BM_EDGE, /* valid restrict masks */
};

static BMWalker connected_vertex_walker_type = {
	connectedVertexWalker_begin,
	connectedVertexWalker_step,
	connectedVertexWalker_yield,
	sizeof(connectedVertexWalker),
	BMW_BREADTH_FIRST,
	BM_VERT, /* valid restrict masks */
};

BMWalker *bm_walker_types[] = {
	&shell_walker_type,              /* BMW_SHELL */
	&loop_walker_type,               /* BMW_LOOP */
	&faceloop_walker_type,           /* BMW_FACELOOP */
	&edgering_walker_type,           /* BMW_EDGERING */
	&loopdata_region_walker_type,    /* BMW_LOOPDATA_ISLAND */
	&islandbound_walker_type,        /* BMW_ISLANDBOUND */
	&island_walker_type,             /* BMW_ISLAND */
	&connected_vertex_walker_type,   /* BMW_CONNECTED_VERTEX */
};

int bm_totwalkers = sizeof(bm_walker_types) / sizeof(*bm_walker_types);
