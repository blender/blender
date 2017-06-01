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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_workspace.c
 *  \ingroup RNA
 */

#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_types.h"

#include "BKE_workspace.h"

#include "WM_api.h"
#include "WM_types.h"

#include "rna_internal.h"


#ifdef RNA_RUNTIME

#include "BKE_global.h"

#include "BLI_listbase.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "RNA_access.h"


void rna_workspace_screens_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	WorkSpace *workspace = ptr->id.data;
	rna_iterator_listbase_begin(iter, BKE_workspace_layouts_get(workspace), NULL);
}

static PointerRNA rna_workspace_screens_item_get(CollectionPropertyIterator *iter)
{
	WorkSpaceLayout *layout = rna_iterator_listbase_get(iter);
	bScreen *screen = BKE_workspace_layout_screen_get(layout);

	return rna_pointer_inherit_refine(&iter->parent, &RNA_Screen, screen);
}

#ifdef USE_WORKSPACE_MODE

static int rna_workspace_object_mode_get(PointerRNA *ptr)
{
	WorkSpace *workspace = ptr->data;
	return (int)BKE_workspace_object_mode_get(workspace);
}

static void rna_workspace_object_mode_set(PointerRNA *ptr, int value)
{
	WorkSpace *workspace = ptr->data;
	BKE_workspace_object_mode_set(workspace, value);
}

#endif /* USE_WORKSPACE_MODE */

static PointerRNA rna_workspace_render_layer_get(PointerRNA *ptr)
{
	WorkSpace *workspace = ptr->data;
	SceneLayer *render_layer = BKE_workspace_render_layer_get(workspace);

	/* XXX hmrf... lookup in getter... but how could we avoid it? */
	for (Scene *scene = G.main->scene.first; scene; scene = scene->id.next) {
		if (BLI_findindex(&scene->render_layers, render_layer) != -1) {
			PointerRNA scene_ptr;

			RNA_id_pointer_create(&scene->id, &scene_ptr);
			return rna_pointer_inherit_refine(&scene_ptr, &RNA_SceneLayer, render_layer);
		}
	}

	return PointerRNA_NULL;
}

static void rna_workspace_render_layer_set(PointerRNA *ptr, PointerRNA value)
{
	WorkSpace *workspace = ptr->data;
	BKE_workspace_render_layer_set(workspace, value.data);
}

#else /* RNA_RUNTIME */

static void rna_def_workspace(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "WorkSpace", "ID");
	RNA_def_struct_sdna(srna, "WorkSpace");
	RNA_def_struct_ui_text(srna, "Workspace", "Workspace data-block, defining the working environment for the user");
	/* TODO: real icon, just to show something */
	RNA_def_struct_ui_icon(srna, ICON_RENDER_RESULT);

	prop = RNA_def_property(srna, "screens", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layouts", NULL);
	RNA_def_property_struct_type(prop, "Screen");
	RNA_def_property_collection_funcs(prop, "rna_workspace_screens_begin", NULL, NULL,
	                                  "rna_workspace_screens_item_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Screens", "Screen layouts of a workspace");

#ifdef USE_WORKSPACE_MODE
	prop = RNA_def_property(srna, "object_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_object_mode_items);
	RNA_def_property_enum_funcs(prop, "rna_workspace_object_mode_get", "rna_workspace_object_mode_set", NULL);
	RNA_def_property_ui_text(prop, "Mode", "Object interaction mode");
#endif

	prop = RNA_def_property(srna, "render_layer", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "SceneLayer");
	RNA_def_property_pointer_funcs(prop, "rna_workspace_render_layer_get", "rna_workspace_render_layer_set",
	                               NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Render Layer", "The active render layer used in this workspace");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
	RNA_def_property_update(prop, NC_SCREEN | ND_LAYER, NULL);
}

void RNA_def_workspace(BlenderRNA *brna)
{
	rna_def_workspace(brna);
}

#endif /* RNA_RUNTIME */
