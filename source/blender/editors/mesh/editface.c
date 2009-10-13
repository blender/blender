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
#include "BLI_arithb.h"
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

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"

#ifndef DISABLE_PYTHON
//#include "BPY_extern.h"
//#include "BPY_menus.h"
#endif

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

/* own include */
#include "mesh_intern.h"

/* ***************** XXX **************** */
static int sample_backbuf_rect() {return 0;}
static int sample_backbuf() {return 0;}
static void error() {}
static int pupmenu() {return 0;}
/* ***************** XXX **************** */


/* copy the face flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting faces (while painting) */
void object_facesel_flush_dm(Object *ob)
{
	Mesh *me= get_mesh(ob);
	DerivedMesh *dm= ob->derivedFinal;
	MFace *faces, *mf, *mf_orig;
	int *index_array = NULL;
	int totface;
	int i;
	
	
	if(me==NULL || dm==NULL || !CustomData_has_layer( &dm->faceData, CD_ORIGINDEX))
		return;
	
	faces = dm->getFaceArray(dm);
	totface = dm->getNumFaces(dm);
	
	index_array = dm->getFaceDataArray(dm, CD_ORIGINDEX);
	
	mf= faces;
	
	for (i= 0; i<totface; i++, mf++) { /* loop over derived mesh faces */
		mf_orig= me->mface + index_array[i];
		mf->flag= mf_orig->flag;;
	}
}

/* returns 0 if not found, otherwise 1 */
int facesel_face_pick(View3D *v3d, Mesh *me, short *mval, unsigned int *index, short rect)
{
	if (!me || me->totface==0)
		return 0;

	if (v3d->flag & V3D_NEEDBACKBUFDRAW) {
// XXX drawview.c!		check_backbuf();
// XXX		persp(PERSP_VIEW);
	}

	if (rect) {
		/* sample rect to increase changes of selecting, so that when clicking
		   on an edge in the backbuf, we can still select a face */
		int dist;
		*index = sample_backbuf_rect(mval, 3, 1, me->totface+1, &dist,0,NULL);
	}
	else
		/* sample only on the exact position */
		*index = sample_backbuf(mval[0], mval[1]);

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

void reveal_tface(Scene *scene)
{
	Mesh *me;
	MFace *mface;
	int a;
	
	me= get_mesh(OBACT);
	if(me==0 || me->totface==0) return;
	
	mface= me->mface;
	a= me->totface;
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
	MFace *mface;
	int a;
	int shift=0, alt= 0; // XXX
	
	me= get_mesh(OBACT);
	if(me==0 || me->totface==0) return;
	
	if(alt) {
		reveal_tface(scene);
		return;
	}
	
	mface= me->mface;
	a= me->totface;
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

void select_linked_tfaces(Scene *scene, View3D *v3d, int mode)
{
	Object *ob;
	Mesh *me;
	short mval[2];
	unsigned int index=0;

	ob = OBACT;
	me = get_mesh(ob);
	if(me==0 || me->totface==0) return;

	if (mode==0 || mode==1) {
		if (!(ob->lay & v3d->lay))
			error("The active object is not in this layer");
			
// XXX		getmouseco_areawin(mval);
		if (!facesel_face_pick(v3d, me, mval, &index, 1)) return;
	}

// XXX unwrapper.c	select_linked_tfaces_with_seams(mode, me, index);
}

void deselectall_tface(Scene *scene)
{
	Mesh *me;
	MFace *mface;
	int a, sel;
		
	me= get_mesh(OBACT);
	if(me==0) return;
	
	mface= me->mface;
	a= me->totface;
	sel= 0;
	while(a--) {
		if(mface->flag & ME_HIDE);
		else if(mface->flag & ME_FACE_SEL) {
			sel= 1;
			break;
		}
		mface++;
	}
	
	mface= me->mface;
	a= me->totface;
	while(a--) {
		if(mface->flag & ME_HIDE);
		else {
			if(sel) mface->flag &= ~ME_FACE_SEL;
			else mface->flag |= ME_FACE_SEL;
		}
		mface++;
	}

	object_facesel_flush_dm(OBACT);
// XXX notifier!		object_tface_flags_changed(OBACT, 0);
}

void selectswap_tface(Scene *scene)
{
	Mesh *me;
	MFace *mface;
	int a;
		
	me= get_mesh(OBACT);
	if(me==0) return;
	
	mface= me->mface;
	a= me->totface;
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
	MFace *mf;
	MTFace *tf;
	MVert *mv;
	int a, ok=0;
	float vec[3], bmat[3][3];
	
	ob = OBACT;
	if (ob==0) return ok;
	me= get_mesh(ob);
	if(me==0 || me->mtface==0) return ok;
	
	Mat3CpyMat4(bmat, ob->obmat);

	mv= me->mvert;
	mf= me->mface;
	tf= me->mtface;
	for (a=me->totface; a>0; a--, mf++, tf++) {
		if (mf->flag & ME_HIDE || !(mf->flag & ME_FACE_SEL))
			continue;

		VECCOPY(vec, (mv+mf->v1)->co);
		Mat3MulVecfl(bmat, vec);
		VecAddf(vec, vec, ob->obmat[3]);
		DO_MINMAX(vec, min, max);		

		VECCOPY(vec, (mv+mf->v2)->co);
		Mat3MulVecfl(bmat, vec);
		VecAddf(vec, vec, ob->obmat[3]);
		DO_MINMAX(vec, min, max);		

		VECCOPY(vec, (mv+mf->v3)->co);
		Mat3MulVecfl(bmat, vec);
		VecAddf(vec, vec, ob->obmat[3]);
		DO_MINMAX(vec, min, max);		

		if (mf->v4) {
			VECCOPY(vec, (mv+mf->v4)->co);
			Mat3MulVecfl(bmat, vec);
			VecAddf(vec, vec, ob->obmat[3]);
			DO_MINMAX(vec, min, max);
		}
		ok= 1;
	}
	return ok;
}

/* ******************** edge loop shortest path ********************* */

#define ME_SEAM_DONE 2		/* reuse this flag */

static float edgetag_cut_cost(EditMesh *em, int e1, int e2, int vert)
{
	EditVert *v = EM_get_vert_for_index(vert);
	EditEdge *eed1 = EM_get_edge_for_index(e1), *eed2 = EM_get_edge_for_index(e2);
	EditVert *v1 = EM_get_vert_for_index( (eed1->v1->tmp.l == vert)? eed1->v2->tmp.l: eed1->v1->tmp.l );
	EditVert *v2 = EM_get_vert_for_index( (eed2->v1->tmp.l == vert)? eed2->v2->tmp.l: eed2->v1->tmp.l );
	float cost, d1[3], d2[3];

	cost = VecLenf(v1->co, v->co);
	cost += VecLenf(v->co, v2->co);

	VecSubf(d1, v->co, v1->co);
	VecSubf(d2, v2->co, v->co);

	cost = cost + 0.5f*cost*(2.0f - fabs(d1[0]*d2[0] + d1[1]*d2[1] + d1[2]*d2[2]));

	return cost;
}

static void edgetag_add_adjacent(EditMesh *em, Heap *heap, int mednum, int vertnum, int *nedges, int *edges, int *prevedge, float *cost)
{
	int startadj, endadj = nedges[vertnum+1];

	for (startadj = nedges[vertnum]; startadj < endadj; startadj++) {
		int adjnum = edges[startadj];
		EditEdge *eedadj = EM_get_edge_for_index(adjnum);
		float newcost;

		if (eedadj->f2 & ME_SEAM_DONE)
			continue;

		newcost = cost[mednum] + edgetag_cut_cost(em, mednum, adjnum, vertnum);

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

		edgetag_add_adjacent(em, heap, mednum, eed->v1->tmp.l, nedges, edges, prevedge, cost);
		edgetag_add_adjacent(em, heap, mednum, eed->v2->tmp.l, nedges, edges, prevedge, cost);
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

// XXX notifier!		object_tface_flags_changed(OBACT, 1);
}

void face_select(Scene *scene, View3D *v3d)
{
	Object *ob;
	Mesh *me;
	MFace *mface, *msel;
	short mval[2];
	unsigned int a, index;
	int shift= 0; // XXX
	
	/* Get the face under the cursor */
	ob = OBACT;
	if (!(ob->lay & v3d->lay)) {
		error("The active object is not in this layer");
	}
	me = get_mesh(ob);
// XXX	getmouseco_areawin(mval);

	if (!facesel_face_pick(v3d, me, mval, &index, 1)) return;
	
	msel= (((MFace*)me->mface)+index);
	if (msel->flag & ME_HIDE) return;
	
	/* clear flags */
	mface = me->mface;
	a = me->totface;
	if ((shift)==0) {
		while (a--) {
			mface->flag &= ~ME_FACE_SEL;
			mface++;
		}
	}
	
	me->act_face = (int)index;

	if (shift) {
		if (msel->flag & ME_FACE_SEL)
			msel->flag &= ~ME_FACE_SEL;
		else
			msel->flag |= ME_FACE_SEL;
	}
	else msel->flag |= ME_FACE_SEL;
	
	/* image window redraw */
	
	object_facesel_flush_dm(OBACT);
// XXX notifier!		object_tface_flags_changed(OBACT, 1);
}

void face_borderselect(Scene *scene, ScrArea *sa, ARegion *ar)
{
	Mesh *me;
	MFace *mface;
	rcti rect;
	struct ImBuf *ibuf;
	unsigned int *rt;
	int a, sx, sy, index, val= 0;
	char *selar;
	
	me= get_mesh(OBACT);
	if(me==0) return;
	if(me->totface==0) return;
	
// XXX	val= get_border(&rect, 3);
	
	if(val) {
		/* without this border select often fails */
#if 0 /* XXX untested in 2.5 */
		if (v3d->flag & V3D_NEEDBACKBUFDRAW) {
			check_backbuf();
			persp(PERSP_VIEW);
		}
#endif
		
		selar= MEM_callocN(me->totface+1, "selar");
		
		sx= (rect.xmax-rect.xmin+1);
		sy= (rect.ymax-rect.ymin+1);
		if(sx*sy<=0) return;

		ibuf = IMB_allocImBuf(sx,sy,32,IB_rect,0);
		rt = ibuf->rect;
		glReadPixels(rect.xmin+ar->winrct.xmin,  rect.ymin+ar->winrct.ymin, sx, sy, GL_RGBA, GL_UNSIGNED_BYTE,  ibuf->rect);
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
					if(val==LEFTMOUSE) mface->flag |= ME_FACE_SEL;
					else mface->flag &= ~ME_FACE_SEL;
				}
			}
		}
		
		IMB_freeImBuf(ibuf);
		MEM_freeN(selar);


// XXX notifier!			object_tface_flags_changed(OBACT, 0);
	}
#ifdef __APPLE__	
	glReadBuffer(GL_BACK);
#endif
	
	object_facesel_flush_dm(OBACT);
}


