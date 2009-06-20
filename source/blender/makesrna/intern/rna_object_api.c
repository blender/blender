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

#include "RNA_define.h"
#include "RNA_types.h"

#include "DNA_object_types.h"

#ifdef RNA_RUNTIME

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_anim.h"
#include "BKE_report.h"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_meshdata_types.h"

#define OBJECT_API_PROP_DUPLILIST "dupli_list"

/* copied from init_render_mesh (render code) */
static Mesh *rna_Object_create_render_mesh(Object *ob, bContext *C, ReportList *reports, int apply_matrix, float *matrix)
{
	CustomDataMask mask = CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL;
	DerivedMesh *dm;
	Mesh *me;
	Scene *sce;
	int a;
	MVert *mvert;

	sce= CTX_data_scene(C);
	
	/* TODO: other types */
	if(ob->type != OB_MESH) {
		BKE_report(reports, RPT_ERROR, "Object should be of type MESH.");
		return NULL;
	}
	
	dm= mesh_create_derived_render(sce, ob, mask);

	if(!dm) {
		/* TODO: report */
		return NULL;
	}

	me= add_mesh("tmp_render_mesh");
	me->id.us--; /* we don't assign it to anything */
	DM_to_mesh(dm, me);
	dm->release(dm);

	if (apply_matrix) {
		float *mat = ob->obmat;

		if (matrix) {
			/* apply custom matrix */
			mat = matrix;
		}

		/* is this really that simple? :) */
		for(a= 0, mvert= me->mvert; a < me->totvert; a++, mvert++) {
			Mat4MulVecfl(ob->obmat, mvert->co);
		}
	}

	return me;
}

/* When no longer needed, duplilist should be freed with Object.free_duplilist */
static void rna_Object_create_duplilist(Object *ob, bContext *C, ReportList *reports)
{
	PointerRNA obptr;
	PointerRNA dobptr;
	Scene *sce;
	DupliObject *dob;
	PropertyRNA *prop;

	if (!(ob->transflag & OB_DUPLI)) {
		BKE_report(reports, RPT_ERROR, "Object does not have duplis.");
		return;
	}

	sce= CTX_data_scene(C);

	RNA_id_pointer_create(&ob->id, &obptr);

	if (!(prop= RNA_struct_find_property(&obptr, OBJECT_API_PROP_DUPLILIST))) {
		// hint: all Objects will now have this property defined
		prop= RNA_def_collection_runtime(obptr.type, OBJECT_API_PROP_DUPLILIST, &RNA_DupliObject, "Dupli list", "List of object's duplis");
	}

	RNA_property_collection_clear(&obptr, prop);
	ob->duplilist= object_duplilist(sce, ob);

	for(dob= (DupliObject*)ob->duplilist->first; dob; dob= dob->next) {
		RNA_pointer_create(NULL, &RNA_Object, dob, &dobptr);
		RNA_property_collection_add(&obptr, prop, &dobptr);
		dob = dob->next;
	}

	/* ob->duplilist should now be freed with Object.free_duplilist */
}

static void rna_Object_free_duplilist(Object *ob, ReportList *reports)
{
	PointerRNA obptr;
	PropertyRNA *prop;

	RNA_id_pointer_create(&ob->id, &obptr);

	if (!(prop= RNA_struct_find_property(&obptr, OBJECT_API_PROP_DUPLILIST))) {
		BKE_report(reports, RPT_ERROR, "Object has no duplilist property.");
		return;
	}

	RNA_property_collection_clear(&obptr, prop);

	free_object_duplilist(ob->duplilist);
	ob->duplilist= NULL;
}

#else

void RNA_api_object(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	/* copied from rna_def_object */
	static EnumPropertyItem object_type_items[] = {
		{OB_EMPTY, "EMPTY", 0, "Empty", ""},
		{OB_MESH, "MESH", 0, "Mesh", ""},
		{OB_CURVE, "CURVE", 0, "Curve", ""},
		{OB_SURF, "SURFACE", 0, "Surface", ""},
		{OB_FONT, "TEXT", 0, "Text", ""},
		{OB_MBALL, "META", 0, "Meta", ""},
		{OB_LAMP, "LAMP", 0, "Lamp", ""},
		{OB_CAMERA, "CAMERA", 0, "Camera", ""},
		{OB_WAVE, "WAVE", 0, "Wave", ""},
		{OB_LATTICE, "LATTICE", 0, "Lattice", ""},
		{OB_ARMATURE, "ARMATURE", 0, "Armature", ""},
		{0, NULL, 0, NULL, NULL}};

	func= RNA_def_function(srna, "create_render_mesh", "rna_Object_create_render_mesh");
	RNA_def_function_ui_description(func, "Create a Mesh datablock with all modifiers applied.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	parm= RNA_def_boolean(func, "apply_matrix", 0, "", "True if object matrix or custom matrix should be applied to geometry.");
	RNA_def_property_clear_flag(parm, PROP_REQUIRED);
	parm= RNA_def_float_matrix(func, "custom_matrix", 16, NULL, 0.0f, 0.0f, "", "Optional custom matrix to apply.", 0.0f, 0.0f);
	RNA_def_property_clear_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "mesh", "Mesh", "", "Mesh created from object, remove it if it is only used for export.");
	RNA_def_function_return(func, parm);
	
	func= RNA_def_function(srna, "create_dupli_list", "rna_Object_create_duplilist");
	RNA_def_function_ui_description(func, "Create a list of dupli objects for this object. When no longer needed, it should be freed with free_dupli_list.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);

	func= RNA_def_function(srna, "free_dupli_list", "rna_Object_free_duplilist");
	RNA_def_function_ui_description(func, "Free the list of dupli objects.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
}

#endif

