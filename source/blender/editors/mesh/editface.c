/*
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

/** \file blender/editors/mesh/editface.c
 *  \ingroup edmesh
 */



#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_context.h"
#include "BKE_tessmesh.h"

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
	Mesh *me = get_mesh(ob);
	DerivedMesh *dm = ob->derivedFinal;
	MPoly *polys, *mp_orig;
	MFace *faces, *mf;
	int *index_array = NULL;
	int totface, totpoly;
	int i;
	
	if (me==NULL || dm==NULL) {
		return;
	}

	/*
	 * Try to push updated mesh poly flags to two other data sets:
	 *  - Mesh polys => Mesh tess faces
	 *  - Mesh polys => Final derived mesh polys
	 */

	if (index_array = CustomData_get_layer(&me->fdata, CD_ORIGINDEX)) {
		faces = me->mface;
		totface = me->totface;
		
		/* loop over tessfaces */
		for (i= 0; i<totface; i++) {
			/* Copy flags onto the tessface from its poly */
			mp_orig = me->mpoly + index_array[i];
			faces[i].flag = mp_orig->flag;
		}
	}

	if (index_array = CustomData_get_layer(&dm->polyData, CD_ORIGINDEX)) {
		polys = dm->getPolyArray(dm);
		totpoly = dm->getNumFaces(dm);

		/* loop over final derived polys */
		for (i= 0; i<totpoly; i++) {
			/* Copy flags onto the mesh poly from its final derived poly */
			mp_orig = me->mpoly + index_array[i];
			polys[i].flag = mp_orig->flag;
		}
	}
}

/* returns 0 if not found, otherwise 1 */
static int facesel_face_pick(struct bContext *C, Mesh *me, Object *ob, const int mval[2], unsigned int *index, short rect)
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
	makeDerivedMesh(scene, ob, NULL, CD_MASK_BAREMESH, 0);

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
static MTexPoly *EDBM_get_active_mtface(BMEditMesh *em, BMFace **act_efa, int sloppy)
{
	BMFace *efa = NULL;
	
	if(!EDBM_texFaceCheck(em))
		return NULL;
	
	efa = BM_get_actFace(em->bm, sloppy);
	
	if (efa) {
		if (act_efa) *act_efa = efa; 
		return CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
	}

	if (act_efa) *act_efa= NULL;
	return NULL;
}

void paintface_hide(Object *ob, const int unselected)
{
	Mesh *me;
	MPoly *mface;
	int a;
	
	me= get_mesh(ob);
	if(me==NULL || me->totpoly==0) return;

	mface= me->mpoly;
	a= me->totpoly;
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
	MPoly *mface;
	int a;

	me= get_mesh(ob);
	if(me==NULL || me->totpoly==0) return;

	mface= me->mpoly;
	a= me->totpoly;
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

static void hash_add_face(EdgeHash *ehash, MPoly *mf, MLoop *mloop)
{
	MLoop *ml, *ml2;
	int i;

	for (i=0, ml=mloop; i<mf->totloop; i++, ml++) {
		ml2 = mloop + (i+1) % mf->totloop;
		BLI_edgehash_insert(ehash, ml->v, ml2->v, NULL);
	}
}


static void select_linked_tfaces_with_seams(int mode, Mesh *me, unsigned int index)
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
}

void paintface_select_linked(bContext *UNUSED(C), Object *ob, int UNUSED(mval[2]), int mode)
{
	Mesh *me;
	unsigned int index=0;

	me = get_mesh(ob);
	if(me==NULL || me->totpoly==0) return;

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

/* note: caller needs to run paintface_flush_flags(ob) after this */
void paintface_deselect_all_visible(Object *ob, int action, short UNUSED(flush_flags))
{
	Mesh *me;
	MPoly *mface;
	int a;

	me= get_mesh(ob);
	if(me==NULL) return;
	
	if(action == SEL_INVERT) {
		mface= me->mpoly;
		a= me->totpoly;
		while(a--) {
			if((mface->flag & ME_HIDE) == 0) {
				mface->flag ^= ME_FACE_SEL;
			}
			mface++;
		}
	} else {
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
	}

	//BMESH_TODO object_facesel_flush_dm(ob);
// XXX notifier!		object_tface_flags_changed(OBACT, 0);
}

static void selectswap_tface(Scene *scene)
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
	}
}

int paintface_minmax(Object *ob, float *min, float *max)
{
	Mesh *me;
	MPoly *mf;
	MTexPoly *tf;
	MLoop *ml;
	MVert *mv;
	int a, b, ok=0;
	float vec[3], bmat[3][3];

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

/* *************************************** */
#if 0
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
}
#endif

int paintface_mouse_select(struct bContext *C, Object *ob, const int mval[2], int extend)
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
	
	paintface_flush_flags(ob);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);
	ED_region_tag_redraw(CTX_wm_region(C)); // XXX - should redraw all 3D views
	return 1;
}

int do_paintface_box_select(ViewContext *vc, rcti *rect, int select, int extend)
{
	Object *ob = vc->obact;
	Mesh *me;
	MPoly *mface;
	struct ImBuf *ibuf;
	unsigned int *rt;
	char *selar;
	int a, index;
	int sx= rect->xmax-rect->xmin+1;
	int sy= rect->ymax-rect->ymin+1;
	
	me= get_mesh(ob);
	if(me==0) return 0;
	if(me->totpoly==0) return 0;

	if(me==NULL || me->totface==0 || sx*sy <= 0)
		return OPERATOR_CANCELLED;

	selar= MEM_callocN(me->totpoly+1, "selar");

	if (extend == 0 && select)
		paintface_deselect_all_visible(vc->obact, SEL_DESELECT, FALSE);

	if (extend == 0 && select) {
		mface= me->mpoly;
		for(a=1; a<=me->totpoly; a++, mface++) {
			if((mface->flag & ME_HIDE) == 0)
				mface->flag &= ~ME_FACE_SEL;
		}
	}

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

#ifdef __APPLE__	
	glReadBuffer(GL_BACK);
#endif

	paintface_flush_flags(vc->obact);

	return OPERATOR_FINISHED;
}


/*  (similar to void paintface_flush_flags(Object *ob))
 * copy the vertex flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting vertices (while painting) */
void paintvert_flush_flags(Object *ob)
{
	Mesh *me= get_mesh(ob);
	DerivedMesh *dm= ob->derivedFinal;
	MVert *dm_mvert, *dm_mv;
	int *index_array = NULL;
	int totvert;
	int i;

	if(me==NULL || dm==NULL)
		return;

	index_array = dm->getVertDataArray(dm, CD_ORIGINDEX);

	dm_mvert = dm->getVertArray(dm);
	totvert = dm->getNumVerts(dm);

	dm_mv= dm_mvert;

	if(index_array) {
		int orig_index;
		for (i= 0; i<totvert; i++, dm_mv++) {
			orig_index= index_array[i];
			if(orig_index != ORIGINDEX_NONE) {
				dm_mv->flag= me->mvert[index_array[i]].flag;
			}
		}
	}
	else {
		for (i= 0; i<totvert; i++, dm_mv++) {
			dm_mv->flag= me->mvert[i].flag;
		}
	}
}
/*  note: if the caller passes FALSE to flush_flags, then they will need to run paintvert_flush_flags(ob) themselves */
void paintvert_deselect_all_visible(Object *ob, int action, short flush_flags)
{
	Mesh *me;
	MVert *mvert;
	int a;

	me= get_mesh(ob);
	if(me==NULL) return;
	
	if(action == SEL_INVERT) {
		mvert= me->mvert;
		a= me->totvert;
		while(a--) {
			if((mvert->flag & ME_HIDE) == 0) {
				mvert->flag ^= SELECT;
			}
			mvert++;
		}
	}
	else {
		if (action == SEL_TOGGLE) {
			action = SEL_SELECT;

			mvert= me->mvert;
			a= me->totvert;
			while(a--) {
				if((mvert->flag & ME_HIDE) == 0 && mvert->flag & SELECT) {
					action = SEL_DESELECT;
					break;
				}
				mvert++;
			}
		}

		mvert= me->mvert;
		a= me->totvert;
		while(a--) {
			if((mvert->flag & ME_HIDE) == 0) {
				switch (action) {
				case SEL_SELECT:
					mvert->flag |= SELECT;
					break;
				case SEL_DESELECT:
					mvert->flag &= ~SELECT;
					break;
				case SEL_INVERT:
					mvert->flag ^= SELECT;
					break;
				}
			}
			mvert++;
		}
	}

	if(flush_flags) {
		paintvert_flush_flags(ob);
	}
}
