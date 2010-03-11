/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_heap.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_customdata.h"
#include "BKE_context.h"
#include "BKE_tessmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"

#ifndef DISABLE_PYTHON
//#include "BPY_extern.h"
//#include "BPY_menus.h"
#endif

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_object.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

/* own include */
#include "mesh_intern.h"

/* ***************** XXX **************** */
static int pupmenu() {return 0;}
/* ***************** XXX **************** */


/* copy the face flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting faces (while painting) */
void object_facesel_flush_dm(Object *ob)
{
	Mesh *me= get_mesh(ob);
	DerivedMesh *dm= ob->derivedFinal;
	MPoly *faces, *mf, *mf_orig;
	DMFaceIter *fiter;
	int *index = NULL;
	int totface;
	int i;
	
	if(me==NULL || dm==NULL)
		return;

	fiter = dm->newFaceIter(dm);
	totface = dm->getNumFaces(dm);

	for (i=0; !fiter->done; fiter->step(fiter), i++) {
		index = fiter->getCDData(fiter, CD_ORIGINDEX, -1);
		if (!index) {
			fiter->free(fiter);
			return;
		}
		
		mf_orig = me->mpoly + *index;
		fiter->flags = mf_orig->flag; 
	}

	fiter->free(fiter);
}

/* returns 0 if not found, otherwise 1 */
int facesel_face_pick(struct bContext *C, Mesh *me, Object *ob, 
					  short *mval, unsigned int *index, short rect)
{
	Scene *scene = CTX_data_scene(C);
	ViewContext vc;
	view3d_set_viewcontext(C, &vc);

	if (!me || me->totpoly==0)
		return 0;

	/*we can't assume mfaces have a correct origindex layer that indices to mpolys.
	  so instead we have to regenerate the tesselation faces altogether.
	  
	  the final 0, 0 paramters causes it to use the index of each mpoly, instead
	  of reading from the origindex layer.*/
	me->totface = mesh_recalcTesselation(&me->fdata, &me->ldata, &me->pdata, 
		me->mvert, me->totface, me->totloop, me->totpoly, 0, 0);
	mesh_update_customdata_pointers(me);
	makeDerivedMesh(scene, ob, NULL, CD_MASK_BAREMESH);

	// XXX 	if (v3d->flag & V3D_INVALID_BACKBUF) {
// XXX drawview.c!		check_backbuf();
// XXX		persp(PERSP_VIEW);
// XXX 	}

	if (rect) {
		/* sample rect to increase changes of selecting, so that when clicking
		   on an edge in the backbuf, we can still select a face */

		int dist;
		*index = view3d_sample_backbuf_rect(&vc, mval, 3, 1, me->totface+1, &dist,0,NULL, NULL);
	}
	else {
		/* sample only on the exact position */
		*index = view3d_sample_backbuf(&vc, mval[0], mval[1]);
	}

	if ((*index)<=0 || (*index)>(unsigned int)me->totpoly)
		return 0;

	(*index)--;
	
	return 1;
}

/* last_sel, use em->act_face otherwise get the last selected face in the editselections
 * at the moment, last_sel is mainly useful for gaking sure the space image dosnt flicker */
MTexPoly *EDBM_get_active_mtface(BMEditMesh *em, BMFace **act_efa, int sloppy)
{
	BMFace *efa = NULL;
	BMLoop *l;
	BMIter iter, liter;
	
	if(!EDBM_texFaceCheck(em))
		return NULL;
	
	efa = EDBM_get_actFace(em, sloppy);
	
	if (efa) {
		if (act_efa) *act_efa = efa; 
		return CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
	}

	if (act_efa) *act_efa= NULL;
	return NULL;
}

void reveal_tface(Scene *scene)
{
	Mesh *me;
	MPoly *mface;
	int a;
	
	me= get_mesh(OBACT);
	if(me==0 || me->totpoly==0) return;
	
	mface= me->mpoly;
	a= me->totpoly;
	while(a--) {
		if(mface->flag & ME_HIDE) {
			mface->flag |= ME_FACE_SEL;
			mface->flag -= ME_HIDE;
		}
		mface++;
	}

	object_facesel_flush_dm(OBACT);
// XXX notifier!	object_tface_flags_changed(OBACT, 0);
}

void hide_tface(Scene *scene)
{
	Mesh *me;
	MPoly *mface;
	int a;
	int shift=0, alt= 0; // XXX
	
	me= get_mesh(OBACT);
	if(me==0 || me->totpoly==0) return;
	
	if(alt) {
		reveal_tface(scene);
		return;
	}
	
	mface= me->mpoly;
	a= me->totpoly;
	while(a--) {
		if(mface->flag & ME_HIDE);
		else {
			if(shift) {
				if( (mface->flag & ME_FACE_SEL)==0) mface->flag |= ME_HIDE;
			}
			else {
				if( (mface->flag & ME_FACE_SEL)) mface->flag |= ME_HIDE;
			}
		}
		if(mface->flag & ME_HIDE) mface->flag &= ~ME_FACE_SEL;
		
		mface++;
	}
	
	object_facesel_flush_dm(OBACT);
// XXX notifier!		object_tface_flags_changed(OBACT, 0);
}

/* Set tface seams based on edge data, uses hash table to find seam edges. */

static void hash_add_face(EdgeHash *ehash, MPoly *mf, MLoop *mloop)
{
	MLoop *ml, *ml2;
	int i;

	for (i=0, ml=mloop; i<mf->totloop; i++, ml++) {
		ml2 = mloop + (i+1) % mf->totloop;
		BLI_edgehash_insert(ehash, ml->v, ml2->v, NULL);
	}
}


void select_linked_tfaces_with_seams(int mode, Mesh *me, unsigned int index)
{
	EdgeHash *ehash, *seamhash;
	MPoly *mf;
	MLoop *ml;
	MEdge *med;
	char *linkflag;
	int a, b, doit=1, mark=0;

	ehash= BLI_edgehash_new();
	seamhash = BLI_edgehash_new();
	linkflag= MEM_callocN(sizeof(char)*me->totpoly, "linkflaguv");

	for(med=me->medge, a=0; a < me->totedge; a++, med++)
		if(med->flag & ME_SEAM)
			BLI_edgehash_insert(seamhash, med->v1, med->v2, NULL);

	if (mode==0 || mode==1) {
		/* only put face under cursor in array */
		mf= ((MPoly*)me->mpoly) + index;
		hash_add_face(ehash, mf, me->mloop + mf->loopstart);
		linkflag[index]= 1;
	}
	else {
		/* fill array by selection */
		mf= me->mpoly;
		for(a=0; a<me->totpoly; a++, mf++) {
			if(mf->flag & ME_HIDE);
			else if(mf->flag & ME_FACE_SEL) {
				hash_add_face(ehash, mf, me->mloop + mf->loopstart);
				linkflag[a]= 1;
			}
		}
	}

	while(doit) {
		doit= 0;

		/* expand selection */
		mf= me->mpoly;
		for(a=0; a<me->totpoly; a++, mf++) {
			if(mf->flag & ME_HIDE)
				continue;

			if(!linkflag[a]) {
				MLoop *mnextl;
				mark= 0;

				ml = me->mloop + mf->loopstart;
				for (b=0; b<mf->totloop; b++, ml++) {
					mnextl = b < mf->totloop-1 ? ml - 1 : me->mloop + mf->loopstart;
					if (!BLI_edgehash_haskey(seamhash, ml->v, mnextl->v))
						if (!BLI_edgehash_haskey(ehash, ml->v, mnextl->v))
							mark = 1;
				}

				if(mark) {
					linkflag[a]= 1;
					hash_add_face(ehash, mf, me->mloop + mf->loopstart);
					doit= 1;
				}
			}
		}

	}

	BLI_edgehash_free(ehash, NULL);
	BLI_edgehash_free(seamhash, NULL);

	if(mode==0 || mode==2) {
		for(a=0, mf=me->mpoly; a<me->totpoly; a++, mf++)
			if(linkflag[a])
				mf->flag |= ME_FACE_SEL;
			else
				mf->flag &= ~ME_FACE_SEL;
	}
	else if(mode==1) {
		for(a=0, mf=me->mpoly; a<me->totpoly; a++, mf++)
			if(linkflag[a] && (mf->flag & ME_FACE_SEL))
				break;

		if (a<me->totpoly) {
			for(a=0, mf=me->mpoly; a<me->totpoly; a++, mf++)
				if(linkflag[a])
					mf->flag &= ~ME_FACE_SEL;
		}
		else {
			for(a=0, mf=me->mpoly; a<me->totpoly; a++, mf++)
				if(linkflag[a])
					mf->flag |= ME_FACE_SEL;
		}
	}

	MEM_freeN(linkflag);

	// BIF_undo_push("Select linked UV face");
	// object_tface_flags_changed(OBACT, 0);
}

void select_linked_tfaces(bContext *C, Object *ob, short mval[2], int mode)
{
	Mesh *me;
	unsigned int index=0;

	me = get_mesh(ob);
	if(me==0 || me->totpoly==0) return;

	if (mode==0 || mode==1) {
		// XXX - Causes glitches, not sure why
		/*
		if (!facesel_face_pick(C, me, mval, &index, 1))
			return;
		*/
	}

	select_linked_tfaces_with_seams(mode, me, index);

	object_facesel_flush_dm(ob);
}

void selectall_tface(Object *ob, int action)
{
	Mesh *me;
	MPoly *mface;
	int a;

	me= get_mesh(ob);
	if(me==0) return;
	
	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;

		mface= me->mpoly;
		a= me->totpoly;
		while(a--) {
			if((mface->flag & ME_HIDE) == 0 && mface->flag & ME_FACE_SEL) {
				action = SEL_DESELECT;
				break;
			}
			mface++;
		}
	}
	
	mface= me->mpoly;
	a= me->totpoly;
	while(a--) {
		if((mface->flag & ME_HIDE) == 0) {
			switch (action) {
			case SEL_SELECT:
				mface->flag |= ME_FACE_SEL;
				break;
			case SEL_DESELECT:
				mface->flag &= ~ME_FACE_SEL;
				break;
			case SEL_INVERT:
				mface->flag ^= ME_FACE_SEL;
				break;
			}
		}
		mface++;
	}

	object_facesel_flush_dm(ob);
// XXX notifier!		object_tface_flags_changed(OBACT, 0);
}

void selectswap_tface(Scene *scene)
{
	Mesh *me;
	MPoly *mface;
	int a;
		
	me= get_mesh(OBACT);
	if(me==0) return;
	
	mface= me->mpoly;
	a= me->totpoly;
	while(a--) {
		if(mface->flag & ME_HIDE);
		else {
			if(mface->flag & ME_FACE_SEL) mface->flag &= ~ME_FACE_SEL;
			else mface->flag |= ME_FACE_SEL;
		}
		mface++;
	}
	
	object_facesel_flush_dm(OBACT);
// XXX notifier!		object_tface_flags_changed(OBACT, 0);
}

int minmax_tface(Scene *scene, float *min, float *max)
{
	Object *ob;
	Mesh *me;
	MPoly *mf;
	MTexPoly *tf;
	MLoop *ml;
	MVert *mv;
	int a, b, ok=0;
	float vec[3], bmat[3][3];
	
	ob = OBACT;
	if (ob==0) return ok;

	me= get_mesh(ob);
	if(!me || !me->mtpoly) return ok;
	
	copy_m3_m4(bmat, ob->obmat);

	mv= me->mvert;
	mf= me->mpoly;
	tf= me->mtpoly;
	for (a=me->totpoly; a>0; a--, mf++, tf++) {
		if (mf->flag & ME_HIDE || !(mf->flag & ME_FACE_SEL))
			continue;

		ml = me->mloop + mf->totloop;
		for (b=0; b<mf->totloop; b++, ml++) {
			VECCOPY(vec, (mv+ml->v)->co);
			mul_m3_v3(bmat, vec);
			add_v3_v3v3(vec, vec, ob->obmat[3]);
			DO_MINMAX(vec, min, max);		
		}

		ok= 1;
	}

	return ok;
}

/* ******************** edge loop shortest path ********************* */

#define ME_SEAM_DONE 2		/* reuse this flag */

static float edgetag_cut_cost(BMEditMesh *em, int e1, int e2, int vert)
{
	BMVert *v = EDBM_get_vert_for_index(em, vert);
	BMEdge *eed1 = EDBM_get_edge_for_index(em, e1), *eed2 = EDBM_get_edge_for_index(em, e2);
	BMVert *v1 = EDBM_get_vert_for_index(em, (BMINDEX_GET(eed1->v1) == vert)? BMINDEX_GET(eed1->v2): BMINDEX_GET(eed1->v1) );
	BMVert *v2 = EDBM_get_vert_for_index(em, (BMINDEX_GET(eed2->v1) == vert)? BMINDEX_GET(eed2->v2): BMINDEX_GET(eed2->v1) );
	float cost, d1[3], d2[3];

	cost = len_v3v3(v1->co, v->co);
	cost += len_v3v3(v->co, v2->co);

	sub_v3_v3v3(d1, v->co, v1->co);
	sub_v3_v3v3(d2, v2->co, v->co);

	cost = cost + 0.5f*cost*(2.0f - fabs(d1[0]*d2[0] + d1[1]*d2[1] + d1[2]*d2[2]));

	return cost;
}

static void edgetag_add_adjacent(BMEditMesh *em, Heap *heap, int mednum, int vertnum, 
								 int *nedges, int *edges, int *prevedge, float *cost)
{
	int startadj, endadj = nedges[vertnum+1];

	for (startadj = nedges[vertnum]; startadj < endadj; startadj++) {
		int adjnum = edges[startadj];
		BMEdge *eedadj = EDBM_get_edge_for_index(em, adjnum);
		float newcost;

		if (eedadj->head.eflag2 & ME_SEAM_DONE)
			continue;

		newcost = cost[mednum] + edgetag_cut_cost(em, mednum, adjnum, vertnum);

		if (cost[adjnum] > newcost) {
			cost[adjnum] = newcost;
			prevedge[adjnum] = mednum;
			BLI_heap_insert(heap, newcost, SET_INT_IN_POINTER(adjnum));
		}
	}
}

void edgetag_context_set(BMEditMesh *em, Scene *scene, BMEdge *eed, int val)
{
	
	switch (scene->toolsettings->edge_mode) {
	case EDGE_MODE_SELECT:
		BM_Select(em->bm, eed, val);
		break;
	case EDGE_MODE_TAG_SEAM:
		if (val)		{BM_SetHFlag(eed, BM_SEAM);}
		else			{BM_ClearHFlag(eed, BM_SEAM);}
		break;
	case EDGE_MODE_TAG_SHARP:
		if (val)		{BM_SetHFlag(eed, BM_SEAM);}
		else			{BM_ClearHFlag(eed, BM_SEAM);}
		break;				
	case EDGE_MODE_TAG_CREASE:	
		if (val)		{eed->crease = 1.0f;}
		else			{eed->crease = 0.0f;}
		break;
	case EDGE_MODE_TAG_BEVEL:
		if (val)		{eed->bweight = 1.0f;}
		else			{eed->bweight = 0.0f;}
		break;
	}
}

int edgetag_context_check(Scene *scene, BMEdge *eed)
{
	switch (scene->toolsettings->edge_mode) {
	case EDGE_MODE_SELECT:
		return BM_TestHFlag(eed, BM_SELECT) ? 1 : 0;
	case EDGE_MODE_TAG_SEAM:
		return BM_TestHFlag(eed, BM_SEAM);
	case EDGE_MODE_TAG_SHARP:
		return BM_TestHFlag(eed, BM_SHARP);
	case EDGE_MODE_TAG_CREASE:	
		return eed->crease ? 1 : 0;
	case EDGE_MODE_TAG_BEVEL:
		return eed->bweight ? 1 : 0;
	}
	return 0;
}


int edgetag_shortest_path(Scene *scene, BMEditMesh *em, BMEdge *source, BMEdge *target)
{
	BMEdge *eed;
	BMVert *ev;
	BMIter iter;
	Heap *heap;
	float *cost;
	int a, totvert=0, totedge=0, *nedges, *edges, *prevedge, mednum = -1, nedgeswap = 0;


	/* we need the vert */
	BM_ITER(ev, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		BMINDEX_SET(ev, totvert);
		totvert++;
	}

	BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		eed->head.eflag2 = 0;
		if (BM_TestHFlag(eed, BM_SELECT)) {
			eed->head.eflag2 |= ME_SEAM_DONE;
		}

		BMINDEX_SET(eed, totedge);
		totedge++;
	}

	/* alloc */
	nedges = MEM_callocN(sizeof(*nedges)*totvert+1, "SeamPathNEdges");
	edges = MEM_mallocN(sizeof(*edges)*totedge*2, "SeamPathEdges");
	prevedge = MEM_mallocN(sizeof(*prevedge)*totedge, "SeamPathPrevious");
	cost = MEM_mallocN(sizeof(*cost)*totedge, "SeamPathCost");

	/* count edges, compute adjacent edges offsets and fill adjacent edges */
	BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		nedges[BMINDEX_GET(eed->v1)+1]++;
		nedges[BMINDEX_GET(eed->v2)+1]++;
	}

	for (a=1; a<totvert; a++) {
		int newswap = nedges[a+1];
		nedges[a+1] = nedgeswap + nedges[a];
		nedgeswap = newswap;
	}
	nedges[0] = nedges[1] = 0;

	BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		edges[nedges[BMINDEX_GET(eed->v1)+1]++] = a;
		edges[nedges[BMINDEX_GET(eed->v2)+1]++] = a;

		cost[a] = 1e20f;
		prevedge[a] = -1;
		a++;
	}

	/* regular dijkstra shortest path, but over edges instead of vertices */
	heap = BLI_heap_new();
	BLI_heap_insert(heap, 0.0f, SET_INT_IN_POINTER(BMINDEX_GET(source)));
	cost[BMINDEX_GET(source)] = 0.0f;

	EDBM_init_index_arrays(em, 1, 1, 0);

	while (!BLI_heap_empty(heap)) {
		mednum = GET_INT_FROM_POINTER(BLI_heap_popmin(heap));
		eed = EDBM_get_edge_for_index(em, mednum);

		if (mednum == BMINDEX_GET(target))
			break;

		if (eed->head.eflag2 & ME_SEAM_DONE)
			continue;

		eed->head.eflag2 |= ME_SEAM_DONE;

		edgetag_add_adjacent(em, heap, mednum, BMINDEX_GET(eed->v1), nedges, edges, prevedge, cost);
		edgetag_add_adjacent(em, heap, mednum, BMINDEX_GET(eed->v2), nedges, edges, prevedge, cost);
	}
	
	MEM_freeN(nedges);
	MEM_freeN(edges);
	MEM_freeN(cost);
	BLI_heap_free(heap, NULL);

	BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		eed->head.eflag2 &= ~ME_SEAM_DONE;
	}

	if (mednum != BMINDEX_GET(target)) {
		MEM_freeN(prevedge);
		EDBM_free_index_arrays(em);
		return 0;
	}

	/* follow path back to source and mark as seam */
	if (mednum == BMINDEX_GET(target)) {
		short allseams = 1;

		mednum = BMINDEX_GET(target);
		do {
			eed = EDBM_get_edge_for_index(em, mednum);
			if (!edgetag_context_check(scene, eed)) {
				allseams = 0;
				break;
			}
			mednum = prevedge[mednum];
		} while (mednum != BMINDEX_GET(source));

		mednum = BMINDEX_GET(target);
		do {
			eed = EDBM_get_edge_for_index(em, mednum);
			if (allseams)
				edgetag_context_set(em, scene, eed, 0);
			else
				edgetag_context_set(em, scene, eed, 1);
			mednum = prevedge[mednum];
		} while (mednum != -1);
	}

	MEM_freeN(prevedge);
	EDBM_free_index_arrays(em);
	return 1;
}

/* *************************************** */

static void seam_edgehash_insert_face(EdgeHash *ehash, MPoly *mf, MLoop *loopstart)
{
	MLoop *ml1, *ml2;
	int a;

	for (a=0; a<mf->totloop; a++) {
		ml1 = loopstart + a;
		ml2 = loopstart + (a+1) % mf->totloop;

		BLI_edgehash_insert(ehash, ml1->v, ml2->v, NULL);
	}
}

void seam_mark_clear_tface(Scene *scene, short mode)
{
	Mesh *me;
	MPoly *mf;
	MLoop *ml1, *ml2;
	MEdge *med;
	int a, b;
	
	me= get_mesh(OBACT);
	if(me==0 ||  me->totpoly==0) return;

	if (mode == 0)
		mode = pupmenu("Seams%t|Mark Border Seam %x1|Clear Seam %x2");

	if (mode != 1 && mode != 2)
		return;

	if (mode == 2) {
		EdgeHash *ehash = BLI_edgehash_new();

		for (a=0, mf=me->mpoly; a<me->totpoly; a++, mf++)
			if (!(mf->flag & ME_HIDE) && (mf->flag & ME_FACE_SEL))
				seam_edgehash_insert_face(ehash, mf, me->mloop + mf->loopstart);

		for (a=0, med=me->medge; a<me->totedge; a++, med++)
			if (BLI_edgehash_haskey(ehash, med->v1, med->v2))
				med->flag &= ~ME_SEAM;

		BLI_edgehash_free(ehash, NULL);
	}
	else {
		/* mark edges that are on both selected and deselected faces */
		EdgeHash *ehash1 = BLI_edgehash_new();
		EdgeHash *ehash2 = BLI_edgehash_new();

		for (a=0, mf=me->mpoly; a<me->totpoly; a++, mf++) {
			if ((mf->flag & ME_HIDE) || !(mf->flag & ME_FACE_SEL))
				seam_edgehash_insert_face(ehash1, mf, me->mloop + mf->loopstart);
			else
				seam_edgehash_insert_face(ehash2, mf, me->mloop + mf->loopstart);
		}

		for (a=0, med=me->medge; a<me->totedge; a++, med++)
			if (BLI_edgehash_haskey(ehash1, med->v1, med->v2) &&
			    BLI_edgehash_haskey(ehash2, med->v1, med->v2))
				med->flag |= ME_SEAM;

		BLI_edgehash_free(ehash1, NULL);
		BLI_edgehash_free(ehash2, NULL);
	}

// XXX	if (G.rt == 8)
//		unwrap_lscm(1);

	me->drawflag |= ME_DRAWSEAMS;

// XXX notifier!		object_tface_flags_changed(OBACT, 1);
}

int face_select(struct bContext *C, Object *ob, short mval[2], int extend)
{
	Mesh *me;
	MPoly *mface, *msel;
	unsigned int a, index;
	
	/* Get the face under the cursor */
	me = get_mesh(ob);

	if (!facesel_face_pick(C, me, ob, mval, &index, 1))
		return 0;
	
	if (index >= me->totpoly || index < 0)
		return 0;

	msel= me->mpoly + index;
	if (msel->flag & ME_HIDE) return 0;
	
	/* clear flags */
	mface = me->mpoly;
	a = me->totpoly;
	if (!extend) {
		while (a--) {
			mface->flag &= ~ME_FACE_SEL;
			mface++;
		}
	}
	
	me->act_face = (int)index;

	if (extend) {
		if (msel->flag & ME_FACE_SEL)
			msel->flag &= ~ME_FACE_SEL;
		else
			msel->flag |= ME_FACE_SEL;
	}
	else msel->flag |= ME_FACE_SEL;
	
	/* image window redraw */
	
	object_facesel_flush_dm(ob);
// XXX notifier!		object_tface_flags_changed(OBACT, 1);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);
	ED_region_tag_redraw(CTX_wm_region(C)); // XXX - should redraw all 3D views
	return 1;
}

void face_borderselect(struct bContext *C, Object *ob, rcti *rect, int select, int extend)
{
	Mesh *me;
	MPoly *mface;
	struct ImBuf *ibuf;
	unsigned int *rt;
	char *selar;
	int a, sx, sy, index;
	
	ViewContext vc;
	view3d_set_viewcontext(C, &vc);

	me= get_mesh(ob);
	if(me==0) return;
	if(me->totpoly==0) return;

	selar= MEM_callocN(me->totpoly+1, "selar");

	sx= (rect->xmax-rect->xmin+1);
	sy= (rect->ymax-rect->ymin+1);
	if(sx*sy<=0) return;

	if (extend == 0 && select) {
		mface= me->mpoly;
		for(a=1; a<=me->totpoly; a++, mface++) {
			if((mface->flag & ME_HIDE) == 0)
				mface->flag &= ~ME_FACE_SEL;
		}
	}

	view3d_validate_backbuf(&vc);

	ibuf = IMB_allocImBuf(sx,sy,32,IB_rect,0);
	rt = ibuf->rect;
	glReadPixels(rect->xmin+vc.ar->winrct.xmin,  rect->ymin+vc.ar->winrct.ymin, sx, sy, GL_RGBA, GL_UNSIGNED_BYTE,  ibuf->rect);
	if(ENDIAN_ORDER==B_ENDIAN) IMB_convert_rgba_to_abgr(ibuf);

	a= sx*sy;
	while(a--) {
		if(*rt) {
			index= WM_framebuffer_to_index(*rt);
			if(index<=me->totface) selar[index]= 1;
		}
		rt++;
	}

	mface= me->mpoly;
	for(a=1; a<=me->totpoly; a++, mface++) {
		if(selar[a]) {
			if(mface->flag & ME_HIDE);
			else {
				if(select) mface->flag |= ME_FACE_SEL;
				else mface->flag &= ~ME_FACE_SEL;
			}
		}
	}

	IMB_freeImBuf(ibuf);
	MEM_freeN(selar);


// XXX notifier!			object_tface_flags_changed(OBACT, 0);
#ifdef __APPLE__	
	glReadBuffer(GL_BACK);
#endif
	
	object_facesel_flush_dm(ob);
}
