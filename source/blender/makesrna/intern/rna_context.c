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
 * Contributor(s): Blender Foundation (2009).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_context.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_ID.h"
#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"
#include "BKE_context.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h" /* own include */

const EnumPropertyItem rna_enum_context_mode_items[] = {
	{CTX_MODE_EDIT_MESH, "EDIT_MESH", 0, "Mesh Edit", ""},
	{CTX_MODE_EDIT_CURVE, "EDIT_CURVE", 0, "Curve Edit", ""},
	{CTX_MODE_EDIT_SURFACE, "EDIT_SURFACE", 0, "Surface Edit", ""},
	{CTX_MODE_EDIT_TEXT, "EDIT_TEXT", 0, "Edit Edit", ""},
	{CTX_MODE_EDIT_ARMATURE, "EDIT_ARMATURE", 0, "Armature Edit", ""}, /* PARSKEL reuse will give issues */
	{CTX_MODE_EDIT_METABALL, "EDIT_METABALL", 0, "Metaball Edit", ""},
	{CTX_MODE_EDIT_LATTICE, "EDIT_LATTICE", 0, "Lattice Edit", ""},
	{CTX_MODE_POSE, "POSE", 0, "Pose ", ""},
	{CTX_MODE_SCULPT, "SCULPT", 0, "Sculpt", ""},
	{CTX_MODE_PAINT_WEIGHT, "PAINT_WEIGHT", 0, "Weight Paint", ""},
	{CTX_MODE_PAINT_VERTEX, "PAINT_VERTEX", 0, "Vertex Paint", ""},
	{CTX_MODE_PAINT_TEXTURE, "PAINT_TEXTURE", 0, "Texture Paint", ""},
	{CTX_MODE_PARTICLE, "PARTICLE", 0, "Particle", ""},
	{CTX_MODE_OBJECT, "OBJECT", 0, "Object", ""},
	{CTX_MODE_GPENCIL_PAINT, "GPENCIL_PAINT", 0, "Grease Pencil Paint", "" },
	{CTX_MODE_GPENCIL_EDIT, "GPENCIL_EDIT", 0, "Grease Pencil Edit", "" },
	{CTX_MODE_GPENCIL_SCULPT, "GPENCIL_SCULPT", 0, "Grease Pencil Sculpt", "" },
	{CTX_MODE_GPENCIL_WEIGHT, "GPENCIL_WEIGHT", 0, "Grease Pencil Weight Paint", "" },
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "RE_engine.h"

static PointerRNA rna_Context_manager_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_WindowManager, CTX_wm_manager(C));
}

static PointerRNA rna_Context_window_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Window, CTX_wm_window(C));
}

static PointerRNA rna_Context_workspace_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_WorkSpace, CTX_wm_workspace(C));
}

static PointerRNA rna_Context_screen_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Screen, CTX_wm_screen(C));
}

static PointerRNA rna_Context_area_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	PointerRNA newptr;
	RNA_pointer_create((ID *)CTX_wm_screen(C), &RNA_Area, CTX_wm_area(C), &newptr);
	return newptr;
}

static PointerRNA rna_Context_space_data_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	PointerRNA newptr;
	RNA_pointer_create((ID *)CTX_wm_screen(C), &RNA_Space, CTX_wm_space_data(C), &newptr);
	return newptr;
}

static PointerRNA rna_Context_region_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	PointerRNA newptr;
	RNA_pointer_create((ID *)CTX_wm_screen(C), &RNA_Region, CTX_wm_region(C), &newptr);
	return newptr;
}

static PointerRNA rna_Context_region_data_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;

	/* only exists for one space still, no generic system yet */
	if (CTX_wm_view3d(C)) {
		PointerRNA newptr;
		RNA_pointer_create((ID *)CTX_wm_screen(C), &RNA_RegionView3D, CTX_wm_region_data(C), &newptr);
		return newptr;
	}

	return PointerRNA_NULL;
}

static PointerRNA rna_Context_gizmo_group_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	PointerRNA newptr;
	RNA_pointer_create(NULL, &RNA_GizmoGroup, CTX_wm_gizmo_group(C), &newptr);
	return newptr;
}

static PointerRNA rna_Context_main_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_BlendData, CTX_data_main(C));
}

static PointerRNA rna_Context_depsgraph_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Depsgraph, CTX_data_depsgraph(C));
}

static PointerRNA rna_Context_scene_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Scene, CTX_data_scene(C));
}

static PointerRNA rna_Context_view_layer_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	Scene *scene = CTX_data_scene(C);
	PointerRNA scene_ptr;

	RNA_id_pointer_create(&scene->id, &scene_ptr);
	return rna_pointer_inherit_refine(&scene_ptr, &RNA_ViewLayer, CTX_data_view_layer(C));
}

static void rna_Context_engine_get(PointerRNA *ptr, char *value)
 {
	bContext *C = (bContext *)ptr->data;
	RenderEngineType *engine_type = CTX_data_engine_type(C);
	strcpy(value, engine_type->idname);
}

static int rna_Context_engine_length(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	RenderEngineType *engine_type = CTX_data_engine_type(C);
	return strlen(engine_type->idname);
}

static PointerRNA rna_Context_collection_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Collection, CTX_data_collection(C));
}

static PointerRNA rna_Context_layer_collection_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	ptr->id.data = CTX_data_scene(C);
	return rna_pointer_inherit_refine(ptr, &RNA_LayerCollection, CTX_data_layer_collection(C));
}

static PointerRNA rna_Context_tool_settings_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	ptr->id.data = CTX_data_scene(C);
	return rna_pointer_inherit_refine(ptr, &RNA_ToolSettings, CTX_data_tool_settings(C));
}

static PointerRNA rna_Context_user_preferences_get(PointerRNA *UNUSED(ptr))
{
	PointerRNA newptr;
	RNA_pointer_create(NULL, &RNA_UserPreferences, &U, &newptr);
	return newptr;
}

static int rna_Context_mode_get(PointerRNA *ptr)
{
	bContext *C = (bContext *)ptr->data;
	return CTX_data_mode_enum(C);
}

#else

void RNA_def_context(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Context", NULL);
	RNA_def_struct_ui_text(srna, "Context", "Current windowmanager and data context");
	RNA_def_struct_sdna(srna, "bContext");

	/* WM */
	prop = RNA_def_property(srna, "window_manager", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "WindowManager");
	RNA_def_property_pointer_funcs(prop, "rna_Context_manager_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "window", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Window");
	RNA_def_property_pointer_funcs(prop, "rna_Context_window_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "workspace", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "WorkSpace");
	RNA_def_property_pointer_funcs(prop, "rna_Context_workspace_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "screen", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Screen");
	RNA_def_property_pointer_funcs(prop, "rna_Context_screen_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "area", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Area");
	RNA_def_property_pointer_funcs(prop, "rna_Context_area_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "space_data", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Space");
	RNA_def_property_pointer_funcs(prop, "rna_Context_space_data_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "region", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Region");
	RNA_def_property_pointer_funcs(prop, "rna_Context_region_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "region_data", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "RegionView3D");
	RNA_def_property_pointer_funcs(prop, "rna_Context_region_data_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "gizmo_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "GizmoGroup");
	RNA_def_property_pointer_funcs(prop, "rna_Context_gizmo_group_get", NULL, NULL, NULL);

	/* Data */
	prop = RNA_def_property(srna, "blend_data", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "BlendData");
	RNA_def_property_pointer_funcs(prop, "rna_Context_main_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "depsgraph", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Depsgraph");
	RNA_def_property_pointer_funcs(prop, "rna_Context_depsgraph_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Scene");
	RNA_def_property_pointer_funcs(prop, "rna_Context_scene_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "view_layer", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "ViewLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Context_view_layer_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "engine", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Context_engine_get", "rna_Context_engine_length", NULL);

	prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Collection");
	RNA_def_property_pointer_funcs(prop, "rna_Context_collection_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "layer_collection", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "LayerCollection");
	RNA_def_property_pointer_funcs(prop, "rna_Context_layer_collection_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "tool_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "ToolSettings");
	RNA_def_property_pointer_funcs(prop, "rna_Context_tool_settings_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "user_preferences", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "UserPreferences");
	RNA_def_property_pointer_funcs(prop, "rna_Context_user_preferences_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_context_mode_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_funcs(prop, "rna_Context_mode_get", NULL, NULL);
}

#endif
