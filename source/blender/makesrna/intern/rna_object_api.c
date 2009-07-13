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
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "DNA_object_types.h"

#include "BLO_sys_types.h" /* needed for intptr_t used in ED_mesh.h */

#include "ED_mesh.h"

/* parameter to rna_Object_create_mesh */
typedef enum CreateMeshType {
	CREATE_MESH_PREVIEW = 0,
	CREATE_MESH_RENDER = 1
} CreateMeshType;

#ifdef RNA_RUNTIME

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_anim.h"
#include "BKE_report.h"
#include "BKE_depsgraph.h"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_arithb.h"

/* copied from init_render_mesh (render code) */
static Mesh *rna_Object_create_mesh(Object *ob, bContext *C, ReportList *reports, int type)
{
	/* CustomDataMask mask = CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL; */
	CustomDataMask mask = CD_MASK_MESH; /* this seems more suitable, exporter,
										   for example, needs CD_MASK_MDEFORMVERT */
	DerivedMesh *dm;
	Mesh *me;
	Scene *sce;

	sce= CTX_data_scene(C);
	
	/* TODO: other types */
	if(ob->type != OB_MESH) {
		BKE_report(reports, RPT_ERROR, "Object should be of type MESH.");
		return NULL;
	}

	if (type == CREATE_MESH_PREVIEW) {
		dm= mesh_create_derived_view(sce, ob, mask);
	}
	else {
		dm= mesh_create_derived_render(sce, ob, mask);
	}

	if(!dm) {
		/* TODO: report */
		return NULL;
	}

	me= add_mesh("tmp_render_mesh");
	me->id.us--; /* we don't assign it to anything */
	DM_to_mesh(dm, me);
	dm->release(dm);

	return me;
}

/* When no longer needed, duplilist should be freed with Object.free_duplilist */
static void rna_Object_create_duplilist(Object *ob, bContext *C, ReportList *reports)
{
	if (!(ob->transflag & OB_DUPLI)) {
		BKE_report(reports, RPT_ERROR, "Object does not have duplis.");
		return;
	}

	/* free duplilist if a user forgets to */
	if (ob->duplilist) {
		BKE_reportf(reports, RPT_WARNING, "Object.dupli_list has not been freed.");

		free_object_duplilist(ob->duplilist);
		ob->duplilist= NULL;
	}

	ob->duplilist= object_duplilist(CTX_data_scene(C), ob);

	/* ob->duplilist should now be freed with Object.free_duplilist */
}

static void rna_Object_free_duplilist(Object *ob, ReportList *reports)
{
	if (ob->duplilist) {
		free_object_duplilist(ob->duplilist);
		ob->duplilist= NULL;
	}
}

static void rna_Object_convert_to_triface(Object *ob, bContext *C, ReportList *reports, Scene *sce)
{
	Mesh *me;
	int ob_editing = CTX_data_edit_object(C) == ob;

	if (ob->type != OB_MESH) {
		BKE_report(reports, RPT_ERROR, "Object should be of type MESH.");
		return;
	}

	me= (Mesh*)ob->data;

	if (!ob_editing)
		make_editMesh(sce, ob);

	/* select all */
	EM_set_flag_all(me->edit_mesh, SELECT);

	convert_to_triface(me->edit_mesh, 0);

	load_editMesh(sce, ob);

	if (!ob_editing)
		free_editMesh(me->edit_mesh);

	DAG_object_flush_update(sce, ob, OB_RECALC_DATA);
}

static bDeformGroup *rna_Object_add_vertex_group(Object *ob, char *group_name)
{
	return add_defgroup_name(ob, group_name);
}

static void rna_Object_add_vertex_to_group(Object *ob, int vertex_index, bDeformGroup *def, float weight, int assignmode)
{
	/* creates dverts if needed */
	add_vert_to_defgroup(ob, def, vertex_index, weight, assignmode);
}

static void rna_Object_dag_update(Object *ob, bContext *C)
{
	DAG_object_flush_update(CTX_data_scene(C), ob, OB_RECALC_DATA);
}

/*
static void rna_Mesh_assign_verts_to_group(Object *ob, bDeformGroup *group, int *indices, int totindex, float weight, int assignmode)
{
	if (ob->type != OB_MESH) {
		BKE_report(reports, RPT_ERROR, "Object should be of MESH type.");
		return;
	}

	Mesh *me = (Mesh*)ob->data;
	int group_index = get_defgroup_num(ob, group);
	if (group_index == -1) {
		BKE_report(reports, RPT_ERROR, "No deform groups assigned to mesh.");
		return;
	}

	if (assignmode != WEIGHT_REPLACE && assignmode != WEIGHT_ADD && assignmode != WEIGHT_SUBTRACT) {
		BKE_report(reports, RPT_ERROR, "Bad assignment mode." );
		return;
	}

	// makes a set of dVerts corresponding to the mVerts
	if (!me->dvert) 
		create_dverts(&me->id);

	// loop list adding verts to group 
	for (i= 0; i < totindex; i++) {
		if(i < 0 || i >= me->totvert) {
			BKE_report(reports, RPT_ERROR, "Bad vertex index in list.");
			return;
		}

		add_vert_defnr(ob, group_index, i, weight, assignmode);
	}
}
*/

#else

void RNA_api_object(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	static EnumPropertyItem mesh_type_items[] = {
		{CREATE_MESH_PREVIEW, "PREVIEW", 0, "Preview", "Apply preview settings."},
		{CREATE_MESH_RENDER, "RENDER", 0, "Render", "Apply render settings."},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem assign_mode_items[] = {
		{WEIGHT_REPLACE, "REPLACE", 0, "Replace", "Replace."}, /* TODO: more meaningful descriptions */
		{WEIGHT_ADD, "ADD", 0, "Add", "Add."},
		{WEIGHT_SUBTRACT, "SUBTRACT", 0, "Subtract", "Subtract."},
		{0, NULL, 0, NULL, NULL}
	};

	/* mesh */
	func= RNA_def_function(srna, "create_mesh", "rna_Object_create_mesh");
	RNA_def_function_ui_description(func, "Create a Mesh datablock with all modifiers applied.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	parm= RNA_def_enum(func, "type", mesh_type_items, 0, "", "Type of mesh settings to apply.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "mesh", "Mesh", "", "Mesh created from object, remove it if it is only used for export.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "convert_to_triface", "rna_Object_convert_to_triface");
	RNA_def_function_ui_description(func, "Convert all mesh faces to triangles.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	parm= RNA_def_pointer(func, "scene", "Scene", "", "Scene where the object belongs.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* duplis */
	func= RNA_def_function(srna, "create_dupli_list", "rna_Object_create_duplilist");
	RNA_def_function_ui_description(func, "Create a list of dupli objects for this object, needs to be freed manually with free_dupli_list.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);

	func= RNA_def_function(srna, "free_dupli_list", "rna_Object_free_duplilist");
	RNA_def_function_ui_description(func, "Free the list of dupli objects.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	/* vertex groups */
	func= RNA_def_function(srna, "add_vertex_group", "rna_Object_add_vertex_group");
	RNA_def_function_ui_description(func, "Add vertex group to object.");
	parm= RNA_def_string(func, "name", "Group", 0, "", "Vertex group name."); /* optional */
	parm= RNA_def_pointer(func, "group", "VertexGroup", "", "New vertex group.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "add_vertex_to_group", "rna_Object_add_vertex_to_group");
	RNA_def_function_ui_description(func, "Add vertex to a vertex group.");
	parm= RNA_def_int(func, "vertex_index", 0, 0, 0, "", "Vertex index.", 0, 0);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "group", "VertexGroup", "", "Vertex group to add vertex to.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_float(func, "weight", 0, 0.0f, 1.0f, "", "Vertex weight.", 0.0f, 1.0f);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "type", assign_mode_items, 0, "", "Vertex assign mode.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* DAG */
	func= RNA_def_function(srna, "dag_update", "rna_Object_dag_update");
	RNA_def_function_ui_description(func, "DAG update."); /* XXX describe better */
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
}

#endif

