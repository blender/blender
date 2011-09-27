#include <limits.h>
#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_utildefines.h"
#include "BLI_smallhash.h"

#include "bmesh.h"
#include "bmesh_private.h"

#include <stdlib.h>
#include <string.h>

/*
 * BME_MODS.C
 *
 * This file contains functions for locally modifying
 * the topology of existing mesh data. (split, join, flip etc).
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
		if (v->e) 
			BM_Kill_Edge(bm, v->e);
		BM_Kill_Vert(bm, v);
		return 1;
	}

	if(BM_Nonmanifold_Vert(bm, v)) {
		if (!v->e) BM_Kill_Vert(bm, v);
		else if (!v->e->l) {
			BM_Kill_Edge(bm, v->e);
			BM_Kill_Vert(bm, v);
		} else return 0;

		return 1;
	}

	return BM_Dissolve_Disk(bm, v);
}

int BM_Dissolve_Disk(BMesh *bm, BMVert *v) {
	BMFace *f, *f2;
	BMEdge *e, *keepedge=NULL, *baseedge=NULL;
	BMLoop *loop;
	int done, len= 0;

	if(BM_Nonmanifold_Vert(bm, v)) {
		return 0;
	}
	
	if(v->e){
		/*v->e we keep, what else?*/
		e = v->e;
		do{
			e = bmesh_disk_nextedge(e,v);
			if(!(BM_Edge_Share_Faces(e, v->e))){
				keepedge = e;
				baseedge = v->e;
				break;
			}
			len++;
		}while(e != v->e);
	}
	
	/*this code for handling 2 and 3-valence verts
	  may be totally bad.*/
	if (keepedge == NULL && len == 3) {
		/*handle specific case for three-valence.  solve it by
		  increasing valence to four.  this may be hackish. . .*/
		loop = e->l;
		if (loop->v == v) loop = loop->next;
		if (!BM_Split_Face(bm, loop->f, v, loop->v, NULL, NULL))
			return 0;

		if (!BM_Dissolve_Disk(bm, v)) {
			return 0;
		}
		return 1;
	} else if (keepedge == NULL && len == 2) {
		/*collapse the vertex*/
		e = BM_Collapse_Vert(bm, v->e, v, 1.0);

		if (!e) {
			return 0;
		}

		/*handle two-valence*/
		f = e->l->f;
		f2 = e->l->radial_next->f;

		if (f != f2 && !BM_Join_TwoFaces(bm, f, f2, NULL))
			return 0;

		return 1;
	}

	if(keepedge){
		done = 0;
		while(!done){
			done = 1;
			e = v->e;
			do{
				f = NULL;
				len = bmesh_radial_length(e->l);
				if(len == 2 && (e!=baseedge) && (e!=keepedge)) {
					f = BM_Join_TwoFaces(bm, e->l->f, e->l->radial_next->f, e);
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
			}while(e != v->e);
		}

		/*collapse the vertex*/
		e = BM_Collapse_Vert(bm, baseedge, v, 1.0);

		if (!e) {
			return 0;
		}
		
		/*get remaining two faces*/
		f = e->l->f;
		f2 = e->l->radial_next->f;

		if (f != f2) {
			/*join two remaining faces*/
			if (!BM_Join_TwoFaces(bm, f, f2, NULL)) {
				return 0;
			}
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
	
	if(v->e){
		done = 0;
		while(!done){
			done = 1;
			
			/*loop the edges looking for an edge to dissolve*/
			for (e=BMIter_New(&iter, bm, BM_EDGES_OF_VERT, v); e;
			     e = BMIter_Step(&iter)) {
				f = NULL;
				len = bmesh_cycle_length(&(e->l->radial));
				if(len == 2){
					f = BM_Join_TwoFaces(bm,e->l->f,((BMLoop*)
					      (e->l->radial_next))->f, 
					       e);
				}
				if(f){ 
					done = 0;
					break;
				}
			};
		}
		BM_Collapse_Vert(bm, v->e, v, 1.0);
	}
}
#endif

/**
 * BM_Join_TwoFaces
 *
 *  Joins two adjacenct faces togather.
 * 
 *  Because this method calls to BM_Join_Faces to do its work, ff a pair
 *  of faces share multiple edges, the pair of faces will be joined at
 *  every edge (not just edge e). This part of the functionality might need
 *  to be reconsidered.
 *
 *  If the windings do not match the winding of the new face will follow
 *  f1's winding (i.e. f2 will be reversed before the join).
 *
 * Returns:
 *	 pointer to the combined face
 */
 
BMFace *BM_Join_TwoFaces(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e)
{
	BMLoop *l1, *l2;
	BMEdge *jed=NULL;
	BMFace *faces[2] = {f1, f2};
	
	jed = e;
	if(!jed){
		/*search for an edge that has both these faces in its radial cycle*/
		l1 = bm_firstfaceloop(f1);
		do{
			if(l1->radial_next->f == f2 ) {
				jed = l1->e;
				break;
			}
			l1 = l1->next;
		}while(l1!=bm_firstfaceloop(f1));
	}

	if (!jed) {
		bmesh_error();
		return NULL;
	}
	
	l1 = jed->l;
	
	if (!l1) {
		bmesh_error();
		return NULL;
	}
	
	l2 = l1->radial_next;
	if (l1->v == l2->v) {
		bmesh_loop_reverse(bm, f2);
	}

	f1 = BM_Join_Faces(bm, faces, 2);
	
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

	/*be warned: this can do weird things in some ngon situation, see BM_LegalSplits*/
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
 * BM_Split_Face
 *
 *  Splits a single face into two.
 *
 *   f - the original face
 *   v1 & v2 - vertices which define the split edge, must be different
 *   nl - pointer which will receive the BMLoop for the split edge in the new face
 *
 *  Notes: the 

 *  Returns -
 *	  Pointer to the newly created face representing one side of the split
 *   if the split is successful (and the original original face will be the
 *   other side). NULL if the split fails.
 *
 */

BMFace *BM_Split_Face(BMesh *bm, BMFace *f, BMVert *v1, BMVert *v2, BMLoop **nl, BMEdge *UNUSED(example))
{
	BMFace *nf, *of;
	
	/*do we have a multires layer?*/
	if (CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		of = BM_Copy_Face(bm, f, 0, 0);
	}
	
	nf = bmesh_sfme(bm, f, v1, v2, nl, NULL);
	
	if (nf) {
		BM_Copy_Attributes(bm, bm, f, nf);
		copy_v3_v3(nf->no, f->no);
	
		/*handle multires update*/
		if (nf != f && CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
			BMLoop *l;

			l = bm_firstfaceloop(f);
			do {
				BM_loop_interp_from_face(bm, l, of, 0, 1);
				l = l->next;
			} while (l != bm_firstfaceloop(f));

			l = bm_firstfaceloop(nf);
			do {
				BM_loop_interp_from_face(bm, l, of, 0, 1);
				l = l->next;
			} while (l != bm_firstfaceloop(nf));

			BM_Kill_Face(bm, of);

			BM_multires_smooth_bounds(bm, f);
			BM_multires_smooth_bounds(bm, nf);
		}
	}

	return nf;
}

/**
 *			bmesh_collapse_vert
 *
 *  Collapses a vertex that has only two manifold edges
 *  onto a vertex it shares an edge with. Fac defines
 *  the amount of interpolation for Custom Data.
 *
 *  Note that this is not a general edge collapse function.
 *
 *  BMESH_TODO:
 *    Insert error checking for KV valance.
 *
 *  Returns -
 *	Nothing
 */
 
BMEdge* BM_Collapse_Vert(BMesh *bm, BMEdge *ke, BMVert *kv, float fac){
	BMFace **faces = NULL, *f;
	BLI_array_staticdeclare(faces, 8);
	BMIter iter;
	BMLoop *l=NULL, *kvloop=NULL, *tvloop=NULL;
	BMEdge *ne = NULL;
	BMVert *tv = bmesh_edge_getothervert(ke,kv);
	void *src[2];
	float w[2];

	/* Only intended to be called for 2-valence vertices */
	BLI_assert(bmesh_disk_count(kv) <= 2);

	w[0] = 1.0f - fac;
	w[1] = fac;

	if(ke->l){
		l = ke->l;
		do{
			if(l->v == tv && l->next->v == kv) {
				tvloop = l;
				kvloop = l->next;

				src[0] = kvloop->head.data;
				src[1] = tvloop->head.data;
				CustomData_bmesh_interp(&bm->ldata, src,w, NULL, 2, kvloop->head.data);
			}
			l=l->radial_next;
		}while(l!=ke->l);
	}

	BM_ITER(f, &iter, bm, BM_FACES_OF_VERT, kv) {
		BLI_array_append(faces, f);
	}
	
	BM_Data_Interp_From_Verts(bm, kv, tv, kv, fac);

	//bmesh_jekv(bm,ke,kv);
	if (faces && BLI_array_count(faces) > 1) {
		BMFace *f2;
		BMEdge *e2;
		BMVert *tv2;

		e2 = bmesh_disk_nextedge(ke, kv);
		tv2 = BM_OtherEdgeVert(e2, kv);

		f2 = BM_Join_Faces(bm, faces, BLI_array_count(faces));
		if (f2) {
			BMLoop *nl = NULL;
			if (BM_Split_Face(bm, f2, tv, tv2, &nl, NULL)) {
				ne = nl->e;
			}
		}
	} else if (faces && BLI_array_count(faces) == 1) {
		BMLoop **loops = NULL;
		BMVert **verts = NULL;
		BMEdge **edges = NULL;
		BMFace *f2;
		BLI_array_staticdeclare(verts, 64);
		BLI_array_staticdeclare(edges, 64);
		BLI_array_staticdeclare(loops, 64);
		int i;
		
		/*create new face excluding kv*/
		f = *faces;
		l = bm_firstfaceloop(f);
		i = 0;
		do {
			if (l->v != kv) {
				BLI_array_append(verts, l->v);

				if (l->e != ke && !BM_Vert_In_Edge(l->e, kv)) {	
					BLI_array_append(edges, l->e);
				} else {
					BMVert *v2;

					/* Create a single edge to replace the two edges incident on kv */
					
					if (BM_Vert_In_Edge(l->next->e, kv))
						v2 = BM_OtherEdgeVert(l->next->e, kv);
					else
						v2 = BM_OtherEdgeVert(l->prev->e, kv);

					/* Only one new edge should be created */
					BLI_assert(ne == NULL);

					ne = BM_Make_Edge(bm, BM_OtherEdgeVert(l->e, kv), v2, l->e, 1);
					BLI_array_append(edges, ne);
				}

				BLI_array_append(loops, l);
				i++;
			}
			
			l = l->next;
		} while (l != bm_firstfaceloop(f));
		
		f2 = BM_Make_Face(bm, verts, edges, BLI_array_count(verts));
		l = bm_firstfaceloop(f2);
		i = 0;
		do {
			BM_Copy_Attributes(bm, bm, loops[i], l);
			BM_loop_interp_multires(bm, loops[i], l->f);
			i++;
			l = l->next;
		} while (l != bm_firstfaceloop(f2));
		
		BM_Copy_Attributes(bm, bm, f, f2);
		BM_Kill_Face(bm, f);
		BM_Kill_Vert(bm, kv);
	} else {
		BMVert *tv2;
		BMEdge *e2;

		/*ok, no faces, means we have a wire edge*/
		e2 = bmesh_disk_nextedge(ke, kv);
		tv2 = BM_OtherEdgeVert(e2, kv);

		ne = BM_Make_Edge(bm, tv, tv2, ke, 0);

		BM_Kill_Edge(bm, ke);
		BM_Kill_Edge(bm, e2);
		BM_Kill_Vert(bm, kv);
	}

	BLI_array_free(faces);

	return ne;
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
	BMFace **oldfaces = NULL;
	BMEdge *dummy;
	BLI_array_staticdeclare(oldfaces, 32);
	SmallHash hash;

	/*we need this for handling multires*/	
	if (!ne) 
		ne = &dummy;

	/*do we have a multires layer?*/
	if (CustomData_has_layer(&bm->ldata, CD_MDISPS) && e->l) {
		BMLoop *l;
		int i;
		
		l = e->l;
		do {
			BLI_array_append(oldfaces, l->f);
			l = l->radial_next;
		} while (l != e->l);
		
		/*create a hash so we can differentiate oldfaces from new faces*/
		BLI_smallhash_init(&hash);
		
		for (i=0; i<BLI_array_count(oldfaces); i++) {
			oldfaces[i] = BM_Copy_Face(bm, oldfaces[i], 1, 1);
			BLI_smallhash_insert(&hash, (intptr_t)oldfaces[i], NULL);
		}		
	}

	v2 = bmesh_edge_getothervert(e,v);
	nv = bmesh_semv(bm,v,e,ne);
	if (nv == NULL) return NULL;

	sub_v3_v3v3(nv->co,v2->co,v->co);
	VECADDFAC(nv->co,v->co,nv->co,percent);

	if (ne) {
		(*ne)->head.flag = e->head.flag;
		BM_Copy_Attributes(bm, bm, e, *ne);
	}

	/*v->nv->v2*/
	BM_Data_Facevert_Edgeinterp(bm, v2, v, nv, e, percent);	
	BM_Data_Interp_From_Verts(bm, v, v2, nv, percent);

	if (CustomData_has_layer(&bm->ldata, CD_MDISPS) && e->l && nv) {
		int i, j;
		
		/*interpolate new/changed loop data from copied old faces*/
		for (j=0; j<2; j++) {
			for (i=0; i<BLI_array_count(oldfaces); i++) {
				BMEdge *e1 = j ? *ne : e;
				BMLoop *l, *l2;
				
				l = e1->l;
				if (!l) {
					bmesh_error();
					break;
				}
				
				do {
					if (!BLI_smallhash_haskey(&hash, (intptr_t)l->f)) {
						l2 = bm_firstfaceloop(l->f);
						do {
							BM_loop_interp_multires(bm, l2, oldfaces[i]);
							l2 = l2->next;
						} while (l2 != bm_firstfaceloop(l->f));
					}
					l = l->radial_next;
				} while (l != e1->l);
			}
		}
		
		/*destroy the old faces*/
		for (i=0; i<BLI_array_count(oldfaces); i++) {
			BM_Kill_Face_Verts(bm, oldfaces[i]);
		}
		
		/*fix boundaries a bit, doesn't work too well quite yet*/
#if 0
		for (j=0; j<2; j++) {
			BMEdge *e1 = j ? *ne : e;
			BMLoop *l, *l2;
			
			l = e1->l;
			if (!l) {
				bmesh_error();
				break;
			}
			
			do {
				BM_multires_smooth_bounds(bm, l->f);				
				l = l->radial_next;
			} while (l != e1->l);
		}
#endif
		
		BLI_array_free(oldfaces);
		BLI_smallhash_release(&hash);
	}
	
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
	BLI_array_declare(verts);
	BMVert **verts = NULL;
	BMLoop *l;
	int ret = 1, i, j;
	
	if (face->len == 2) {
		fprintf(err, "warning: found two-edged face. face ptr: %p\n", face);
		fflush(err);
	}

	for (l=BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, face);l;l=BMIter_Step(&iter)) {
		BLI_array_growone(verts);
		verts[BLI_array_count(verts)-1] = l->v;
		
		if (l->e->v1 == l->e->v2) {
			fprintf(err, "Found bmesh edge with identical verts!\n");
			fprintf(err, "  edge ptr: %p, vert: %p\n",  l->e, l->e->v1);
			fflush(err);
			ret = 0;
		}
	}

	for (i=0; i<BLI_array_count(verts); i++) {
		for (j=0; j<BLI_array_count(verts); j++) {
			if (j == i) continue;
			if (verts[i] == verts[j]) {
				fprintf(err, "Found duplicate verts in bmesh face!\n");
				fprintf(err, "  face ptr: %p, vert: %p\n", face, verts[i]);
				fflush(err);
				ret = 0;
			}
		}
	}
	
	BLI_array_free(verts);
	return ret;
}

/*
            BM Rotate Edge

    Spins an edge topologically, either counter-clockwise or clockwise.
    If ccw is true, the edge is spun counter-clockwise, otherwise it is
    spun clockwise.
    
    Returns the spun edge.  Note that this works by dissolving the edge
    then re-creating it, so the returned edge won't have the same pointer
    address as the original one.

    Returns NULL on error (e.g., if the edge isn't surrounded by exactly
    two faces).
*/
BMEdge *BM_Rotate_Edge(BMesh *bm, BMEdge *e, int ccw)
{
	BMVert *v1, *v2;
	BMLoop *l, *l1, *l2, *nl;
	BMFace *f;
	BMIter liter;

	v1 = e->v1;
	v2 = e->v2;

	if (BM_Edge_FaceCount(e) != 2)
		return NULL;

	f = BM_Join_TwoFaces(bm, e->l->f, e->l->radial_next->f, e);
	
	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		if (l->v == v1)
			l1 = l;
		else if (l->v == v2)
			l2 = l;
	}
	
	if (ccw) {
		l1 = l1->prev;
		l2 = l2->prev;
	} else {
		l1 = l1->next;
		l2 = l2->next;
	}

	if (!BM_Split_Face(bm, f, l1->v, l2->v, &nl, NULL))
		return NULL;

	return nl->e;
}
