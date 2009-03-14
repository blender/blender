#include <limits.h>
#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "BKE_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_ghash.h"
#include "BLI_arithb.h"

#include "bmesh.h"
#include "bmesh_private.h"

#include <stdlib.h>
#include <string.h>

/*
 * BME_MODS.C
 *
 * This file contains functions for locally modifying
 * the topology of existing mesh data. (split, join, flip ect).
 *
*/

/**
 *			bmesh_dissolve_disk
 *
 *  Turns the face region surrounding a manifold vertex into 
 *  A single polygon.
 *
 * 
 * Example:
 * 
 *          |=========|             |=========|
 *          |  \   /  |             |         |
 * Before:  |    V    |      After: |         |
 *          |  /   \  |             |         |
 *          |=========|             |=========|
 * 
 * 
 */
#if 1
int BM_Dissolve_Vert(BMesh *bm, BMVert *v) {
	BMIter iter;
	BMEdge *e;
	int len=0;

	if (!v) return 0;
	
	e = BMIter_New(&iter, bm, BM_EDGES_OF_VERT, v);
	for (; e; e=BMIter_Step(&iter)) {
		len++;
	}
	
	if (len == 1) {
		bmesh_ke(bm, v->edge);
		bmesh_kv(bm, v);
		return 1;
	}

	if(BM_Nonmanifold_Vert(bm, v)) {
		if (!v->edge) bmesh_kv(bm, v);
		else if (!v->edge->loop) {
			bmesh_ke(bm, v->edge);
			bmesh_kv(bm, v);
		} else return 0;

		return 1;
	}

	return BM_Dissolve_Disk(bm, v);
}

int BM_Dissolve_Disk(BMesh *bm, BMVert *v) {
	BMFace *f, *f2;
	BMEdge *e, *keepedge=NULL, *baseedge=NULL;
	BMLoop *loop;
	int done, len;

	if(BM_Nonmanifold_Vert(bm, v)) {
		return 0;
	}
	
	if(v->edge){
		/*v->edge we keep, what else?*/
		e = v->edge;
		len = 0;
		do{
			e = bmesh_disk_nextedge(e,v);
			if(!(BM_Edge_Share_Faces(e, v->edge))){
				keepedge = e;
				baseedge = v->edge;
				break;
			}
			len++;
		}while(e != v->edge);
	}
	
	/*this code for handling 2 and 3-valence verts
	  may be totally bad.*/
	if (keepedge == NULL && len == 3) {
		/*handle specific case for three-valence.  solve it by
		  increasing valence to four.  this may be hackish. . .*/
		loop = e->loop;
		if (loop->v == v) loop = (BMLoop*) loop->head.next;
		if (!BM_Split_Face(bm, loop->f, v, loop->v, NULL, NULL))
			return 0;

		BM_Dissolve_Disk(bm, v);
		return 1;
	} else if (keepedge == NULL && len == 2) {
		/*handle two-valence*/
		f = v->edge->loop->f;
		f2 = ((BMLoop*)v->edge->loop->radial.next->data)->f;
		/*collapse the vertex*/
		BM_Collapse_Vert(bm, v->edge, v, 1.0);
		BM_Join_Faces(bm, f, f2, NULL);

		return 1;
	}

	if(keepedge){
		done = 0;
		while(!done){
			done = 1;
			e = v->edge;
			do{
				f = NULL;
				len = bmesh_cycle_length(&(e->loop->radial));
				if(len == 2 && (e!=baseedge) && (e!=keepedge)) {
					f = BM_Join_Faces(bm, e->loop->f, ((BMLoop*)(e->loop->radial.next->data))->f, e); 
					/*return if couldn't join faces in manifold
					  conditions.*/
					//!disabled for testing why bad things happen
					if (!f) return 0;
				}

				if(f){
					done = 0;
					break;
				}
				e = bmesh_disk_nextedge(e, v);
			}while(e != v->edge);
		}

		/*get remaining two faces*/
		f = v->edge->loop->f;
		f2 = ((BMLoop*)v->edge->loop->radial.next->data)->f;

		/*collapse the vertex*/
		BM_Collapse_Vert(bm, baseedge, v, 1.0);
		
		if (f != f2) {
			/*join two remaining faces*/
			if (!BM_Join_Faces(bm, f, f2, NULL)) return 0;
		}
	}

	return 1;
}
#else
void BM_Dissolve_Disk(BMesh *bm, BMVert *v){
	BMFace *f;
	BMEdge *e;
	BMIter iter;
	int done, len;
	
	if(v->edge){
		done = 0;
		while(!done){
			done = 1;
			
			/*loop the edges looking for an edge to dissolve*/
			for (e=BMIter_New(&iter, bm, BM_EDGES_OF_VERT, v); e;
			     e = BMIter_Step(&iter)) {
				f = NULL;
				len = bmesh_cycle_length(&(e->loop->radial));
				if(len == 2){
					f = BM_Join_Faces(bm,e->loop->f,((BMLoop*)
					      (e->loop->radial.next->data))->f, 
					       e);
				}
				if(f){ 
					done = 0;
					break;
				}
			};
		}
		BM_Collapse_Vert(bm, v->edge, v, 1.0);
	}
}
#endif

/**
 *			bmesh_join_faces
 *
 *  joins two adjacenct faces togather.
 * 
 *  Returns -
 *	BMFace pointer
 */
 
BMFace *BM_Join_Faces(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e) {

	BMLoop *l1, *l2;
	BMEdge *jed=NULL;
	
	jed = e;
	if(!jed){
		/*search for an edge that has both these faces in its radial cycle*/
		l1 = f1->loopbase;
		do{
			if( ((BMLoop*)l1->radial.next->data)->f == f2 ){
				jed = l1->e;
				break;
			}
			l1 = ((BMLoop*)(l1->head.next));
		}while(l1!=f1->loopbase);
	}

	l1 = jed->loop;
	l2 = l1->radial.next->data;
	if (l1->v == l2->v) {
		bmesh_loop_reverse(bm, f2);
	}

	f1 = bmesh_jfke(bm, f1, f2, jed);
	
	return f1;
}

/*connects two verts together, automatically (if very naively) finding the
  face they both share (if there is one) and splittling it.  use this at your 
  own risk, as it doesn't handle the many complex cases it should (like zero-area faces,
  multiple faces, etc).

  this is really only meant for cases where you don't know before hand the face
  the two verts belong to for splitting (e.g. the subdivision operator).
*/

BMEdge *BM_Connect_Verts(BMesh *bm, BMVert *v1, BMVert *v2, BMFace **nf) {
	BMIter iter, iter2;
	BMVert *v;
	BMLoop *nl;
	BMFace *face;

	/*this isn't the best thing in the world.  it doesn't handle cases where there's
	  multiple faces yet.  that might require a convexity test to figure out which
	  face is "best," and who knows what for non-manifold conditions.*/
	for (face = BMIter_New(&iter, bm, BM_FACES_OF_VERT, v1); face; face=BMIter_Step(&iter)) {
		for (v=BMIter_New(&iter2, bm, BM_VERTS_OF_FACE, face); v; v=BMIter_Step(&iter2)) {
			if (v == v2) {
				face = BM_Split_Face(bm, face, v1, v2, &nl, NULL);

				if (nf) *nf = face;
				return nl->e;
			}
		}
	}

	return NULL;
}

/**
 *			BM_split_face
 *
 *  Splits a single face into two.
 * 
 *  Returns -
 *	BMFace pointer
 */
 
BMFace *BM_Split_Face(BMesh *bm, BMFace *f, BMVert *v1, BMVert *v2, BMLoop **nl, BMEdge *example)
{
	BMFace *nf;
	nf = bmesh_sfme(bm,f,v1,v2,nl);
	
	if (nf) BM_Copy_Attributes(bm, bm, f, nf);

	return nf;
}

/**
 *			bmesh_collapse_vert
 *
 *  Collapses a vertex that has only two manifold edges
 *  onto a vertex it shares an edge with. Fac defines
 *  the amount of interpolation for Custom Data.
 *
 *  Note that this is not a general edge collapse function. For
 *  that see BM_manifold_edge_collapse 
 *
 *  TODO:
 *    Insert error checking for KV valance.
 *
 *  Returns -
 *	Nothing
 */
 
void BM_Collapse_Vert(BMesh *bm, BMEdge *ke, BMVert *kv, float fac){
	void *src[2];
	float w[2];
	BMLoop *l=NULL, *kvloop=NULL, *tvloop=NULL;
	BMVert *tv = bmesh_edge_getothervert(ke,kv);

	w[0] = 1.0f - fac;
	w[1] = fac;

	if(ke->loop){
		l = ke->loop;
		do{
			if(l->v == tv && ((BMLoop*)(l->head.next))->v == kv){
				tvloop = l;
				kvloop = ((BMLoop*)(l->head.next));

				src[0] = kvloop->data;
				src[1] = tvloop->data;
				CustomData_bmesh_interp(&bm->ldata, src,w, NULL, 2, kvloop->data); 								
			}
			l=l->radial.next->data;
		}while(l!=ke->loop);
	}
	BM_Data_Interp_From_Verts(bm, kv, tv, kv, fac);   
	bmesh_jekv(bm,ke,kv);
}

/**
 *			BM_split_edge
 *	
 *	Splits an edge. v should be one of the vertices in e and
 *  defines the direction of the splitting operation for interpolation
 *  purposes.
 *
 *  Returns -
 *	the new vert
 */

BMVert *BM_Split_Edge(BMesh *bm, BMVert *v, BMEdge *e, BMEdge **ne, float percent) {
	BMVert *nv, *v2;

	v2 = bmesh_edge_getothervert(e,v);
	nv = bmesh_semv(bm,v,e,ne);
	if (nv == NULL) return NULL;
	VECSUB(nv->co,v2->co,v->co);
	VECADDFAC(nv->co,v->co,nv->co,percent);
	if (ne) {
		if(bmesh_test_sysflag(&(e->head), BM_SELECT)) bmesh_set_sysflag(&((*ne)->head), BM_SELECT);
		if(bmesh_test_sysflag(&(e->head), BM_HIDDEN)) bmesh_set_sysflag(&((*ne)->head), BM_HIDDEN);
	}
	/*v->nv->v2*/
	BM_Data_Facevert_Edgeinterp(bm,v2, v, nv, e, percent);	
	BM_Data_Interp_From_Verts(bm, v2, v, nv, percent);
	return nv;
}

BMVert  *BM_Split_Edge_Multi(BMesh *bm, BMEdge *e, int numcuts)
{
	int i;
	float percent;
	BMVert *nv = NULL;
	
	for(i=0; i < numcuts; i++){
		percent = 1.0f / (float)(numcuts + 1 - i);
		nv = BM_Split_Edge(bm, e->v2, e, NULL, percent);
	}
	return nv;
}

int BM_Validate_Face(BMesh *bm, BMFace *face, FILE *err) 
{
	BMIter iter;
	V_DECLARE(verts);
	BMVert **verts = NULL;
	BMLoop *l;
	int ret = 1, i, j;
	
	if (face->len == 2) {
		fprintf(err, "warning: found two-edged face. face ptr: %p\n", face);
		fflush(err);
	}

	for (l=BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, face);l;l=BMIter_Step(&iter)) {
		V_GROW(verts);
		verts[V_COUNT(verts)-1] = l->v;
		
		if (l->e->v1 == l->e->v2) {
			fprintf(err, "Found bmesh edge with identical verts!\n");
			fprintf(err, "  edge ptr: %p, vert: %p\n",  l->e, l->e->v1);
			fflush(err);
			ret = 0;
		}
	}

	for (i=0; i<V_COUNT(verts); i++) {
		for (j=0; j<V_COUNT(verts); j++) {
			if (j == i) continue;
			if (verts[i] == verts[j]) {
				fprintf(err, "Found duplicate verts in bmesh face!\n");
				fprintf(err, "  face ptr: %p, vert: %p\n", face, verts[i]);
				fflush(err);
				ret = 0;
			}
		}
	}
	
	V_FREE(verts);
	return ret;
}
