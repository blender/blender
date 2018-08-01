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

#include "ED_render.h"

#include "RE_engine.h"

#include "WM_api.h"
#include "WM_types.h"

#include "rna_internal.h"

/* Allow accessing private members of DNA_workspace_types.h */
#define DNA_PRIVATE_WORKSPACE_ALLOW
#include "DNA_workspace_types.h"

#ifdef RNA_RUNTIME

#include "BLI_listbase.h"

#include "BKE_global.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.h"

#include "WM_toolsystem.h"

static void rna_window_update_all(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
	WM_main_add_notifier(NC_WINDOW, NULL);
}

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

/* workspace.owner_ids */

static wmOwnerID *rna_WorkSpace_owner_ids_new(
        WorkSpace *workspace, const char *name)
{
	wmOwnerID *owner_id = MEM_callocN(sizeof(*owner_id), __func__);
	BLI_addtail(&workspace->owner_ids, owner_id);
	BLI_strncpy(owner_id->name, name, sizeof(owner_id->name));
	WM_main_add_notifier(NC_WINDOW, NULL);
	return owner_id;
}

static void rna_WorkSpace_owner_ids_remove(
        WorkSpace *workspace, ReportList *reports, PointerRNA *wstag_ptr)
{
	wmOwnerID *owner_id = wstag_ptr->data;
	if (BLI_remlink_safe(&workspace->owner_ids, owner_id) == false) {
		BKE_reportf(reports, RPT_ERROR,
		            "wmOwnerID '%s' not in workspace '%s'",
		            owner_id->name, workspace->id.name + 2);
		return;
	}

	MEM_freeN(owner_id);
	RNA_POINTER_INVALIDATE(wstag_ptr);

	WM_main_add_notifier(NC_WINDOW, NULL);
}

static void rna_WorkSpace_owner_ids_clear(
        WorkSpace *workspace)
{
	BLI_freelistN(&workspace->owner_ids);
	WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, workspace);
}

static bToolRef *rna_WorkSpace_tools_from_tkey(WorkSpace *workspace, const bToolKey *tkey, bool create)
{
	if (create) {
		bToolRef *tref;
		WM_toolsystem_ref_ensure(workspace, tkey, &tref);
		return tref;
	}
	return WM_toolsystem_ref_find(workspace, tkey);
}

static bToolRef *rna_WorkSpace_tools_from_space_view3d_mode(
        WorkSpace *workspace, int mode, bool create)
{
	return rna_WorkSpace_tools_from_tkey(workspace, &(bToolKey){ .space_type = SPACE_VIEW3D, .mode = mode}, create);
}

static bToolRef *rna_WorkSpace_tools_from_space_image_mode(
        WorkSpace *workspace, int mode, bool create)
{
	return rna_WorkSpace_tools_from_tkey(workspace, &(bToolKey){ .space_type = SPACE_IMAGE, .mode = mode}, create);
}

const EnumPropertyItem *rna_WorkSpace_tools_mode_itemf(
        bContext *UNUSED(C), PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *UNUSED(r_free))
{
	WorkSpace *workspace = ptr->id.data;

	switch (workspace->tools_space_type) {
		case SPACE_VIEW3D:
			return rna_enum_context_mode_items;
		case SPACE_IMAGE:
			return rna_enum_space_image_mode_items;
	}
	return DummyRNA_DEFAULT_items;
}

static int rna_WorkspaceTool_index_get(PointerRNA *ptr)
{
	bToolRef *tref = ptr->data;
	return (tref->runtime) ? tref->runtime->index : 0;
}

#else /* RNA_RUNTIME */

static void rna_def_workspace_owner(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "wmOwnerID", NULL);
	RNA_def_struct_sdna(srna, "wmOwnerID");
	RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
	RNA_def_struct_ui_text(srna, "Work Space UI Tag", "");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
}

static void rna_def_workspace_owner_ids(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "wmOwnerIDs");
	srna = RNA_def_struct(brna, "wmOwnerIDs", NULL);
	RNA_def_struct_sdna(srna, "WorkSpace");
	RNA_def_struct_ui_text(srna, "WorkSpace UI Tags", "");

	/* add owner_id */
	func = RNA_def_function(srna, "new", "rna_WorkSpace_owner_ids_new");
	RNA_def_function_ui_description(func, "Add ui tag");
	parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the tag");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	/* return type */
	parm = RNA_def_pointer(func, "owner_id", "wmOwnerID", "", "");
	RNA_def_function_return(func, parm);

	/* remove owner_id */
	func = RNA_def_function(srna, "remove", "rna_WorkSpace_owner_ids_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove ui tag");
	/* owner_id to remove */
	parm = RNA_def_pointer(func, "owner_id", "wmOwnerID", "", "Tag to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

	/* clear all modifiers */
	func = RNA_def_function(srna, "clear", "rna_WorkSpace_owner_ids_clear");
	RNA_def_function_ui_description(func, "Remove all tags");
}

static void rna_def_workspace_tool(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "WorkspaceTool", NULL);
	RNA_def_struct_sdna(srna, "bToolRef");
	RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
	RNA_def_struct_ui_text(srna, "Work Space Tool", "");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "idname");
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Index", "");
	RNA_def_property_int_funcs(prop, "rna_WorkspaceTool_index_get", NULL, NULL);

	prop = RNA_def_property(srna, "space_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "space_type");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, rna_enum_space_type_items);
	RNA_def_property_ui_text(prop, "Space Type", "");

	RNA_api_workspace_tool(srna);
}

static void rna_def_workspace_tools(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "wmTools");
	srna = RNA_def_struct(brna, "wmTools", NULL);
	RNA_def_struct_sdna(srna, "WorkSpace");
	RNA_def_struct_ui_text(srna, "WorkSpace UI Tags", "");

	/* add owner_id */
	func = RNA_def_function(srna, "from_space_view3d_mode", "rna_WorkSpace_tools_from_space_view3d_mode");
	RNA_def_function_ui_description(func, "");
	parm = RNA_def_enum(func, "mode", rna_enum_context_mode_items, 0, "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_boolean(func, "create", false, "Create", "");
	/* return type */
	parm = RNA_def_pointer(func, "result", "WorkspaceTool", "", "");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "from_space_image_mode", "rna_WorkSpace_tools_from_space_image_mode");
	RNA_def_function_ui_description(func, "");
	parm = RNA_def_enum(func, "mode", rna_enum_space_image_mode_items, 0, "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_boolean(func, "create", false, "Create", "");
	/* return type */
	parm = RNA_def_pointer(func, "result", "WorkspaceTool", "", "");
	RNA_def_function_return(func, parm);
}

static void rna_def_workspace(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "WorkSpace", "ID");
	RNA_def_struct_sdna(srna, "WorkSpace");
	RNA_def_struct_ui_text(srna, "Workspace", "Workspace data-block, defining the working environment for the user");
	/* TODO: real icon, just to show something */
	RNA_def_struct_ui_icon(srna, ICON_SPLITSCREEN);

	prop = RNA_def_property(srna, "screens", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layouts", NULL);
	RNA_def_property_struct_type(prop, "Screen");
	RNA_def_property_collection_funcs(prop, "rna_workspace_screens_begin", NULL, NULL,
	                                  "rna_workspace_screens_item_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Screens", "Screen layouts of a workspace");

	prop = RNA_def_property(srna, "owner_ids", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "wmOwnerID");
	RNA_def_property_ui_text(prop, "UI Tags", "");
	rna_def_workspace_owner_ids(brna, prop);

	prop = RNA_def_property(srna, "tools", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "tools", NULL);
	RNA_def_property_struct_type(prop, "WorkspaceTool");
	RNA_def_property_ui_text(prop, "Tools", "");
	rna_def_workspace_tools(brna, prop);

	prop = RNA_def_property(srna, "tools_space_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "tools_space_type");
	RNA_def_property_enum_items(prop, rna_enum_space_type_items);
	RNA_def_property_ui_text(prop, "Active Tool Space", "Tool space type");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "tools_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "tools_mode");
	RNA_def_property_enum_items(prop, DummyRNA_DEFAULT_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_WorkSpace_tools_mode_itemf");
	RNA_def_property_ui_text(prop, "Active Tool Space", "Tool mode");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

#if 0
	prop = RNA_def_property(srna, "object_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_object_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "Object interaction mode used in this window");
#endif

	/* Flags */
	prop = RNA_def_property(srna, "use_filter_by_owner", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", WORKSPACE_USE_FILTER_BY_ORIGIN);
	RNA_def_property_ui_text(prop, "Use UI Tags",
	                         "Filter the UI by tags");
	RNA_def_property_update(prop, 0, "rna_window_update_all");

	RNA_api_workspace(srna);
}

void RNA_def_workspace(BlenderRNA *brna)
{
	rna_def_workspace_owner(brna);
	rna_def_workspace_tool(brna);

	rna_def_workspace(brna);
}

#endif /* RNA_RUNTIME */
