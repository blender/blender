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


#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_utildefines.h"
#include "BLI_array.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "RE_render_ext.h"

#include "mesh_intern.h"

#define GET_CD_DATA(me, data) (me->edit_btmesh ? &me->edit_btmesh->bm->data : &me->data)
static void delete_customdata_layer(bContext *C, Object *ob, CustomDataLayer *layer)
{
	Mesh *me = ob->data;
	CustomData *data;
	void *actlayerdata, *rndlayerdata, *clonelayerdata, *stencillayerdata, *layerdata = layer->data;
	int type = layer->type;
	int index;
	int i, actindex, rndindex, cloneindex, stencilindex, tot;

	if (layer->type == CD_MLOOPCOL || layer->type == CD_MLOOPUV) {
		if (me->edit_btmesh) {
			data = &me->edit_btmesh->bm->ldata;
			tot = me->edit_btmesh->bm->totloop;
		}
		else {
			data = &me->ldata;
			tot = me->totloop;
		}
	}
	else {
		if (me->edit_btmesh) {
			data = &me->edit_btmesh->bm->pdata;
			tot = me->edit_btmesh->bm->totface;
		}
		else {
			data = &me->pdata;
			tot = me->totpoly;
		}
	}
	
	index = CustomData_get_layer_index(data, type);

	/* ok, deleting a non-active layer needs to preserve the active layer indices.
	 * to do this, we store a pointer to the .data member of both layer and the active layer,
	 * (to detect if we're deleting the active layer or not), then use the active
	 * layer data pointer to find where the active layer has ended up.
	 *
	 * this is necessary because the deletion functions only support deleting the active
	 * layer. */
	actlayerdata = data->layers[CustomData_get_active_layer_index(data, type)].data;
	rndlayerdata = data->layers[CustomData_get_render_layer_index(data, type)].data;
	clonelayerdata = data->layers[CustomData_get_clone_layer_index(data, type)].data;
	stencillayerdata = data->layers[CustomData_get_stencil_layer_index(data, type)].data;
	CustomData_set_layer_active(data, type, layer - &data->layers[index]);

	if (me->edit_btmesh) {
		BM_data_layer_free(me->edit_btmesh->bm, data, type);
	}
	else {
		CustomData_free_layer_active(data, type, tot);
		mesh_update_customdata_pointers(me, TRUE);
	}

	if (!CustomData_has_layer(data, type) && (type == CD_MLOOPCOL && (ob->mode & OB_MODE_VERTEX_PAINT)))
		ED_object_toggle_modes(C, OB_MODE_VERTEX_PAINT);

	/* reconstruct active layer */
	if (actlayerdata != layerdata) {
		/* find index */
		actindex = CustomData_get_layer_index(data, type);
		for (i = actindex; i < data->totlayer; i++) {
			if (data->layers[i].data == actlayerdata) {
				actindex = i - actindex;
				break;
			}
		}
		
		/* set index */
		CustomData_set_layer_active(data, type, actindex);
	}
	
	if (rndlayerdata != layerdata) {
		/* find index */
		rndindex = CustomData_get_layer_index(data, type);
		for (i = rndindex; i < data->totlayer; i++) {
			if (data->layers[i].data == rndlayerdata) {
				rndindex = i - rndindex;
				break;
			}
		}
		
		/* set index */
		CustomData_set_layer_render(data, type, rndindex);
	}
	
	if (clonelayerdata != layerdata) {
		/* find index */
		cloneindex = CustomData_get_layer_index(data, type);
		for (i = cloneindex; i < data->totlayer; i++) {
			if (data->layers[i].data == clonelayerdata) {
				cloneindex = i - cloneindex;
				break;
			}
		}
		
		/* set index */
		CustomData_set_layer_clone(data, type, cloneindex);
	}
	
	if (stencillayerdata != layerdata) {
		/* find index */
		stencilindex = CustomData_get_layer_index(data, type);
		for (i = stencilindex; i < data->totlayer; i++) {
			if (data->layers[i].data == stencillayerdata) {
				stencilindex = i - stencilindex;
				break;
			}
		}
		
		/* set index */
		CustomData_set_layer_stencil(data, type, stencilindex);
	}
}

/* copies from active to 'index' */
static void editmesh_face_copy_customdata(BMEditMesh *em, int type, int index)
{
	BMesh *bm = em->bm;
	CustomData *pdata = &bm->pdata;
	BMIter iter;
	BMFace *efa;
	const int n = CustomData_get_active_layer(pdata, type);

	/* ensure all current elements follow new customdata layout */
	BM_ITER(efa, &iter, bm, BM_FACES_OF_MESH, NULL) {
		void *data = CustomData_bmesh_get_n(pdata, efa->head.data, type, n);
		CustomData_bmesh_set_n(pdata, efa->head.data, type, index, data);
	}
}

/* copies from active to 'index' */
static void editmesh_loop_copy_customdata(BMEditMesh *em, int type, int index)
{
	BMesh *bm = em->bm;
	CustomData *ldata = &bm->ldata;
	BMIter iter;
	BMIter liter;
	BMFace *efa;
	BMLoop *loop;
	const int n = CustomData_get_active_layer(ldata, type);

	/* ensure all current elements follow new customdata layout */
	BM_ITER(efa, &iter, bm, BM_FACES_OF_MESH, NULL) {
		BM_ITER(loop, &liter, bm, BM_LOOPS_OF_FACE, efa) {
			void *data = CustomData_bmesh_get_n(ldata, loop->head.data, type, n);
			CustomData_bmesh_set_n(ldata, loop->head.data, type, index, data);
		}
	}
}

int ED_mesh_uv_loop_reset_ex(struct bContext *C, struct Mesh *me, const int layernum)
{
	BMEditMesh *em = me->edit_btmesh;
	MLoopUV *luv;
	BLI_array_declare(polylengths);
	int *polylengths = NULL;
	BLI_array_declare(uvs);
	float **uvs = NULL;
	float **fuvs = NULL;
	int i, j;

	if (em) {
		/* Collect BMesh UVs */

		BMFace *efa;
		BMLoop *l;
		BMIter iter, liter;

		BLI_assert(CustomData_has_layer(&em->bm->ldata, CD_MLOOPUV));

		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
				continue;

			i = 0;
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get_n(&em->bm->ldata, l->head.data, CD_MLOOPUV, layernum);
				BLI_array_append(uvs, luv->uv);
				i++;
			}

			BLI_array_append(polylengths, efa->len);
		}
	}
	else {
		/* Collect Mesh UVs */

		MPoly *mp;
		MLoopUV *mloouv;

		BLI_assert(CustomData_has_layer(&me->ldata, CD_MLOOPUV));
		mloouv = CustomData_get_layer_n(&me->ldata, CD_MLOOPUV, layernum);

		for (j = 0; j < me->totpoly; j++) {
			mp = &me->mpoly[j];

			for (i = 0; i < mp->totloop; i++) {
				luv = &mloouv[mp->loopstart + i];
				BLI_array_append(uvs, luv->uv);
			}

			BLI_array_append(polylengths, mp->totloop);
		}
	}

	fuvs = uvs;
	for (j = 0; j < BLI_array_count(polylengths); j++) {
		int len = polylengths[j];

		if (len == 3) {
			fuvs[0][0] = 0.0;
			fuvs[0][1] = 0.0;
			
			fuvs[1][0] = 1.0;
			fuvs[1][1] = 0.0;

			fuvs[2][0] = 1.0;
			fuvs[2][1] = 1.0;
		}
		else if (len == 4) {
			fuvs[0][0] = 0.0;
			fuvs[0][1] = 0.0;
			
			fuvs[1][0] = 1.0;
			fuvs[1][1] = 0.0;

			fuvs[2][0] = 1.0;
			fuvs[2][1] = 1.0;

			fuvs[3][0] = 0.0;
			fuvs[3][1] = 1.0;
			/*make sure we ignore 2-sided faces*/
		}
		else if (len > 2) {
			float fac = 0.0f, dfac = 1.0f / (float)len;

			dfac *= M_PI * 2;

			for (i = 0; i < len; i++) {
				fuvs[i][0] = 0.5f * sin(fac) + 0.5f;
				fuvs[i][1] = 0.5f * cos(fac) + 0.5f;

				fac += dfac;
			}
		}

		fuvs += len;
	}

	BLI_array_free(uvs);
	BLI_array_free(polylengths);

	DAG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return 1;
}

int ED_mesh_uv_loop_reset(struct bContext *C, struct Mesh *me)
{
	/* could be ldata or pdata */
	CustomData *pdata = GET_CD_DATA(me, pdata);
	const int layernum = CustomData_get_active_layer_index(pdata, CD_MTEXPOLY);
	return ED_mesh_uv_loop_reset_ex(C, me, layernum);
}

/* note: keep in sync with ED_mesh_color_add */
int ED_mesh_uv_texture_add(bContext *C, Mesh *me, const char *name, int active_set)
{
	BMEditMesh *em;
	int layernum;

	short is_init = FALSE;

	if (me->edit_btmesh) {
		em = me->edit_btmesh;

		layernum = CustomData_number_of_layers(&em->bm->pdata, CD_MTEXPOLY);
		if (layernum >= MAX_MTFACE)
			return -1;

		/* CD_MTEXPOLY */
		BM_data_layer_add_named(em->bm, &em->bm->pdata, CD_MTEXPOLY, name);
		/* copy data from active UV */
		if (layernum) {
			editmesh_face_copy_customdata(em, CD_MTEXPOLY, layernum);
		}
		if (active_set || layernum == 0) {
			CustomData_set_layer_active(&em->bm->pdata, CD_MTEXPOLY, layernum);
		}

		/* CD_MLOOPUV */
		BM_data_layer_add_named(em->bm, &em->bm->ldata, CD_MLOOPUV, name);
		/* copy data from active UV */
		if (layernum) {
			editmesh_loop_copy_customdata(em, CD_MLOOPUV, layernum);
			is_init = TRUE;
		}
		if (active_set || layernum == 0) {
			CustomData_set_layer_active(&em->bm->ldata, CD_MLOOPUV, layernum);
		}
	}
	else {
		layernum = CustomData_number_of_layers(&me->pdata, CD_MTEXPOLY);
		if (layernum >= MAX_MTFACE)
			return -1;

		if (me->mtpoly) {
			CustomData_add_layer_named(&me->pdata, CD_MTEXPOLY, CD_DUPLICATE, me->mtpoly, me->totpoly, name);
			CustomData_add_layer_named(&me->ldata, CD_MLOOPUV, CD_DUPLICATE, me->mloopuv, me->totloop, name);
			CustomData_add_layer_named(&me->fdata, CD_MTFACE, CD_DUPLICATE, me->mtface, me->totface, name);
			is_init = TRUE;
		}
		else {
			CustomData_add_layer_named(&me->pdata, CD_MTEXPOLY, CD_DEFAULT, NULL, me->totpoly, name);
			CustomData_add_layer_named(&me->ldata, CD_MLOOPUV, CD_DEFAULT, NULL, me->totloop, name);
			CustomData_add_layer_named(&me->fdata, CD_MTFACE, CD_DEFAULT, NULL, me->totface, name);
		}
		
		if (active_set || layernum == 0) {
			CustomData_set_layer_active(&me->pdata, CD_MTEXPOLY, layernum);
			CustomData_set_layer_active(&me->ldata, CD_MLOOPUV, layernum);

			CustomData_set_layer_active(&me->fdata, CD_MTFACE, layernum);
		}

		mesh_update_customdata_pointers(me, TRUE);
	}

	/* don't overwrite our copied coords */
	if (is_init == FALSE) {
		ED_mesh_uv_loop_reset_ex(C, me, layernum);
	}

	DAG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return layernum;
}

int ED_mesh_uv_texture_remove(bContext *C, Object *ob, Mesh *me)
{
	CustomData *pdata = GET_CD_DATA(me, pdata), *ldata = GET_CD_DATA(me, ldata);
	CustomDataLayer *cdlp, *cdlu;
	int index;

	index = CustomData_get_active_layer_index(pdata, CD_MTEXPOLY);
	cdlp = (index == -1) ? NULL : &pdata->layers[index];

	index = CustomData_get_active_layer_index(ldata, CD_MLOOPUV);
	cdlu = (index == -1) ? NULL : &ldata->layers[index];
	
	if (!cdlp || !cdlu)
		return 0;

	delete_customdata_layer(C, ob, cdlp);
	delete_customdata_layer(C, ob, cdlu);
	
	DAG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return 1;
}

/* note: keep in sync with ED_mesh_uv_texture_add */
int ED_mesh_color_add(bContext *C, Scene *UNUSED(scene), Object *UNUSED(ob), Mesh *me, const char *name, int active_set)
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
			editmesh_loop_copy_customdata(em, CD_MLOOPCOL, layernum);
		}
		if (active_set || layernum == 0) {
			CustomData_set_layer_active(&em->bm->ldata, CD_MLOOPCOL, layernum);
		}
	}
	else {
		layernum = CustomData_number_of_layers(&me->ldata, CD_MLOOPCOL);
		if (layernum >= CD_MLOOPCOL) {
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

		mesh_update_customdata_pointers(me, TRUE);
	}

	DAG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return layernum;
}

int ED_mesh_color_remove(bContext *C, Object *ob, Mesh *me)
{
	CustomData *ldata = GET_CD_DATA(me, ldata);
	CustomDataLayer *cdl;
	int index;

	index = CustomData_get_active_layer_index(ldata, CD_MLOOPCOL);
	cdl = (index == -1) ? NULL : &ldata->layers[index];

	if (!cdl)
		return 0;

	delete_customdata_layer(C, ob, cdl);
	DAG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return 1;
}

int ED_mesh_color_remove_named(bContext *C, Object *ob, Mesh *me, const char *name)
{
	CustomData *ldata = GET_CD_DATA(me, ldata);
	CustomDataLayer *cdl;
	int index;

	index = CustomData_get_named_layer_index(ldata, CD_MLOOPCOL, name);
	cdl = (index == -1) ? NULL : &ldata->layers[index];

	if (!cdl)
		return 0;

	delete_customdata_layer(C, ob, cdl);
	DAG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return 1;
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

	if (ED_mesh_uv_texture_add(C, me, NULL, TRUE) == -1)
		return OPERATOR_CANCELLED;

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

static int drop_named_image_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	Base *base = ED_view3d_give_base_under_cursor(C, event->mval);
	Image *ima = NULL;
	Mesh *me;
	Object *obedit;
	int exitmode = 0;
	char name[MAX_ID_NAME - 2];
	
	/* Check context */
	if (base == NULL || base->object->type != OB_MESH) {
		BKE_report(op->reports, RPT_ERROR, "Not an Object or Mesh");
		return OPERATOR_CANCELLED;
	}
	
	/* check input variables */
	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		char path[FILE_MAX];
		
		RNA_string_get(op->ptr, "filepath", path);
		ima = BKE_add_image_file(path);
	}
	else {
		RNA_string_get(op->ptr, "name", name);
		ima = (Image *)find_id("IM", name);
	}
	
	if (!ima) {
		BKE_report(op->reports, RPT_ERROR, "Not an Image");
		return OPERATOR_CANCELLED;
	}
	
	/* put mesh in editmode */

	obedit = base->object;
	me = obedit->data;
	if (me->edit_btmesh == NULL) {
		EDBM_mesh_make(scene->toolsettings, scene, obedit);
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
	ot->name = "Assign Image to UV Map";
	ot->description = "Assign Image to active UV Map, or create an UV Map";
	ot->idname = "MESH_OT_drop_named_image";
	
	/* api callbacks */
	ot->poll = layers_poll;
	ot->invoke = drop_named_image_invoke;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* properties */
	RNA_def_string(ot->srna, "name", "Image", MAX_ID_NAME - 2, "Name", "Image name to assign");
	RNA_def_string(ot->srna, "filepath", "Path", FILE_MAX, "Filepath", "Path to image file");
}

static int mesh_uv_texture_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Mesh *me = ob->data;

	if (!ED_mesh_uv_texture_remove(C, ob, me))
		return OPERATOR_CANCELLED;

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
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_context(C);
	Mesh *me = ob->data;

	if (ED_mesh_color_add(C, scene, ob, me, NULL, TRUE) == -1)
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

	if (!ED_mesh_color_remove(C, ob, me))
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

/*********************** sticky operators ************************/

static int mesh_sticky_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	Object *ob = ED_object_context(C);
	Mesh *me = ob->data;

	/* why is this commented out? */
#if 0
	if (me->msticky)
		return OPERATOR_CANCELLED;
#endif

	RE_make_sticky(scene, v3d);

	DAG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return OPERATOR_FINISHED;
}

void MESH_OT_sticky_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Sticky";
	ot->description = "Add sticky UV texture layer";
	ot->idname = "MESH_OT_sticky_add";
	
	/* api callbacks */
	ot->poll = layers_poll;
	ot->exec = mesh_sticky_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int mesh_sticky_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Mesh *me = ob->data;

	if (!me->msticky)
		return OPERATOR_CANCELLED;

	CustomData_free_layer_active(&me->vdata, CD_MSTICKY, me->totvert);
	me->msticky = NULL;

	DAG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return OPERATOR_FINISHED;
}

void MESH_OT_sticky_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Sticky";
	ot->description = "Remove sticky UV texture layer";
	ot->idname = "MESH_OT_sticky_remove";
	
	/* api callbacks */
	ot->poll = layers_poll;
	ot->exec = mesh_sticky_remove_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Add Geometry Layers *************************/

void ED_mesh_update(Mesh *mesh, bContext *C, int calc_edges, int calc_tessface)
{
	int *polyindex = NULL;
	float (*face_nors)[3];
	int tessface_input = FALSE;

	if (mesh->totface > 0 && mesh->totpoly == 0) {
		BKE_mesh_convert_mfaces_to_mpolys(mesh);

		/* would only be converting back again, don't bother */
		tessface_input = TRUE;

		/* it also happens that converting the faces calculates edges, skip this */
		calc_edges = FALSE;
	}

	if (calc_edges || (mesh->totpoly && mesh->totedge == 0))
		BKE_mesh_calc_edges(mesh, calc_edges);

	if (calc_tessface) {
		if (tessface_input == FALSE) {
			BKE_mesh_tessface_calc(mesh);
		}
	}
	else {
		/* default state is not to have tessface's so make sure this is the case */
		BKE_mesh_tessface_clear(mesh);
	}

	/* note on this if/else - looks like these layers are not needed
	 * so rather then add poly-index layer and calculate normals for it
	 * calculate normals only for the mvert's. - campbell */
#ifdef USE_BMESH_MPOLY_NORMALS
	polyindex = CustomData_get_layer(&mesh->fdata, CD_POLYINDEX);
	/* add a normals layer for tessellated faces, a tessface normal will
	 * contain the normal of the poly the face was tessellated from. */
	face_nors = CustomData_add_layer(&mesh->fdata, CD_NORMAL, CD_CALLOC, NULL, mesh->totface);

	mesh_calc_normals_mapping_ex(mesh->mvert, mesh->totvert,
	                             mesh->mloop, mesh->mpoly,
	                             mesh->totloop, mesh->totpoly,
	                             NULL /* polyNors_r */,
	                             mesh->mface, mesh->totface,
	                             polyindex, face_nors, FALSE);
#else
	mesh_calc_normals(mesh->mvert, mesh->totvert,
	                  mesh->mloop, mesh->mpoly, mesh->totloop, mesh->totpoly,
	                  NULL);
	(void)polyindex;
	(void)face_nors;
#endif

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
	mesh_update_customdata_pointers(mesh, FALSE);

	/* scan the input list and insert the new vertices */

	mvert = &mesh->mvert[mesh->totvert];
	for (i = 0; i < len; i++, mvert++)
		mvert->flag |= SELECT;

	/* set final vertex list size */
	mesh->totvert = totvert;
}

void ED_mesh_transform(Mesh *me, float *mat)
{
	int i;
	MVert *mvert = me->mvert;

	for (i = 0; i < me->totvert; i++, mvert++)
		mul_m4_v3((float (*)[4])mat, mvert->co);

	mesh_calc_normals_mapping(me->mvert, me->totvert, me->mloop, me->mpoly, me->totloop, me->totpoly, NULL, NULL, 0, NULL, NULL);
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
	mesh_update_customdata_pointers(mesh, FALSE); /* new edges don't change tessellation */

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
	mesh_update_customdata_pointers(mesh, TRUE);

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
	mesh_update_customdata_pointers(mesh, TRUE);

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
	mesh_update_customdata_pointers(mesh, TRUE);

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
		BKE_report(reports, RPT_ERROR, "Can't add geometry in edit mode");
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
		BKE_report(reports, RPT_ERROR, "Can't add tessfaces in edit mode");
		return;
	}

	if (mesh->mpoly) {
		BKE_report(reports, RPT_ERROR, "Can't add tessfaces to a mesh that already has polygons");
		return;
	}

	mesh_add_tessfaces(mesh, count);
}

void ED_mesh_edges_add(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Can't add edges in edit mode");
		return;
	}

	mesh_add_edges(mesh, count);
}

void ED_mesh_vertices_add(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Can't add vertices in edit mode");
		return;
	}

	mesh_add_verts(mesh, count);
}

void ED_mesh_faces_remove(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Can't remove faces in edit mode");
		return;
	}
	else if (count > mesh->totface) {
		BKE_report(reports, RPT_ERROR, "Can't remove more faces than the mesh contains");
		return;
	}

	mesh_remove_faces(mesh, count);
}

void ED_mesh_edges_remove(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Can't remove edges in edit mode");
		return;
	}
	else if (count > mesh->totedge) {
		BKE_report(reports, RPT_ERROR, "Can't remove more edges than the mesh contains");
		return;
	}

	mesh_remove_edges(mesh, count);
}

void ED_mesh_vertices_remove(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Can't remove vertices in edit mode");
		return;
	}
	else if (count > mesh->totvert) {
		BKE_report(reports, RPT_ERROR, "Can't remove more vertices than the mesh contains");
		return;
	}

	mesh_remove_verts(mesh, count);
}

void ED_mesh_loops_add(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Can't add loops in edit mode.");
		return;
	}

	mesh_add_loops(mesh, count);
}

void ED_mesh_polys_add(Mesh *mesh, ReportList *reports, int count)
{
	if (mesh->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Can't add polys in edit mode.");
		return;
	}

	mesh_add_polys(mesh, count);
}

void ED_mesh_calc_normals(Mesh *mesh)
{
#ifdef USE_BMESH_MPOLY_NORMALS
	mesh_calc_normals_mapping_ex(mesh->mvert, mesh->totvert,
	                             mesh->mloop, mesh->mpoly, mesh->totloop, mesh->totpoly,
	                             NULL, NULL, 0, NULL, NULL, FALSE);
#else
	mesh_calc_normals(mesh->mvert, mesh->totvert,
	                  mesh->mloop, mesh->mpoly, mesh->totloop, mesh->totpoly,
	                  NULL);
#endif
}

void ED_mesh_calc_tessface(Mesh *mesh)
{
	if (mesh->edit_btmesh) {
		BMEdit_RecalcTessellation(mesh->edit_btmesh);
	}
	else {
		BKE_mesh_tessface_calc(mesh);
	}
}
