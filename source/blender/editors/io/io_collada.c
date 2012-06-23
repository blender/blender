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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/io/collada.c
 *  \ingroup collada
 */
#ifdef WITH_COLLADA
#include "DNA_scene_types.h"

#include "BLF_translation.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "ED_screen.h"
#include "ED_object.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "../../collada/collada.h"

static int wm_collada_export_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{	
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		char filepath[FILE_MAX];

		if (G.main->name[0] == 0)
			BLI_strncpy(filepath, "untitled", sizeof(filepath));
		else
			BLI_strncpy(filepath, G.main->name, sizeof(filepath));

		BLI_replace_extension(filepath, sizeof(filepath), ".dae");
		RNA_string_set(op->ptr, "filepath", filepath);
	}

	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* function used for WM_OT_save_mainfile too */
static int wm_collada_export_exec(bContext *C, wmOperator *op)
{
	char filepath[FILE_MAX];
	int apply_modifiers;
	int export_mesh_type;
	int selected;
	int include_children;
	int include_armatures;
	int deform_bones_only;

	int include_uv_textures;
	int use_texture_copies;
	int active_uv_only;

	int use_object_instantiation;
	int sort_by_name;
	int second_life; 

	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}

	RNA_string_get(op->ptr, "filepath", filepath);
	BLI_ensure_extension(filepath, sizeof(filepath), ".dae");

	/* Options panel */
	apply_modifiers          = RNA_boolean_get(op->ptr, "apply_modifiers");
	export_mesh_type         = RNA_enum_get(op->ptr,    "export_mesh_type_selection");
	selected                 = RNA_boolean_get(op->ptr, "selected");
	include_children         = RNA_boolean_get(op->ptr, "include_children");
	include_armatures        = RNA_boolean_get(op->ptr, "include_armatures");
	deform_bones_only        = RNA_boolean_get(op->ptr, "deform_bones_only");

	include_uv_textures      = RNA_boolean_get(op->ptr, "include_uv_textures");
	use_texture_copies       = RNA_boolean_get(op->ptr, "use_texture_copies");
	active_uv_only           = RNA_boolean_get(op->ptr, "active_uv_only");

	use_object_instantiation = RNA_boolean_get(op->ptr, "use_object_instantiation");
	sort_by_name             = RNA_boolean_get(op->ptr, "sort_by_name");
	second_life              = RNA_boolean_get(op->ptr, "second_life");

	/* get editmode results */
	ED_object_exit_editmode(C, 0);  /* 0 = does not exit editmode */

	if (collada_export(
	        CTX_data_scene(C),
	        filepath,
	        apply_modifiers,
			export_mesh_type,
	        selected,
	        include_children,
	        include_armatures,
	        deform_bones_only,

			include_uv_textures,
			active_uv_only,
			use_texture_copies,

	        use_object_instantiation,
	        sort_by_name,
	        second_life)) {
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void uiCollada_exportSettings(uiLayout *layout, PointerRNA *imfptr)
{
	uiLayout *box, *row, *col, *split;

	/* Export Options: */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, 0);
	uiItemL(row, IFACE_("Export Data Options:"), ICON_MESH_DATA);

	row = uiLayoutRow(box, 0);
	split = uiLayoutSplit(row, 0.6f, UI_LAYOUT_ALIGN_RIGHT);
	col   = uiLayoutColumn(split,0);
	uiItemR(col, imfptr, "apply_modifiers", 0, NULL, ICON_NONE);
	col   = uiLayoutColumn(split,0);
	uiItemR(col, imfptr, "export_mesh_type_selection", 0, "", ICON_NONE);
	uiLayoutSetEnabled(col, RNA_boolean_get(imfptr, "apply_modifiers"));

	row = uiLayoutRow(box, 0);
	uiItemR(row, imfptr, "selected", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, 0);
	uiItemR(row, imfptr, "include_children", 0, NULL, ICON_NONE);
	uiLayoutSetEnabled(row, RNA_boolean_get(imfptr, "selected"));

	row = uiLayoutRow(box, 0);
	uiItemR(row, imfptr, "include_armatures", 0, NULL, ICON_NONE);
	uiLayoutSetEnabled(row, RNA_boolean_get(imfptr, "selected"));

	// Texture options
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, 0);
	uiItemL(row, IFACE_("Texture Options:"), ICON_TEXTURE_DATA);

	row = uiLayoutRow(box, 0);
	uiItemR(row, imfptr, "active_uv_only", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, 0);
	uiItemR(row, imfptr, "include_uv_textures", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, 0);
	uiItemR(row, imfptr, "use_texture_copies", 1, NULL, ICON_NONE);


	// Armature options
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, 0);
	uiItemL(row, IFACE_("Armature Options:"), ICON_ARMATURE_DATA);

	row = uiLayoutRow(box, 0);
	uiItemR(row, imfptr, "deform_bones_only", 0, NULL, ICON_NONE);
	row = uiLayoutRow(box, 0);
	uiItemR(row, imfptr, "second_life", 0, NULL, ICON_NONE);

	/* Collada options: */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, 0);
	uiItemL(row, IFACE_("Collada Options:"), ICON_MODIFIER);

	row = uiLayoutRow(box, 0);
	uiItemR(row, imfptr, "use_object_instantiation", 0, NULL, ICON_NONE);
	row = uiLayoutRow(box, 0);
	uiItemR(row, imfptr, "sort_by_name", 0, NULL, ICON_NONE);

}

static void wm_collada_export_draw(bContext *UNUSED(C), wmOperator *op)
{
	PointerRNA ptr;

	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
	uiCollada_exportSettings(op->layout, &ptr);
}

void WM_OT_collada_export(wmOperatorType *ot)
{
	static EnumPropertyItem prop_bc_export_mesh_type[] = {
		{BC_MESH_TYPE_VIEW, "view", 0, "View", "Apply modifier's view settings"},
		{BC_MESH_TYPE_RENDER, "render", 0, "Render", "Apply modifier's render settings"},
		{0, NULL, 0, NULL, NULL}
	};

	ot->name = "Export COLLADA";
	ot->description = "Save a Collada file";
	ot->idname = "WM_OT_collada_export";
	
	ot->invoke = wm_collada_export_invoke;
	ot->exec = wm_collada_export_exec;
	ot->poll = WM_operator_winactive;

	ot->flag |= OPTYPE_PRESET;

	ot->ui = wm_collada_export_draw;
	
	WM_operator_properties_filesel(ot, FOLDERFILE | COLLADAFILE, FILE_BLENDER, FILE_SAVE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);

	RNA_def_boolean(ot->srna,
	                "apply_modifiers", 0, "Apply Modifiers",
	                "Apply modifiers to exported mesh (non destructive))");

	RNA_def_int(ot->srna, "export_mesh_type", 0, INT_MIN, INT_MAX,
	            "Resolution", "Modifier resolution for export", INT_MIN, INT_MAX);

	RNA_def_enum(ot->srna, "export_mesh_type_selection", prop_bc_export_mesh_type, 0,
	             "Resolution", "Modifier resolution for export");

	RNA_def_boolean(ot->srna, "selected", 0, "Selection Only",
	                "Export only selected elements");

	RNA_def_boolean(ot->srna, "include_children", 0, "Include Children",
	                "Export all children of selected objects (even if not selected)");

	RNA_def_boolean(ot->srna, "include_armatures", 0, "Include Armatures",
	                "Export related armatures (even if not selected)");

	RNA_def_boolean(ot->srna, "deform_bones_only", 0, "Deform Bones only",
	                "Only export deforming bones with armatures");


	RNA_def_boolean(ot->srna, "active_uv_only", 0, "Only Active UV layer",
					"Export textures assigned to the object UV maps");

	RNA_def_boolean(ot->srna, "include_uv_textures", 0, "Include UV Textures",
					"Export textures assigned to the object UV maps");

	RNA_def_boolean(ot->srna, "use_texture_copies", 1, "copy", 
	                "Copy textures to same folder where the .dae file is exported");


	RNA_def_boolean(ot->srna, "use_object_instantiation", 1, "Use Object Instances",
	                "Instantiate multiple Objects from same Data");

	RNA_def_boolean(ot->srna, "sort_by_name", 0, "Sort by Object name",
	                "Sort exported data by Object name");

	RNA_def_boolean(ot->srna, "second_life", 0, "Export for Second Life",
	                "Compatibility mode for Second Life");
}


/* function used for WM_OT_save_mainfile too */
static int wm_collada_import_exec(bContext *C, wmOperator *op)
{
	char filename[FILE_MAX];
	
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}

	RNA_string_get(op->ptr, "filepath", filename);
	if (collada_import(C, filename)) return OPERATOR_FINISHED;
	
	BKE_report(op->reports, RPT_ERROR, "Errors found during parsing COLLADA document. Please see console for error log.");
	
	return OPERATOR_FINISHED;
}

void WM_OT_collada_import(wmOperatorType *ot)
{
	ot->name = "Import COLLADA";
	ot->description = "Load a Collada file";
	ot->idname = "WM_OT_collada_import";
	
	ot->invoke = WM_operator_filesel;
	ot->exec = wm_collada_import_exec;
	ot->poll = WM_operator_winactive;
	
	WM_operator_properties_filesel(ot, FOLDERFILE | COLLADAFILE, FILE_BLENDER, FILE_OPENFILE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
}
#endif
