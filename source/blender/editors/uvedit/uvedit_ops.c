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

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf_types.h" // XXX remove?

#include "ED_mesh.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "uvedit_intern.h"

/* local prototypes */
static void sel_uvco_inside_radius(SpaceImage *sima, Scene *scene, short sel, EditFace *efa, MTFace *tface, int index, float *offset, float *ell, short select_index);
void uvedit_selectionCB(SpaceImage *sima, Scene *scene, ARegion *ar, short selecting, Object *obedit, short *mval, float rad); /* used in edit.c */
void select_edgeloop_tface_uv(bContext *C, Scene *scene, Image *ima, Object *obedit, EditFace *startefa, int starta, int shift, int *flush);
void select_linked_tface_uv(bContext *C, Scene *scene, Image *ima, Object *obedit, int mode);
void uvface_setsel__internal(bContext *C, SpaceImage *sima, Scene *scene, Object *obedit, short select);

/************************* state testing ************************/

int uvedit_test_silent(Object *obedit)
{
	if(obedit->type != OB_MESH)
		return 0;

	return EM_texFaceCheck(((Mesh*)obedit->data)->edit_mesh);
}

int uvedit_test(Object *obedit)
{
	// XXX if(!obedit)
	// XXX	error("Enter Edit Mode to perform this action");

	return uvedit_test_silent(obedit);
}

/************************* assign image ************************/

void ED_uvedit_assign_image(Scene *scene, Object *obedit, Image *ima, Image *previma)
{
	EditMesh *em;
	EditFace *efa;
	MTFace *tf;
	int update;
	
	/* skip assigning these procedural images... */
	if(ima && (ima->type==IMA_TYPE_R_RESULT || ima->type==IMA_TYPE_COMPOSITE))
		return;

	/* verify we have a mesh we can work with */
	if(!obedit || (obedit->type != OB_MESH))
		return;

	em= ((Mesh*)obedit->data)->edit_mesh;
	if(!em || !em->faces.first);
		return;
	
	/* ensure we have a uv layer */
	if(!CustomData_has_layer(&em->fdata, CD_MTFACE)) {
		EM_add_data_layer(em, &em->fdata, CD_MTFACE);
		update= 1;
	}

	/* now assign to all visible faces */
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, previma, efa, tf)) {
			if(ima) {
				tf->tpage= ima;
				tf->mode |= TF_TEX;
				
				if(ima->tpageflag & IMA_TILES) tf->mode |= TF_TILES;
				else tf->mode &= ~TF_TILES;
				
				if(ima->id.us==0) id_us_plus(&ima->id);
				else id_lib_extern(&ima->id);
			}
			else {
				tf->tpage= NULL;
				tf->mode &= ~TF_TEX;
			}

			update = 1;
		}
	}

	/* and update depdency graph */
	if(update) {
		DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
		/* XXX ensure undo, notifiers? */
	}
}

/* dotile -	1, set the tile flag (from the space image)
 * 			2, set the tile index for the faces. */
void ED_uvedit_set_tile(Scene *scene, Object *obedit, Image *ima, int curtile, int dotile)
{
	EditMesh *em;
	EditFace *efa;
	MTFace *tf;
	
	/* verify if we have something to do */
	if(!ima || !uvedit_test_silent(obedit))
		return;
	
	/* skip assigning these procedural images... */
	if(ima->type==IMA_TYPE_R_RESULT || ima->type==IMA_TYPE_COMPOSITE)
		return;
	
	em= ((Mesh*)obedit->data)->edit_mesh;

	for(efa= em->faces.first; efa; efa= efa->next) {
		tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(efa->h==0 && efa->f & SELECT) {
			if(dotile==1) {
				/* set tile flag */
				if(ima->tpageflag & IMA_TILES)
					tf->mode |= TF_TILES;
				else
					tf->mode &= ~TF_TILES;
			}
			else if(dotile==2)
				tf->tile= curtile; /* set tile index */
		}
	}

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	// XXX WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
}

/*********************** connection testing *********************/

static void uvedit_connection_limit(bContext *C, float *limit, float pixellimit)
{
	ImBuf *ibuf= CTX_data_edit_image_buffer(C);
	float width, height;

	if(ibuf && ibuf->x > 0 && ibuf->y > 0) {
		width= ibuf->x;
		height= ibuf->y;
	}
	else {
		width= 256.0f;
		height= 256.0f;
	}

	limit[0]= pixellimit/width;
	limit[1]= pixellimit/height;
}

/*************** visibility and selection utilities **************/

int uvedit_face_visible_nolocal(Scene *scene, EditFace *efa)
{
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION)
		return (efa->h==0);
	else
		return (efa->h==0 && (efa->f & SELECT));
}

int uvedit_face_visible(Scene *scene, Image *ima, EditFace *efa, MTFace *tf)
{
	if(scene->toolsettings->uv_flag & UV_SHOW_SAME_IMAGE)
		return (tf->tpage==ima)? uvedit_face_visible_nolocal(scene, efa): 0;
	else
		return uvedit_face_visible_nolocal(scene, efa);
}

int uvedit_face_selected(Scene *scene, EditFace *efa, MTFace *tf)
{
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION)
		return (efa->f & SELECT);
	else
		return (!(~tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3)) &&(!efa->v4 || tf->flag & TF_SEL4));
}

void uvedit_face_select(Scene *scene, EditFace *efa, MTFace *tf)
{
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION)
		EM_select_face(efa, 1);
	else
		tf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
}

void uvedit_face_deselect(Scene *scene, EditFace *efa, MTFace *tf)
{
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION)
		EM_select_face(efa, 0);
	else
		tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
}

int uvedit_uv_selected(Scene *scene, EditFace *efa, MTFace *tf, int i)
{
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
		if(scene->selectmode == SCE_SELECT_FACE)
			return (efa->f & SELECT);
		else
			return (*(&efa->v1 + i))->f & SELECT;
	}
	else
		return tf->flag & TF_SEL_MASK(i);
}

void uvedit_uv_select(Scene *scene, EditFace *efa, MTFace *tf, int i)
{
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
		if(scene->selectmode == SCE_SELECT_FACE)
			EM_select_face(efa, 1);
		else
			(*(&efa->v1 + i))->f |= SELECT;
	}
	else
		tf->flag |= TF_SEL_MASK(i);
}

void uvedit_uv_deselect(Scene *scene, EditFace *efa, MTFace *tf, int i)
{
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
		if(scene->selectmode == SCE_SELECT_FACE)
			EM_select_face(efa, 0);
		else
			(*(&efa->v1 + i))->f &= ~SELECT;
	}
	else
		tf->flag &= ~TF_SEL_MASK(i);
}

/*********************** geometric utilities ***********************/

void uv_center(float uv[][2], float cent[2], int quad)
{
	if(quad) {
		cent[0] = (uv[0][0] + uv[1][0] + uv[2][0] + uv[3][0]) / 4.0;
		cent[1] = (uv[0][1] + uv[1][1] + uv[2][1] + uv[3][1]) / 4.0;		
	}
	else {
		cent[0] = (uv[0][0] + uv[1][0] + uv[2][0]) / 3.0;
		cent[1] = (uv[0][1] + uv[1][1] + uv[2][1]) / 3.0;		
	}
}

float uv_area(float uv[][2], int quad)
{
	if(quad)
		return AreaF2Dfl(uv[0], uv[1], uv[2]) + AreaF2Dfl(uv[0], uv[2], uv[3]); 
	else
		return AreaF2Dfl(uv[0], uv[1], uv[2]); 
}

void uv_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy)
{
	uv[0][0] = uv_orig[0][0]*aspx;
	uv[0][1] = uv_orig[0][1]*aspy;
	
	uv[1][0] = uv_orig[1][0]*aspx;
	uv[1][1] = uv_orig[1][1]*aspy;
	
	uv[2][0] = uv_orig[2][0]*aspx;
	uv[2][1] = uv_orig[2][1]*aspy;
	
	uv[3][0] = uv_orig[3][0]*aspx;
	uv[3][1] = uv_orig[3][1]*aspy;
}

int ED_uvedit_minmax(Scene *scene, Image *ima, Object *obedit, float *min, float *max)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	MTFace *tf;
	int sel;

	INIT_MINMAX2(min, max);

	sel= 0;
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tf)) {
			if(uvedit_uv_selected(scene, efa, tf, 0))				{ DO_MINMAX2(tf->uv[0], min, max); sel = 1; }
			if(uvedit_uv_selected(scene, efa, tf, 1))				{ DO_MINMAX2(tf->uv[1], min, max); sel = 1; }
			if(uvedit_uv_selected(scene, efa, tf, 2))				{ DO_MINMAX2(tf->uv[2], min, max); sel = 1; }
			if(efa->v4 && (uvedit_uv_selected(scene, efa, tf, 3)))	{ DO_MINMAX2(tf->uv[3], min, max); sel = 1; }
		}
	}

	return sel;
}

int uvedit_center(Scene *scene, Image *ima, Object *obedit, float *cent, int mode)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	MTFace *tf;
	float min[2], max[2];
	int change= 0;
	
	if(mode==0) {
		if(ED_uvedit_minmax(scene, ima, obedit, min, max))
			change = 1;
	}
	else if(mode==1) {
		INIT_MINMAX2(min, max);
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0))				{ DO_MINMAX2(tf->uv[0], min, max);	change= 1;}
				if(uvedit_uv_selected(scene, efa, tf, 1))				{ DO_MINMAX2(tf->uv[1], min, max);	change= 1;}
				if(uvedit_uv_selected(scene, efa, tf, 2))				{ DO_MINMAX2(tf->uv[2], min, max);	change= 1;}
				if(efa->v4 && (uvedit_uv_selected(scene, efa, tf, 3)))	{ DO_MINMAX2(tf->uv[3], min, max);	change= 1;}
			}
		}
	}
	
	if(change) {
		cent[0]= (min[0]+max[0])/2.0;
		cent[1]= (min[1]+max[1])/2.0;

		return 1;
	}

	return 0;
}

/************************** constraints ****************************/

void uvedit_constrain_square(Scene *scene, Image *ima, EditMesh *em)
{
	EditFace *efa;
	MTFace *tf;

	/* if 1 vertex selected: doit (with the selected vertex) */
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->v4) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0)) {
					if(tf->uv[1][0] == tf->uv[2][0] ) {
						tf->uv[1][1]= tf->uv[0][1];
						tf->uv[3][0]= tf->uv[0][0];
					}
					else {	
						tf->uv[1][0]= tf->uv[0][0];
						tf->uv[3][1]= tf->uv[0][1];
					}
					
				}

				if(uvedit_uv_selected(scene, efa, tf, 1)) {
					if(tf->uv[2][1] == tf->uv[3][1] ) {
						tf->uv[2][0]= tf->uv[1][0];
						tf->uv[0][1]= tf->uv[1][1];
					}
					else {
						tf->uv[2][1]= tf->uv[1][1];
						tf->uv[0][0]= tf->uv[1][0];
					}

				}

				if(uvedit_uv_selected(scene, efa, tf, 2)) {
					if(tf->uv[3][0] == tf->uv[0][0] ) {
						tf->uv[3][1]= tf->uv[2][1];
						tf->uv[1][0]= tf->uv[2][0];
					}
					else {
						tf->uv[3][0]= tf->uv[2][0];
						tf->uv[1][1]= tf->uv[2][1];
					}
				}

				if(uvedit_uv_selected(scene, efa, tf, 3)) {
					if(tf->uv[0][1] == tf->uv[1][1] ) {
						tf->uv[0][0]= tf->uv[3][0];
						tf->uv[2][1]= tf->uv[3][1];
					}
					else  {
						tf->uv[0][1]= tf->uv[3][1];
						tf->uv[2][0]= tf->uv[3][0];
					}
				}
			}
		}
	}
}

/* ******************** mirror operator **************** */

/* XXX */
#if 0
	short mode= 0;
	mode= pupmenu("Mirror%t|X Axis%x1|Y Axis%x2|");
#endif

static int mirror_exec(bContext *C, wmOperator *op)
{
	float mat[3][3];
	int axis;
	
	Mat3One(mat);
	axis= RNA_enum_get(op->ptr, "axis");

	if(axis == 'x') {
		/* XXX initTransform(TFM_MIRROR, CTX_NO_PET|CTX_AUTOCONFIRM);
		BIF_setSingleAxisConstraint(mat[0], " on X axis");
		Transform(); */
	}
	else {
		/* XXX initTransform(TFM_MIRROR, CTX_NO_PET|CTX_AUTOCONFIRM);
		BIF_setSingleAxisConstraint(mat[1], " on Y axis");
		Transform(); */
	}

	return OPERATOR_FINISHED;
}

void UV_OT_mirror(wmOperatorType *ot)
{
	static EnumPropertyItem axis_items[] = {
		{'x', "MIRROR_X", "Mirror X", "Mirror UVs over X axis."},
		{'y', "MIRROR_Y", "Mirror Y", "Mirror UVs over Y axis."},
		{0, NULL, NULL, NULL}};

	/* identifiers */
	ot->name= "Mirror";
	ot->idname= "UV_OT_mirror";
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* api callbacks */
	ot->exec= mirror_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_enum(ot->srna, "axis", axis_items, 'x', "Axis", "Axis to mirror UV locations over.");
}

/* ******************** align operator **************** */

/* XXX */
#if 0
void weld_align_menu_tface_uv(bContext *C)
{
	short mode= 0;

	mode= pupmenu("Weld/Align%t|Weld%x1|Align Auto%x2|Align X%x3|Align Y%x4");

	if(mode==-1) return;
	if(mode==1) weld_align_uv(C, 'w');
	else if(mode==2) weld_align_uv(C, 'a');
	else if(mode==3) weld_align_uv(C, 'x');
	else if(mode==4) weld_align_uv(C, 'y');
}
#endif

static void weld_align_uv(bContext *C, int tool)
{
	Scene *scene;
	Object *obedit;
	Image *ima;
	EditMesh *em;
	EditFace *efa;
	MTFace *tf;
	float cent[2], min[2], max[2];
	
	scene= CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);
	em= ((Mesh*)obedit->data)->edit_mesh;
	ima= CTX_data_edit_image(C);

	INIT_MINMAX2(min, max);

	if(tool == 'a') {
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0))
					DO_MINMAX2(tf->uv[0], min, max)
				if(uvedit_uv_selected(scene, efa, tf, 1))
					DO_MINMAX2(tf->uv[1], min, max)
				if(uvedit_uv_selected(scene, efa, tf, 2))
					DO_MINMAX2(tf->uv[2], min, max)
				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3))
					DO_MINMAX2(tf->uv[3], min, max)
			}
		}

		tool= (max[0]-min[0] >= max[1]-min[1])? 'y': 'x';
	}

	uvedit_center(scene, ima, obedit, cent, 0);

	if(tool == 'x' || tool == 'w') {
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0))
					tf->uv[0][0]= cent[0];
				if(uvedit_uv_selected(scene, efa, tf, 1))
					tf->uv[1][0]= cent[0];
				if(uvedit_uv_selected(scene, efa, tf, 2))
					tf->uv[2][0]= cent[0];
				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3))
					tf->uv[3][0]= cent[0];
			}
		}
	}

	if(tool == 'y' || tool == 'w') {
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0))
					tf->uv[0][1]= cent[1];
				if(uvedit_uv_selected(scene, efa, tf, 1))
					tf->uv[1][1]= cent[1];
				if(uvedit_uv_selected(scene, efa, tf, 2))
					tf->uv[2][1]= cent[1];
				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3))
					tf->uv[3][1]= cent[1];
			}
		}
	}

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit); // XXX
}

static int align_exec(bContext *C, wmOperator *op)
{
	weld_align_uv(C, RNA_enum_get(op->ptr, "axis"));

	return OPERATOR_FINISHED;
}

void UV_OT_align(wmOperatorType *ot)
{
	static EnumPropertyItem axis_items[] = {
		{'a', "ALIGN_AUTO", "Align Auto", "Automatically choose the axis on which there is most alignment already."},
		{'x', "ALIGN_X", "Align X", "Align UVs on X axis."},
		{'y', "ALIGN_Y", "Align Y", "Align UVs on Y axis."},
		{0, NULL, NULL, NULL}};

	/* identifiers */
	ot->name= "Align";
	ot->idname= "UV_OT_align";
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* api callbacks */
	ot->exec= align_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_enum(ot->srna, "axis", axis_items, 'a', "Axis", "Axis to align UV locations on.");
}

/* ******************** weld operator **************** */

static int weld_exec(bContext *C, wmOperator *op)
{
	weld_align_uv(C, 'w');

	return OPERATOR_FINISHED;
}

void UV_OT_weld(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Weld";
	ot->idname= "UV_OT_weld";
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* api callbacks */
	ot->exec= weld_exec;
	ot->poll= ED_operator_uvedit;
}

/* ******************** stitch operator **************** */

/* just for averaging UVs */
typedef struct UVVertAverage {
	float uv[2];
	int count;
} UVVertAverage;

static int stitch_exec(bContext *C, wmOperator *op)
{
	Scene *scene;
	Object *obedit;
	EditMesh *em;
	EditFace *efa;
	EditVert *eve;
	Image *ima;
	MTFace *tf;
	
	scene= CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);
	em= ((Mesh*)obedit->data)->edit_mesh;
	ima= CTX_data_edit_image(C);
	
	if(RNA_boolean_get(op->ptr, "use_limit")) {
		UvVertMap *vmap;
		UvMapVert *vlist, *iterv;
		float newuv[2], limit[2], pixels;
		int a, vtot;

		pixels= RNA_float_get(op->ptr, "limit");
		uvedit_connection_limit(C, limit, pixels);

		EM_init_index_arrays(em, 0, 0, 1);
		vmap= EM_make_uv_vert_map(em, 1, 0, limit);

		if(vmap == NULL)
			return OPERATOR_CANCELLED;

		for(a=0, eve= em->verts.first; eve; a++, eve= eve->next) {
			vlist= EM_get_uv_map_vert(vmap, a);

			while(vlist) {
				newuv[0]= 0; newuv[1]= 0;
				vtot= 0;

				for(iterv=vlist; iterv; iterv=iterv->next) {
					if((iterv != vlist) && iterv->separate)
						break;

					efa = EM_get_face_for_index(iterv->f);
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					
					if(uvedit_uv_selected(scene, efa, tf, iterv->tfindex)) {
						newuv[0] += tf->uv[iterv->tfindex][0];
						newuv[1] += tf->uv[iterv->tfindex][1];
						vtot++;
					}
				}

				if(vtot > 1) {
					newuv[0] /= vtot; newuv[1] /= vtot;

					for(iterv=vlist; iterv; iterv=iterv->next) {
						if((iterv != vlist) && iterv->separate)
							break;

						efa = EM_get_face_for_index(iterv->f);
						tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

						if(uvedit_uv_selected(scene, efa, tf, iterv->tfindex)) {
							tf->uv[iterv->tfindex][0]= newuv[0];
							tf->uv[iterv->tfindex][1]= newuv[1];
						}
					}
				}

				vlist= iterv;
			}
		}

		EM_free_uv_vert_map(vmap);
		EM_free_index_arrays();
	}
	else {
		UVVertAverage *uv_average, *uvav;
		int count;

		// index and count verts
		for(count=0, eve=em->verts.first; eve; count++, eve= eve->next)
			eve->tmp.l = count;
		
		uv_average= MEM_callocN(sizeof(UVVertAverage)*count, "Stitch");
		
		// gather uv averages per vert
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0)) {
					uvav = uv_average + efa->v1->tmp.l;
					uvav->count++;
					uvav->uv[0] += tf->uv[0][0];
					uvav->uv[1] += tf->uv[0][1];
				}

				if(uvedit_uv_selected(scene, efa, tf, 1)) {
					uvav = uv_average + efa->v2->tmp.l;
					uvav->count++;
					uvav->uv[0] += tf->uv[1][0];
					uvav->uv[1] += tf->uv[1][1];
				}

				if(uvedit_uv_selected(scene, efa, tf, 2)) {
					uvav = uv_average + efa->v3->tmp.l;
					uvav->count++;
					uvav->uv[0] += tf->uv[2][0];
					uvav->uv[1] += tf->uv[2][1];
				}

				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3)) {
					uvav = uv_average + efa->v4->tmp.l;
					uvav->count++;
					uvav->uv[0] += tf->uv[3][0];
					uvav->uv[1] += tf->uv[3][1];
				}
			}
		}
		
		// apply uv welding
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0)) {
					uvav = uv_average + efa->v1->tmp.l;
					tf->uv[0][0] = uvav->uv[0]/uvav->count;
					tf->uv[0][1] = uvav->uv[1]/uvav->count;
				}

				if(uvedit_uv_selected(scene, efa, tf, 1)) {
					uvav = uv_average + efa->v2->tmp.l;
					tf->uv[1][0] = uvav->uv[0]/uvav->count;
					tf->uv[1][1] = uvav->uv[1]/uvav->count;
				}

				if(uvedit_uv_selected(scene, efa, tf, 2)) {
					uvav = uv_average + efa->v3->tmp.l;
					tf->uv[2][0] = uvav->uv[0]/uvav->count;
					tf->uv[2][1] = uvav->uv[1]/uvav->count;
				}

				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3)) {
					uvav = uv_average + efa->v4->tmp.l;
					tf->uv[3][0] = uvav->uv[0]/uvav->count;
					tf->uv[3][1] = uvav->uv[1]/uvav->count;
				}
			}
		}

		MEM_freeN(uv_average);
	}

	// XXX if(sima->flag & SI_BE_SQUARE)
	// XXX	uvedit_constrain_square(scene, sima->image, em);

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit); // XXX

	return OPERATOR_FINISHED;
}

void UV_OT_stitch(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Stitch";
	ot->idname= "UV_OT_stitch";
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* api callbacks */
	ot->exec= stitch_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "use_limit", 1, "Use Limit", "Stitch UVs within a specified limit distance.");
	RNA_def_float(ot->srna, "limit", 20.0, 0.0f, FLT_MAX, "Limit", "Limit distance in image pixels.", -FLT_MAX, FLT_MAX);
}

/* ******************** (de)select all operator **************** */

static int select_inverse_exec(bContext *C, wmOperator *op)
{
	Scene *scene;
	Object *obedit;
	EditMesh *em;
	EditFace *efa;
	Image *ima;
	MTFace *tf;
	
	scene= CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);
	em= ((Mesh*)obedit->data)->edit_mesh;
	ima= CTX_data_edit_image(C);

	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
		// XXX selectswap_mesh();
	}
	else {
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				tf->flag ^= TF_SEL1;
				tf->flag ^= TF_SEL2;
				tf->flag ^= TF_SEL3;
				if(efa->v4) tf->flag ^= TF_SEL4;
			}
		}
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void UV_OT_select_inverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Invert";
	ot->idname= "UV_OT_select_inverse";
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* api callbacks */
	ot->exec= select_inverse_exec;
	ot->poll= ED_operator_uvedit;
}

/* ******************** (de)select all operator **************** */

static int de_select_all_exec(bContext *C, wmOperator *op)
{
	Scene *scene;
	Object *obedit;
	EditMesh *em;
	EditFace *efa;
	Image *ima;
	MTFace *tf;
	int sel;
	
	scene= CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);
	em= ((Mesh*)obedit->data)->edit_mesh;
	ima= CTX_data_edit_image(C);
	
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
		// XXX deselectall_mesh();
	}
	else {
		sel= 0;

		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(tf->flag & (TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4)) {
					sel= 1;
					break;
				}
			}
		}
	
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(efa->v4) {
					if(sel) tf->flag &= ~(TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
					else tf->flag |= (TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
				}
				else {
					if(sel) tf->flag &= ~(TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
					else tf->flag |= (TF_SEL1+TF_SEL2+TF_SEL3);
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void UV_OT_de_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select/Deselect All";
	ot->idname= "UV_OT_de_select_all";
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* api callbacks */
	ot->exec= de_select_all_exec;
	ot->poll= ED_operator_uvedit;
}

/* ******************** mouse select operator **************** */

static int msel_hit(float *limit, unsigned int *hitarray, unsigned int vertexid, float **uv, float *uv2, int sticky)
{
	int i;
	for(i=0; i< 4; i++) {
		if(hitarray[i] == vertexid) {
			if(sticky == 2) {
				if(fabs(uv[i][0]-uv2[0]) < limit[0] &&
			    fabs(uv[i][1]-uv2[1]) < limit[1])
					return 1;
			}
			else return 1;
		}
	}
	return 0;
}

static void find_nearest_uv_edge(Scene *scene, Image *ima, Object *obedit, MTFace **nearesttf, EditFace **nearestefa, int *nearestedge)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	MTFace *tf;
	EditFace *efa;
	float mvalf[2], v1[2], v2[2];
	int i, nverts, mindist, dist, uval1[2], uval2[2];
	short mval[2];

	mval[0]= mval[1]= 0; // XXX getmouseco_areawin(mval);
	mvalf[0]= mval[0];
	mvalf[1]= mval[1];

	mindist= 0x7FFFFFF;
	*nearesttf= NULL;
	*nearestefa= NULL;
	*nearestedge= 0;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, ima, efa, tf)) {
			nverts= efa->v4? 4: 3;
			for(i=0; i<nverts; i++) {
				uval1[0]= uval1[1]= 0; // XXX uvco_to_areaco_noclip(tf->uv[i], uval1);
				uval2[0]= uval2[1]= 0; // XXX uvco_to_areaco_noclip(tf->uv[(i+1)%nverts], uval2);

				v1[0]= uval1[0];
				v1[1]= uval1[1];
				v2[0]= uval2[0];
				v2[1]= uval2[1];

				dist= PdistVL2Dfl(mvalf, v1, v2);
				if(dist < mindist) {
					*nearesttf= tf;
					*nearestefa= efa;
					*nearestedge= i;
					mindist= dist;
				}
			}
		}
	}
}

static void find_nearest_tface(Scene *scene, Image *ima, Object *obedit, MTFace **nearesttf, EditFace **nearestefa)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	MTFace *tf;
	EditFace *efa;
	int i, nverts, mindist, dist, fcenter[2], uval[2];
	short mval[2];

	mval[0]= mval[1]= 0; // XXX getmouseco_areawin(mval);	

	mindist= 0x7FFFFFF;
	*nearesttf= NULL;
	*nearestefa= NULL;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tf)) {
			fcenter[0]= fcenter[1]= 0;
			nverts= efa->v4? 4: 3;
			for(i=0; i<nverts; i++) {
				uval[0]= uval[0]= 0; // XXX uvco_to_areaco_noclip(tf->uv[i], uval);
				fcenter[0] += uval[0];
				fcenter[1] += uval[1];
			}

			fcenter[0] /= nverts;
			fcenter[1] /= nverts;

			dist= abs(mval[0]- fcenter[0])+ abs(mval[1]- fcenter[1]);
			if(dist < mindist) {
				*nearesttf= tf;
				*nearestefa= efa;
				mindist= dist;
			}
		}
	}
}

static int nearest_uv_between(MTFace *tf, int nverts, int id, short *mval, int *uval)
{
	float m[3], v1[3], v2[3], c1, c2;
	int id1, id2;

	id1= (id+nverts-1)%nverts;
	id2= (id+nverts+1)%nverts;

	m[0] = (float)(mval[0]-uval[0]);
	m[1] = (float)(mval[1]-uval[1]);
	Vec2Subf(v1, tf->uv[id1], tf->uv[id]);
	Vec2Subf(v2, tf->uv[id2], tf->uv[id]);

	/* m and v2 on same side of v-v1? */
	c1= v1[0]*m[1] - v1[1]*m[0];
	c2= v1[0]*v2[1] - v1[1]*v2[0];

	if(c1*c2 < 0.0f)
		return 0;

	/* m and v1 on same side of v-v2? */
	c1= v2[0]*m[1] - v2[1]*m[0];
	c2= v2[0]*v1[1] - v2[1]*v1[0];

	return (c1*c2 >= 0.0f);
}

void find_nearest_uv(Scene *scene, Image *ima, Object *obedit, MTFace **nearesttf, EditFace **nearestefa, unsigned int *nearestv, int *nearestuv)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	MTFace *tf;
	int i, nverts, mindist, dist, uval[2];
	short mval[2];

	mval[0]= mval[1]= 0; // XXX getmouseco_areawin(mval);	

	mindist= 0x7FFFFFF;
	if(nearesttf) *nearesttf= NULL;
	if(nearestefa) *nearestefa= NULL;
	
	if(nearestv) {
		EditVert *ev;
		for(i=0, ev=em->verts.first; ev; ev = ev->next, i++)
			ev->tmp.l = i;
	}
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tf)) {
			nverts= efa->v4? 4: 3;
			for(i=0; i<nverts; i++) {
				uval[0]= uval[1]= 0; // XXX uvco_to_areaco_noclip(tf->uv[i], uval);
				dist= abs(mval[0]-uval[0]) + abs(mval[1]-uval[1]);

				if(uvedit_uv_selected(scene, efa, tf, i))
					dist += 5;

				if(dist<=mindist) {
					if(dist==mindist)
						if(!nearest_uv_between(tf, nverts, i, mval, uval))
							continue;

					mindist= dist;
					*nearestuv= i;
					
					if(nearesttf)		*nearesttf= tf;
					if(nearestefa)		*nearestefa= efa;
					if(nearestv) {
						if(i==0) *nearestv=  efa->v1->tmp.l;
						else if(i==1) *nearestv=  efa->v2->tmp.l;
						else if(i==2) *nearestv=  efa->v3->tmp.l;
						else *nearestv=  efa->v4->tmp.l;
					}
				}
			}
		}
	}
}

void mouse_select_sima(bContext *C, SpaceImage *sima, Scene *scene, Image *ima, Object *obedit)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	MTFace *tf, *nearesttf;
	EditFace *nearestefa=NULL;
	int a, selectsticky, edgeloop, actface, nearestuv, nearestedge, i, shift, island=0;
	char sticky= 0;
	int flush = 0; /* 0 == dont flush, 1 == sel, -1 == desel;  only use when selection sync is enabled */
	unsigned int hitv[4], nearestv;
	float *hituv[4], limit[2];
	
	if(!uvedit_test(obedit)) return;

	uvedit_connection_limit(C, limit, 0.05);
	
	edgeloop= 0; // XXX G.qual & LR_ALTKEY;
	shift= 0; // XXX G.qual & LR_SHIFTKEY;
	
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
		/* copy from mesh */
		if(scene->selectmode == SCE_SELECT_FACE) {
			actface= 1;
			sticky= 0;
		}
		else {
			actface= scene->selectmode & SCE_SELECT_FACE;
			sticky= 2;
		}
	}
	else {
		/* normal operation */
		actface= scene->toolsettings->uv_selectmode == UV_SELECT_FACE;
		island= scene->toolsettings->uv_selectmode == UV_SELECT_ISLAND;
		
		switch(sima->sticky) {
		case SI_STICKY_LOC:
			sticky=2;
			break;
		case SI_STICKY_DISABLE:
			sticky=0;
			break;
		case SI_STICKY_VERTEX:
			if(0) // XXX G.qual & LR_CTRLKEY)
				sticky=0;
			else
				sticky=1;
			break;
		}
	}

	if(edgeloop) {
		find_nearest_uv_edge(scene, ima, obedit, &nearesttf, &nearestefa, &nearestedge);
		if(nearesttf==NULL)
			return;

		select_edgeloop_tface_uv(C, scene, ima, obedit, nearestefa, nearestedge, shift, &flush);
	}
	else if(actface) {
		find_nearest_tface(scene, ima, obedit, &nearesttf, &nearestefa);
		if(nearesttf==NULL)
			return;
		
		EM_set_actFace(em, nearestefa);

		for(i=0; i<4; i++)
			hituv[i]= nearesttf->uv[i];

		hitv[0]= nearestefa->v1->tmp.l;
		hitv[1]= nearestefa->v2->tmp.l;
		hitv[2]= nearestefa->v3->tmp.l;
		
		if(nearestefa->v4)	hitv[3]= nearestefa->v4->tmp.l;
		else				hitv[3]= 0xFFFFFFFF;
	}
	else if(island) {

	}
	else {
		find_nearest_uv(scene, ima, obedit, &nearesttf, &nearestefa, &nearestv, &nearestuv);
		if(nearesttf==NULL)
			return;

		if(sticky) {
			for(i=0; i<4; i++)
				hitv[i]= 0xFFFFFFFF;
			hitv[nearestuv]= nearestv;
			hituv[nearestuv]= nearesttf->uv[nearestuv];
		}
	}

	if(island) {
		if(shift) select_linked_tface_uv(C, scene, ima, obedit, 1);
		else select_linked_tface_uv(C, scene, ima, obedit, 0);
	}
	else if(!edgeloop && shift) {
		/* (de)select face */
		if(actface) {
			if(uvedit_face_selected(scene, nearestefa, nearesttf)) {
				uvedit_face_deselect(scene, nearestefa, nearesttf);
				selectsticky= 0;
			}
			else {
				uvedit_face_select(scene, nearestefa, nearesttf);
				selectsticky= 1;
			}
			flush = -1;
		}
		/* (de)select uv node */
		else {
			if(uvedit_uv_selected(scene, nearestefa, nearesttf, nearestuv)) {
				uvedit_uv_deselect(scene, nearestefa, nearesttf, nearestuv);
				selectsticky= 0;
			}
			else {
				uvedit_uv_select(scene, nearestefa, nearesttf, nearestuv);
				selectsticky= 1;
			}
			flush = 1;
		}

		/* (de)select sticky uv nodes */
		if(sticky || actface) {
			EditVert *ev;
			
			for(a=0, ev=em->verts.first; ev; ev = ev->next, a++)
				ev->tmp.l = a;
			
			/* deselect */
			if(selectsticky==0) {
				for(efa= em->faces.first; efa; efa= efa->next) {
					tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					if(uvedit_face_visible(scene, ima, efa, tf)) {
						/*if(nearesttf && tf!=nearesttf) tf->flag &=~ TF_ACTIVE;*/ /* TODO - deal with editmesh active face */
						if(!sticky) continue;
	
						if(msel_hit(limit, hitv, efa->v1->tmp.l, hituv, tf->uv[0], sticky))
							uvedit_uv_deselect(scene, efa, tf, 0);
						if(msel_hit(limit, hitv, efa->v2->tmp.l, hituv, tf->uv[1], sticky))
							uvedit_uv_deselect(scene, efa, tf, 1);
						if(msel_hit(limit, hitv, efa->v3->tmp.l, hituv, tf->uv[2], sticky))
							uvedit_uv_deselect(scene, efa, tf, 2);
						if(efa->v4)
							if(msel_hit(limit, hitv, efa->v4->tmp.l, hituv, tf->uv[3], sticky))
								uvedit_uv_deselect(scene, efa, tf, 3);
					}
				}
				flush = -1;
			}
			/* select */
			else {
				for(efa= em->faces.first; efa; efa= efa->next) {
					tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					if(uvedit_face_visible(scene, ima, efa, tf)) {
						if(!sticky) continue;
						if(msel_hit(limit, hitv, efa->v1->tmp.l, hituv, tf->uv[0], sticky))
							uvedit_uv_select(scene, efa, tf, 0);
						if(msel_hit(limit, hitv, efa->v2->tmp.l, hituv, tf->uv[1], sticky))
							uvedit_uv_select(scene, efa, tf, 1);
						if(msel_hit(limit, hitv, efa->v3->tmp.l, hituv, tf->uv[2], sticky))
							uvedit_uv_select(scene, efa, tf, 2);
						if(efa->v4)
							if(msel_hit(limit, hitv, efa->v4->tmp.l, hituv, tf->uv[3], sticky))
								uvedit_uv_select(scene, efa, tf, 3);
					}
				}
				
				if(actface)
					EM_set_actFace(em, nearestefa);
				
				flush = 1;
			}			
		}
	}
	else if(!edgeloop) {
		/* select face and deselect other faces */ 
		if(actface) {
			for(efa= em->faces.first; efa; efa= efa->next) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				uvedit_face_deselect(scene, efa, tf);
			}
			if(nearesttf) {
				uvedit_face_select(scene, nearestefa, nearesttf);
				EM_set_actFace(em, nearestefa);
			}
				
		}

		/* deselect uvs, and select sticky uvs */
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(!actface) uvedit_face_deselect(scene, efa, tf);
				if(!sticky) continue;

				if(msel_hit(limit, hitv, efa->v1->tmp.l, hituv, tf->uv[0], sticky))
					uvedit_uv_select(scene, efa, tf, 0);
				if(msel_hit(limit, hitv, efa->v2->tmp.l, hituv, tf->uv[1], sticky))
					uvedit_uv_select(scene, efa, tf, 1);
				if(msel_hit(limit, hitv, efa->v3->tmp.l, hituv, tf->uv[2], sticky))
					uvedit_uv_select(scene, efa, tf, 2);
				if(efa->v4)
					if(msel_hit(limit, hitv, efa->v4->tmp.l, hituv, tf->uv[3], sticky))
						uvedit_uv_select(scene, efa, tf, 3);
				flush= 1;
			}
		}
		
		if(!actface) {
			uvedit_uv_select(scene, nearestefa, nearesttf, nearestuv);
			flush= 1;
		}
	}
	
	// XXX force_draw(1);
	
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
		/* flush for mesh selection */
		if(scene->selectmode != SCE_SELECT_FACE) {
			if(flush==1)		EM_select_flush(em);
			else if(flush==-1)	EM_deselect_flush(em);
		}
		// XXX allqueue(REDRAWVIEW3D, 0); /* mesh selection has changed */
	}
	
	// XXX BIF_undo_push("Select UV");
	// XXX rightmouse_transform();
}

void borderselect_sima(bContext *C, SpaceImage *sima, Scene *scene, Image *ima, Object *obedit, short whichuvs)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	MTFace *tface;
	rcti rect;
	rctf rectf;
	int val, ok = 1;
	short mval[2], select;

	if(!uvedit_test(obedit)) return;

	val= 0; // XXX get_border(&rect, 3);
	select = 0; // XXX (val==LEFTMOUSE) ? 1 : 0; 
	
	if(val) {
		mval[0]= rect.xmin;
		mval[1]= rect.ymin;
		// XXX areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax;
		// XXX areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
		
		if(0) { // XXX draw_uvs_face_check() && whichuvs != UV_SELECT_PINNED) {
			float cent[2];
			ok = 0;
			for(efa= em->faces.first; efa; efa= efa->next) {
				/* assume not touched */
				efa->tmp.l = 0;
				tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if(uvedit_face_visible(scene, ima, efa, tface)) {
					uv_center(tface->uv, cent, efa->v4 != NULL);
					if(BLI_in_rctf(&rectf, cent[0], cent[1])) {
						efa->tmp.l = ok = 1;
					}
				}
			}
			/* (de)selects all tagged faces and deals with sticky modes */
			if(ok)
				uvface_setsel__internal(C, sima, scene, obedit, select);
		}
		else {
			for(efa= em->faces.first; efa; efa= efa->next) {
				tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if(uvedit_face_visible(scene, ima, efa, tface)) {
					if(whichuvs == UV_SELECT_ALL || (scene->toolsettings->uv_flag & UV_SYNC_SELECTION) ) {
						/* UV_SYNC_SELECTION - cant do pinned selection */
						if(BLI_in_rctf(&rectf, tface->uv[0][0], tface->uv[0][1])) {
							if(select)	uvedit_uv_select(scene, efa, tface, 0);
							else		uvedit_uv_deselect(scene, efa, tface, 0);
						}
						if(BLI_in_rctf(&rectf, tface->uv[1][0], tface->uv[1][1])) {
							if(select)	uvedit_uv_select(scene, efa, tface, 1);
							else		uvedit_uv_deselect(scene, efa, tface, 1);
						}
						if(BLI_in_rctf(&rectf, tface->uv[2][0], tface->uv[2][1])) {
							if(select)	uvedit_uv_select(scene, efa, tface, 2);
							else		uvedit_uv_deselect(scene, efa, tface, 2);
						}
						if(efa->v4 && BLI_in_rctf(&rectf, tface->uv[3][0], tface->uv[3][1])) {
							if(select)	uvedit_uv_select(scene, efa, tface, 3);
							else		uvedit_uv_deselect(scene, efa, tface, 3);
						}
					} else if(whichuvs == UV_SELECT_PINNED) {
						if((tface->unwrap & TF_PIN1) && 
							BLI_in_rctf(&rectf, tface->uv[0][0], tface->uv[0][1])) {
							
							if(select)	uvedit_uv_select(scene, efa, tface, 0);
							else		uvedit_uv_deselect(scene, efa, tface, 0);
						}
						if((tface->unwrap & TF_PIN2) && 
							BLI_in_rctf(&rectf, tface->uv[1][0], tface->uv[1][1])) {
							
							if(select)	uvedit_uv_select(scene, efa, tface, 1);
							else		uvedit_uv_deselect(scene, efa, tface, 1);
						}
						if((tface->unwrap & TF_PIN3) && 
							BLI_in_rctf(&rectf, tface->uv[2][0], tface->uv[2][1])) {
							
							if(select)	uvedit_uv_select(scene, efa, tface, 2);
							else		uvedit_uv_deselect(scene, efa, tface, 2);
						}
						if((efa->v4) && (tface->unwrap & TF_PIN4) && BLI_in_rctf(&rectf, tface->uv[3][0], tface->uv[3][1])) {
							if(select)	uvedit_uv_select(scene, efa, tface, 3);
							else		uvedit_uv_deselect(scene, efa, tface, 3);
						}
					}
				}
			}
		}
		if(ok) {
			/* make sure newly selected vert selection is updated*/
			if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
				if(scene->selectmode != SCE_SELECT_FACE) {
					if(select)	EM_select_flush(em);
					else		EM_deselect_flush(em);
				}
			}
			// XXX allqueue(REDRAWVIEW3D, 0); /* mesh selection has changed */
			
			// XXX BIF_undo_push("Border select UV");
			// XXX scrarea_queue_winredraw(curarea);
		}
	}
}

int snap_uv_sel_to_curs(Scene *scene, Image *ima, Object *obedit, View2D *v2d)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	MTFace *tface;
	short change = 0;

	for(efa= em->faces.first; efa; efa= efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(uvedit_uv_selected(scene, efa, tface, 0))		VECCOPY2D(tface->uv[0], v2d->cursor);
			if(uvedit_uv_selected(scene, efa, tface, 1))		VECCOPY2D(tface->uv[1], v2d->cursor);
			if(uvedit_uv_selected(scene, efa, tface, 2))		VECCOPY2D(tface->uv[2], v2d->cursor);
			if(efa->v4)
				if(uvedit_uv_selected(scene, efa, tface, 3))	VECCOPY2D(tface->uv[3], v2d->cursor);
			change = 1;
		}
	}
	return change;
}

int snap_uv_sel_to_adj_unsel(Scene *scene, Image *ima, Object *obedit)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	EditVert *eve;
	MTFace *tface;
	short change = 0;
	int count = 0;
	float *coords;
	short *usercount, users;
	
	/* set all verts to -1 : an unused index*/
	for(eve= em->verts.first; eve; eve= eve->next)
		eve->tmp.l=-1;
	
	/* index every vert that has a selected UV using it, but only once so as to
	 * get unique indicies and to count how much to malloc */
	for(efa= em->faces.first; efa; efa= efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(uvedit_uv_selected(scene, efa, tface, 0) && efa->v1->tmp.l==-1)		efa->v1->tmp.l= count++;
			if(uvedit_uv_selected(scene, efa, tface, 1) && efa->v2->tmp.l==-1)		efa->v2->tmp.l= count++;
			if(uvedit_uv_selected(scene, efa, tface, 2) && efa->v3->tmp.l==-1)		efa->v3->tmp.l= count++;
			if(efa->v4)
				if(uvedit_uv_selected(scene, efa, tface, 3) && efa->v4->tmp.l==-1)	efa->v4->tmp.l= count++;
			change = 1;
			
			/* optional speedup */
			efa->tmp.p = tface;
		}
		else
			efa->tmp.p = NULL;
	}
	
	coords = MEM_callocN(sizeof(float)*count*2, "snap to adjacent coords");
	usercount = MEM_callocN(sizeof(short)*count, "snap to adjacent counts");
	
	/* add all UV coords from visible, unselected UV coords as well as counting them to average later */
	for(efa= em->faces.first; efa; efa= efa->next) {
//		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
//		if(uvedit_face_visible(scene, ima, efa, tface)) {
		if((tface=(MTFace *)efa->tmp.p)) {
			
			/* is this an unselected UV we can snap to? */
			if(efa->v1->tmp.l >= 0 && (!uvedit_uv_selected(scene, efa, tface, 0))) {
				coords[efa->v1->tmp.l*2] +=		tface->uv[0][0];
				coords[(efa->v1->tmp.l*2)+1] +=	tface->uv[0][1];
				usercount[efa->v1->tmp.l]++;
				change = 1;
			}
			if(efa->v2->tmp.l >= 0 && (!uvedit_uv_selected(scene, efa, tface, 1))) {
				coords[efa->v2->tmp.l*2] +=		tface->uv[1][0];
				coords[(efa->v2->tmp.l*2)+1] +=	tface->uv[1][1];
				usercount[efa->v2->tmp.l]++;
				change = 1;
			}
			if(efa->v3->tmp.l >= 0 && (!uvedit_uv_selected(scene, efa, tface, 2))) {
				coords[efa->v3->tmp.l*2] +=		tface->uv[2][0];
				coords[(efa->v3->tmp.l*2)+1] +=	tface->uv[2][1];
				usercount[efa->v3->tmp.l]++;
				change = 1;
			}
			
			if(efa->v4) {
				if(efa->v4->tmp.l >= 0 && (!uvedit_uv_selected(scene, efa, tface, 3))) {
					coords[efa->v4->tmp.l*2] +=		tface->uv[3][0];
					coords[(efa->v4->tmp.l*2)+1] +=	tface->uv[3][1];
					usercount[efa->v4->tmp.l]++;
					change = 1;
				}
			}
		}
	}
	
	/* no other verts selected, bail out */
	if(!change) {
		MEM_freeN(coords);
		MEM_freeN(usercount);
		return change;
	}
	
	/* copy the averaged unselected UVs back to the selected UVs */
	for(efa= em->faces.first; efa; efa= efa->next) {
//		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
//		if(uvedit_face_visible(scene, ima, efa, tface)) {
		if((tface=(MTFace *)efa->tmp.p)) {
			
			if(	uvedit_uv_selected(scene, efa, tface, 0) &&
					efa->v1->tmp.l >= 0 &&
					(users = usercount[efa->v1->tmp.l])
			) {
				tface->uv[0][0] = coords[efa->v1->tmp.l*2]		/ users;
				tface->uv[0][1] = coords[(efa->v1->tmp.l*2)+1]	/ users;
			}

			if(	uvedit_uv_selected(scene, efa, tface, 1) &&
					efa->v2->tmp.l >= 0 &&
					(users = usercount[efa->v2->tmp.l])
			) {
				tface->uv[1][0] = coords[efa->v2->tmp.l*2]		/ users;
				tface->uv[1][1] = coords[(efa->v2->tmp.l*2)+1]	/ users;
			}
			
			if(	uvedit_uv_selected(scene, efa, tface, 2) &&
					efa->v3->tmp.l >= 0 &&
					(users = usercount[efa->v3->tmp.l])
			) {
				tface->uv[2][0] = coords[efa->v3->tmp.l*2]		/ users;
				tface->uv[2][1] = coords[(efa->v3->tmp.l*2)+1]	/ users;
			}
			
			if(efa->v4) {
				if(	uvedit_uv_selected(scene, efa, tface, 3) &&
						efa->v4->tmp.l >= 0 &&
						(users = usercount[efa->v4->tmp.l])
				) {
					tface->uv[3][0] = coords[efa->v4->tmp.l*2]		/ users;
					tface->uv[3][1] = coords[(efa->v4->tmp.l*2)+1]	/ users;
				}
			}
		}
	}
	
	MEM_freeN(coords);
	MEM_freeN(usercount);
	return change;
}

void snap_coord_to_pixel(float *uvco, float w, float h)
{
	uvco[0] = ((float) ((int)((uvco[0]*w) + 0.5))) / w;  
	uvco[1] = ((float) ((int)((uvco[1]*h) + 0.5))) / h;  
}

int snap_uv_sel_to_pixels(SpaceImage *sima, Scene *scene, Object *obedit) /* warning, sanity checks must alredy be done */
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	Image *ima= sima->image;
	EditFace *efa;
	MTFace *tface;
	int width= 0, height= 0;
	float w, h;
	short change = 0;

	// XXX get_space_image_size(sima, &width, &height);
	w = (float)width;
	h = (float)height;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(uvedit_uv_selected(scene, efa, tface, 0)) snap_coord_to_pixel(tface->uv[0], w, h);
			if(uvedit_uv_selected(scene, efa, tface, 1)) snap_coord_to_pixel(tface->uv[1], w, h);
			if(uvedit_uv_selected(scene, efa, tface, 2)) snap_coord_to_pixel(tface->uv[2], w, h);
			if(efa->v4)
				if(uvedit_uv_selected(scene, efa, tface, 3)) snap_coord_to_pixel(tface->uv[3], w, h);
			change = 1;
		}
	}
	return change;
}

void snap_uv_curs_to_pixels(SpaceImage *sima, View2D *v2d)
{
	int width= 0, height= 0;

	// XXX get_space_image_size(sima, &width, &height);
	snap_coord_to_pixel(v2d->cursor, width, height);
}

int snap_uv_curs_to_sel(Scene *scene, Image *ima, Object *obedit, View2D *v2d)
{
	if(!uvedit_test(obedit)) return 0;
	return uvedit_center(scene, ima, obedit, v2d->cursor, 0);
}

void snap_menu_sima(SpaceImage *sima, Scene *scene, Object *obedit, View2D *v2d)
{
	short event;

	if(!uvedit_test(obedit) || !v2d) return; /* !G.v2d should never happen */
	
	event = 0; // XXX pupmenu("Snap %t|Selection -> Pixels%x1|Selection -> Cursor%x2|Selection -> Adjacent Unselected%x3|Cursor -> Selection%x4|Cursor -> Pixel%x5");
	switch (event) {
		case 1:
		    if(snap_uv_sel_to_pixels(sima, scene, obedit)) {
		    	// XXX BIF_undo_push("Snap UV Selection to Pixels");
				DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
		    }
		    break;
		case 2:
		    if(snap_uv_sel_to_curs(scene, sima->image, obedit, v2d)) {
		    	// XXX BIF_undo_push("Snap UV Selection to Cursor");
				DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
		    }
		    break;
		case 3:
		    if(snap_uv_sel_to_adj_unsel(scene, sima->image, obedit)) {
		    	// XXX BIF_undo_push("Snap UV Selection to Cursor");
				DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
		    }
		    break;
		case 4:
		    if(snap_uv_curs_to_sel(scene, sima->image, obedit, v2d))
		    	// XXX allqueue(REDRAWIMAGE, 0);
		    break;
		case 5:
		    snap_uv_curs_to_pixels(sima, v2d);
			// XXX scrarea_queue_winredraw(curarea);
		    break;
	}
}


/** This is an ugly function to set the Tface selection flags depending
  * on whether its UV coordinates are inside the normalized 
  * area with radius rad and offset offset. These coordinates must be
  * normalized to 1.0 
  * Just for readability...
  */

static void sel_uvco_inside_radius(SpaceImage *sima, Scene *scene, short sel, EditFace *efa, MTFace *tface, int index, float *offset, float *ell, short select_index)
{
	// normalized ellipse: ell[0] = scaleX,
	//                        [1] = scaleY

	float *uv = tface->uv[index];
	float x, y, r2;

	x = (uv[0] - offset[0]) * ell[0];
	y = (uv[1] - offset[1]) * ell[1];

	r2 = x * x + y * y;
	if(r2 < 1.0) {
		if(sel == 0 /* XXX LEFTMOUSE */)	uvedit_uv_select(scene, efa, tface, select_index);
		else					uvedit_uv_deselect(scene, efa, tface, select_index);
	}
}

// see below:
/** gets image dimensions of the 2D view 'v' */
static void getSpaceImageDimension(SpaceImage *sima, ARegion *ar, float *xy)
{
	float zoomx= 0, zoomy= 0;
	int width= 0, height= 0;

	// XXX get_space_image_size(sima, &width, &height);
	// XXX get_space_image_zoom(sima, ar, &zoomx, &zoomy);

	xy[0]= width*zoomx;
	xy[1]= height*zoomy;
}

/** Callback function called by circle_selectCB to enable 
  * brush select in UV editor.
  */

void uvedit_selectionCB(SpaceImage *sima, Scene *scene, ARegion *ar, short selecting, Object *obedit, short *mval, float rad) 
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	float offset[2];
	MTFace *tface;
	float ellipse[2]; // we need to deal with ellipses, as
	                  // non square textures require for circle
					  // selection. this ellipse is normalized; r = 1.0

	getSpaceImageDimension(sima, ar, ellipse);
	ellipse[0] /= rad;
	ellipse[1] /= rad;

	// XXX areamouseco_to_ipoco(G.v2d, mval, &offset[0], &offset[1]);
	
	if(selecting) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			sel_uvco_inside_radius(sima, scene, selecting, efa, tface, 0, offset, ellipse, 0);
			sel_uvco_inside_radius(sima, scene, selecting, efa, tface, 1, offset, ellipse, 1);
			sel_uvco_inside_radius(sima, scene, selecting, efa, tface, 2, offset, ellipse, 2);
			if(efa->v4)
				sel_uvco_inside_radius(sima, scene, selecting, efa, tface, 3, offset, ellipse, 3);
		}

		/* XXX */
#if 0
		if(G.f & G_DRAWFACES) { /* full redraw only if necessary */
			draw_sel_circle(0, 0, 0, 0, 0); /* signal */
			force_draw(0);
		}
		else { /* force_draw() is no good here... */
			glDrawBuffer(GL_FRONT);
			draw_uvs_sima();
			bglFlush();
			glDrawBuffer(GL_BACK);
		}
#endif	
		
		if(selecting == 0 /* XXX LEFTMOUSE */)	EM_select_flush(em);
		else						EM_deselect_flush(em);
		
		if(sima->lock && (scene->toolsettings->uv_flag & UV_SYNC_SELECTION))
			; // XXX force_draw_plus(SPACE_VIEW3D, 0);
	}
}

void select_linked_tface_uv(bContext *C, Scene *scene, Image *ima, Object *obedit, int mode) /* TODO */
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa, *nearestefa=NULL;
	MTFace *tf, *nearesttf=NULL;
	UvVertMap *vmap;
	UvMapVert *vlist, *iterv, *startv;
	unsigned int *stack, stacksize= 0, nearestv;
	char *flag;
	int a, nearestuv, i, nverts, j;
	float limit[2];

	if(!uvedit_test(obedit)) return;

	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
		// XXX error("Can't select linked when Sync Mesh Selection is enabled");
		return;
	}
	
	if(mode == 2) {
		nearesttf= NULL;
		nearestuv= 0;
	}
	if(mode!=2) {
		find_nearest_uv(scene, ima, obedit, &nearesttf, &nearestefa, &nearestv, &nearestuv);
		if(nearesttf==NULL)
			return;
	}

	uvedit_connection_limit(C, limit, 0.05);
	vmap= EM_make_uv_vert_map(em, 1, 1, limit);
	if(vmap == NULL)
		return;

	stack= MEM_mallocN(sizeof(*stack)* BLI_countlist(&em->faces), "UvLinkStack");
	flag= MEM_callocN(sizeof(*flag)*BLI_countlist(&em->faces), "UvLinkFlag");

	if(mode == 2) {
		for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)) {
					stack[stacksize]= a;
					stacksize++;
					flag[a]= 1;
				}
			}
		}
	}
	else {
		for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(tf == nearesttf) {
				stack[stacksize]= a;
				stacksize++;
				flag[a]= 1;
				break;
			}
		}
	}

	while(stacksize > 0) {
		stacksize--;
		a= stack[stacksize];
		
		for(j=0, efa= em->faces.first; efa; efa= efa->next, j++) {
			if(j==a) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				break;
			}
		}

		nverts= efa->v4? 4: 3;

		for(i=0; i<nverts; i++) {
			/* make_uv_vert_map_EM sets verts tmp.l to the indicies */
			vlist= EM_get_uv_map_vert(vmap, (*(&efa->v1 + i))->tmp.l);
			
			startv= vlist;

			for(iterv=vlist; iterv; iterv=iterv->next) {
				if(iterv->separate)
					startv= iterv;
				if(iterv->f == a)
					break;
			}

			for(iterv=startv; iterv; iterv=iterv->next) {
				if((startv != iterv) && (iterv->separate))
					break;
				else if(!flag[iterv->f]) {
					flag[iterv->f]= 1;
					stack[stacksize]= iterv->f;;
					stacksize++;
				}
			}
		}
	}

	if(mode==0 || mode==2) {
		for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(flag[a])
				tf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
			else
				tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
		}
	}
	else if(mode==1) {
		for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			if(flag[a]) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if(efa->v4) {
					if((tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)))
						break;
				}
				else if(tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
					break;
			}
		}

		if(efa) {
			for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
				if(flag[a]) {
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
			}
		}
		else {
			for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
				if(flag[a]) {
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					tf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
			}
		}
	}
	
	MEM_freeN(stack);
	MEM_freeN(flag);
	EM_free_uv_vert_map(vmap);

	// XXX BIF_undo_push("Select linked UV");
	// XXX scrarea_queue_winredraw(curarea);
}

void unlink_selection(Scene *scene, Image *ima, Object *obedit)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	MTFace *tface;

	if(!uvedit_test(obedit)) return;

	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
		; // XXX error("Can't select unlinked when Sync Mesh Selection is enabled");
		return;
	}
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(efa->v4) {
				if(~tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4))
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
			}
			else {
				if(~tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3);
			}
		}
	}
	
	// XXX BIF_undo_push("Unlink UV selection");
	// XXX scrarea_queue_winredraw(curarea);
}

/* this function sets the selection on tagged faces
 * This is needed because setting the selection on a face is done in
 * a number of places but it also needs to respect the sticky modes
 * for the UV verts - dealing with the sticky modes is best done in a seperate function
 * 
 * de-selects faces that have been tagged on efa->tmp.l 
 */
void uvface_setsel__internal(bContext *C, SpaceImage *sima, Scene *scene, Object *obedit, short select)
{
	
	/* All functions calling this should call
	 * draw_uvs_face_check() 
	 */
	
		
	/* selecting UV Faces with some modes requires us to change 
	 * the selection in other faces (depending on the stickt mode)
	 * 
	 * This only needs to be done when the Mesh is not used for selection
	 * (So for sticky modes - vertex or location based)
	 * */
	
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	MTFace *tf;
	int nverts, i;
	
	if((scene->toolsettings->uv_flag & UV_SYNC_SELECTION)==0 && sima->sticky == SI_STICKY_VERTEX) {
		/* tag all verts as untouched,
		 * then touch the ones that have a face center in the loop
		 * and select all MTFace UV's that use a touched vert */
		
		EditVert *eve;
		
		for(eve= em->verts.first; eve; eve= eve->next)
			eve->tmp.l = 0;
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->tmp.l) {
				if(efa->v4)
					efa->v1->tmp.l=	efa->v2->tmp.l= efa->v3->tmp.l= efa->v4->tmp.l=1;
				else
					efa->v1->tmp.l= efa->v2->tmp.l= efa->v3->tmp.l= 1;
			}
		}
		/* now select tagged verts */
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);		
			nverts= efa->v4? 4: 3;
			for(i=0; i<nverts; i++) {
				if((*(&efa->v1 + i))->tmp.l) {
					if(select)
						uvedit_uv_select(scene, efa, tf, i);
					else
						uvedit_uv_deselect(scene, efa, tf, i);
				}
			}
		}
	} else if((scene->toolsettings->uv_flag & UV_SYNC_SELECTION)==0 && sima->sticky == SI_STICKY_LOC) {
		EditFace *efa_vlist;
		MTFace *tf_vlist;
		UvMapVert *start_vlist=NULL, *vlist_iter;
		struct UvVertMap *vmap;
		float limit[2];
		int efa_index;
		//EditVert *eve; /* removed vert counting for now */ 
		//int a;
		
		uvedit_connection_limit(C, limit, 0.05);
		
		EM_init_index_arrays(em, 0, 0, 1);
		vmap= EM_make_uv_vert_map(em, 0, 0, limit);
		
		/* verts are numbered above in make_uv_vert_map_EM, make sure this stays true! */
		/*for(a=0, eve= em->verts.first; eve; a++, eve= eve->next)
			eve->tmp.l = a; */
		
		if(vmap == NULL)
			return;
		
		for(efa_index=0, efa= em->faces.first; efa; efa_index++, efa= efa->next) {
			if(efa->tmp.l) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				nverts= efa->v4? 4: 3;

				for(i=0; i<nverts; i++) {
					if(select)
						uvedit_uv_select(scene, efa, tf, i);
					else
						uvedit_uv_deselect(scene, efa, tf, i);
					
					vlist_iter= EM_get_uv_map_vert(vmap, (*(&efa->v1 + i))->tmp.l);
					
					while (vlist_iter) {
						if(vlist_iter->separate)
							start_vlist = vlist_iter;
						
						if(efa_index == vlist_iter->f)
							break;

						vlist_iter = vlist_iter->next;
					}
				
					vlist_iter = start_vlist;
					while (vlist_iter) {
						
						if(vlist_iter != start_vlist && vlist_iter->separate)
							break;
						
						if(efa_index != vlist_iter->f) {
							efa_vlist = EM_get_face_for_index(vlist_iter->f);
							tf_vlist = CustomData_em_get(&em->fdata, efa_vlist->data, CD_MTFACE);
							
							if(select)
								uvedit_uv_select(scene, efa_vlist, tf_vlist, vlist_iter->tfindex);
							else
								uvedit_uv_deselect(scene, efa_vlist, tf_vlist, vlist_iter->tfindex);
						}
						vlist_iter = vlist_iter->next;
					}
				}
			}
		}
		EM_free_index_arrays();
		EM_free_uv_vert_map(vmap);
		
	}
	else { /* SI_STICKY_DISABLE or scene->toolsettings->uv_flag & UV_SYNC_SELECTION */
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->tmp.l) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if(select)
					uvedit_face_select(scene, efa, tf);
				else
					uvedit_face_deselect(scene, efa, tf);
			}
		}
	}
}

void pin_tface_uv(Scene *scene, Image *ima, Object *obedit, int mode)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	MTFace *tface;
	
	if(!uvedit_test(obedit)) return;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(mode ==1) {
				if(uvedit_uv_selected(scene, efa, tface, 0)) tface->unwrap |= TF_PIN1;
				if(uvedit_uv_selected(scene, efa, tface, 1)) tface->unwrap |= TF_PIN2;
				if(uvedit_uv_selected(scene, efa, tface, 2)) tface->unwrap |= TF_PIN3;
				if(efa->v4)
					if(uvedit_uv_selected(scene, efa, tface, 3)) tface->unwrap |= TF_PIN4;
			}
			else if(mode ==0) {
				if(uvedit_uv_selected(scene, efa, tface, 0)) tface->unwrap &= ~TF_PIN1;
				if(uvedit_uv_selected(scene, efa, tface, 1)) tface->unwrap &= ~TF_PIN2;
				if(uvedit_uv_selected(scene, efa, tface, 2)) tface->unwrap &= ~TF_PIN3;
				if(efa->v4)
					if(uvedit_uv_selected(scene, efa, tface, 3)) tface->unwrap &= ~TF_PIN4;
			}
		}
	}
	
	// XXX BIF_undo_push("Pin UV");
	// XXX scrarea_queue_winredraw(curarea);
}

void select_pinned_tface_uv(Scene *scene, Image *ima, Object *obedit)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditFace *efa;
	MTFace *tface;
	
	if(!uvedit_test(obedit)) return;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(tface->unwrap & TF_PIN1) uvedit_uv_select(scene, efa, tface, 0);
			if(tface->unwrap & TF_PIN2) uvedit_uv_select(scene, efa, tface, 1);
			if(tface->unwrap & TF_PIN3) uvedit_uv_select(scene, efa, tface, 2);
			if(efa->v4) {
				if(tface->unwrap & TF_PIN4) uvedit_uv_select(scene, efa, tface, 3);
			}
			
		}
	}
	
	if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
		// XXX allqueue(REDRAWVIEW3D, 0); /* mesh selection has changed */
	}
	
	// XXX BIF_undo_push("Select Pinned UVs");
	// XXX scrarea_queue_winredraw(curarea);
}

/* UV edge loop select, follows same rules as editmesh */

static void uv_vertex_loop_flag(UvMapVert *first)
{
	UvMapVert *iterv;
	int count= 0;

	for(iterv=first; iterv; iterv=iterv->next) {
		if(iterv->separate && iterv!=first)
			break;

		count++;
	}
	
	if(count < 5)
		first->flag= 1;
}

static UvMapVert *uv_vertex_map_get(UvVertMap *vmap, EditFace *efa, int a)
{
	UvMapVert *iterv, *first;
	
	first= EM_get_uv_map_vert(vmap, (*(&efa->v1 + a))->tmp.l);

	for(iterv=first; iterv; iterv=iterv->next) {
		if(iterv->separate)
			first= iterv;
		if(iterv->f == efa->tmp.l)
			return first;
	}
	
	return NULL;
}

static int uv_edge_tag_faces(UvMapVert *first1, UvMapVert *first2, int *totface)
{
	UvMapVert *iterv1, *iterv2;
	EditFace *efa;
	int tot = 0;

	/* count number of faces this edge has */
	for(iterv1=first1; iterv1; iterv1=iterv1->next) {
		if(iterv1->separate && iterv1 != first1)
			break;

		for(iterv2=first2; iterv2; iterv2=iterv2->next) {
			if(iterv2->separate && iterv2 != first2)
				break;

			if(iterv1->f == iterv2->f) {
				/* if face already tagged, don't do this edge */
				efa= EM_get_face_for_index(iterv1->f);
				if(efa->f1)
					return 0;

				tot++;
				break;
			}
		}
	}

	if(*totface == 0) /* start edge */
		*totface= tot;
	else if(tot != *totface) /* check for same number of faces as start edge */
		return 0;

	/* tag the faces */
	for(iterv1=first1; iterv1; iterv1=iterv1->next) {
		if(iterv1->separate && iterv1 != first1)
			break;

		for(iterv2=first2; iterv2; iterv2=iterv2->next) {
			if(iterv2->separate && iterv2 != first2)
				break;

			if(iterv1->f == iterv2->f) {
				efa= EM_get_face_for_index(iterv1->f);
				efa->f1= 1;
				break;
			}
		}
	}

	return 1;
}

void select_edgeloop_tface_uv(bContext *C, Scene *scene, Image *ima, Object *obedit, EditFace *startefa, int starta, int shift, int *flush)
{
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	EditVert *eve;
	EditFace *efa;
	MTFace *tface;
	UvVertMap *vmap;
	UvMapVert *iterv1, *iterv2;
	float limit[2];
	int a, count, looking, nverts, starttotface, select;
	
	if(!uvedit_test(obedit)) return;

	/* setup */
	EM_init_index_arrays(em, 0, 0, 1);

	uvedit_connection_limit(C, limit, 0.05);
	vmap= EM_make_uv_vert_map(em, 0, 0, limit);

	for(count=0, eve=em->verts.first; eve; count++, eve= eve->next)
		eve->tmp.l = count;

	for(count=0, efa= em->faces.first; efa; count++, efa= efa->next) {
		if(!shift) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			uvedit_face_deselect(scene, efa, tface);
		}

		efa->tmp.l= count;
		efa->f1= 0;
	}
	
	/* set flags for first face and verts */
	nverts= (startefa->v4)? 4: 3;
	iterv1= uv_vertex_map_get(vmap, startefa, starta);
	iterv2= uv_vertex_map_get(vmap, startefa, (starta+1)%nverts);
	uv_vertex_loop_flag(iterv1);
	uv_vertex_loop_flag(iterv2);

	starttotface= 0;
	uv_edge_tag_faces(iterv1, iterv2, &starttotface);

	/* sorry, first edge isnt even ok */
	if(iterv1->flag==0 && iterv2->flag==0) looking= 0;
	else looking= 1;

	/* iterate */
	while(looking) {
		looking= 0;

		/* find correct valence edges which are not tagged yet, but connect to tagged one */
		for(efa= em->faces.first; efa; efa=efa->next) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(!efa->f1 && uvedit_face_visible(scene, ima, efa, tface)) {
				nverts= (efa->v4)? 4: 3;
				for(a=0; a<nverts; a++) {
					/* check face not hidden and not tagged */
					iterv1= uv_vertex_map_get(vmap, efa, a);
					iterv2= uv_vertex_map_get(vmap, efa, (a+1)%nverts);

					/* check if vertex is tagged and has right valence */
					if(iterv1->flag || iterv2->flag) {
						if(uv_edge_tag_faces(iterv1, iterv2, &starttotface)) {
							looking= 1;
							efa->f1= 1;

							uv_vertex_loop_flag(iterv1);
							uv_vertex_loop_flag(iterv2);
							break;
						}
					}
				}
			}
		}
	}

	/* do the actual select/deselect */
	nverts= (startefa->v4)? 4: 3;
	iterv1= uv_vertex_map_get(vmap, startefa, starta);
	iterv2= uv_vertex_map_get(vmap, startefa, (starta+1)%nverts);
	iterv1->flag= 1;
	iterv2->flag= 1;

	if(shift) {
		tface= CustomData_em_get(&em->fdata, startefa->data, CD_MTFACE);
		if(uvedit_uv_selected(scene, startefa, tface, starta) && uvedit_uv_selected(scene, startefa, tface, starta))
			select= 0;
		else
			select= 1;
	}
	else
		select= 1;
	
	if(select) *flush= 1;
	else *flush= -1;

	for(efa= em->faces.first; efa; efa=efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		nverts= (efa->v4)? 4: 3;
		for(a=0; a<nverts; a++) {
			iterv1= uv_vertex_map_get(vmap, efa, a);

			if(iterv1->flag) {
				if(select) uvedit_uv_select(scene, efa, tface, a);
				else uvedit_uv_deselect(scene, efa, tface, a);
			}
		}
	}

	/* cleanup */
	EM_free_uv_vert_map(vmap);
	EM_free_index_arrays();
}

/* ************************** registration **********************************/

void ED_operatortypes_uvedit(void)
{
	WM_operatortype_append(UV_OT_de_select_all);
	WM_operatortype_append(UV_OT_select_inverse);

	WM_operatortype_append(UV_OT_align);
	WM_operatortype_append(UV_OT_mirror);
	WM_operatortype_append(UV_OT_stitch);
	WM_operatortype_append(UV_OT_weld);
}

void ED_keymap_uvedit(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "UVEdit", 0, 0);
	
	WM_keymap_add_item(keymap, "UV_OT_de_select_all", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_select_inverse", IKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "UV_OT_stitch", VKEY, KM_PRESS, 0, 0);
}

