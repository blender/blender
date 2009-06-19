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

#ifdef RNA_RUNTIME

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_anim.h"
#include "BKE_report.h"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#define OBJECT_API_PROP_DUPLILIST "dupli_list"

/* copied from init_render_mesh (render code) */
Mesh *rna_Object_create_render_mesh(Object *ob, Scene *scene)
{
	CustomDataMask mask = CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL;
	DerivedMesh *dm;
	Mesh *me;
	
	/* TODO: other types */
	if(ob->type != OB_MESH)
		return NULL;
	
	dm= mesh_create_derived_render(scene, ob, mask);

	if(!dm)
		return NULL;

	me= add_mesh("tmp_render_mesh");
	me->id.us--; /* we don't assign it to anything */
	DM_to_mesh(dm, me);
	dm->release(dm);

	return me;
}

/* When no longer needed, duplilist should be freed with Object.free_duplilist */
void rna_Object_create_duplilist(Object *ob, bContext *C, ReportList *reports)
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
		prop= RNA_def_collection_runtime(obptr.type, OBJECT_API_PROP_DUPLILIST, "DupliObject", "Dupli list", "List of object's duplis");
	}

	RNA_property_collection_clear(&obptr, prop);
	ob->duplilist= object_duplilist(sce, ob);

	for(dob= (DupliObject*)ob->duplilist->first; dob; dob= dob->next) {
		RNA_pointer_create(NULL, &RNA_Object, dob, &dobptr);
		RNA_property_collection_add(&obptr, prop, &dobptr);
		dob = dob->next;
	}

	/* ob->duplilist should now be freed with Object.free_duplilist */

	return *((CollectionPropertyRNA*)prop);
}

void rna_Object_free_duplilist(Object *ob, ReportList *reports)
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

	func= RNA_def_function(srna, "create_render_mesh", "rna_Object_create_render_mesh");
	RNA_def_function_ui_description(func, "Create a Mesh datablock with all modifiers applied.");
	parm= RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
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

