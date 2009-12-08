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
#include "RNA_enum_types.h"

#ifdef RNA_RUNTIME

#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_armature.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_image.h"
#include "BKE_texture.h"

#include "DNA_armature_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

static Mesh *rna_Main_add_mesh(Main *bmain, char *name)
{
	Mesh *me= add_mesh(name);
	me->id.us--;
	return me;
}

static void rna_Main_remove_mesh(Main *bmain, ReportList *reports, Mesh *me)
{
	if(me->id.us == 0)
		free_libblock(&bmain->mesh, me);
	else
		BKE_report(reports, RPT_ERROR, "Mesh must have zero users to be removed.");
	
	/* XXX python now has invalid pointer? */
}

static void rna_Main_remove_armature(Main *bmain, ReportList *reports, bArmature *arm)
{
	if(arm->id.us == 0)
		free_libblock(&bmain->armature, arm);
	else
		BKE_report(reports, RPT_ERROR, "Armature must have zero users to be removed.");

	/* XXX python now has invalid pointer? */
}

static bArmature *rna_Main_add_armature(Main *bmain, char *name)
{
	bArmature *arm= add_armature(name);
	arm->id.us--;
	return arm;
}

static Lamp *rna_Main_add_lamp(Main *bmain, char *name)
{
	Lamp *la= add_lamp(name);
	la->id.us--;
	return la;
}

/*
static void rna_Main_remove_lamp(Main *bmain, ReportList *reports, Lamp *la)
{
	if(la->id.us == 0)
		free_libblock(&main->lamp, la);
	else
		BKE_report(reports, RPT_ERROR, "Lamp must have zero users to be removed.");
}
*/

static Object* rna_Main_add_object(Main *bmain, int type, char *name)
{
	Object *ob= add_only_object(type, name);
	ob->id.us--;
	return ob;
}

/*
  NOTE: the following example shows when this function should _not_ be called

  ob = bpy.data.add_object()
  scene.add_object(ob)

  # ob is freed here
  scene.remove_object(ob)

  # don't do this since ob is already freed!
  bpy.data.remove_object(ob)
*/
static void rna_Main_remove_object(Main *bmain, ReportList *reports, Object *ob)
{
	if(ob->id.us == 0)
		free_libblock(&bmain->object, ob);
	else
		BKE_report(reports, RPT_ERROR, "Object must have zero users to be removed.");
}

static Material *rna_Main_add_material(Main *bmain, char *name)
{
	return add_material(name);
}

/* TODO: remove material? */

struct Tex *rna_Main_add_texture(Main *bmain, char *name)
{
	return add_texture(name);
}

/* TODO: remove texture? */

struct Image *rna_Main_add_image(Main *bmain, char *filename)
{
	return BKE_add_image_file(filename, 0);
}

#else

void RNA_api_main(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "add_object", "rna_Main_add_object");
	RNA_def_function_ui_description(func, "Add a new object.");
	parm= RNA_def_enum(func, "type", object_type_items, 0, "", "Type of Object.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "name", "Object", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "object", "Object", "", "New object.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove_object", "rna_Main_remove_object");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove an object if it has zero users.");
	parm= RNA_def_pointer(func, "object", "Object", "", "Object to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "add_mesh", "rna_Main_add_mesh");
	RNA_def_function_ui_description(func, "Add a new mesh.");
	parm= RNA_def_string(func, "name", "Mesh", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "mesh", "Mesh", "", "New mesh.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove_mesh", "rna_Main_remove_mesh");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a mesh if it has zero users.");
	parm= RNA_def_pointer(func, "mesh", "Mesh", "", "Mesh to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "add_armature", "rna_Main_add_armature");
	RNA_def_function_ui_description(func, "Add a new armature.");
	parm= RNA_def_string(func, "name", "Armature", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "armature", "Armature", "", "New armature.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove_armature", "rna_Main_remove_armature");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove an armature if it has zero users.");
	parm= RNA_def_pointer(func, "armature", "Armature", "", "Armature to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "add_lamp", "rna_Main_add_lamp");
	RNA_def_function_ui_description(func, "Add a new lamp.");
	parm= RNA_def_string(func, "name", "Lamp", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "mesh", "Lamp", "", "New lamp.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "add_material", "rna_Main_add_material");
	RNA_def_function_ui_description(func, "Add a new material.");
	parm= RNA_def_string(func, "name", "Material", 0, "", "New name for the datablock."); /* optional */
	parm= RNA_def_pointer(func, "material", "Material", "", "New material.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "add_texture", "rna_Main_add_texture");
	RNA_def_function_ui_description(func, "Add a new texture.");
	parm= RNA_def_string(func, "name", "Tex", 0, "", "New name for the datablock."); /* optional */
	parm= RNA_def_pointer(func, "texture", "Texture", "", "New texture.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "add_image", "rna_Main_add_image");
	RNA_def_function_ui_description(func, "Add a new image.");
	parm= RNA_def_string(func, "filename", "", 0, "", "Filename to load image from.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "image", "Image", "", "New image.");
	RNA_def_function_return(func, parm);

}

#endif

