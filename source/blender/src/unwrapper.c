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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"
#include "BKE_customdata.h"

#include "BLI_arithb.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"

#include "BIF_editsima.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_editmesh.h"

#include "blendef.h"
#include "mydevice.h"

#include "BDR_unwrapper.h"

#include "PIL_time.h"

#include "parametrizer.h"

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

void select_linked_tfaces_with_seams(int mode, Mesh *me, unsigned int index)
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
	
	BIF_undo_push("Select linked UV face");
	object_tface_flags_changed(OBACT, 0);
}

/* Parametrizer */
ParamHandle *construct_param_handle(EditMesh *em, short implicit, short fill, short sel)
{
	int a;
	MTFace *tf;
	
	EditFace *efa;
	EditEdge *eed;
	EditVert *ev;
	
	ParamHandle *handle;
	
	handle = param_construct_begin();

	if ((G.scene->toolsettings->uvcalc_flag & UVCALC_NO_ASPECT_CORRECT)==0) {
		EditMesh *em = G.editMesh;
		EditFace *efa = EM_get_actFace(1);
		if (efa) {
			float aspx, aspy;
			MTFace *tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			image_final_aspect(tface->tpage, &aspx, &aspy);
		
			if (aspx!=aspy)
				param_aspect_ratio(handle, aspx, aspy);
		}
	}
	
	/* we need the vert indicies */
	for (ev= em->verts.first, a=0; ev; ev= ev->next, a++)
		ev->tmp.l = a;
	
	for (efa= em->faces.first; efa; efa= efa->next) {
		ParamKey key, vkeys[4];
		ParamBool pin[4], select[4];
		float *co[4];
		float *uv[4];
		int nverts;
		
		if ((efa->h) || (sel && (efa->f & SELECT)==0)) 
			continue;

		tf= (MTFace *)CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		
		if (implicit &&
			!(	simaUVSel_Check(efa, tf, 0) ||
				simaUVSel_Check(efa, tf, 1) ||
				simaUVSel_Check(efa, tf, 2) ||
				(efa->v4 && simaUVSel_Check(efa, tf, 3)) )
		) {
			continue;
		}

		key = (ParamKey)efa;
		vkeys[0] = (ParamKey)efa->v1->tmp.l;
		vkeys[1] = (ParamKey)efa->v2->tmp.l;
		vkeys[2] = (ParamKey)efa->v3->tmp.l;

		co[0] = efa->v1->co;
		co[1] = efa->v2->co;
		co[2] = efa->v3->co;

		uv[0] = tf->uv[0];
		uv[1] = tf->uv[1];
		uv[2] = tf->uv[2];

		pin[0] = ((tf->unwrap & TF_PIN1) != 0);
		pin[1] = ((tf->unwrap & TF_PIN2) != 0);
		pin[2] = ((tf->unwrap & TF_PIN3) != 0);

		select[0] = ((simaUVSel_Check(efa, tf, 0)) != 0);
		select[1] = ((simaUVSel_Check(efa, tf, 1)) != 0);
		select[2] = ((simaUVSel_Check(efa, tf, 2)) != 0);

		if (efa->v4) {
			vkeys[3] = (ParamKey)efa->v4->tmp.l;
			co[3] = efa->v4->co;
			uv[3] = tf->uv[3];
			pin[3] = ((tf->unwrap & TF_PIN4) != 0);
			select[3] = (simaUVSel_Check(efa, tf, 3) != 0);
			nverts = 4;
		}
		else
			nverts = 3;

		param_face_add(handle, key, nverts, vkeys, co, uv, pin, select);
	}

	if (!implicit) {
		for (eed= em->edges.first; eed; eed= eed->next) {
			if(eed->seam) {
				ParamKey vkeys[2];
				vkeys[0] = (ParamKey)eed->v1->tmp.l;
				vkeys[1] = (ParamKey)eed->v2->tmp.l;
				param_edge_set_seam(handle, vkeys);
			}
		}
	}

	param_construct_end(handle, fill, implicit);

	return handle;
}


extern int EM_texFaceCheck(void);

void unwrap_lscm(short seamcut)
{
	EditMesh *em = G.editMesh;
	ParamHandle *handle;
	short abf = G.scene->toolsettings->unwrapper == 1;
	short fillholes = G.scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;
	
	/* add uvs if there not here */
	if (!EM_texFaceCheck()) {
		if (em && em->faces.first)
			EM_add_data_layer(&em->fdata, CD_MTFACE);
		
		if (!EM_texFaceCheck())
			return;
		
		if (G.sima && G.sima->image) /* this is a bit of a kludge, but assume they want the image on their mesh when UVs are added */
			image_changed(G.sima, G.sima->image);
		
		/* select new UV's */
		if ((G.sima==0 || G.sima->flag & SI_SYNC_UVSEL)==0) {
			EditFace *efa;
			MTFace *tf;
			for(efa=em->faces.first; efa; efa=efa->next) {
				tf= (MTFace *)CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				simaFaceSel_Set(efa, tf);
			}
		}
	}

	handle = construct_param_handle(em, 0, fillholes, seamcut == 0);

	param_lscm_begin(handle, PARAM_FALSE, abf);
	param_lscm_solve(handle);
	param_lscm_end(handle);
	
	param_pack(handle);

	param_flush(handle);

	param_delete(handle);

	if (!seamcut)
		BIF_undo_push("UV unwrap");

	object_uvs_changed(OBACT);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

void minimize_stretch_tface_uv(void)
{
	EditMesh *em = G.editMesh;
	ParamHandle *handle;
	double lasttime;
	short doit = 1, escape = 0, val, blend = 0;
	unsigned short event = 0;
	short fillholes = G.scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;
	
	if(!EM_texFaceCheck()) return;

	handle = construct_param_handle(em, 1, fillholes, 1);

	lasttime = PIL_check_seconds_timer();

	param_stretch_begin(handle);

	while (doit) {
		param_stretch_iter(handle);

		while (qtest()) {
			event= extern_qread(&val);

			if (val) {
				switch (event) {
					case ESCKEY:
						escape = 1;
					case RETKEY:
					case PADENTER:
						doit = 0;
						break;
					case PADPLUSKEY:
					case WHEELUPMOUSE:
						if (blend < 10) {
							blend++;
							param_stretch_blend(handle, blend*0.1f);
							param_flush(handle);
							lasttime = 0.0f;
						}
						break;
					case PADMINUS:
					case WHEELDOWNMOUSE:
						if (blend > 0) {
							blend--;
							param_stretch_blend(handle, blend*0.1f);
							param_flush(handle);
							lasttime = 0.0f;
						}
						break;
				}
			}
			else if ((event == LEFTMOUSE) || (event == RIGHTMOUSE)) {
					escape = (event == RIGHTMOUSE);
					doit = 0;
			}
		}
		
		if (!doit)
			break;

		if (PIL_check_seconds_timer() - lasttime > 0.5) {
			char str[100];

			param_flush(handle);

			sprintf(str, "Stretch minimize. Blend %.2f.", blend*0.1f);
			headerprint(str);

			lasttime = PIL_check_seconds_timer();
			object_uvs_changed(OBACT);
			if(G.sima->lock) force_draw_plus(SPACE_VIEW3D, 0);
			else force_draw(0);
		}
	}

	if (escape)
		param_flush_restore(handle);
	else
		param_flush(handle);

	param_stretch_end(handle);

	param_delete(handle);

	BIF_undo_push("UV stretch minimize");

	object_uvs_changed(OBACT);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

void pack_charts_tface_uv(void)
{
	EditMesh *em = G.editMesh;
	ParamHandle *handle;
	
	if(!EM_texFaceCheck()) return;

	handle = construct_param_handle(em, 1, 0, 1);
	param_pack(handle);
	param_flush(handle);
	param_delete(handle);

	BIF_undo_push("UV pack islands");

	object_uvs_changed(OBACT);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}


void average_charts_tface_uv(void)
{
	EditMesh *em = G.editMesh;
	ParamHandle *handle;
	
	if(!EM_texFaceCheck()) return;

	handle = construct_param_handle(em, 1, 0, 1);
	param_average(handle);
	param_flush(handle);
	param_delete(handle);

	BIF_undo_push("UV average island scale");

	object_uvs_changed(OBACT);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

/* LSCM live mode */

static ParamHandle *liveHandle = NULL;

void unwrap_lscm_live_begin(void)
{
	EditMesh *em = G.editMesh;
	short abf = G.scene->toolsettings->unwrapper == 1;
	short fillholes = G.scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;

	if(!EM_texFaceCheck()) return;

	liveHandle = construct_param_handle(em, 0, fillholes, 1);

	param_lscm_begin(liveHandle, PARAM_TRUE, abf);
}

void unwrap_lscm_live_re_solve(void)
{
	if (liveHandle) {
		param_lscm_solve(liveHandle);
		param_flush(liveHandle);
	}
}
	
void unwrap_lscm_live_end(short cancel)
{
	if (liveHandle) {
		param_lscm_end(liveHandle);
		if (cancel)
			param_flush_restore(liveHandle);
		param_delete(liveHandle);
		liveHandle = NULL;
	}
}

