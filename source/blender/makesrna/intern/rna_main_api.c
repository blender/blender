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

#include "BKE_utildefines.h"

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
#include "BKE_text.h"
#include "BKE_action.h"
#include "BKE_group.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"
#include "DNA_group_types.h"

#include "ED_screen.h"

Tex *rna_Main_add_texture(Main *bmain, char *name)
{
	return add_texture(name);
}

Image *rna_Main_add_image(Main *bmain, char *filename)
{
	return BKE_add_image_file(filename, 0);
}

Camera *rna_Main_cameras_new(Main *bmain, char* name)
{
	return add_camera(name);
}
void rna_Main_cameras_remove(Main *bmain, ReportList *reports, struct Camera *camera)
{
	if(ID_REAL_USERS(camera) == 0)
		free_libblock(&bmain->camera, camera);
	else
		BKE_reportf(reports, RPT_ERROR, "Camera \"%s\" must have zero users to be removed, found %d.", camera->id.name+2, ID_REAL_USERS(camera));

	/* XXX python now has invalid pointer? */
}

Scene *rna_Main_scenes_new(Main *bmain, char* name)
{
	return add_scene(name);
}
void rna_Main_scenes_remove(Main *bmain, bContext *C, ReportList *reports, struct Scene *scene)
{
	/* dont call free_libblock(...) directly */
	Scene *newscene;

	if(scene->id.prev)
		newscene= scene->id.prev;
	else if(scene->id.next)
		newscene= scene->id.next;
	else {
		BKE_reportf(reports, RPT_ERROR, "Scene \"%s\" is the last, cant ve removed.", scene->id.name+2);
		return;
	}

	ED_screen_set_scene(C, newscene);

	unlink_scene(bmain, scene, newscene);
}

Object *rna_Main_objects_new(Main *bmain, char* name, int type)
{
	Object *ob= add_only_object(type, name);
	ob->id.us--;
	return ob;
}
void rna_Main_objects_remove(Main *bmain, ReportList *reports, struct Object *object)
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
	if(ID_REAL_USERS(object) == 0)
		free_libblock(&bmain->object, object);
	else
		BKE_reportf(reports, RPT_ERROR, "Object \"%s\" must have zero users to be removed, found %d.", object->id.name+2, ID_REAL_USERS(object));
}

struct Material *rna_Main_materials_new(Main *bmain, char* name)
{
	return add_material(name);
}
void rna_Main_materials_remove(Main *bmain, ReportList *reports, struct Material *material)
{
	if(ID_REAL_USERS(material) == 0)
		free_libblock(&bmain->mat, material);
	else
		BKE_reportf(reports, RPT_ERROR, "Material \"%s\" must have zero users to be removed, found %d.", material->id.name+2, ID_REAL_USERS(material));

	/* XXX python now has invalid pointer? */
}

Mesh *rna_Main_meshes_new(Main *bmain, char* name)
{
	Mesh *me= add_mesh(name);
	me->id.us--;
	return me;
}
void rna_Main_meshes_remove(Main *bmain, ReportList *reports, Mesh *mesh)
{
	if(ID_REAL_USERS(mesh) == 0)
		free_libblock(&bmain->mesh, mesh);
	else
		BKE_reportf(reports, RPT_ERROR, "Mesh \"%s\" must have zero users to be removed, found %d.", mesh->id.name+2, ID_REAL_USERS(mesh));

	/* XXX python now has invalid pointer? */
}

Lamp *rna_Main_lamps_new(Main *bmain, char* name)
{
	Lamp *lamp= add_lamp(name);
	lamp->id.us--;
	return lamp;
}
void rna_Main_lamps_remove(Main *bmain, ReportList *reports, Lamp *lamp)
{
	if(ID_REAL_USERS(lamp) == 0)
		free_libblock(&bmain->lamp, lamp);
	else
		BKE_reportf(reports, RPT_ERROR, "Lamp \"%s\" must have zero users to be removed, found %d.", lamp->id.name+2, ID_REAL_USERS(lamp));

	/* XXX python now has invalid pointer? */
}

Tex *rna_Main_textures_new(Main *bmain, char* name)
{
	Tex *tex= add_texture(name);
	tex->id.us--;
	return tex;
}
void rna_Main_textures_remove(Main *bmain, ReportList *reports, struct Tex *tex)
{
	if(ID_REAL_USERS(tex) == 0)
		free_libblock(&bmain->tex, tex);
	else
		BKE_reportf(reports, RPT_ERROR, "Texture \"%s\" must have zero users to be removed, found %d.", tex->id.name+2, ID_REAL_USERS(tex));
}

Group *rna_Main_groups_new(Main *bmain, char* name)
{
	return add_group(name);
}
void rna_Main_groups_remove(Main *bmain, ReportList *reports, Group *group)
{
	unlink_group(group);
	group->id.us= 0;
	free_libblock(&bmain->group, group);
	/* XXX python now has invalid pointer? */
}

Text *rna_Main_texts_new(Main *bmain, char* name)
{
	return add_empty_text(name);
}
void rna_Main_texts_remove(Main *bmain, ReportList *reports, Text *text)
{
	unlink_text(bmain, text);
	free_libblock(&bmain->text, text);
	/* XXX python now has invalid pointer? */
}
Text *rna_Main_texts_load(Main *bmain, ReportList *reports, char* path)
{
	Text *txt= add_text(path, bmain->name);
	if(txt==NULL)
		BKE_reportf(reports, RPT_ERROR, "Couldn't load text from path \"%s\".", path);

	return txt;
}

bArmature *rna_Main_armatures_new(Main *bmain, char* name)
{
	bArmature *arm= add_armature(name);
	arm->id.us--;
	return arm;
}
void rna_Main_armatures_remove(Main *bmain, ReportList *reports, bArmature *arm)
{
	if(ID_REAL_USERS(arm) == 0)
		free_libblock(&bmain->armature, arm);
	else
		BKE_reportf(reports, RPT_ERROR, "Armature \"%s\" must have zero users to be removed, found %d.", arm->id.name+2, ID_REAL_USERS(arm));

	/* XXX python now has invalid pointer? */
}

bAction *rna_Main_actions_new(Main *bmain, char* name)
{
	bAction *act= add_empty_action(name);
	act->id.us--;
	act->id.flag &= ~LIB_FAKEUSER;
	return act;
}
void rna_Main_actions_remove(Main *bmain, ReportList *reports, bAction *act)
{
	if(ID_REAL_USERS(act) == 0)
		free_libblock(&bmain->action, act);
	else
		BKE_reportf(reports, RPT_ERROR, "Action \"%s\" must have zero users to be removed, found %d.", act->id.name+2, ID_REAL_USERS(act));

	/* XXX python now has invalid pointer? */
}

#else

void RNA_api_main(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

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
	RNA_def_struct_ui_text(srna, "Main Cameras", "Collection of cameras.");

	func= RNA_def_function(srna, "new", "rna_Main_cameras_new");
	RNA_def_function_ui_description(func, "Add a new camera to the main database");
	parm= RNA_def_string(func, "name", "Camera", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "camera", "Camera", "", "New camera datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_cameras_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
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
	RNA_def_struct_ui_text(srna, "Main Scenes", "Collection of scenes.");

	func= RNA_def_function(srna, "new", "rna_Main_scenes_new");
	RNA_def_function_ui_description(func, "Add a new scene to the main database");
	parm= RNA_def_string(func, "name", "Scene", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "scene", "Scene", "", "New scene datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_scenes_remove");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
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
	RNA_def_struct_ui_text(srna, "Main Objects", "Collection of objects.");

	func= RNA_def_function(srna, "new", "rna_Main_objects_new");
	RNA_def_function_ui_description(func, "Add a new object to the main database");
	parm= RNA_def_string(func, "name", "Object", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "type", object_type_items, 0, "", "Type of Object.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* return type */
	parm= RNA_def_pointer(func, "object", "Object", "", "New object datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_objects_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
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
	RNA_def_struct_ui_text(srna, "Main Material", "Collection of materials.");

	func= RNA_def_function(srna, "new", "rna_Main_materials_new");
	RNA_def_function_ui_description(func, "Add a new material to the main database");
	parm= RNA_def_string(func, "name", "Material", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "material", "Material", "", "New material datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_materials_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
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
	RNA_def_struct_ui_text(srna, "Main Meshes", "Collection of meshes.");

	func= RNA_def_function(srna, "new", "rna_Main_meshes_new");
	RNA_def_function_ui_description(func, "Add a new mesh to the main database");
	parm= RNA_def_string(func, "name", "Mesh", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "mesh", "Mesh", "", "New mesh datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_meshes_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
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
	RNA_def_struct_ui_text(srna, "Main Lamps", "Collection of lamps.");

	func= RNA_def_function(srna, "new", "rna_Main_lamps_new");
	RNA_def_function_ui_description(func, "Add a new lamp to the main database");
	parm= RNA_def_string(func, "name", "Lamp", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "lamp", "Lamp", "", "New lamp datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_lamps_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
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
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainTextures");
	srna= RNA_def_struct(brna, "MainTextures", NULL);
	RNA_def_struct_ui_text(srna, "Main Textures", "Collection of groups.");

	func= RNA_def_function(srna, "new", "rna_Main_textures_new");
	RNA_def_function_ui_description(func, "Add a new texture to the main database");
	parm= RNA_def_string(func, "name", "Texture", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "texture", "Texture", "", "New texture datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_textures_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a texture from the current blendfile.");
	parm= RNA_def_pointer(func, "texture", "Texture", "", "Texture to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}
void RNA_def_main_brushes(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_worlds(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_groups(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainGroups");
	srna= RNA_def_struct(brna, "MainGroups", NULL);
	RNA_def_struct_ui_text(srna, "Main Groups", "Collection of groups.");

	func= RNA_def_function(srna, "new", "rna_Main_groups_new");
	RNA_def_function_ui_description(func, "Add a new group to the main database");
	parm= RNA_def_string(func, "name", "Group", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "group", "Group", "", "New group datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_groups_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a group from the current blendfile.");
	parm= RNA_def_pointer(func, "group", "Group", "", "Group to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}
void RNA_def_main_texts(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainTexts");
	srna= RNA_def_struct(brna, "MainTexts", NULL);
	RNA_def_struct_ui_text(srna, "Main Texts", "Collection of texts.");

	func= RNA_def_function(srna, "new", "rna_Main_texts_new");
	RNA_def_function_ui_description(func, "Add a new text to the main database");
	parm= RNA_def_string(func, "name", "Text", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "text", "Text", "", "New text datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_texts_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a text from the current blendfile.");
	parm= RNA_def_pointer(func, "text", "Text", "", "Text to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* load func */
	func= RNA_def_function(srna, "load", "rna_Main_texts_load");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Add a new text to the main database from a file");
	parm= RNA_def_string(func, "path", "Path", FILE_MAXDIR + FILE_MAXFILE, "", "path for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "text", "Text", "", "New text datablock.");
	RNA_def_function_return(func, parm);
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
	RNA_def_struct_ui_text(srna, "Main Armatures", "Collection of armatures.");

	func= RNA_def_function(srna, "new", "rna_Main_armatures_new");
	RNA_def_function_ui_description(func, "Add a new armature to the main database");
	parm= RNA_def_string(func, "name", "Armature", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "armature", "Armature", "", "New armature datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_armatures_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a armature from the current blendfile.");
	parm= RNA_def_pointer(func, "armature", "Armature", "", "Armature to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}
void RNA_def_main_actions(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MainActions");
	srna= RNA_def_struct(brna, "MainActions", NULL);
	RNA_def_struct_ui_text(srna, "Main Actions", "Collection of actions.");

	func= RNA_def_function(srna, "new", "rna_Main_actions_new");
	RNA_def_function_ui_description(func, "Add a new action to the main database");
	parm= RNA_def_string(func, "name", "Action", 0, "", "New name for the datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_pointer(func, "action", "Action", "", "New action datablock.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove", "rna_Main_actions_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a action from the current blendfile.");
	parm= RNA_def_pointer(func, "action", "Action", "", "Action to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}
void RNA_def_main_particles(BlenderRNA *brna, PropertyRNA *cprop)
{

}
void RNA_def_main_gpencil(BlenderRNA *brna, PropertyRNA *cprop)
{

}

#endif

