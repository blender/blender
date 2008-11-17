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

#ifdef RNA_RUNTIME

#include "BKE_main.h"

/* all the list begin functions are added manually here, Main is not in SDNA */

static void rna_Main_scene_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->scene);
}

static void rna_Main_object_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->object);
}

#if 0
static void rna_Main_library_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->library);
}
#endif

static void rna_Main_mesh_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->mesh);
}

#if 0
static void rna_Main_curve_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->curve);
}

static void rna_Main_mball_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->mball);
}

static void rna_Main_mat_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->mat);
}

static void rna_Main_tex_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->tex);
}

static void rna_Main_image_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->image);
}

static void rna_Main_latt_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->latt);
}

static void rna_Main_lamp_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->lamp);
}

static void rna_Main_camera_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->camera);
}

static void rna_Main_ipo_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->ipo);
}

static void rna_Main_key_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->key);
}

static void rna_Main_world_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->world);
}

static void rna_Main_screen_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->screen);
}

static void rna_Main_script_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->script);
}

static void rna_Main_vfont_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->vfont);
}

static void rna_Main_text_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->text);
}

static void rna_Main_sound_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->sound);
}

static void rna_Main_group_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->group);
}

static void rna_Main_armature_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->armature);
}

static void rna_Main_action_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->action);
}

static void rna_Main_nodetree_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->nodetree);
}

static void rna_Main_brush_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->brush);
}

static void rna_Main_particle_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->particle);
}

static void rna_Main_wm_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Main *bmain= (Main*)ptr->data;
	rna_iterator_listbase_begin(iter, &bmain->wm);
}
#endif

#else

void RNA_def_main(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	const char *lists[][5]= {
		{"scenes", "Scene", "rna_Main_scene_begin", "Scenes", "Scene datablocks."},
		{"objects", "Object", "rna_Main_object_begin", "Objects", "Object datablocks."},
		{"meshes", "Mesh", "rna_Main_mesh_begin", "Meshes", "Mesh datablocks."}, 
		{NULL, NULL, NULL, NULL, NULL},
		{"libraries", "Library", "rna_Main_library_begin", "Libraries", "Library datablocks."},
		{"curves", "Curve", "rna_Main_curve_begin", "Curves", "Curve datablocks."}, 
		{"metaballs", "MBall", "rna_Main_mball_begin", "Metaballs", "Metaball datablocks."}, 
		{"materials", "Material", "rna_Main_mat_begin", "Materials", "Material datablocks."},
		{"textures", "Texture", "rna_Main_tex_begin", "Textures", "Texture datablocks."},
		{"images", "Image", "rna_Main_image_begin", "Images", "Image datablocks."},
		{"lattices", "Lattice", "rna_Main_latt_begin", "Lattices", "Lattice datablocks."},
		{"lamps", "Lamp", "rna_Main_lamp_begin", "Lamps", "Lamp datablocks."},
		{"cameras", "Camera", "rna_Main_camera_begin", "Cameras", "Camera datablocks."},
		{"ipos", "Ipo", "rna_Main_ipo_begin", "Ipos", "Ipo datablocks."},
		{"keys", "Key", "rna_Main_key_begin", "Keys", "Key datablocks."},
		{"worlds", "World", "rna_Main_world_begin", "Worlds", "World datablocks."},
		{"screens", "Screen", "rna_Main_screen_begin", "Screens", "Screen datablocks."},
		{"scripts", "Script", "rna_Main_script_begin", "Scripts", "Script datablocks."},
		{"vfonts", "VFont", "rna_Main_vfont_begin", "VFonts", "VFont datablocks."},
		{"texts", "Text", "rna_Main_text_begin", "Texts", "Text datablocks."},
		{"sounds", "Sound", "rna_Main_sound_begin", "Sounds", "Sound datablocks."},
		{"groups", "Group", "rna_Main_group_begin", "Groups", "Group datablocks."},
		{"armatures", "Armature", "rna_Main_armature_begin", "Armatures", "Armature datablocks."},
		{"actions", "Action", "rna_Main_action_begin", "Actions", "Action datablocks."},
		{"nodegroups", "NodeGroup", "rna_Main_nodetree_begin", "Node Groups", "Node group datablocks."},
		{"brushes", "Brush", "rna_Main_brush_begin", "Brushes", "Brush datablocks."},
		{"particles", "Particle", "rna_Main_particle_begin", "Particles", "Particle datablocks."},
		{"windowmanagers", "wmWindowManager", "rna_Main_wm_begin", "Window Managers", "Window manager datablocks."},
		{NULL, NULL, NULL, NULL, NULL}};
	int i;
	
	srna= RNA_def_struct(brna, "Main", "Main");

	for(i=0; lists[i][0]; i++)
	{
		prop= RNA_def_property(srna, lists[i][0], PROP_COLLECTION, PROP_NONE);
		RNA_def_property_struct_type(prop, lists[i][1]);
		RNA_def_property_collection_funcs(prop, lists[i][2], "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0, 0);
		RNA_def_property_ui_text(prop, lists[i][3], lists[i][4]);
	}
}

#endif

