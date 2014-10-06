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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/mesh_data.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_path_util.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "mesh_intern.h"  /* own include */


static CustomData *mesh_customdata_get_type(Mesh *me, const char htype, int *r_tot)
{
	CustomData *data;
	BMesh *bm = (me->edit_btmesh) ? me->edit_btmesh->bm : NULL;
	int tot;

	/* this  */
	switch (htype) {
		case BM_VERT:
			if (bm) {
				data = &bm->vdata;
				tot  = bm->totvert;
			}
			else {
				data = &me->vdata;
				tot  = me->totvert;
			}
			break;
		case BM_EDGE:
			if (bm) {
				data = &bm->edata;
				tot  = bm->totedge;
			}
			else {
				data = &me->edata;
				tot  = me->totedge;
			}
			break;
		case BM_LOOP:
			if (bm) {
				data = &bm->ldata;
				tot  = bm->totloop;
			}
			else {
				data = &me->ldata;
				tot  = me->totloop;
			}
			break;
		case BM_FACE:
			if (bm) {
				data = &bm->pdata;
				tot  = bm->totface;
			}
			else {
				data = &me->pdata;
				tot  = me->totpoly;
			}
			break;
		default:
			BLI_assert(0);
			tot = 0;
			data = NULL;
			break;
	}

	*r_tot = tot;
	return data;
}

#define GET_CD_DATA(me, data) (me->edit_btmesh ? &me->edit_btmesh->bm->data : &me->data)
static void delete_customdata_layer(Mesh *me, CustomDataLayer *layer)
{
	const int type = layer->type;
	CustomData *data;
	int layer_index, tot, n;

	data = mesh_customdata_get_type(me, (ELEM(type, CD_MLOOPUV, CD_MLOOPCOL)) ? BM_LOOP : BM_FACE, &tot);
	layer_index = CustomData_get_layer_index(data, type);
	n = (layer - &data->layers[layer_index]);
	BLI_assert(n >= 0 && (n + layer_index) < data->totlayer);

	if (me->edit_btmesh) {
		BM_data_layer_free_n(me->edit_btmesh->bm, data, type, n);
	}
	else {
		CustomData_free_layer(data, type, tot, layer_index + n);
		BKE_mesh_update_customdata_pointers(me, true);
	}
}

static void mesh_uv_reset_array(float **fuv, const int len)
{
	if (len == 3) {
		fuv[0][0] = 0.0;
		fuv[0][1] = 0.0;

		fuv[1][0] = 1.0;
		fuv[1][1] = 0.0;

		fuv[2][0] = 1.0;
		fuv[2][1] = 1.0;
	}
	else if (len == 4) {
		fuv[0][0] = 0.0;
		fuv[0][1] = 0.0;

		fuv[1][0] = 1.0;
		fuv[1][1] = 0.0;

		fuv[2][0] = 1.0;
		fuv[2][1] = 1.0;

		fuv[3][0] = 0.0;
		fuv[3][1] = 1.0;
		/*make sure we ignore 2-sided faces*/
	}
	else if (len > 2) {
		float fac = 0.0f, dfac = 1.0f / (float)len;
		int i;

		dfac *= (float)M_PI * 2.0f;

		for (i = 0; i < len; i++) {
			fuv[i][0] = 0.5f * sinf(fac) + 0.5f;
			fuv[i][1] = 0.5f * cosf(fac) + 0.5f;

			fac += dfac;
		}
	}
}

static void mesh_uv_reset_bmface(BMFace *f, const int cd_loop_uv_offset)
{
	float **fuv = BLI_array_alloca(fuv, f->len);
	BMIter liter;
	BMLoop *l;
	int i;

	BM_ITER_ELEM_INDEX (l, &liter, f, BM_LOOPS_OF_FACE, i) {
		fuv[i] = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset))->uv;
	}

	mesh_uv_reset_array(fuv, f->len);
}

static void mesh_uv_reset_mface(MPoly *mp, MLoopUV *mloopuv)
{
	float **fuv = BLI_array_alloca(fuv, mp->totloop);
	int i;

	for (i = 0; i < mp->totloop; i++) {
		fuv[i] = mloopuv[mp->loopstart + i].uv;
	}

	mesh_uv_reset_array(fuv, mp->totloop);
}

/* without bContext, called in uvedit */
void ED_mesh_uv_loop_reset_ex(struct Mesh *me, const int layernum)
{
	BMEditMesh *em = me->edit_btmesh;

	if (em) {
		/* Collect BMesh UVs */
		const int cd_loop_uv_offset = CustomData_get_n_offset(&em->bm->ldata, CD_MLOOPUV, layernum);

		BMFace *efa;
		BMIter iter;

		BLI_assert(cd_loop_uv_offset != -1);

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
				continue;

			mesh_uv_reset_bmface(efa, cd_loop_uv_offset);
		}
	}
	else {
		/* Collect Mesh UVs */
		MLoopUV *mloopuv;
		int i;

		BLI_assert(CustomData_has_layer(&me->ldata, CD_MLOOPUV));
		mloopuv = CustomData_get_layer_n(&me->ldata, CD_MLOOPUV, layernum);

		for (i = 0; i < me->totpoly; i++) {
			mesh_uv_reset_mface(&me->mpoly[i], mloopuv);
		}
	}

	DAG_id_tag_update(&me->id, 0);
}

void ED_mesh_uv_loop_reset(struct bContext *C, struct Mesh *me)
{
	/* could be ldata or pdata */
	CustomData *pdata = GET_CD_DATA(me, pdata);
	const int layernum = CustomData_get_active_layer_index(pdata, CD_MTEXPOLY);
	ED_mesh_uv_loop_reset_ex(me, layernum);
	
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);
}

/* note: keep in sync with ED_mesh_color_add */
int ED_mesh_uv_texture_add(Mesh *me, const char *name, const bool active_set)
{
	BMEditMesh *em;
	int layernum_dst;

	bool is_init = false;

	if (me->edit_btmesh) {
		em = me->edit_btmesh;

		layernum_dst = CustomData_number_of_layers(&em->bm->pdata, CD_MTEXPOLY);
		if (layernum_dst >= MAX_MTFACE)
			return -1;

		/* CD_MTEXPOLY */
		BM_data_layer_add_named(em->bm, &em->bm->pdata, CD_MTEXPOLY, name);
		/* copy data from active UV */
		if (layernum_dst) {
			const int layernum_src = CustomData_get_active_layer(&em->bm->pdata, CD_MTEXPOLY);
			BM_data_layer_copy(em->bm, &em->bm->pdata, CD_MTEXPOLY, layernum_src, layernum_dst);
		}
		if (active_set || layernum_dst == 0) {
			CustomData_set_layer_active(&em->bm->pdata, CD_MTEXPOLY, layernum_dst);
		}

		/* CD_MLOOPUV */
		BM_data_layer_add_named(em->bm, &em->bm->ldata, CD_MLOOPUV, name);
		/* copy data from active UV */
		if (layernum_dst) {
			const int layernum_src = CustomData_get_active_layer(&em->bm->ldata, CD_MLOOPUV);
			BM_data_layer_copy(em->bm, &em->bm->ldata, CD_MLOOPUV, layernum_src, layernum_dst);

			is_init = true;
		}
		if (active_set || layernum_dst == 0) {
			CustomData_set_layer_active(&em->bm->ldata, CD_MLOOPUV, layernum_dst);
		}
	}
	else {
		layernum_dst = CustomData_number_of_layers(&me->pdata, CD_MTEXPOLY);
		if (layernum_dst >= MAX_MTFACE)
			return -1;

		if (me->mtpoly) {
			CustomData_add_layer_named(&me->pdata, CD_MTEXPOLY, CD_DUPLICATE, me->mtpoly, me->totpoly, name);
			CustomData_add_layer_named(&me->ldata, CD_MLOOPUV, CD_DUPLICATE, me->mloopuv, me->totloop, name);
			CustomData_add_layer_named(&me->fdata, CD_MTFACE, CD_DUPLICATE, me->mtface, me->totface, name);
			is_init = true;
		}
		else {
			CustomData_add_layer_named(&me->pdata, CD_MTEXPOLY, CD_DEFAULT, NULL, me->totpoly, name);
			CustomData_add_layer_named(&me->ldata, CD_MLOOPUV, CD_DEFAULT, NULL, me->totloop, name);
			CustomData_add_layer_named(&me->fdata, CD_MTFACE, CD_DEFAULT, NULL, me->totface, name);
		}
		
		if (active_set || layernum_dst == 0) {
			CustomData_set_layer_active(&me->pdata, CD_MTEXPOLY, layernum_dst);
			CustomData_set_layer_active(&me->ldata, CD_MLOOPUV, layernum_dst);

			CustomData_set_layer_active(&me->fdata, CD_MTFACE, layernum_dst);
		}

		BKE_mesh_update_customdata_pointers(me, true);
	}

	/* don't overwrite our copied coords */
	if (is_init == false) {
		ED_mesh_uv_loop_reset_ex(me, layernum_dst);
	}

	DAG_id_tag_update(&me->id, 0);
	WM_main_add_notifier(NC_GEOM | ND_DATA, me);

	return layernum_dst;
}

bool ED_mesh_uv_texture_remove_index(Mesh *me, const int n)
{
	CustomData *pdata = GET_CD_DATA(me, pdata), *ldata = GET_CD_DATA(me, ldata);
	CustomDataLayer *cdlp, *cdlu;
	int index;

	index = CustomData_get_layer_index_n(pdata, CD_MTEXPOLY, n);
	cdlp = (index == -1) ? NULL : &pdata->layers[index];

	index = CustomData_get_layer_index_n(ldata, CD_MLOOPUV, n);
	cdlu = (index == -1) ? NULL : &ldata->layers[index];

	if (!cdlp || !cdlu)
		return false;

	delete_customdata_layer(me, cdlp);
	delete_customdata_layer(me, cdlu);

	DAG_id_tag_update(&me->id, 0);
	WM_main_add_notifier(NC_GEOM | ND_DATA, me);

	return true;
}
bool ED_mesh_uv_texture_remove_active(Mesh *me)
{
	/* texpoly/uv are assumed to be in sync */
	CustomData *pdata = GET_CD_DATA(me, pdata);
	const int n = CustomData_get_active_layer(pdata, CD_MTEXPOLY);

	/* double check active layers align! */
#ifdef DEBUG
	CustomData *ldata = GET_CD_DATA(me, ldata);
	BLI_assert(CustomData_get_active_layer(ldata, CD_MLOOPUV) == n);
#endif

	if (n != -1) {
		return ED_mesh_uv_texture_remove_index(me, n);
	}
	else {
		return false;
	}
}
bool ED_mesh_uv_texture_remove_named(Mesh *me, const char *name)
{
	/* texpoly/uv are assumed to be in sync */
	CustomData *pdata = GET_CD_DATA(me, pdata);
	const int n = CustomData_get_named_layer(pdata, CD_MTEXPOLY, name);
	if (n != -1) {
		return ED_mesh_uv_texture_remove_index(me, n);
	}
	else {
		return false;
	}
}

/* note: keep in sync with ED_mesh_uv_texture_add */
int ED_mesh_color_add(Mesh *me, const char *name, const bool active_set)
{
	BMEditMesh *em;
	int layernum;

	if (me->edit_btmesh) {
		em = me->edit_btmesh;

		layernum = CustomData_number_of_layers(&em->bm->ldata, CD_MLOOPCOL);
		if (layernum >= MAX_MCOL) {
			return -1;
		}

		/* CD_MLOOPCOL */
		BM_data_layer_add_named(em->bm, &em->bm->ldata, CD_MLOOPCOL, name);
		/* copy data from active vertex color layer */
		if (layernum) {
			const int layernum_dst = CustomData_get_active_layer(&em->bm->ldata, CD_MLOOPCOL);
			BM_data_layer_copy(em->bm, &em->bm->ldata, CD_MLOOPCOL, layernum, layernum_dst);
		}
		if (active_set || layernum == 0) {
			CustomData_set_layer_active(&em->bm->ldata, CD_MLOOPCOL, layernum);
		}
	}
	else {
		layernum = CustomData_number_of_layers(&me->ldata, CD_MLOOPCOL);
		if (layernum >= MAX_MCOL) {
			return -1;
		}

		if (me->mloopcol) {
			CustomData_add_layer_named(&me->ldata, CD_MLOOPCOL, CD_DUPLICATE, me->mloopcol, me->totloop, name);
			CustomData_add_layer_named(&me->fdata, CD_MCOL, CD_DUPLICATE, me->mcol, me->totface, name);
		}
		else {
			CustomData_add_layer_named(&me->ldata, CD_MLOOPCOL, CD_DEFAULT, NULL, me->totloop, name);
			CustomData_add_layer_named(&me->fdata, CD_MCOL, CD_DEFAULT, NULL, me->totface, name);
		}

		if (active_set || layernum == 0) {
			CustomData_set_layer_active(&me->ldata, CD_MLOOPCOL, layernum);
			CustomData_set_layer_active(&me->fdata, CD_MCOL, layernum);
		}

		BKE_mesh_update_customdata_pointers(me, true);
	}

	DAG_id_tag_update(&me->id, 0);
	WM_main_add_notifier(NC_GEOM | ND_DATA, me);

	return layernum;
}

bool ED_mesh_color_remove_index(Mesh *me, const int n)
{
	CustomData *ldata = GET_CD_DATA(me, ldata);
	CustomDataLayer *cdl;
	int index;

	index = CustomData_get_layer_index_n(ldata, CD_MLOOPCOL, n);
	cdl = (index == -1) ? NULL : &ldata->layers[index];

	if (!cdl)
		return false;

	delete_customdata_layer(me, cdl);
	DAG_id_tag_update(&me->id, 0);
	WM_main_add_notifier(NC_GEOM | ND_DATA, me);

	return true;
}
bool ED_mesh_color_remove_active(Mesh *me)
{
	CustomData *ldata = GET_CD_DATA(me, ldata);
	const int n = CustomData_get_active_layer(ldata, CD_MLOOPCOL);
	if (n != -1) {
		return ED_mesh_color_remove_index(me, n);
	}
	else {
		return false;
	}
}
bool ED_mesh_color_remove_named(Mesh *me, const char *name)
{
	CustomData *ldata = GET_CD_DATA(me, ldata);
	const int n = CustomData_get_named_layer(ldata, CD_MLOOPCOL, name);
	if (n != -1) {
		return ED_mesh_color_remove_index(me, n);
	}
	else {
		return false;
	}
}

/*********************** UV texture operators ************************/

static int layers_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;
	return (ob && !ob->id.lib && ob->type == OB_MESH && data && !data->lib);
}

static int mesh_uv_texture_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Mesh *me = ob->data;

	if (ED_mesh_uv_texture_add(me, NULL, true) == -1)
		return OPERATOR_CANCELLED;

	if (ob->mode & OB_MODE_TEXTURE_PAINT) {
		Scene *scene = CTX_data_scene(C);
		BKE_paint_proj_mesh_data_check(scene, ob, NULL, NULL, NULL, NULL);
		WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);
	}
	
	return OPERATOR_FINISHED;
}

void MESH_OT_uv_texture_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add UV Map";
	ot->description = "Add UV Map";
	ot->idname = "MESH_OT_uv_texture_add";
	
	/* api callbacks */
	ot->poll = layers_poll;
	ot->exec = mesh_uv_texture_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int drop_named_image_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	Base *base;
	Image *ima = NULL;
	Mesh *me;
	Object *obedit;
	int exitmode = 0;
	
	if (v3d == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No 3D View Available");
		return OPERATOR_CANCELLED;
	}

	base = ED_view3d_give_base_under_cursor(C, event->mval);

	/* Check context */
	if (base == NULL || base->object->type != OB_MESH) {
		BKE_report(op->reports, RPT_ERROR, "Not an object or mesh");
		return OPERATOR_CANCELLED;
	}
	
	/* check input variables */
	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		char path[FILE_MAX];
		
		RNA_string_get(op->ptr, "filepath", path);
		ima = BKE_image_load_exists(path);
	}
	else {
		char name[MAX_ID_NAME - 2];
		RNA_string_get(op->ptr, "name", name);
		ima = (Image *)BKE_libblock_find_name(ID_IM, name);
	}
	
	if (!ima) {
		BKE_report(op->reports, RPT_ERROR, "Not an image");
		return OPERATOR_CANCELLED;
	}
	
	/* put mesh in editmode */

	obedit = base->object;
	me = obedit->data;
	if (me->edit_btmesh == NULL) {
		EDBM_mesh_make(scene->toolsettings, obedit);
		exitmode = 1;
	}
	if (me->edit_btmesh == NULL)
		return OPERATOR_CANCELLED;
	
	ED_uvedit_assign_image(bmain, scene, obedit, ima, NULL);

	if (exitmode) {
		EDBM_mesh_load(obedit);
		EDBM_mesh_free(me->edit_btmesh);
		MEM_freeN(me->edit_btmesh);
		me->edit_btmesh = NULL;

		/* load_editMesh free's pointers used by CustomData layers which might be used by DerivedMesh too,
		 * so signal to re-create DerivedMesh here (sergey) */
		DAG_id_tag_update(&me->id, 0);
	}

	/* dummie drop support; ensure view shows a result :) */
	if (v3d)
		v3d->flag2 |= V3D_SOLID_TEX;
	
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_drop_named_image(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Drop Image to Mesh UV Map";
	ot->description = "Assign Image to active UV Map, or create an UV Map";
	ot->idname = "MESH_OT_drop_named_image";
	
	/* api callbacks */
	ot->poll = layers_poll;
	ot->invoke = drop_named_image_invoke;
	
	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
	
	/* properties */
	RNA_def_string(ot->srna, "name", "Image", MAX_ID_NAME - 2, "Name", "Image name to assign");
	RNA_def_string(ot->srna, "filepath", "Path", FILE_MAX, "Filepath", "Path to image file");
}

static int mesh_uv_texture_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Mesh *me = ob->data;

	if (!ED_mesh_uv_texture_remove_active(me))
		return OPERATOR_CANCELLED;

	if (ob->mode & OB_MODE_TEXTURE_PAINT) {
		Scene *scene = CTX_data_scene(C);
		BKE_paint_proj_mesh_data_check(scene, ob, NULL, NULL, NULL, NULL);
		WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);
	}
	
	return OPERATOR_FINISHED;
}

void MESH_OT_uv_texture_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove UV Map";
	ot->description = "Remove UV Map";
	ot->idname = "MESH_OT_uv_texture_remove";
	
	/* api callbacks */
	ot->poll = layers_poll;
	ot->exec = mesh_uv_texture_remove_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/*********************** vertex color operators ************************/

static int mesh_vertex_color_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Mesh *me = ob->data;

	if (ED_mesh_color_add(me, NULL, true) == -1)
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

void MESH_OT_vertex_color_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Vertex Color";
	ot->description = "Add vertex color layer";
	ot->idname = "MESH_OT_vertex_color_add";
	
	/* api callbacks */
	ot->poll = layers_poll;
	ot->exec = mesh_vertex_color_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int mesh_vertex_color_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Mesh *me = ob->data;

	if (!ED_mesh_color_remove_active(me))
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

void MESH_OT_vertex_color_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Vertex Color";
	ot->description = "Remove vertex color layer";
	ot->idname = "MESH_OT_vertex_color_remove";
	
	/* api callbacks */
	ot->exec = mesh_vertex_color_remove_exec;
	ot->poll = layers_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *** CustomData clear functions, we need an operator for each *** */

static int mesh_customdata_clear_exec__internal(bContext *C,
                                                char htype, int type)
{
	Object *obedit = ED_object_context(C);
	Mesh       *me = obedit->data;

	int tot;
	CustomData *data = mesh_customdata_get_type(me, htype, &tot);

	BLI_assert(CustomData_layertype_is_singleton(type) == true);

	if (CustomData_has_layer(data, type)) {
		if (me->edit_btmesh) {
			BM_data_layer_free(me->edit_btmesh->bm, data, type);
		}
		else {
			CustomData_free_layers(data, type, tot);
		}

		DAG_id_tag_update(&me->id, 0);
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

/* Clear Mask */
static int mesh_customdata_clear_mask_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	if (ob && ob->type == OB_MESH) {
		Mesh *me = ob->data;

		/* special case - can't run this if we're in sculpt mode */
		if (ob->mode & OB_MODE_SCULPT) {
			return false;
		}

		if (me->id.lib == NULL) {
			CustomData *data = GET_CD_DATA(me, vdata);
			if (CustomData_has_layer(data, CD_PAINT_MASK)) {
				return true;
			}
			data = GET_CD_DATA(me, ldata);
			if (CustomData_has_layer(data, CD_GRID_PAINT_MASK)) {
				return true;
			}
		}
	}
	return false;
}
static int mesh_customdata_clear_mask_exec(bContext *C, wmOperator *UNUSED(op))
{
	int ret_a = mesh_customdata_clear_exec__internal(C, BM_VERT, CD_PAINT_MASK);
	int ret_b = mesh_customdata_clear_exec__internal(C, BM_LOOP, CD_GRID_PAINT_MASK);

	if (ret_a == OPERATOR_FINISHED ||
	    ret_b == OPERATOR_FINISHED)
	{
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MESH_OT_customdata_clear_mask(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Clear Sculpt-Mask Data";
	ot->idname = "MESH_OT_customdata_clear_mask";
	ot->description = "Clear vertex sculpt masking data from the mesh";

	/* api callbacks */
	ot->exec = mesh_customdata_clear_mask_exec;
	ot->poll = mesh_customdata_clear_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Clear Skin */
static int mesh_customdata_clear_skin_poll(bContext *C)
{
	Object *ob = ED_object_context(C);

	if (ob && ob->type == OB_MESH) {
		Mesh *me = ob->data;
		if (me->id.lib == NULL) {
			CustomData *data = GET_CD_DATA(me, vdata);
			if (CustomData_has_layer(data, CD_MVERT_SKIN)) {
				return true;
			}
		}
	}
	return false;
}
static int mesh_customdata_clear_skin_exec(bContext *C, wmOperator *UNUSED(op))
{
	return mesh_customdata_clear_exec__internal(C, BM_VERT, CD_MVERT_SKIN);
}

void MESH_OT_customdata_clear_skin(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Skin Data";
	ot->idname = "MESH_OT_customdata_clear_skin";
	ot->description = "Clear vertex skin layer";

	/* api callbacks */
	ot->exec = mesh_customdata_clear_skin_exec;
	ot->poll = mesh_customdata_clear_skin_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Add Geometry Layers *************************/

void ED_mesh_update(Mesh *mesh, bContext *C, int calc_edges, int calc_tessface)
{
	bool tessface_input = false;

	if (mesh->totface > 0 && mesh->totpoly == 0) {
		BKE_mesh_convert_mfaces_to_mpolys(mesh);

		/* would only be converting back again, don't bother */
		tessface_input = true;
	}

	if (calc_edges || ((mesh->totpoly || mesh->totface) && mesh->totedge == 0))
		BKE_mesh_calc_edges(mesh, calc_edges, true);

	if (calc_tessface) {
		if (tessface_input == false) {
			BKE_mesh_tessface_calc(mesh);
		}
	}
	else {
		/* default state is not to have tessface's so make sure this is the case */
		BKE_mesh_tessface_clear(mesh);
	}

	BKE_mesh_calc_normals(mesh);

	DAG_id_tag_update(&mesh->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);
}

static void mesh_add_verts(Mesh *mesh, int len)
{
	CustomData vdata;
	MVert *mvert;
	int i, totvert;

	if (len == 0)
		return;

	totvert = mesh->totvert + len;
	CustomData_copy(&mesh->vdata, &vdata, CD_MASK_MESH, CD_DEFAULT, totvert);
	CustomData_copy_data(&mesh->vdata, &vdata, 0, 0, mesh->totvert);

	if (!CustomData_has_layer(&vdata, CD_MVERT))
		CustomData_add_layer(&vdata, CD_MVERT, CD_CALLOC, NULL, totvert);

	CustomData_free(&mesh->vdata, mesh->totvert);
	mesh->vdata = vdata;
	BKE_mesh_update_customdata_pointers(mesh, false);

	/* scan the input list and insert the new vertices */

	/* set default flags */
	mvert = &mesh->mvert[mesh->totvert];
	for (i = 0; i < len; i++, mvert++)
		mvert->flag |= SELECT;

	/* set final vertex list size */
	mesh->totvert = totvert;
}

static void mesh_add_edges(Mesh *mesh, int len)
{
	CustomData edata;
	MEdge *medge;
	int i, totedge;

	if (len == 0)
		return;

	totedge = mesh->totedge + len;

	/* update customdata  */
	CustomData_copy(&mesh->edata, &edata, CD_MASK_MESH, CD_DEFAULT, totedge);
	CustomData_copy_data(&mesh->edata, &edata, 0, 0, mesh->totedge);

	if (!CustomData_has_layer(&edata, CD_MEDGE))
		CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);

	CustomData_free(&mesh->edata, mesh->totedge);
	mesh->edata = edata;
	BKE_mesh_update_customdata_pointers(mesh, false); /* new edges don't change tessellation */

	/* set default flags */
	medge = &mesh->medge[mesh->totedge];
	for (i = 0; i < len; i++, medge++)
		medge->flag = ME_EDGEDRAW | ME_EDGERENDER | SELECT;

	mesh->totedge = totedge;
}

static void mesh_add_tessfaces(Mesh *mesh, int len)
{
	CustomData fdata;
	MFace *mface;
	int i, totface;

	if (len == 0)
		return;

	totface = mesh->totface + len;   /* new face count */

	/* update customdata */
	CustomData_copy(&mesh->fdata, &fdata, CD_MASK_MESH, CD_DEFAULT, totface);
	CustomData_copy_data(&mesh->fdata, &fdata, 0, 0, mesh->totface);

	if (!CustomData_has_layer(&fdata, CD_MFACE))
		CustomData_add_layer(&fdata, CD_MFACE, CD_CALLOC, NULL, totface);

	CustomData_free(&mesh->fdata, mesh->totface);
	mesh->fdata = fdata;
	BKE_mesh_update_customdata_pointers(mesh, true);

	/* set default flags */
	mface = &mesh->mface[mesh->totface];
	for (i = 0; i < len; i++, mface++)
		mface->flag = ME_FACE_SEL;

	mesh->totface = totface;
}

static void mesh_add_loops(Mesh *mesh, int len)
{
	CustomData ldata;
	int totloop;

	if (len == 0)
		return;

	totloop = mesh->totloop + len;   /* new face count */

	/* update customdata */
	CustomData_copy(&mesh->ldata, &ldata, CD_MASK_MESH, CD_DEFAULT, totloop);
	CustomData_copy_data(&mesh->ldata, &ldata, 0, 0, mesh->totloop);

	if (!CustomData_has_layer(&ldata, CD_MLOOP))
		CustomData_add_layer(&ldata, CD_MLOOP, CD_CALLOC, NULL, totloop);

	CustomData_free(&mesh->ldata, mesh->totloop);
	mesh->ldata = ldata;
	BKE_mesh_update_customdata_pointers(mesh, true);

	mesh->totloop = totloop;
}

static void mesh_add_polys(Mesh *mesh, int len)
{
	CustomData pdata;
	MPoly *mpoly;
	int i, totpoly;

	if (len == 0)
		return;

	totpoly = mesh->totpoly + len;   /* new face count */

	/* update customdata */
	CustomData_copy(&mesh->pdata, &pdata, CD_MASK_MESH, CD_DEFAULT, totpoly);
	CustomData_copy_data(&mesh->pdata, &pdata, 0, 0, mesh->totpoly);

	if (!CustomData_has_layer(&pdata, CD_MPOLY))
		CustomData_add_layer(&pdata, CD_MPOLY, CD_CALLOC, NULL, totpoly);

	CustomData_free(&mesh->pdata, mesh->totpoly);
	mesh->pdata = pdata;
	BKE_mesh_update_customdata_pointers(mesh, true);

	/* set default flags */
	mpoly = &mesh->mpoly[mesh->totpoly];
	for (i = 0; i < len; i++, mpoly++)
		mpoly->flag = ME_FACE_SEL;

	mesh->totpoly = totpoly;
}

static void mesh_remove_verts(Mesh *mesh, int len)
{
	int totvert;

	if (len == 0)
		return;

	totvert = mesh->totvert - len;
	CustomData_free_elem(&mesh->vdata, totvert, len);

	/* set final vertex list size */
	mesh->totvert = totvert;
}

static void mesh_remove_edges(Mesh *mesh, int len)
{
	int totedge;

	if (len == 0)
		return;

	totedge = mesh->totedge - len;
	CustomData_free_elem(&mesh->edata, totedge, len);

	mesh->totedge = totedge;
}

static void mesh_remove_faces(Mesh *mesh, int len)
{
	int totface;

	if (len == 0)
		return;

	totface = mesh->totface - len;   /* new face count */
	CustomData_free_elem(&mesh->fdata, totface, len);

	mesh->totface = totface;
}

#if 0
void ED_mesh_geometry_add(Mesh *mesh, ReportList *reports, int verts, int edges, int faces)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot add geometry in edit mode");
		return;
	}

	if (verts)
		mesh_add_verts(mesh, verts);
	if (edges)
		mesh_add_edges(mesh, edges);
	if (faces)
		mesh_add_faces(mesh, faces);
}
#endif

void ED_mesh_tessfaces_add(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot add tessfaces in edit mode");
		return;
	}

	if (mesh->mpoly) {
		BKE_report(reports, RPT_ERROR, "Cannot add tessfaces to a mesh that already has polygons");
		return;
	}

	mesh_add_tessfaces(mesh, count);
}

void ED_mesh_edges_add(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot add edges in edit mode");
		return;
	}

	mesh_add_edges(mesh, count);
}

void ED_mesh_vertices_add(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot add vertices in edit mode");
		return;
	}

	mesh_add_verts(mesh, count);
}

void ED_mesh_faces_remove(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot remove faces in edit mode");
		return;
	}
	else if (count > mesh->totface) {
		BKE_report(reports, RPT_ERROR, "Cannot remove more faces than the mesh contains");
		return;
	}

	mesh_remove_faces(mesh, count);
}

void ED_mesh_edges_remove(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot remove edges in edit mode");
		return;
	}
	else if (count > mesh->totedge) {
		BKE_report(reports, RPT_ERROR, "Cannot remove more edges than the mesh contains");
		return;
	}

	mesh_remove_edges(mesh, count);
}

void ED_mesh_vertices_remove(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot remove vertices in edit mode");
		return;
	}
	else if (count > mesh->totvert) {
		BKE_report(reports, RPT_ERROR, "Cannot remove more vertices than the mesh contains");
		return;
	}

	mesh_remove_verts(mesh, count);
}

void ED_mesh_loops_add(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot add loops in edit mode");
		return;
	}

	mesh_add_loops(mesh, count);
}

void ED_mesh_polys_add(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot add polygons in edit mode");
		return;
	}

	mesh_add_polys(mesh, count);
}

void ED_mesh_calc_tessface(Mesh *mesh)
{
	if (mesh->edit_btmesh) {
		BKE_editmesh_tessface_calc(mesh->edit_btmesh);
	}
	else {
		BKE_mesh_tessface_calc(mesh);
	}
}

void ED_mesh_report_mirror_ex(wmOperator *op, int totmirr, int totfail,
                              char selectmode)
{
	const char *elem_type;

	if (selectmode & SCE_SELECT_VERTEX) {
		elem_type = "vertices";
	}
	else if (selectmode & SCE_SELECT_EDGE) {
		elem_type = "edges";
	}
	else {
		elem_type = "faces";
	}

	if (totfail) {
		BKE_reportf(op->reports, RPT_WARNING, "%d %s mirrored, %d failed", totmirr, elem_type, totfail);
	}
	else {
		BKE_reportf(op->reports, RPT_INFO, "%d %s mirrored", totmirr, elem_type);
	}
}

void ED_mesh_report_mirror(wmOperator *op, int totmirr, int totfail)
{
	ED_mesh_report_mirror_ex(op, totmirr, totfail, SCE_SELECT_VERTEX);
}
