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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_mesh.h"

#include "BLI_editVert.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_view3d.h"

#include "mesh_intern.h"

static void delete_customdata_layer(bContext *C, Object *ob, CustomDataLayer *layer)
{
	Mesh *me = ob->data;
	CustomData *data= (me->edit_mesh)? &me->edit_mesh->fdata: &me->fdata;
	void *actlayerdata, *rndlayerdata, *clonelayerdata, *masklayerdata, *layerdata=layer->data;
	int type= layer->type;
	int index= CustomData_get_layer_index(data, type);
	int i, actindex, rndindex, cloneindex, maskindex;
	
	/* ok, deleting a non-active layer needs to preserve the active layer indices.
	  to do this, we store a pointer to the .data member of both layer and the active layer,
	  (to detect if we're deleting the active layer or not), then use the active
	  layer data pointer to find where the active layer has ended up.
	  
	  this is necassary because the deletion functions only support deleting the active
	  layer. */
	actlayerdata = data->layers[CustomData_get_active_layer_index(data, type)].data;
	rndlayerdata = data->layers[CustomData_get_render_layer_index(data, type)].data;
	clonelayerdata = data->layers[CustomData_get_clone_layer_index(data, type)].data;
	masklayerdata = data->layers[CustomData_get_mask_layer_index(data, type)].data;
	CustomData_set_layer_active(data, type, layer - &data->layers[index]);

	if(me->edit_mesh) {
		EM_free_data_layer(me->edit_mesh, data, type);
	}
	else {
		CustomData_free_layer_active(data, type, me->totface);
		mesh_update_customdata_pointers(me);
	}

	if(!CustomData_has_layer(data, type) && (type == CD_MCOL && (ob->mode & OB_MODE_VERTEX_PAINT)))
		ED_object_toggle_modes(C, OB_MODE_VERTEX_PAINT);

	/* reconstruct active layer */
	if (actlayerdata != layerdata) {
		/* find index */
		actindex = CustomData_get_layer_index(data, type);
		for (i=actindex; i<data->totlayer; i++) {
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
		for (i=rndindex; i<data->totlayer; i++) {
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
		for (i=cloneindex; i<data->totlayer; i++) {
			if (data->layers[i].data == clonelayerdata) {
				cloneindex = i - cloneindex;
				break;
			}
		}
		
		/* set index */
		CustomData_set_layer_clone(data, type, cloneindex);
	}
	
	if (masklayerdata != layerdata) {
		/* find index */
		maskindex = CustomData_get_layer_index(data, type);
		for (i=maskindex; i<data->totlayer; i++) {
			if (data->layers[i].data == masklayerdata) {
				maskindex = i - maskindex;
				break;
			}
		}
		
		/* set index */
		CustomData_set_layer_mask(data, type, maskindex);
	}
}

/*********************** UV texture operators ************************/

static int layers_poll(bContext *C)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	ID *data= (ob)? ob->data: NULL;
	return (ob && !ob->id.lib && ob->type==OB_MESH && data && !data->lib);
}

static int uv_texture_add_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Mesh *me= ob->data;
	EditMesh *em;
	int layernum;

	if(me->edit_mesh) {
		em= me->edit_mesh;

		layernum= CustomData_number_of_layers(&em->fdata, CD_MTFACE);
		if(layernum >= MAX_MTFACE)
			return OPERATOR_CANCELLED;

		EM_add_data_layer(em, &em->fdata, CD_MTFACE);
		CustomData_set_layer_active(&em->fdata, CD_MTFACE, layernum);
	}
	else {
		layernum= CustomData_number_of_layers(&me->fdata, CD_MTFACE);
		if(layernum >= MAX_MTFACE)
			return OPERATOR_CANCELLED;

		if(me->mtface)
			CustomData_add_layer(&me->fdata, CD_MTFACE, CD_DUPLICATE, me->mtface, me->totface);
		else
			CustomData_add_layer(&me->fdata, CD_MTFACE, CD_DEFAULT, NULL, me->totface);

		CustomData_set_layer_active(&me->fdata, CD_MTFACE, layernum);
		mesh_update_customdata_pointers(me);
	}

	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}

void MESH_OT_uv_texture_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add UV Texture";
	ot->description= "Add UV texture layer.";
	ot->idname= "MESH_OT_uv_texture_add";
	
	/* api callbacks */
	ot->poll= layers_poll;
	ot->exec= uv_texture_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int uv_texture_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Mesh *me= ob->data;
	CustomDataLayer *cdl;
	int index;

 	index= CustomData_get_active_layer_index(&me->fdata, CD_MTFACE);
	cdl= (index == -1)? NULL: &me->fdata.layers[index];

	if(!cdl)
		return OPERATOR_CANCELLED;

	delete_customdata_layer(C, ob, cdl);

	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}

void MESH_OT_uv_texture_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove UV Texture";
	ot->description= "Remove UV texture layer.";
	ot->idname= "MESH_OT_uv_texture_remove";
	
	/* api callbacks */
	ot->poll= layers_poll;
	ot->exec= uv_texture_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/*********************** vertex color operators ************************/

static int vertex_color_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Mesh *me= ob->data;
	EditMesh *em;
	MCol *mcol;
	int layernum;

	if(me->edit_mesh) {
		em= me->edit_mesh;

		layernum= CustomData_number_of_layers(&em->fdata, CD_MCOL);
		if(layernum >= MAX_MCOL)
			return OPERATOR_CANCELLED;

		EM_add_data_layer(em, &em->fdata, CD_MCOL);
		CustomData_set_layer_active(&em->fdata, CD_MCOL, layernum);
	}
	else {
		layernum= CustomData_number_of_layers(&me->fdata, CD_MCOL);
		if(layernum >= MAX_MCOL)
			return OPERATOR_CANCELLED;

		mcol= me->mcol;

		if(me->mcol)
			CustomData_add_layer(&me->fdata, CD_MCOL, CD_DUPLICATE, me->mcol, me->totface);
		else
			CustomData_add_layer(&me->fdata, CD_MCOL, CD_DEFAULT, NULL, me->totface);

		CustomData_set_layer_active(&me->fdata, CD_MCOL, layernum);
		mesh_update_customdata_pointers(me);

		if(!mcol)
			shadeMeshMCol(scene, ob, me);
	}

	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}

void MESH_OT_vertex_color_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Vertex Color";
	ot->description= "Add vertex color layer.";
	ot->idname= "MESH_OT_vertex_color_add";
	
	/* api callbacks */
	ot->poll= layers_poll;
	ot->exec= vertex_color_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int vertex_color_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Mesh *me= ob->data;
	CustomDataLayer *cdl;
	int index;

 	index= CustomData_get_active_layer_index(&me->fdata, CD_MCOL);
	cdl= (index == -1)? NULL: &me->fdata.layers[index];

	if(!cdl)
		return OPERATOR_CANCELLED;

	delete_customdata_layer(C, ob, cdl);

	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}

void MESH_OT_vertex_color_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Vertex Color";
	ot->description= "Remove vertex color layer.";
	ot->idname= "MESH_OT_vertex_color_remove";
	
	/* api callbacks */
	ot->exec= vertex_color_remove_exec;
	ot->poll= layers_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/*********************** sticky operators ************************/

static int sticky_add_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Mesh *me= ob->data;

	if(me->msticky)
		return OPERATOR_CANCELLED;

	// XXX RE_make_sticky();

	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}

void MESH_OT_sticky_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Sticky";
	ot->description= "Add sticky UV texture layer.";
	ot->idname= "MESH_OT_sticky_add";
	
	/* api callbacks */
	ot->poll= layers_poll;
	ot->exec= sticky_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int sticky_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Mesh *me= ob->data;

	if(!me->msticky)
		return OPERATOR_CANCELLED;

	CustomData_free_layer_active(&me->vdata, CD_MSTICKY, me->totvert);
	me->msticky= NULL;

	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}

void MESH_OT_sticky_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Sticky";
	ot->description= "Remove sticky UV texture layer.";
	ot->idname= "MESH_OT_sticky_remove";
	
	/* api callbacks */
	ot->poll= layers_poll;
	ot->exec= sticky_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

