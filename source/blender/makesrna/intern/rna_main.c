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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include "BKE_main.h"
#include "BKE_mesh.h"

/* all the list begin functions are added manually here, Main is not in SDNA */

static void rna_Main_filename_get(PointerRNA *ptr, char *value)
{
	Main *bmain= (Main*)ptr->data;
	BLI_strncpy(value, bmain->name, sizeof(bmain->name));
}

static int rna_Main_filename_length(PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	return strlen(bmain->name);
}

#if 0
static void rna_Main_filename_set(PointerRNA *ptr, const char *value)
{
	Main *bmain= (Main*)ptr->data;
	BLI_strncpy(bmain->name, value, sizeof(bmain->name));
}
#endif

static void rna_Main_scene_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->scene, NULL);
}

static void rna_Main_object_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->object, NULL);
}

static void rna_Main_lamp_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->lamp, NULL);
}

static void rna_Main_library_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->library, NULL);
}

static void rna_Main_mesh_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->mesh, NULL);
}

static void rna_Main_curve_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->curve, NULL);
}

static void rna_Main_mball_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->mball, NULL);
}

static void rna_Main_mat_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->mat, NULL);
}

static void rna_Main_tex_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->tex, NULL);
}

static void rna_Main_image_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->image, NULL);
}

static void rna_Main_latt_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->latt, NULL);
}

static void rna_Main_camera_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->camera, NULL);
}

static void rna_Main_key_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->key, NULL);
}

static void rna_Main_world_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->world, NULL);
}

static void rna_Main_screen_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->screen, NULL);
}

static void rna_Main_script_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->script, NULL);
}

static void rna_Main_vfont_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->vfont, NULL);
}

static void rna_Main_text_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->text, NULL);
}

static void rna_Main_sound_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->sound, NULL);
}

static void rna_Main_group_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->group, NULL);
}

static void rna_Main_armature_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->armature, NULL);
}

static void rna_Main_action_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->action, NULL);
}

static void rna_Main_nodetree_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->nodetree, NULL);
}

static void rna_Main_brush_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->brush, NULL);
}

static void rna_Main_particle_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->particle, NULL);
}

static void rna_Main_gpencil_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->gpencil, NULL);
}

static void rna_Main_wm_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->wm, NULL);
}

#ifdef UNIT_TEST

static PointerRNA rna_Test_test_get(PointerRNA *ptr)
{
	PointerRNA ret= *ptr;
	ret.type= &RNA_Test;

	return ret;
}

#endif

#else

void RNA_def_main(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	const char *lists[][5]= {
		{"cameras", "Camera", "rna_Main_camera_begin", "Cameras", "Camera datablocks."},
		{"scenes", "Scene", "rna_Main_scene_begin", "Scenes", "Scene datablocks."},
		{"objects", "Object", "rna_Main_object_begin", "Objects", "Object datablocks."},
		{"materials", "Material", "rna_Main_mat_begin", "Materials", "Material datablocks."},
		{"nodegroups", "NodeTree", "rna_Main_nodetree_begin", "Node Groups", "Node group datablocks."},
		{"meshes", "Mesh", "rna_Main_mesh_begin", "Meshes", "Mesh datablocks."}, // "add_mesh", "remove_mesh"
		{"lamps", "Lamp", "rna_Main_lamp_begin", "Lamps", "Lamp datablocks."},
		{"libraries", "Library", "rna_Main_library_begin", "Libraries", "Library datablocks."},
		{"screens", "Screen", "rna_Main_screen_begin", "Screens", "Screen datablocks."},
		{"windowmanagers", "WindowManager", "rna_Main_wm_begin", "Window Managers", "Window manager datablocks."},
		{"images", "Image", "rna_Main_image_begin", "Images", "Image datablocks."},
		{"lattices", "Lattice", "rna_Main_latt_begin", "Lattices", "Lattice datablocks."},
		{"curves", "Curve", "rna_Main_curve_begin", "Curves", "Curve datablocks."} ,
		{"metaballs", "MetaBall", "rna_Main_mball_begin", "Metaballs", "Metaball datablocks."},
		{"vfonts", "VectorFont", "rna_Main_vfont_begin", "Vector Fonts", "Vector font datablocks."},
		{"textures", "Texture", "rna_Main_tex_begin", "Textures", "Texture datablocks."},
		{"brushes", "Brush", "rna_Main_brush_begin", "Brushes", "Brush datablocks."},
		{"worlds", "World", "rna_Main_world_begin", "Worlds", "World datablocks."},
		{"groups", "Group", "rna_Main_group_begin", "Groups", "Group datablocks."},
		{"keys", "Key", "rna_Main_key_begin", "Keys", "Key datablocks."},
		{"scripts", "ID", "rna_Main_script_begin", "Scripts", "Script datablocks (DEPRECATED)."},
		{"texts", "Text", "rna_Main_text_begin", "Texts", "Text datablocks."},
		{"sounds", "Sound", "rna_Main_sound_begin", "Sounds", "Sound datablocks."},
		{"armatures", "Armature", "rna_Main_armature_begin", "Armatures", "Armature datablocks."},
		{"actions", "Action", "rna_Main_action_begin", "Actions", "Action datablocks."},
		{"particles", "ParticleSettings", "rna_Main_particle_begin", "Particles", "Particle datablocks."},
		{"gpencil", "GreasePencil", "rna_Main_gpencil_begin", "Grease Pencil", "Grease Pencil datablocks."},
		{NULL, NULL, NULL, NULL, NULL}};
	int i;
	
	srna= RNA_def_struct(brna, "Main", NULL);
	RNA_def_struct_ui_text(srna, "Main", "Main data structure representing a .blend file and all its datablocks.");
	RNA_def_struct_ui_icon(srna, ICON_BLENDER);

	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_maxlength(prop, 240);
	RNA_def_property_string_funcs(prop, "rna_Main_filename_get", "rna_Main_filename_length", "rna_Main_filename_set");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Filename", "Path to the .blend file.");

	for(i=0; lists[i][0]; i++)
	{
		prop= RNA_def_property(srna, lists[i][0], PROP_COLLECTION, PROP_NONE);
		RNA_def_property_struct_type(prop, lists[i][1]);
		RNA_def_property_collection_funcs(prop, lists[i][2], "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0);
		RNA_def_property_ui_text(prop, lists[i][3], lists[i][4]);
	}

	RNA_api_main(srna);

#ifdef UNIT_TEST

	RNA_define_verify_sdna(0);

	prop= RNA_def_property(srna, "test", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Test");
	RNA_def_property_pointer_funcs(prop, "rna_Test_test_get", NULL, NULL);

	RNA_define_verify_sdna(1);

#endif
}

#endif

