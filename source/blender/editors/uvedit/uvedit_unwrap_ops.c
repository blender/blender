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

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"

#include "PIL_time.h"

#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "uvedit_intern.h"
#include "uvedit_parametrizer.h"

/* Parametrizer */
ParamHandle *construct_param_handle(Scene *scene, EditMesh *em, short implicit, short fill, short sel)
{
	ParamHandle *handle;
	EditFace *efa;
	EditEdge *eed;
	EditVert *ev;
	MTFace *tf;
	int a;
	
	handle = param_construct_begin();

	if ((scene->toolsettings->uvcalc_flag & UVCALC_NO_ASPECT_CORRECT)==0) {
		efa = EM_get_actFace(em, 1);

		if (efa) {
			float aspx = 1.0f, aspy= 1.0f;
			// XXX MTFace *tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			// XXX image_final_aspect(tf->tpage, &aspx, &aspy);
		
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
			!(	uvedit_uv_selected(scene, efa, tf, 0) ||
				uvedit_uv_selected(scene, efa, tf, 1) ||
				uvedit_uv_selected(scene, efa, tf, 2) ||
				(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3)) )
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

		select[0] = ((uvedit_uv_selected(scene, efa, tf, 0)) != 0);
		select[1] = ((uvedit_uv_selected(scene, efa, tf, 1)) != 0);
		select[2] = ((uvedit_uv_selected(scene, efa, tf, 2)) != 0);

		if (efa->v4) {
			vkeys[3] = (ParamKey)efa->v4->tmp.l;
			co[3] = efa->v4->co;
			uv[3] = tf->uv[3];
			pin[3] = ((tf->unwrap & TF_PIN4) != 0);
			select[3] = (uvedit_uv_selected(scene, efa, tf, 3) != 0);
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

void unwrap_lscm(Scene *scene, Object *obedit, short seamcut)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	ParamHandle *handle;
	short abf = scene->toolsettings->unwrapper == 1;
	short fillholes = scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;
	
	/* add uvs if there not here */
	if (!ED_uvedit_test(obedit)) {
#if 0
		if (em && em->faces.first)
			EM_add_data_layer(&em->fdata, CD_MTFACE);
		
		if (!ED_uvedit_test(obedit))
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
#endif
	}

	handle = construct_param_handle(scene, em, 0, fillholes, seamcut == 0);

	param_lscm_begin(handle, PARAM_FALSE, abf);
	param_lscm_solve(handle);
	param_lscm_end(handle);
	
	param_pack(handle);

	param_flush(handle);

	param_delete(handle);

	if (!seamcut)
		; // XXX BIF_undo_push("UV unwrap");

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

	// XXX allqueue(REDRAWVIEW3D, 0);
	// XXX allqueue(REDRAWIMAGE, 0);
}

void minimize_stretch_tface_uv(Scene *scene, Object *obedit)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	ParamHandle *handle;
	double lasttime;
	short doit = 1, escape = 0, val, blend = 0;
	unsigned short event = 0;
	short fillholes = scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;
	
	if(!ED_uvedit_test(obedit)) return;

	handle = construct_param_handle(scene, em, 1, fillholes, 1);

	lasttime = PIL_check_seconds_timer();

	param_stretch_begin(handle);

	while (doit) {
		param_stretch_iter(handle);

		while (0) { // XXX qtest()) {
			event= 0; // XXX extern_qread(&val);

			if (val) {
#if 0
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
#endif
			}
			else if (0) { // XXX (event == LEFTMOUSE) || (event == RIGHTMOUSE)) {
					escape = 0; // XXX (event == RIGHTMOUSE);
					doit = 0;
			}
		}
		
		if (!doit)
			break;

		if (PIL_check_seconds_timer() - lasttime > 0.5) {
			char str[100];

			param_flush(handle);

			sprintf(str, "Stretch minimize. Blend %.2f.", blend*0.1f);
			// XXX headerprint(str);

			lasttime = PIL_check_seconds_timer();
			DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
			// XXX if(G.sima->lock) force_draw_plus(SPACE_VIEW3D, 0);
			// XXX else force_draw(0);
		}
	}

	if (escape)
		param_flush_restore(handle);
	else
		param_flush(handle);

	param_stretch_end(handle);

	param_delete(handle);

	// XXX BIF_undo_push("UV stretch minimize");

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

	// XXX allqueue(REDRAWVIEW3D, 0);
	// XXX allqueue(REDRAWIMAGE, 0);
}

void pack_charts_tface_uv(Scene *scene, Object *obedit)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	ParamHandle *handle;
	
	if(!ED_uvedit_test(obedit)) return;

	handle = construct_param_handle(scene, em, 1, 0, 1);
	param_pack(handle);
	param_flush(handle);
	param_delete(handle);

	// XXX BIF_undo_push("UV pack islands");

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

	// XXX allqueue(REDRAWVIEW3D, 0);
	// XXX allqueue(REDRAWIMAGE, 0);
}


void average_charts_tface_uv(Scene *scene, Object *obedit)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	ParamHandle *handle;
	
	if(!ED_uvedit_test(obedit)) return;

	handle = construct_param_handle(scene, em, 1, 0, 1);
	param_average(handle);
	param_flush(handle);
	param_delete(handle);

	// XXX BIF_undo_push("UV average island scale");

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

	// XXX allqueue(REDRAWVIEW3D, 0);
	// XXX allqueue(REDRAWIMAGE, 0);
}

/* LSCM live mode */

static ParamHandle *liveHandle = NULL;

void ED_uvedit_live_unwrap_begin(Scene *scene, Object *obedit)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	short abf = scene->toolsettings->unwrapper == 1;
	short fillholes = scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;

	if(!ED_uvedit_test(obedit)) return;

	liveHandle = construct_param_handle(scene, em, 0, fillholes, 1);

	param_lscm_begin(liveHandle, PARAM_TRUE, abf);
}

void ED_uvedit_live_unwrap_re_solve(void)
{
	if (liveHandle) {
		param_lscm_solve(liveHandle);
		param_flush(liveHandle);
	}
}
	
void ED_uvedit_live_unwrap_end(short cancel)
{
	if (liveHandle) {
		param_lscm_end(liveHandle);
		if (cancel)
			param_flush_restore(liveHandle);
		param_delete(liveHandle);
		liveHandle = NULL;
	}
}

