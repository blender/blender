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
#include "BKE_scene.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

static Tex *rna_Main_add_texture(Main *bmain, char *name)
{
	return add_texture(name);
}

/* TODO: remove texture? */

static Image *rna_Main_add_image(Main *bmain, char *filename)
{
	return BKE_add_image_file(filename, 0);
}

static Camera *rna_Main_cameras_new(bContext *C, char* name)
{
	return add_camera(name);
}
static void rna_Main_cameras_remove(bContext *C, ReportList *reports, struct Camera *camera)
{
	Main *bmain= CTX_data_main(C);
	if(camera->id.us == 0)
		free_libblock(&bmain->camera, camera);
	else
		BKE_reportf(reports, RPT_ERROR, "Camera \"%s\" must have zero users to be removed, found %d.", camera->id.name+2, camera->id.us);

	/* XXX python now has invalid pointer? */
}

static Scene *rna_Main_scenes_new(bContext *C, char* name)
{
	return add_scene(name);
}
static void rna_Main_scenes_remove(bContext *C, ReportList *reports, struct Scene *scene)
{
	Main *bmain= CTX_data_main(C);
	free_libblock(&bmain->scene, scene);
}

static Object *rna_Main_objects_new(bContext *C, char* name, int type)
{
	Object *ob= add_only_object(type, name);
	ob->id.us--;
	return ob;
}
static void rna_Main_objects_remove(bContext *C, ReportList *reports, struct Object *object)
{
	/*
	  NOTE: the following example shows when this function should _not_ be called

	  ob = bpy.data.add_object()
	  scene.add_object(ob)

	  # ob is freed here
	  scene.remove_object(ob)

	  # don't do this since ob is already freed!
	  bpy.data.remove_object(ob)
	*/
	Main *bmain= CTX_data_main(C);
	if(object->id.us == 0)
		free_libblock(&bmain->object, object);
	else
		BKE_reportf(reports, RPT_ERROR, "Object \"%s\" must have zero users to be removed, found %d.", object->id.name+2, object->id.us);
}

static Material *rna_Main_materials_new(bContext *C, char* name)
{
	return add_material(name);
}
static void rna_Main_materials_remove(bContext *C, ReportList *reports, struct Material *material)
{
	Main *bmain= CTX_data_main(C);
	if(material->id.us == 0)
		free_libblock(&bmain->mat, material);
	else
		BKE_reportf(reports, RPT_ERROR, "Material \"%s\" must have zero users to be removed, found %d.", material->id.name+2, material->id.us);

	/* XXX python now has invalid pointer? */
}

static Mesh *rna_Main_meshes_new(bContext *C, char* name)
{
	Mesh *me= add_mesh(name);
	me->id.us--;
	return me;
}
static void rna_Main_meshes_remove(bContext *C, ReportList *reports, Mesh *mesh)
{
	Main *bmain= CTX_data_main(C);
	if(mesh->id.us == 0)
		free_libblock(&bmain->mesh, mesh);
	else
		BKE_reportf(reports, RPT_ERROR, "Mesh \"%s\" must have zero users to be removed, found %d.", mesh->id.name+2, mesh->id.us);

	/* XXX python now has invalid pointer? */
}

static Lamp *rna_Main_lamps_new(bContext *C, char* name)
{
	Lamp *lamp= add_lamp(name);
	lamp->id.us--;
	return lamp;
}
static void rna_Main_lamps_remove(bContext *C, ReportList *reports, Lamp *lamp)
{
	Main *bmain= CTX_data_main(C);
	if(lamp->id.us == 0)
		free_libblock(&bmain->lamp, lamp);
	else
		BKE_reportf(reports, RPT_ERROR, "Lamp \"%s\" must have zero users to be removed, found %d.", lamp->id.name+2, lamp->id.us);

	/* XXX python now has invalid pointer? */
}

static bArmature *rna_Main_armatures_new(bContext *C, char* name)
{
	bArmature *arm= add_armature(name);
	arm->id.us--;
	return arm;
}
static void rna_Main_armatures_remove(bContext *C, ReportList *reports, bArmature *arm)
{
	Main *bmain= CTX_data_main(C);
	if(arm->id.us == 0)
		free_libblock(&bmain->armature, arm);
	else
		BKE_reportf(reports, RPT_ERROR, "Armature \"%s\" must have zero users to be removed, found %d.", arm->id.name+2, arm->id.us);

	/* XXX python now has invalid pointer? */
}

#else

void RNA_api_main(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

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

void RNA_def_main_cameras(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainCameras");
	srna= RNA_def_struct(brna, "MainCameras", NULL);
	RNA_def_struct_sdna(srna, "Camera");
	RNA_def_struct_ui_text(srna, "Main Cameras", "Collection of cameras.");

	func= RNA_def_function(srna, "new", "rna_Main_cameras_new");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Add a new camera to the main database");
	parm= RNA_def_string(func, "name", "Camera", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "camera", "Camera", "", "New camera datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_cameras_remove");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a camera from the current blendfile.");
	parm= RNA_def_pointer(func, "camera", "Camera", "", "Camera to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

void RNA_def_main_scenes(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainScenes");
	srna= RNA_def_struct(brna, "MainScenes", NULL);
	RNA_def_struct_sdna(srna, "Scene");
	RNA_def_struct_ui_text(srna, "Main Scenes", "Collection of scenes.");

	func= RNA_def_function(srna, "new", "rna_Main_scenes_new");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Add a new scene to the main database");
	parm= RNA_def_string(func, "name", "Scene", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "scene", "Scene", "", "New scene datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_scenes_remove");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	parm= RNA_def_pointer(func, "scene", "Scene", "", "Scene to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_function_ui_description(func, "Remove a scene from the current blendfile.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

void RNA_def_main_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainObjects");
	srna= RNA_def_struct(brna, "MainObjects", NULL);
	RNA_def_struct_sdna(srna, "Object");
	RNA_def_struct_ui_text(srna, "Main Objects", "Collection of objects.");

	func= RNA_def_function(srna, "new", "rna_Main_objects_new");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Add a new object to the main database");
	parm= RNA_def_string(func, "name", "Object", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "type", object_type_items, 0, "", "Type of Object.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* return type */
	parm= RNA_def_pointer(func, "object", "Object", "", "New object datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_objects_remove");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	parm= RNA_def_pointer(func, "object", "Object", "", "Object to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_function_ui_description(func, "Remove a object from the current blendfile.");
}

void RNA_def_main_materials(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainMaterials");
	srna= RNA_def_struct(brna, "MainMaterials", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_ui_text(srna, "Main Material", "Collection of materials.");

	func= RNA_def_function(srna, "new", "rna_Main_materials_new");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Add a new material to the main database");
	parm= RNA_def_string(func, "name", "Material", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "material", "Material", "", "New material datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_materials_remove");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a material from the current blendfile.");
	parm= RNA_def_pointer(func, "material", "Material", "", "Material to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}
void RNA_def_main_node_groups(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_meshes(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainMeshes");
	srna= RNA_def_struct(brna, "MainMeshes", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "Main Meshes", "Collection of meshes.");

	func= RNA_def_function(srna, "new", "rna_Main_meshes_new");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Add a new mesh to the main database");
	parm= RNA_def_string(func, "name", "Mesh", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "mesh", "Mesh", "", "New mesh datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_meshes_remove");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a mesh from the current blendfile.");
	parm= RNA_def_pointer(func, "mesh", "Mesh", "", "Mesh to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}
void RNA_def_main_lamps(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainLamps");
	srna= RNA_def_struct(brna, "MainLamps", NULL);
	RNA_def_struct_sdna(srna, "Lamp");
	RNA_def_struct_ui_text(srna, "Main Lamps", "Collection of lamps.");

	func= RNA_def_function(srna, "new", "rna_Main_lamps_new");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Add a new lamp to the main database");
	parm= RNA_def_string(func, "name", "Lamp", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "lamp", "Lamp", "", "New lamp datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_lamps_remove");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a lamp from the current blendfile.");
	parm= RNA_def_pointer(func, "lamp", "Lamp", "", "Lamp to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}
void RNA_def_main_libraries(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_screens(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_window_managers(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_images(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_lattices(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_curves(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_metaballs(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_vfonts(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_textures(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_brushes(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_worlds(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_groups(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_texts(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_sounds(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_armatures(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainArmatures");
	srna= RNA_def_struct(brna, "MainArmatures", NULL);
	RNA_def_struct_sdna(srna, "Armature");
	RNA_def_struct_ui_text(srna, "Main Armatures", "Collection of armatures.");

	func= RNA_def_function(srna, "new", "rna_Main_armatures_new");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Add a new armature to the main database");
	parm= RNA_def_string(func, "name", "Armature", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "armature", "Armature", "", "New armature datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_armatures_remove");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a armature from the current blendfile.");
	parm= RNA_def_pointer(func, "armature", "Armature", "", "Armature to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}
void RNA_def_main_actions(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_particles(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_gpencil(BlenderRNA *brna, PropertyRNA *cprop)
{

}

#endif

