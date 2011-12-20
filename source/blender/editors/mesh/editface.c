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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editface.c
 *  \ingroup edmesh
 */



#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_heap.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_context.h"

#include "BIF_gl.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

/* own include */
#include "mesh_intern.h"

/* copy the face flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting faces (while painting) */
void paintface_flush_flags(Object *ob)
{
	Mesh *me= get_mesh(ob);
	DerivedMesh *dm= ob->derivedFinal;
	MFace *faces, *mf, *mf_orig;
	int *index_array = NULL;
	int totface;
	int i;
	
	if(me==NULL || dm==NULL)
		return;

	index_array = dm->getFaceDataArray(dm, CD_ORIGINDEX);

	if(!index_array)
		return;
	
	faces = dm->getFaceArray(dm);
	totface = dm->getNumFaces(dm);
	
	mf= faces;
	
	for (i= 0; i<totface; i++, mf++) { /* loop over derived mesh faces */
		mf_orig= me->mface + index_array[i];
		mf->flag= mf_orig->flag;
	}
}

/* returns 0 if not found, otherwise 1 */
static int facesel_face_pick(struct bContext *C, Mesh *me, const int mval[2], unsigned int *index, short rect)
{
	ViewContext vc;
	view3d_set_viewcontext(C, &vc);

	if (!me || me->totface==0)
		return 0;

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

	if ((*index)<=0 || (*index)>(unsigned int)me->totface)
		return 0;

	(*index)--;
	
	return 1;
}

/* last_sel, use em->act_face otherwise get the last selected face in the editselections
 * at the moment, last_sel is mainly useful for gaking sure the space image dosnt flicker */
MTFace *EM_get_active_mtface(EditMesh *em, EditFace **act_efa, MCol **mcol, int sloppy)
{
	EditFace *efa = NULL;
	
	if(!EM_texFaceCheck(em))
		return NULL;
	
	efa = EM_get_actFace(em, sloppy);
	
	if (efa) {
		if (mcol) {
			if (CustomData_has_layer(&em->fdata, CD_MCOL))
				*mcol = CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
			else
				*mcol = NULL;
		}
		if (act_efa) *act_efa = efa; 
		return CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
	}
	if (act_efa) *act_efa= NULL;
	if(mcol) *mcol = NULL;
	return NULL;
}

void paintface_hide(Object *ob, const int unselected)
{
	Mesh *me;
	MFace *mface;
	int a;
	
	me= get_mesh(ob);
	if(me==NULL || me->totface==0) return;

	mface= me->mface;
	a= me->totface;
	while(a--) {
		if((mface->flag & ME_HIDE) == 0) {
			if(unselected) {
				if( (mface->flag & ME_FACE_SEL)==0) mface->flag |= ME_HIDE;
			}
			else {
				if( (mface->flag & ME_FACE_SEL)) mface->flag |= ME_HIDE;
			}
		}
		if(mface->flag & ME_HIDE) mface->flag &= ~ME_FACE_SEL;
		
		mface++;
	}
	
	paintface_flush_flags(ob);
}


void paintface_reveal(Object *ob)
{
	Mesh *me;
	MFace *mface;
	int a;

	me= get_mesh(ob);
	if(me==NULL || me->totface==0) return;

	mface= me->mface;
	a= me->totface;
	while(a--) {
		if(mface->flag & ME_HIDE) {
			mface->flag |= ME_FACE_SEL;
			mface->flag -= ME_HIDE;
		}
		mface++;
	}

	paintface_flush_flags(ob);
}

/* Set tface seams based on edge data, uses hash table to find seam edges. */

static void hash_add_face(EdgeHash *ehash, MFace *mf)
{
	BLI_edgehash_insert(ehash, mf->v1, mf->v2, NULL);
	BLI_edgehash_insert(ehash, mf->v2, mf->v3, NULL);
	if(mf->v4) {
		BLI_edgehash_insert(ehash, mf->v3, mf->v4, NULL);
		BLI_edgehash_insert(ehash, mf->v4, mf->v1, NULL);
	}
	else
		BLI_edgehash_insert(ehash, mf->v3, mf->v1, NULL);
}


static void select_linked_tfaces_with_seams(int mode, Mesh *me, unsigned int index)
{
	MFace *mf;
	int a, doit=1, mark=0;
	char *linkflag;
	EdgeHash *ehash, *seamhash;
	MEdge *med;

	ehash= BLI_edgehash_new();
	seamhash = BLI_edgehash_new();
	linkflag= MEM_callocN(sizeof(char)*me->totface, "linkflaguv");

	for(med=me->medge, a=0; a < me->totedge; a++, med++)
		if(med->flag & ME_SEAM)
			BLI_edgehash_insert(seamhash, med->v1, med->v2, NULL);

	if (mode==0 || mode==1) {
		/* only put face under cursor in array */
		mf= ((MFace*)me->mface) + index;
		hash_add_face(ehash, mf);
		linkflag[index]= 1;
	}
	else {
		/* fill array by selection */
		mf= me->mface;
		for(a=0; a<me->totface; a++, mf++) {
			if(mf->flag & ME_HIDE);
			else if(mf->flag & ME_FACE_SEL) {
				hash_add_face(ehash, mf);
				linkflag[a]= 1;
			}
		}
	}

	while(doit) {
		doit= 0;

		/* expand selection */
		mf= me->mface;
		for(a=0; a<me->totface; a++, mf++) {
			if(mf->flag & ME_HIDE)
				continue;

			if(!linkflag[a]) {
				mark= 0;

				if(!BLI_edgehash_haskey(seamhash, mf->v1, mf->v2))
					if(BLI_edgehash_haskey(ehash, mf->v1, mf->v2))
						mark= 1;
				if(!BLI_edgehash_haskey(seamhash, mf->v2, mf->v3))
					if(BLI_edgehash_haskey(ehash, mf->v2, mf->v3))
						mark= 1;
				if(mf->v4) {
					if(!BLI_edgehash_haskey(seamhash, mf->v3, mf->v4))
						if(BLI_edgehash_haskey(ehash, mf->v3, mf->v4))
							mark= 1;
					if(!BLI_edgehash_haskey(seamhash, mf->v4, mf->v1))
						if(BLI_edgehash_haskey(ehash, mf->v4, mf->v1))
							mark= 1;
				}
				else if(!BLI_edgehash_haskey(seamhash, mf->v3, mf->v1))
					if(BLI_edgehash_haskey(ehash, mf->v3, mf->v1))
						mark = 1;

				if(mark) {
					linkflag[a]= 1;
					hash_add_face(ehash, mf);
					doit= 1;
				}
			}
		}

	}

	BLI_edgehash_free(ehash, NULL);
	BLI_edgehash_free(seamhash, NULL);

	if(mode==0 || mode==2) {
		for(a=0, mf=me->mface; a<me->totface; a++, mf++)
			if(linkflag[a])
				mf->flag |= ME_FACE_SEL;
			else
				mf->flag &= ~ME_FACE_SEL;
	}
	else if(mode==1) {
		for(a=0, mf=me->mface; a<me->totface; a++, mf++)
			if(linkflag[a] && (mf->flag & ME_FACE_SEL))
				break;

		if (a<me->totface) {
			for(a=0, mf=me->mface; a<me->totface; a++, mf++)
				if(linkflag[a])
					mf->flag &= ~ME_FACE_SEL;
		}
		else {
			for(a=0, mf=me->mface; a<me->totface; a++, mf++)
				if(linkflag[a])
					mf->flag |= ME_FACE_SEL;
		}
	}

	MEM_freeN(linkflag);
}

void paintface_select_linked(bContext *UNUSED(C), Object *ob, int UNUSED(mval[2]), int mode)
{
	Mesh *me;
	unsigned int index=0;

	me = get_mesh(ob);
	if(me==NULL || me->totface==0) return;

	if (mode==0 || mode==1) {
		// XXX - Causes glitches, not sure why
		/*
		if (!facesel_face_pick(C, me, mval, &index, 1))
			return;
		*/
	}

	select_linked_tfaces_with_seams(mode, me, index);

	paintface_flush_flags(ob);
}

void paintface_deselect_all_visible(Object *ob, int action, short flush_flags)
{
	Mesh *me;
	MFace *mface;
	int a;

	me= get_mesh(ob);
	if(me==NULL) return;
	
	if(action == SEL_INVERT) {
		mface= me->mface;
		a= me->totface;
		while(a--) {
			if((mface->flag & ME_HIDE) == 0) {
				mface->flag ^= ME_FACE_SEL;
			}
			mface++;
		}
	}
	else {
		if (action == SEL_TOGGLE) {
			action = SEL_SELECT;

			mface= me->mface;
			a= me->totface;
			while(a--) {
				if((mface->flag & ME_HIDE) == 0 && mface->flag & ME_FACE_SEL) {
					action = SEL_DESELECT;
					break;
				}
				mface++;
			}
		}

		mface= me->mface;
		a= me->totface;
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
	}

	if(flush_flags) {
		paintface_flush_flags(ob);
	}
}

int paintface_minmax(Object *ob, float *min, float *max)
{
	Mesh *me= get_mesh(ob);
	MFace *mf;
	MVert *mv;
	int a, ok=0;
	float vec[3];

	if(me==NULL)
		return ok;

	mv= me->mvert;
	mf= me->mface;
	for (a=me->totface; a>0; a--, mf++) {
		if ((mf->flag & ME_HIDE || !(mf->flag & ME_FACE_SEL)) == 0) {
			int i= mf->v4 ? 3:2;
			do {
				mul_v3_m4v3(vec, ob->obmat, (mv + (*(&mf->v1 + i)))->co);
				DO_MINMAX(vec, min, max);
			} while (i--);
			ok= 1;
		}
	}
	return ok;
}

/* ******************** edge loop shortest path ********************* */

#define ME_SEAM_DONE 2		/* reuse this flag */

static float edgetag_cut_cost(int e1, int e2, int vert)
{
	EditVert *v = EM_get_vert_for_index(vert);
	EditEdge *eed1 = EM_get_edge_for_index(e1), *eed2 = EM_get_edge_for_index(e2);
	EditVert *v1 = EM_get_vert_for_index( (eed1->v1->tmp.l == vert)? eed1->v2->tmp.l: eed1->v1->tmp.l );
	EditVert *v2 = EM_get_vert_for_index( (eed2->v1->tmp.l == vert)? eed2->v2->tmp.l: eed2->v1->tmp.l );
	float cost, d1[3], d2[3];

	cost = len_v3v3(v1->co, v->co);
	cost += len_v3v3(v->co, v2->co);

	sub_v3_v3v3(d1, v->co, v1->co);
	sub_v3_v3v3(d2, v2->co, v->co);

	cost = cost + 0.5f*cost*(2.0f - fabsf(d1[0]*d2[0] + d1[1]*d2[1] + d1[2]*d2[2]));

	return cost;
}

static void edgetag_add_adjacent(Heap *heap, int mednum, int vertnum, int *nedges, int *edges, int *prevedge, float *cost)
{
	int startadj, endadj = nedges[vertnum+1];

	for (startadj = nedges[vertnum]; startadj < endadj; startadj++) {
		int adjnum = edges[startadj];
		EditEdge *eedadj = EM_get_edge_for_index(adjnum);
		float newcost;

		if (eedadj->f2 & ME_SEAM_DONE)
			continue;

		newcost = cost[mednum] + edgetag_cut_cost(mednum, adjnum, vertnum);

		if (cost[adjnum] > newcost) {
			cost[adjnum] = newcost;
			prevedge[adjnum] = mednum;
			BLI_heap_insert(heap, newcost, SET_INT_IN_POINTER(adjnum));
		}
	}
}

void edgetag_context_set(Scene *scene, EditEdge *eed, int val)
{
	
	switch (scene->toolsettings->edge_mode) {
	case EDGE_MODE_SELECT:
		EM_select_edge(eed, val);
		break;
	case EDGE_MODE_TAG_SEAM:
		if (val)		{eed->seam = 255;}
		else			{eed->seam = 0;}
		break;
	case EDGE_MODE_TAG_SHARP:
		if (val)		{eed->sharp = 1;}
		else			{eed->sharp = 0;}
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

int edgetag_context_check(Scene *scene, EditEdge *eed)
{
	switch (scene->toolsettings->edge_mode) {
	case EDGE_MODE_SELECT:
		return (eed->f & SELECT) ? 1 : 0;
	case EDGE_MODE_TAG_SEAM:
		return eed->seam ? 1 : 0;
	case EDGE_MODE_TAG_SHARP:
		return eed->sharp ? 1 : 0;
	case EDGE_MODE_TAG_CREASE:	
		return eed->crease ? 1 : 0;
	case EDGE_MODE_TAG_BEVEL:
		return eed->bweight ? 1 : 0;
	}
	return 0;
}


int edgetag_shortest_path(Scene *scene, EditMesh *em, EditEdge *source, EditEdge *target)
{
	EditEdge *eed;
	EditVert *ev;
	
	Heap *heap;
	float *cost;
	int a, totvert=0, totedge=0, *nedges, *edges, *prevedge, mednum = -1, nedgeswap = 0;


	/* we need the vert */
	for (ev= em->verts.first, totvert=0; ev; ev= ev->next) {
		ev->tmp.l = totvert;
		totvert++;
	}

	for (eed= em->edges.first; eed; eed = eed->next) {
		eed->f2 = 0;
		if (eed->h) {
			eed->f2 |= ME_SEAM_DONE;
		}
		eed->tmp.l = totedge;
		totedge++;
	}

	/* alloc */
	nedges = MEM_callocN(sizeof(*nedges)*totvert+1, "SeamPathNEdges");
	edges = MEM_mallocN(sizeof(*edges)*totedge*2, "SeamPathEdges");
	prevedge = MEM_mallocN(sizeof(*prevedge)*totedge, "SeamPathPrevious");
	cost = MEM_mallocN(sizeof(*cost)*totedge, "SeamPathCost");

	/* count edges, compute adjacent edges offsets and fill adjacent edges */
	for (eed= em->edges.first; eed; eed = eed->next) {
		nedges[eed->v1->tmp.l+1]++;
		nedges[eed->v2->tmp.l+1]++;
	}

	for (a=1; a<totvert; a++) {
		int newswap = nedges[a+1];
		nedges[a+1] = nedgeswap + nedges[a];
		nedgeswap = newswap;
	}
	nedges[0] = nedges[1] = 0;

	for (a=0, eed= em->edges.first; eed; a++, eed = eed->next) {
		edges[nedges[eed->v1->tmp.l+1]++] = a;
		edges[nedges[eed->v2->tmp.l+1]++] = a;

		cost[a] = 1e20f;
		prevedge[a] = -1;
	}

	/* regular dijkstra shortest path, but over edges instead of vertices */
	heap = BLI_heap_new();
	BLI_heap_insert(heap, 0.0f, SET_INT_IN_POINTER(source->tmp.l));
	cost[source->tmp.l] = 0.0f;

	EM_init_index_arrays(em, 1, 1, 0);


	while (!BLI_heap_empty(heap)) {
		mednum = GET_INT_FROM_POINTER(BLI_heap_popmin(heap));
		eed = EM_get_edge_for_index( mednum );

		if (mednum == target->tmp.l)
			break;

		if (eed->f2 & ME_SEAM_DONE)
			continue;

		eed->f2 |= ME_SEAM_DONE;

		edgetag_add_adjacent(heap, mednum, eed->v1->tmp.l, nedges, edges, prevedge, cost);
		edgetag_add_adjacent(heap, mednum, eed->v2->tmp.l, nedges, edges, prevedge, cost);
	}
	
	
	MEM_freeN(nedges);
	MEM_freeN(edges);
	MEM_freeN(cost);
	BLI_heap_free(heap, NULL);

	for (eed= em->edges.first; eed; eed = eed->next) {
		eed->f2 &= ~ME_SEAM_DONE;
	}

	if (mednum != target->tmp.l) {
		MEM_freeN(prevedge);
		EM_free_index_arrays();
		return 0;
	}

	/* follow path back to source and mark as seam */
	if (mednum == target->tmp.l) {
		short allseams = 1;

		mednum = target->tmp.l;
		do {
			eed = EM_get_edge_for_index( mednum );
			if (!edgetag_context_check(scene, eed)) {
				allseams = 0;
				break;
			}
			mednum = prevedge[mednum];
		} while (mednum != source->tmp.l);

		mednum = target->tmp.l;
		do {
			eed = EM_get_edge_for_index( mednum );
			if (allseams)
				edgetag_context_set(scene, eed, 0);
			else
				edgetag_context_set(scene, eed, 1);
			mednum = prevedge[mednum];
		} while (mednum != -1);
	}

	MEM_freeN(prevedge);
	EM_free_index_arrays();
	return 1;
}

/* *************************************** */
#if 0
static void seam_edgehash_insert_face(EdgeHash *ehash, MFace *mf)
{
	BLI_edgehash_insert(ehash, mf->v1, mf->v2, NULL);
	BLI_edgehash_insert(ehash, mf->v2, mf->v3, NULL);
	if (mf->v4) {
		BLI_edgehash_insert(ehash, mf->v3, mf->v4, NULL);
		BLI_edgehash_insert(ehash, mf->v4, mf->v1, NULL);
	}
	else
		BLI_edgehash_insert(ehash, mf->v3, mf->v1, NULL);
}

void seam_mark_clear_tface(Scene *scene, short mode)
{
	Mesh *me;
	MFace *mf;
	MEdge *med;
	int a;
	
	me= get_mesh(OBACT);
	if(me==0 ||  me->totface==0) return;

	if (mode == 0)
		mode = pupmenu("Seams%t|Mark Border Seam %x1|Clear Seam %x2");

	if (mode != 1 && mode != 2)
		return;

	if (mode == 2) {
		EdgeHash *ehash = BLI_edgehash_new();

		for (a=0, mf=me->mface; a<me->totface; a++, mf++)
			if (!(mf->flag & ME_HIDE) && (mf->flag & ME_FACE_SEL))
				seam_edgehash_insert_face(ehash, mf);

		for (a=0, med=me->medge; a<me->totedge; a++, med++)
			if (BLI_edgehash_haskey(ehash, med->v1, med->v2))
				med->flag &= ~ME_SEAM;

		BLI_edgehash_free(ehash, NULL);
	}
	else {
		/* mark edges that are on both selected and deselected faces */
		EdgeHash *ehash1 = BLI_edgehash_new();
		EdgeHash *ehash2 = BLI_edgehash_new();

		for (a=0, mf=me->mface; a<me->totface; a++, mf++) {
			if ((mf->flag & ME_HIDE) || !(mf->flag & ME_FACE_SEL))
				seam_edgehash_insert_face(ehash1, mf);
			else
				seam_edgehash_insert_face(ehash2, mf);
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
}
#endif

int paintface_mouse_select(struct bContext *C, Object *ob, const int mval[2], int extend)
{
	Mesh *me;
	MFace *mface, *msel;
	unsigned int a, index;
	
	/* Get the face under the cursor */
	me = get_mesh(ob);

	if (!facesel_face_pick(C, me, mval, &index, 1))
		return 0;
	
	msel= (((MFace*)me->mface)+index);
	if (msel->flag & ME_HIDE) return 0;
	
	/* clear flags */
	mface = me->mface;
	a = me->totface;
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
	
	paintface_flush_flags(ob);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);
	ED_region_tag_redraw(CTX_wm_region(C)); // XXX - should redraw all 3D views
	return 1;
}

int do_paintface_box_select(ViewContext *vc, rcti *rect, int select, int extend)
{
	Mesh *me;
	MFace *mface;
	struct ImBuf *ibuf;
	unsigned int *rt;
	int a, index;
	char *selar;
	int sx= rect->xmax-rect->xmin+1;
	int sy= rect->ymax-rect->ymin+1;

	me= get_mesh(vc->obact);

	if(me==NULL || me->totface==0 || sx*sy <= 0)
		return OPERATOR_CANCELLED;

	selar= MEM_callocN(me->totface+1, "selar");

	if (extend == 0 && select)
		paintface_deselect_all_visible(vc->obact, SEL_DESELECT, FALSE);

	view3d_validate_backbuf(vc);

	ibuf = IMB_allocImBuf(sx,sy,32,IB_rect);
	rt = ibuf->rect;
	glReadPixels(rect->xmin+vc->ar->winrct.xmin,  rect->ymin+vc->ar->winrct.ymin, sx, sy, GL_RGBA, GL_UNSIGNED_BYTE,  ibuf->rect);
	if(ENDIAN_ORDER==B_ENDIAN) IMB_convert_rgba_to_abgr(ibuf);

	a= sx*sy;
	while(a--) {
		if(*rt) {
			index= WM_framebuffer_to_index(*rt);
			if(index<=me->totface) selar[index]= 1;
		}
		rt++;
	}

	mface= me->mface;
	for(a=1; a<=me->totface; a++, mface++) {
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

#ifdef __APPLE__	
	glReadBuffer(GL_BACK);
#endif

	paintface_flush_flags(vc->obact);

	return OPERATOR_FINISHED;
}


/* ********************* MESH VERTEX MIRR TOPO LOOKUP *************** */
/* note, this is not the best place for the function to be but moved
 * here to for the purpose of syncing with bmesh */

typedef int MirrTopoHash_t;

typedef struct MirrTopoVert_t {
	MirrTopoHash_t  hash;
	int             v_index;
} MirrTopoVert_t;

static int mirrtopo_hash_sort(const void *l1, const void *l2)
{
	if       ((MirrTopoHash_t)(intptr_t)l1 > (MirrTopoHash_t)(intptr_t)l2 ) return  1;
	else if  ((MirrTopoHash_t)(intptr_t)l1 < (MirrTopoHash_t)(intptr_t)l2 ) return -1;
	return 0;
}

static int mirrtopo_vert_sort(const void *v1, const void *v2)
{
	if      (((MirrTopoVert_t *)v1)->hash > ((MirrTopoVert_t *)v2)->hash ) return  1;
	else if (((MirrTopoVert_t *)v1)->hash < ((MirrTopoVert_t *)v2)->hash ) return -1;
	return 0;
}

int ED_mesh_mirrtopo_recalc_check(Mesh *me, const int ob_mode, MirrTopoStore_t *mesh_topo_store)
{
	int totvert;
	int totedge;

	if (me->edit_mesh) {
		totvert = me->edit_mesh->totvert;
		totedge = me->edit_mesh->totedge;
	}
	else {
		totvert = me->totvert;
		totedge = me->totedge;
	}

	if(	(mesh_topo_store->index_lookup==NULL) ||
		(mesh_topo_store->prev_ob_mode != ob_mode) ||
		(totvert != mesh_topo_store->prev_vert_tot) ||
		(totedge != mesh_topo_store->prev_edge_tot))
	{
		return TRUE;
	}
	else {
		return FALSE;
	}

}

void ED_mesh_mirrtopo_init(Mesh *me, const int ob_mode, MirrTopoStore_t *mesh_topo_store,
                           const short skip_em_vert_array_init)
{
	MEdge *medge;
	EditMesh *em = me->edit_mesh;
	void **eve_tmp_back = NULL; /* some of the callers are using eve->tmp so restore after */

	/* editmode*/
	EditEdge *eed;

	int a, last;
	int totvert, totedge;
	int tot_unique = -1, tot_unique_prev = -1;

	MirrTopoHash_t *topo_hash = NULL;
	MirrTopoHash_t *topo_hash_prev = NULL;
	MirrTopoVert_t *topo_pairs;

	intptr_t *index_lookup; /* direct access to mesh_topo_store->index_lookup */

	/* reallocate if needed */
	ED_mesh_mirrtopo_free(mesh_topo_store);

	mesh_topo_store->prev_ob_mode = ob_mode;

	if(em) {
		EditVert *eve;
		totvert = 0;
		eve_tmp_back = MEM_mallocN(em->totvert * sizeof(void *), "TopoMirr");
		for(eve = em->verts.first; eve; eve = eve->next) {
			eve_tmp_back[totvert]= eve->tmp.p;
			eve->tmp.l = totvert++;
		}
	}
	else {
		totvert = me->totvert;
	}

	topo_hash = MEM_callocN(totvert * sizeof(MirrTopoHash_t), "TopoMirr");

	/* Initialize the vert-edge-user counts used to detect unique topology */
	if(em) {
		totedge = 0;

		for(eed=em->edges.first; eed; eed = eed->next, totedge++) {
			topo_hash[eed->v1->tmp.l]++;
			topo_hash[eed->v2->tmp.l]++;
		}
	}
	else {
		totedge = me->totedge;

		for(a=0, medge=me->medge; a < me->totedge; a++, medge++) {
			topo_hash[medge->v1]++;
			topo_hash[medge->v2]++;
		}
	}

	topo_hash_prev = MEM_dupallocN(topo_hash);

	tot_unique_prev = -1;
	while(1) {
		/* use the number of edges per vert to give verts unique topology IDs */

		if(em) {
			for(eed=em->edges.first; eed; eed = eed->next) {
				topo_hash[eed->v1->tmp.l] += topo_hash_prev[eed->v2->tmp.l];
				topo_hash[eed->v2->tmp.l] += topo_hash_prev[eed->v1->tmp.l];
			}
		}
		else {
			for(a=0, medge=me->medge; a<me->totedge; a++, medge++) {
				/* This can make really big numbers, wrapping around here is fine */
				topo_hash[medge->v1] += topo_hash_prev[medge->v2];
				topo_hash[medge->v2] += topo_hash_prev[medge->v1];
			}
		}
		memcpy(topo_hash_prev, topo_hash, sizeof(MirrTopoHash_t) * totvert);

		/* sort so we can count unique values */
		qsort(topo_hash_prev, totvert, sizeof(MirrTopoHash_t), mirrtopo_hash_sort);

		tot_unique = 1; /* account for skiping the first value */
		for(a=1; a<totvert; a++) {
			if (topo_hash_prev[a-1] != topo_hash_prev[a]) {
				tot_unique++;
			}
		}

		if (tot_unique <= tot_unique_prev) {
			/* Finish searching for unique valus when 1 loop dosnt give a
			 * higher number of unique values compared to the previous loop */
			break;
		}
		else {
			tot_unique_prev = tot_unique;
		}
		/* Copy the hash calculated this iter, so we can use them next time */
		memcpy(topo_hash_prev, topo_hash, sizeof(MirrTopoHash_t) * totvert);
	}

	/* restore eve->tmp.* */
	if(eve_tmp_back) {
		EditVert *eve;
		totvert = 0;
		for(eve = em->verts.first; eve; eve = eve->next) {
			eve->tmp.p = eve_tmp_back[totvert++];
		}

		MEM_freeN(eve_tmp_back);
		eve_tmp_back = NULL;
	}


	/* Hash/Index pairs are needed for sorting to find index pairs */
	topo_pairs = MEM_callocN( sizeof(MirrTopoVert_t) * totvert, "MirrTopoPairs");

	/* since we are looping through verts, initialize these values here too */
	index_lookup = MEM_mallocN(totvert * sizeof(*index_lookup), "mesh_topo_lookup");

	if(em) {
		if (skip_em_vert_array_init == FALSE) {
			EM_init_index_arrays(em, 1, 0, 0);
		}
	}


	for(a=0; a<totvert; a++) {
		topo_pairs[a].hash    = topo_hash[a];
		topo_pairs[a].v_index = a;

		/* initialize lookup */
		index_lookup[a] = -1;
	}

	qsort(topo_pairs, totvert, sizeof(MirrTopoVert_t), mirrtopo_vert_sort);

	/* Since the loop starts at 2, we must define the last index where the hash's differ */
	last = ((totvert >= 2) && (topo_pairs[0].hash == topo_pairs[1].hash)) ? 0 : 1;

	/* Get the pairs out of the sorted hashes, note, totvert+1 means we can use the previous 2,
	 * but you cant ever access the last 'a' index of MirrTopoPairs */
	for(a=2; a <= totvert; a++) {
		/* printf("I %d %ld %d\n", (a-last), MirrTopoPairs[a  ].hash, MirrTopoPairs[a  ].vIndex ); */
		if ((a==totvert) || (topo_pairs[a-1].hash != topo_pairs[a].hash)) {
			if (a-last==2) {
				if(em) {
					index_lookup[topo_pairs[a-1].v_index] =	(intptr_t)EM_get_vert_for_index(topo_pairs[a-2].v_index);
					index_lookup[topo_pairs[a-2].v_index] =	(intptr_t)EM_get_vert_for_index(topo_pairs[a-1].v_index);
				}
				else {
					index_lookup[topo_pairs[a-1].v_index] =	topo_pairs[a-2].v_index;
					index_lookup[topo_pairs[a-2].v_index] =	topo_pairs[a-1].v_index;
				}
			}
			last = a;
		}
	}
	if(em) {
		if (skip_em_vert_array_init == FALSE) {
			EM_free_index_arrays();
		}
	}

	MEM_freeN(topo_pairs);
	topo_pairs = NULL;

	MEM_freeN(topo_hash);
	MEM_freeN(topo_hash_prev);

	mesh_topo_store->index_lookup  = index_lookup;
	mesh_topo_store->prev_vert_tot = totvert;
	mesh_topo_store->prev_edge_tot = totedge;
}

void ED_mesh_mirrtopo_free(MirrTopoStore_t *mesh_topo_store)
{
	if (mesh_topo_store->index_lookup) {
		MEM_freeN(mesh_topo_store->index_lookup);
	}
	mesh_topo_store->index_lookup  = NULL;
	mesh_topo_store->prev_vert_tot = -1;
	mesh_topo_store->prev_edge_tot = -1;
}
