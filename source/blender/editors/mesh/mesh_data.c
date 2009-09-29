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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_material_types.h"
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
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_report.h"

#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_edgehash.h"

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

int ED_mesh_uv_texture_add(bContext *C, Scene *scene, Object *ob, Mesh *me)
{
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

	return 1;
}

int ED_mesh_uv_texture_remove(bContext *C, Object *ob, Mesh *me)
{
	CustomDataLayer *cdl;
	int index;

 	index= CustomData_get_active_layer_index(&me->fdata, CD_MTFACE);
	cdl= (index == -1)? NULL: &me->fdata.layers[index];

	if(!cdl)
		return 0;

	delete_customdata_layer(C, ob, cdl);
	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return 1;
}

int ED_mesh_color_add(bContext *C, Scene *scene, Object *ob, Mesh *me)
{
	EditMesh *em;
	MCol *mcol;
	int layernum;

	if(me->edit_mesh) {
		em= me->edit_mesh;

		layernum= CustomData_number_of_layers(&em->fdata, CD_MCOL);
		if(layernum >= MAX_MCOL)
			return 0;

		EM_add_data_layer(em, &em->fdata, CD_MCOL);
		CustomData_set_layer_active(&em->fdata, CD_MCOL, layernum);
	}
	else {
		layernum= CustomData_number_of_layers(&me->fdata, CD_MCOL);
		if(layernum >= MAX_MCOL)
			return 0;

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

	return 1;
}

int ED_mesh_color_remove(bContext *C, Object *ob, Mesh *me)
{
	CustomDataLayer *cdl;
	int index;

 	index= CustomData_get_active_layer_index(&me->fdata, CD_MCOL);
	cdl= (index == -1)? NULL: &me->fdata.layers[index];

	if(!cdl)
		return 0;

	delete_customdata_layer(C, ob, cdl);
	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return 1;
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
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Mesh *me= ob->data;

	if(!ED_mesh_uv_texture_add(C, scene, ob, me))
		return OPERATOR_CANCELLED;

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

	if(!ED_mesh_uv_texture_remove(C, ob, me))
		return OPERATOR_CANCELLED;

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

	if(!ED_mesh_color_add(C, scene, ob, me))
		return OPERATOR_CANCELLED;

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

	if(!ED_mesh_color_remove(C, ob, me))
		return OPERATOR_CANCELLED;

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

/************************** Add Geometry Layers *************************/

static void mesh_calc_edges(Mesh *mesh)
{
	CustomData edata;
	EdgeHashIterator *ehi;
	MFace *mf = mesh->mface;
	MEdge *med;
	EdgeHash *eh = BLI_edgehash_new();
	int i, *index, totedge, totface = mesh->totface;

	for (i = 0; i < totface; i++, mf++) {
		if (!BLI_edgehash_haskey(eh, mf->v1, mf->v2))
			BLI_edgehash_insert(eh, mf->v1, mf->v2, NULL);
		if (!BLI_edgehash_haskey(eh, mf->v2, mf->v3))
			BLI_edgehash_insert(eh, mf->v2, mf->v3, NULL);
		
		if (mf->v4) {
			if (!BLI_edgehash_haskey(eh, mf->v3, mf->v4))
				BLI_edgehash_insert(eh, mf->v3, mf->v4, NULL);
			if (!BLI_edgehash_haskey(eh, mf->v4, mf->v1))
				BLI_edgehash_insert(eh, mf->v4, mf->v1, NULL);
		} else {
			if (!BLI_edgehash_haskey(eh, mf->v3, mf->v1))
				BLI_edgehash_insert(eh, mf->v3, mf->v1, NULL);
		}
	}

	totedge = BLI_edgehash_size(eh);

	/* write new edges into a temporary CustomData */
	memset(&edata, 0, sizeof(edata));
	CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);

	ehi = BLI_edgehashIterator_new(eh);
	med = CustomData_get_layer(&edata, CD_MEDGE);
	for(i = 0; !BLI_edgehashIterator_isDone(ehi);
	    BLI_edgehashIterator_step(ehi), ++i, ++med, ++index) {
		BLI_edgehashIterator_getKey(ehi, (int*)&med->v1, (int*)&med->v2);

		med->flag = ME_EDGEDRAW|ME_EDGERENDER;
	}
	BLI_edgehashIterator_free(ehi);

	/* free old CustomData and assign new one */
	CustomData_free(&mesh->edata, mesh->totedge);
	mesh->edata = edata;
	mesh->totedge = totedge;

	mesh->medge = CustomData_get_layer(&mesh->edata, CD_MEDGE);

	BLI_edgehash_free(eh, NULL);
}

void ED_mesh_update(Mesh *mesh, bContext *C)
{
	if(mesh->totface && mesh->totedge == 0)
		mesh_calc_edges(mesh);

	mesh_calc_normals(mesh->mvert, mesh->totvert, mesh->mface, mesh->totface, NULL);

	DAG_id_flush_update(&mesh->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, mesh);
}

static void mesh_add_verts(Mesh *mesh, int len)
{
	CustomData vdata;
	MVert *mvert;
	int i, totvert;

	if(len == 0)
		return;

	totvert= mesh->totvert + len;
	CustomData_copy(&mesh->vdata, &vdata, CD_MASK_MESH, CD_DEFAULT, totvert);
	CustomData_copy_data(&mesh->vdata, &vdata, 0, 0, mesh->totvert);

	if(!CustomData_has_layer(&vdata, CD_MVERT))
		CustomData_add_layer(&vdata, CD_MVERT, CD_CALLOC, NULL, totvert);

	CustomData_free(&mesh->vdata, mesh->totvert);
	mesh->vdata= vdata;
	mesh_update_customdata_pointers(mesh);

	/* scan the input list and insert the new vertices */

	mvert= &mesh->mvert[mesh->totvert];
	for(i=0; i<len; i++, mvert++)
		mvert->flag |= SELECT;

	/* set final vertex list size */
	mesh->totvert= totvert;
}

void ED_mesh_transform(Mesh *me, float *mat)
{
	int i;
	MVert *mvert= me->mvert;

	for(i= 0; i < me->totvert; i++, mvert++)
		Mat4MulVecfl((float (*)[4])mat, mvert->co);

	mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
}

static void mesh_add_edges(Mesh *mesh, int len)
{
	CustomData edata;
	MEdge *medge;
	int i, totedge;

	if(len == 0)
		return;

	totedge= mesh->totedge+len;

	/* update customdata  */
	CustomData_copy(&mesh->edata, &edata, CD_MASK_MESH, CD_DEFAULT, totedge);
	CustomData_copy_data(&mesh->edata, &edata, 0, 0, mesh->totedge);

	if(!CustomData_has_layer(&edata, CD_MEDGE))
		CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);

	CustomData_free(&mesh->edata, mesh->totedge);
	mesh->edata= edata;
	mesh_update_customdata_pointers(mesh);

	/* set default flags */
	medge= &mesh->medge[mesh->totedge];
	for(i=0; i<len; i++, medge++)
		medge->flag= ME_EDGEDRAW|ME_EDGERENDER|SELECT;

	mesh->totedge= totedge;
}

static void mesh_add_faces(Mesh *mesh, int len)
{
	CustomData fdata;
	MFace *mface;
	int i, totface;

	if(len == 0)
		return;

	totface= mesh->totface + len;	/* new face count */

	/* update customdata */
	CustomData_copy(&mesh->fdata, &fdata, CD_MASK_MESH, CD_DEFAULT, totface);
	CustomData_copy_data(&mesh->fdata, &fdata, 0, 0, mesh->totface);

	if(!CustomData_has_layer(&fdata, CD_MFACE))
		CustomData_add_layer(&fdata, CD_MFACE, CD_CALLOC, NULL, totface);

	CustomData_free(&mesh->fdata, mesh->totface);
	mesh->fdata= fdata;
	mesh_update_customdata_pointers(mesh);

	/* set default flags */
	mface= &mesh->mface[mesh->totface];
	for(i=0; i<len; i++, mface++)
		mface->flag= SELECT;

	mesh->totface= totface;
}

void ED_mesh_geometry_add(Mesh *mesh, ReportList *reports, int verts, int edges, int faces)
{
	if(mesh->edit_mesh) {
		BKE_report(reports, RPT_ERROR, "Can't add geometry in edit mode.");
		return;
	}

	if(verts)
		mesh_add_verts(mesh, verts);
	if(edges)
		mesh_add_edges(mesh, edges);
	if(faces)
		mesh_add_faces(mesh, faces);
}

void ED_mesh_calc_normals(Mesh *me)
{
	mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
}

void ED_mesh_material_add(Mesh *me, Material *ma)
{
	int i;
	int totcol = me->totcol + 1;
	Material **mat;

	/* don't add if mesh already has it */
	for(i = 0; i < me->totcol; i++)
		if(me->mat[i] == ma)
			return;

	mat= MEM_callocN(sizeof(void*)*totcol, "newmatar");

	if(me->totcol) memcpy(mat, me->mat, sizeof(void*) * me->totcol);
	if(me->mat) MEM_freeN(me->mat);

	me->mat = mat;
	me->mat[me->totcol++] = ma;
	ma->id.us++;

	test_object_materials((ID*)me);
}

