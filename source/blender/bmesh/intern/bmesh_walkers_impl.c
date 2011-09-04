/**
 *  bmesh_walkers_impl.c    april 2011
 *
 *	BMesh Walker Code.
 *
 * $Id: $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Levi Schooley.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <string.h>

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BLI_utildefines.h"
#include "BLI_mempool.h"
#include "BLI_array.h"

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

static void shellWalker_begin(BMWalker *walker, void *data){
	BMIter eiter;
	BMEdge *e;
	BMVert *v = data;
	shellWalker *shellWalk = NULL;

	if (!v->e)
		return;

	if (walker->restrictflag) {
		BM_ITER(e, &eiter, walker->bm, BM_EDGES_OF_VERT, v) {
			if (BMO_TestFlag(walker->bm, e, walker->restrictflag))
				break;
		}
	} else {
		e = v->e;
	}

	if (!e) 
		return;

	if (BLI_ghash_haskey(walker->visithash, e))
		return;

	BMW_pushstate(walker);

	shellWalk = walker->currentstate;
	shellWalk->base = v;
	shellWalk->curedge = e;
	BLI_ghash_insert(walker->visithash, e, NULL);
}

static void *shellWalker_yield(BMWalker *walker)
{
	shellWalker *shellWalk = walker->currentstate;
	return shellWalk->curedge;
}

static void *shellWalker_step(BMWalker *walker)
{
	shellWalker *swalk = walker->currentstate;
	BMEdge *e, *e2;
	BMVert *v;
	BMIter iter;
	int i;

	BMW_popstate(walker);

	e = swalk->curedge;
	for (i=0; i<2; i++) {
		v = i ? e->v2 : e->v1;
		BM_ITER(e2, &iter, walker->bm, BM_EDGES_OF_VERT, v) {
			if (walker->restrictflag && !BMO_TestFlag(walker->bm, e2, walker->restrictflag))
				continue;
			if (BLI_ghash_haskey(walker->visithash, e2))
				continue;
			
			BMW_pushstate(walker);
			BLI_ghash_insert(walker->visithash, e2, NULL);

			swalk = walker->currentstate;
			swalk->curedge = e2;
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
	shellWalker shellWalk = *((shellWalker*)walker->currentstate);
	
	if (!BLI_ghash_haskey(walker->visithash, shellWalk.base))
		BLI_ghash_insert(walker->visithash, shellWalk.base, NULL);

	BMW_popstate(walker);


	/*find the next edge whose other vertex has not been visited*/
	curedge = shellWalk.curedge;
	do{
		if (!BLI_ghash_haskey(walker->visithash, curedge)) { 
			if(!walker->restrictflag || (walker->restrictflag &&
			   BMO_TestFlag(walker->bm, curedge, walker->restrictflag)))
			{
				ov = BM_OtherEdgeVert(curedge, shellWalk.base);
				
				/*push a new state onto the stack*/
				BMW_pushstate(walker);
				BLI_ghash_insert(walker->visithash, curedge, NULL);
				
				/*populate the new state*/

				((shellWalker*)walker->currentstate)->base = ov;
				((shellWalker*)walker->currentstate)->curedge = curedge;
			}
		}
		curedge = bmesh_disk_nextedge(curedge, shellWalk.base);
	}while(curedge != shellWalk.curedge);
	
	return shellWalk.curedge;
}
#endif

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

static void islandboundWalker_begin(BMWalker *walker, void *data){
	BMLoop *l = data;
	islandboundWalker *iwalk = NULL;

	BMW_pushstate(walker);

	iwalk = walker->currentstate;

	iwalk->base = iwalk->curloop = l;
	iwalk->lastv = l->v;

	BLI_ghash_insert(walker->visithash, data, NULL);

}

static void *islandboundWalker_yield(BMWalker *walker)
{
	islandboundWalker *iwalk = walker->currentstate;

	return iwalk->curloop;
}

static void *islandboundWalker_step(BMWalker *walker)
{
	islandboundWalker *iwalk = walker->currentstate, owalk;
	BMVert *v;
	BMEdge *e = iwalk->curloop->e;
	BMFace *f;
	BMLoop *l = iwalk->curloop;
	/* int found=0; */

	owalk = *iwalk;

	if (iwalk->lastv == e->v1) v = e->v2;
	else v = e->v1;

	if (BM_Nonmanifold_Vert(walker->bm, v)) {
		BMW_reset(walker);
		BMO_RaiseError(walker->bm, NULL,BMERR_WALKER_FAILED,
			"Non-manifold vert"
			" while searching region boundary");
		return NULL;
	}
	
	/*pop off current state*/
	BMW_popstate(walker);
	
	f = l->f;
	
	while (1) {
		l = BM_OtherFaceLoop(e, f, v);
		if (bmesh_radial_nextloop(l) != l) {
			l = bmesh_radial_nextloop(l);
			f = l->f;
			e = l->e;
			if(walker->restrictflag && !BMO_TestFlag(walker->bm, f, walker->restrictflag)){
				l = l->radial_next;
				break;
			}
		} else {
			f = l->f;
			e = l->e;
			break;
		}
	}
	
	if (l == owalk.curloop) return NULL;
	if (BLI_ghash_haskey(walker->visithash, l)) return owalk.curloop;

	BLI_ghash_insert(walker->visithash, l, NULL);
	BMW_pushstate(walker);
	iwalk = walker->currentstate;
	iwalk->base = owalk.base;

	//if (!BMO_TestFlag(walker->bm, l->f, walker->restrictflag))
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

static void islandWalker_begin(BMWalker *walker, void *data){
	islandWalker *iwalk = NULL;

	BMW_pushstate(walker);

	iwalk = walker->currentstate;
	BLI_ghash_insert(walker->visithash, data, NULL);

	iwalk->cur = data;
}

static void *islandWalker_yield(BMWalker *walker)
{
	islandWalker *iwalk = walker->currentstate;

	return iwalk->cur;
}

static void *islandWalker_step(BMWalker *walker)
{
	islandWalker *iwalk = walker->currentstate, *owalk;
	BMIter iter, liter;
	BMFace *f, *curf = iwalk->cur;
	BMLoop *l;
	owalk = iwalk;
	
	BMW_popstate(walker);

	l = BMIter_New(&liter, walker->bm, BM_LOOPS_OF_FACE, iwalk->cur);
	for (; l; l=BMIter_Step(&liter)) {
		f = BMIter_New(&iter, walker->bm, BM_FACES_OF_EDGE, l->e);
		for (; f; f=BMIter_Step(&iter)) {
			if (walker->restrictflag && !BMO_TestFlag(walker->bm, f, walker->restrictflag))
				continue;
			if (BLI_ghash_haskey(walker->visithash, f)) continue;
			
			BMW_pushstate(walker);
			iwalk = walker->currentstate;
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

static void loopWalker_begin(BMWalker *walker, void *data){
	loopWalker *lwalk = NULL, owalk;
	BMEdge *e = data;
	BMVert *v;
	int /*  found=1, */ val;

	v = e->v1;

	val = BM_Vert_EdgeCount(v);

	BMW_pushstate(walker);
	
	lwalk = walker->currentstate;
	BLI_ghash_insert(walker->visithash, e, NULL);
	
	lwalk->cur = lwalk->start = e;
	lwalk->lastv = lwalk->startv = v;
	lwalk->stage2 = 0;
	lwalk->startrad = BM_Edge_FaceCount(e);

	/*rewind*/
	while (walker->currentstate) {
		owalk = *((loopWalker*)walker->currentstate);
		BMW_walk(walker);
	}

	BMW_pushstate(walker);
	lwalk = walker->currentstate;
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
	loopWalker *lwalk = walker->currentstate;

	return lwalk->cur;
}

static void *loopWalker_step(BMWalker *walker)
{
	loopWalker *lwalk = walker->currentstate, owalk;
	BMEdge *e = lwalk->cur /* , *nexte = NULL */;
	BMLoop *l, *l2;
	BMVert *v;
	int val, rlen /* , found=0 */, i=0, stopi;

	owalk = *lwalk;
	
	if (e->v1 == lwalk->lastv) v = e->v2;
	else v = e->v1;

	val = BM_Vert_EdgeCount(v);
	
	BMW_popstate(walker);
	
	rlen = owalk.startrad;
	l = e->l;
	
	/*handle wire edge case*/
	if (!l && val == 2) {
		e = bmesh_disk_nextedge(e, v);
		
		if (!BLI_ghash_haskey(walker->visithash, e)) {
			BMW_pushstate(walker);
			lwalk = walker->currentstate;
			*lwalk = owalk;
			lwalk->cur = e;
			lwalk->lastv = v;
			
			BLI_ghash_insert(walker->visithash, e, NULL);			
		}
		
		return owalk.cur;
	}
	
	if (val == 4 || val == 2 || rlen == 1) {		
		i = 0;
		stopi = val / 2;
		while (1) {
			if (rlen != 1 && i == stopi) break;

			l = BM_OtherFaceLoop(l->e, l->f, v);

			if (!l)
				break;

			l2 = bmesh_radial_nextloop(l);
			
			if (l2 == l) {
				break;
			}

			l = l2;
			i += 1;
		}
	}
	
	if (!l)
		return owalk.cur;

	if (l != e->l && !BLI_ghash_haskey(walker->visithash, l->e)) {
		if (!(rlen != 1 && i != stopi)) {
			BMW_pushstate(walker);
			lwalk = walker->currentstate;
			*lwalk = owalk;
			lwalk->cur = l->e;
			lwalk->lastv = v;
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

/*Check whether the face loop should includes the face specified
  by the given BMLoop*/
static int faceloopWalker_include_face(BMWalker *walker, BMLoop *l)
{
	/*face must have degree 4*/
	if (l->f->len != 4)
		return 0;

	/*the face must not have been already visited*/
	if (BLI_ghash_haskey(walker->visithash, l->f))
		return 0;

	return 1;
}

/*Check whether the face loop can start from the given edge*/
static int faceloopWalker_edge_begins_loop(BMWalker *walker, BMEdge *e)
{
	BMesh *bm = walker->bm;

	/*There is no face loop starting from a wire edges*/
	if (BM_Wire_Edge(bm, e)) {
		return 0;
	}
	
	/*Don't start a loop from a boundary edge if it cannot
	  be extended to cover any faces*/
	if (BM_Edge_FaceCount(e) == 1) {
		if (!faceloopWalker_include_face(walker, e->l))
			return 0;
	}
	
	/*Don't start a face loop from non-manifold edges*/
	if (BM_Nonmanifold_Edge(bm, e)) {
		return 0;
	}

	return 1;
}

static void faceloopWalker_begin(BMWalker *walker, void *data)
{
	faceloopWalker *lwalk, owalk;
	BMEdge *e = data;
	BMesh *bm = walker->bm;
	int fcount = BM_Edge_FaceCount(e);

	if (!faceloopWalker_edge_begins_loop(walker, e))
		return;

	BMW_pushstate(walker);

	lwalk = walker->currentstate;
	lwalk->l = e->l;
	lwalk->nocalc = 0;
	BLI_ghash_insert(walker->visithash, lwalk->l->f, NULL);

	/*rewind*/
	while (walker->currentstate) {
		owalk = *((faceloopWalker*)walker->currentstate);
		BMW_walk(walker);
	}

	BMW_pushstate(walker);
	lwalk = walker->currentstate;
	*lwalk = owalk;
	lwalk->nocalc = 0;

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 3");
	BLI_ghash_insert(walker->visithash, lwalk->l->f, NULL);
}

static void *faceloopWalker_yield(BMWalker *walker)
{
	faceloopWalker *lwalk = walker->currentstate;
	
	if (!lwalk) return NULL;

	return lwalk->l->f;
}

static void *faceloopWalker_step(BMWalker *walker)
{
	faceloopWalker *lwalk = walker->currentstate;
	BMFace *f = lwalk->l->f;
	BMLoop *l = lwalk->l, *origl = lwalk->l;

	BMW_popstate(walker);

	l = l->radial_next;
	
	if (lwalk->nocalc)
		return f;

	if (!faceloopWalker_include_face(walker, l)) {
		l = lwalk->l;
		l = l->next->next;
		if (BM_Edge_FaceCount(l->e) != 2) {
			l = l->prev->prev;
		}
		l = l->radial_next;
	}

	if (faceloopWalker_include_face(walker, l)) {
		BMW_pushstate(walker);
		lwalk = walker->currentstate;
		lwalk->l = l;

		if (l->f->len != 4) {
			lwalk->nocalc = 1;
			lwalk->l = origl;
		} else
			lwalk->nocalc = 0;

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

	BMW_pushstate(walker);

	lwalk = walker->currentstate;
	lwalk->l = e->l;

	if (!lwalk->l) {
		lwalk->wireedge = e;
		return;
	} else {
		lwalk->wireedge = NULL;
	}

	BLI_ghash_insert(walker->visithash, lwalk->l->e, NULL);

	/*rewind*/
	while (walker->currentstate) {
		owalk = *((edgeringWalker*)walker->currentstate);
		BMW_walk(walker);
	}

	BMW_pushstate(walker);
	lwalk = walker->currentstate;
	*lwalk = owalk;

	if (lwalk->l->f->len != 4)
		lwalk->l = lwalk->l->radial_next;

	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 4");
	BLI_ghash_insert(walker->visithash, lwalk->l->e, NULL);
}

static void *edgeringWalker_yield(BMWalker *walker)
{
	edgeringWalker *lwalk = walker->currentstate;
	
	if (!lwalk) return NULL;

	if (lwalk->l)
		return lwalk->l->e;
	else
		return lwalk->wireedge;
}

static void *edgeringWalker_step(BMWalker *walker)
{
	edgeringWalker *lwalk = walker->currentstate;
	BMEdge *e;
	BMLoop *l = lwalk->l /* , *origl = lwalk->l */;
	BMesh *bm = walker->bm;

	BMW_popstate(walker);

	if (!l)
		return lwalk->wireedge;

	e = l->e;
	if (BM_Nonmanifold_Edge(bm, e)) {
		/*walker won't traverse to a non-manifold edge, but may
		  be started on one, and should not traverse *away* from
		  a non-manfold edge (non-manifold edges are never in an
		  edge ring with manifold edges*/
		return e;
	}

	l = l->radial_next;
	l = l->next->next;
	
	if ((l->f->len != 4) || BM_Nonmanifold_Edge(bm, l->e)) {
		l = lwalk->l->next->next;
	}

	/*only walk to manifold edges*/
	if ((l->f->len == 4) && !BM_Nonmanifold_Edge(bm, l->e) &&
		 !BLI_ghash_haskey(walker->visithash, l->e)) {
		BMW_pushstate(walker);
		lwalk = walker->currentstate;
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

	BMW_pushstate(walker);
	lwalk = walker->currentstate;
	lwalk->l = l;
	BLI_ghash_insert(walker->visithash, l, NULL);
}

static void *uvedgeWalker_yield(BMWalker *walker)
{
	uvedgeWalker *lwalk = walker->currentstate;
	
	if (!lwalk) return NULL;
	
	return lwalk->l;
}

static void *uvedgeWalker_step(BMWalker *walker)
{
	uvedgeWalker *lwalk = walker->currentstate;
	BMLoop *l, *l2, *l3, *nl, *cl;
	BMIter liter;
	void *d1, *d2;
	int i, j, rlen, type;

	l = lwalk->l;
	nl = l->next;
	type = walker->bm->ldata.layers[walker->flag].type;

	BMW_popstate(walker);
	
	if (walker->restrictflag && !BMO_TestFlag(walker->bm, l->e, walker->restrictflag))
		return l;

	/*go over loops around l->v and nl->v and see which ones share l and nl's 
	  mloopuv's coordinates. in addition, push on l->next if necassary.*/
	for (i=0; i<2; i++) {
		cl = i ? nl : l;
		BM_ITER(l2, &liter, walker->bm, BM_LOOPS_OF_VERT, cl->v) {
			d1 = CustomData_bmesh_get_layer_n(&walker->bm->ldata, 
			             cl->head.data, walker->flag);
			
			rlen = BM_Edge_FaceCount(l2->e);
			for (j=0; j<rlen; j++) {
				if (BLI_ghash_haskey(walker->visithash, l2))
					continue;
				if (walker->restrictflag && !(BMO_TestFlag(walker->bm, l2->e, walker->restrictflag)))
				{
					if (l2->v != cl->v)
						continue;
				}
				
				l3 = l2->v != cl->v ? (BMLoop*)l2->next : l2;
				d2 = CustomData_bmesh_get_layer_n(&walker->bm->ldata, 
					     l3->head.data, walker->flag);

				if (!CustomData_data_equals(type, d1, d2))
					continue;
				
				BMW_pushstate(walker);
				BLI_ghash_insert(walker->visithash, l2, NULL);
				lwalk = walker->currentstate;

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
};

static BMWalker islandbound_walker_type = {
	islandboundWalker_begin,
	islandboundWalker_step,
	islandboundWalker_yield,
	sizeof(islandboundWalker),
};


static BMWalker island_walker_type = {
	islandWalker_begin,
	islandWalker_step,
	islandWalker_yield,
	sizeof(islandWalker),
};

static BMWalker loop_walker_type = {
	loopWalker_begin,
	loopWalker_step,
	loopWalker_yield,
	sizeof(loopWalker),
};


static BMWalker faceloop_walker_type = {
	faceloopWalker_begin,
	faceloopWalker_step,
	faceloopWalker_yield,
	sizeof(faceloopWalker),
};

static BMWalker edgering_walker_type = {
	edgeringWalker_begin,
	edgeringWalker_step,
	edgeringWalker_yield,
	sizeof(edgeringWalker),
};

static BMWalker loopdata_region_walker_type = {
	uvedgeWalker_begin,
	uvedgeWalker_step,
	uvedgeWalker_yield,
	sizeof(uvedgeWalker),
};

BMWalker *bm_walker_types[] = {
	&shell_walker_type,
	&loop_walker_type,
	&faceloop_walker_type,
	&edgering_walker_type,
	&loopdata_region_walker_type,
	&islandbound_walker_type,
	&island_walker_type,
};

int bm_totwalkers = sizeof(bm_walker_types) / sizeof(*bm_walker_types);


