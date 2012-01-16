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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_main.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <string.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "BKE_utildefines.h"

#ifdef RNA_RUNTIME

#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_global.h"

/* all the list begin functions are added manually here, Main is not in SDNA */

static int rna_Main_is_saved_get(PointerRNA *UNUSED(ptr))
{
	return G.relbase_valid;
}

static int rna_Main_is_dirty_get(PointerRNA *ptr)
{
	/* XXX, not totally nice to do it this way, should store in main ? */
	Main *bmain= (Main*)ptr->data;
	wmWindowManager *wm;
	for(wm= bmain->wm.first; wm; wm= wm->id.next) {
		return !wm->file_saved;
	}

	return TRUE;
}

static void rna_Main_filepath_get(PointerRNA *ptr, char *value)
{
	Main *bmain= (Main*)ptr->data;
	BLI_strncpy(value, bmain->name, sizeof(bmain->name));
}

static int rna_Main_filepath_length(PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	return strlen(bmain->name);
}

#if 0
static void rna_Main_filepath_set(PointerRNA *ptr, const char *value)
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

static void rna_Main_font_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->vfont, NULL);
}

static void rna_Main_text_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->text, NULL);
}

static void rna_Main_speaker_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->speaker, NULL);
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

static void rna_Main_movieclips_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->movieclip, NULL);
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

/* local convenience types */
typedef void (CollectionDefFunc)(struct BlenderRNA *brna, struct PropertyRNA *cprop);

typedef struct MainCollectionDef {
	const char	*identifier;
	const char	*type;
	const char	*iter_begin;
	const char	*name;
	const char	*description;
	CollectionDefFunc *func;
} MainCollectionDef;

void RNA_def_main(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	CollectionDefFunc *func;

	/* plural must match idtypes in readblenentry.c */
	MainCollectionDef lists[]= {
		{"cameras", "Camera", "rna_Main_camera_begin", "Cameras", "Camera datablocks", RNA_def_main_cameras},
		{"scenes", "Scene", "rna_Main_scene_begin", "Scenes", "Scene datablocks", RNA_def_main_scenes},
		{"objects", "Object", "rna_Main_object_begin", "Objects", "Object datablocks", RNA_def_main_objects},
		{"materials", "Material", "rna_Main_mat_begin", "Materials", "Material datablocks", RNA_def_main_materials},
		{"node_groups", "NodeTree", "rna_Main_nodetree_begin", "Node Groups", "Node group datablocks", RNA_def_main_node_groups},
		{"meshes", "Mesh", "rna_Main_mesh_begin", "Meshes", "Mesh datablocks", RNA_def_main_meshes},
		{"lamps", "Lamp", "rna_Main_lamp_begin", "Lamps", "Lamp datablocks", RNA_def_main_lamps},
		{"libraries", "Library", "rna_Main_library_begin", "Libraries", "Library datablocks", RNA_def_main_libraries},
		{"screens", "Screen", "rna_Main_screen_begin", "Screens", "Screen datablocks", RNA_def_main_screens},
		{"window_managers", "WindowManager", "rna_Main_wm_begin", "Window Managers", "Window manager datablocks", RNA_def_main_window_managers},
		{"images", "Image", "rna_Main_image_begin", "Images", "Image datablocks", RNA_def_main_images},
		{"lattices", "Lattice", "rna_Main_latt_begin", "Lattices", "Lattice datablocks", RNA_def_main_lattices},
		{"curves", "Curve", "rna_Main_curve_begin", "Curves", "Curve datablocks", RNA_def_main_curves} ,
		{"metaballs", "MetaBall", "rna_Main_mball_begin", "Metaballs", "Metaball datablocks", RNA_def_main_metaballs},
		{"fonts", "VectorFont", "rna_Main_font_begin", "Vector Fonts", "Vector font datablocks", RNA_def_main_fonts},
		{"textures", "Texture", "rna_Main_tex_begin", "Textures", "Texture datablocks", RNA_def_main_textures},
		{"brushes", "Brush", "rna_Main_brush_begin", "Brushes", "Brush datablocks", RNA_def_main_brushes},
		{"worlds", "World", "rna_Main_world_begin", "Worlds", "World datablocks", RNA_def_main_worlds},
		{"groups", "Group", "rna_Main_group_begin", "Groups", "Group datablocks", RNA_def_main_groups},
		{"shape_keys", "Key", "rna_Main_key_begin", "Shape Keys", "Shape Key datablocks", NULL},
		{"scripts", "ID", "rna_Main_script_begin", "Scripts", "Script datablocks (DEPRECATED)", NULL},
		{"texts", "Text", "rna_Main_text_begin", "Texts", "Text datablocks", RNA_def_main_texts},
		{"speakers", "Speaker", "rna_Main_speaker_begin", "Speakers", "Speaker datablocks", RNA_def_main_speakers},
		{"sounds", "Sound", "rna_Main_sound_begin", "Sounds", "Sound datablocks", RNA_def_main_sounds},
		{"armatures", "Armature", "rna_Main_armature_begin", "Armatures", "Armature datablocks", RNA_def_main_armatures},
		{"actions", "Action", "rna_Main_action_begin", "Actions", "Action datablocks", RNA_def_main_actions},
		{"particles", "ParticleSettings", "rna_Main_particle_begin", "Particles", "Particle datablocks", RNA_def_main_particles},
		{"grease_pencil", "GreasePencil", "rna_Main_gpencil_begin", "Grease Pencil", "Grease Pencil datablocks", RNA_def_main_gpencil},
		{"movieclips", "MovieClip", "rna_Main_movieclips_begin", "Movie Clips", "Movie Clip datablocks", RNA_def_main_movieclips},
		{NULL, NULL, NULL, NULL, NULL, NULL}};

	int i;
	
	srna= RNA_def_struct(brna, "BlendData", NULL);
	RNA_def_struct_ui_text(srna, "Blendfile Data", "Main data structure representing a .blend file and all its datablocks");
	RNA_def_struct_ui_icon(srna, ICON_BLENDER);

	prop= RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_maxlength(prop, FILE_MAX);
	RNA_def_property_string_funcs(prop, "rna_Main_filepath_get", "rna_Main_filepath_length", "rna_Main_filepath_set");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Filename", "Path to the .blend file");
	
	prop= RNA_def_property(srna, "is_dirty", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Main_is_dirty_get", NULL);
	RNA_def_property_ui_text(prop, "File Has Unsaved Changes", "Have recent edits been saved to disk");

	prop= RNA_def_property(srna, "is_saved", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Main_is_saved_get", NULL);
	RNA_def_property_ui_text(prop, "File is Saved", "Has the current session been saved to disk as a .blend file");

	for(i=0; lists[i].name; i++)
	{
		prop= RNA_def_property(srna, lists[i].identifier, PROP_COLLECTION, PROP_NONE);
		RNA_def_property_struct_type(prop, lists[i].type);
		RNA_def_property_collection_funcs(prop, lists[i].iter_begin, "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", NULL, NULL, NULL, NULL);
		RNA_def_property_ui_text(prop, lists[i].name, lists[i].description);

		/* collection functions */
		func= lists[i].func;
		if(func)
			func(brna, prop);
	}

	RNA_api_main(srna);

#ifdef UNIT_TEST

	RNA_define_verify_sdna(0);

	prop= RNA_def_property(srna, "test", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Test");
	RNA_def_property_pointer_funcs(prop, "rna_Test_test_get", NULL, NULL, NULL);

	RNA_define_verify_sdna(1);

#endif
}

#endif

