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

/** \file blender/editors/io/io_collada.c
 *  \ingroup collada
 */
#ifdef WITH_COLLADA
#include "DNA_scene_types.h"

#include "BLT_translation.h"

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

#include "io_collada.h"

static int wm_collada_export_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
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
	int include_shapekeys;
	int deform_bones_only;

	int export_texture_type;
	int use_texture_copies;
	int active_uv_only;

	int triangulate;
	int use_object_instantiation;
	int use_blender_profile;
	int sort_by_name;
	int export_transformation_type;

	int open_sim;
	int limit_precision;
	int keep_bind_info;

	int export_count;

	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}

	RNA_string_get(op->ptr, "filepath", filepath);
	BLI_ensure_extension(filepath, sizeof(filepath), ".dae");


	/* Avoid File write exceptions in Collada */
	if (!BLI_exists(filepath)) {
		BLI_make_existing_file(filepath);
		if (!BLI_file_touch(filepath)) {
			BKE_report(op->reports, RPT_ERROR, "Can't create export file");
			fprintf(stdout, "Collada export: Can not create: %s\n", filepath);
			return OPERATOR_CANCELLED;
		}
	}
	else if (!BLI_file_is_writable(filepath)) {
		BKE_report(op->reports, RPT_ERROR, "Can't overwrite export file");
		fprintf(stdout, "Collada export: Can not modify: %s\n", filepath);
		return OPERATOR_CANCELLED;
	}

	/* Now the exporter can create and write the export file */

	/* Options panel */
	apply_modifiers          = RNA_boolean_get(op->ptr, "apply_modifiers");
	export_mesh_type         = RNA_enum_get(op->ptr,    "export_mesh_type_selection");
	selected                 = RNA_boolean_get(op->ptr, "selected");
	include_children         = RNA_boolean_get(op->ptr, "include_children");
	include_armatures        = RNA_boolean_get(op->ptr, "include_armatures");
	include_shapekeys        = RNA_boolean_get(op->ptr, "include_shapekeys");
	deform_bones_only        = RNA_boolean_get(op->ptr, "deform_bones_only");

	export_texture_type      = RNA_enum_get(op->ptr, "export_texture_type_selection");
	use_texture_copies       = RNA_boolean_get(op->ptr, "use_texture_copies");
	active_uv_only           = RNA_boolean_get(op->ptr, "active_uv_only");

	triangulate                = RNA_boolean_get(op->ptr, "triangulate");
	use_object_instantiation   = RNA_boolean_get(op->ptr, "use_object_instantiation");
	use_blender_profile        = RNA_boolean_get(op->ptr, "use_blender_profile");
	sort_by_name               = RNA_boolean_get(op->ptr, "sort_by_name");
	export_transformation_type = RNA_enum_get(op->ptr,    "export_transformation_type_selection");
	open_sim                   = RNA_boolean_get(op->ptr, "open_sim");

	limit_precision = RNA_boolean_get(op->ptr, "limit_precision");
	keep_bind_info = RNA_boolean_get(op->ptr, "keep_bind_info");

	/* get editmode results */
	ED_object_editmode_load(CTX_data_edit_object(C));


	export_count = collada_export(CTX_data_scene(C),
		filepath,
		apply_modifiers,
		export_mesh_type,
		selected,
		include_children,
		include_armatures,
		include_shapekeys,
		deform_bones_only,

		active_uv_only,
		export_texture_type,
		use_texture_copies,

		triangulate,
		use_object_instantiation,
		use_blender_profile,
		sort_by_name,
		export_transformation_type,

		open_sim,
		limit_precision,
		keep_bind_info
	);

	if (export_count == 0) {
		BKE_report(op->reports, RPT_WARNING, "No objects selected -- Created empty export file");
		return OPERATOR_CANCELLED;
	}
	else if (export_count < 0) {
		BKE_report(op->reports, RPT_WARNING, "Error during export (see Console)");
		return OPERATOR_CANCELLED;
	}
	else {
		char buff[100];
		sprintf(buff, "Exported %d Objects", export_count);
		BKE_report(op->reports, RPT_INFO, buff);
		return OPERATOR_FINISHED;
	}
}

static void uiCollada_exportSettings(uiLayout *layout, PointerRNA *imfptr)
{
	uiLayout *box, *row, *col, *split;

	/* Export Options: */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Export Data Options:"), ICON_MESH_DATA);

	row = uiLayoutRow(box, false);
	split = uiLayoutSplit(row, 0.6f, UI_LAYOUT_ALIGN_RIGHT);
	col   = uiLayoutColumn(split, false);
	uiItemR(col, imfptr, "apply_modifiers", 0, NULL, ICON_NONE);
	col   = uiLayoutColumn(split, false);
	uiItemR(col, imfptr, "export_mesh_type_selection", 0, "", ICON_NONE);
	uiLayoutSetEnabled(col, RNA_boolean_get(imfptr, "apply_modifiers"));

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "selected", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "include_children", 0, NULL, ICON_NONE);
	uiLayoutSetEnabled(row, RNA_boolean_get(imfptr, "selected"));

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "include_armatures", 0, NULL, ICON_NONE);
	uiLayoutSetEnabled(row, RNA_boolean_get(imfptr, "selected"));

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "include_shapekeys", 0, NULL, ICON_NONE);
	uiLayoutSetEnabled(row, RNA_boolean_get(imfptr, "selected"));

	/* Texture options */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Texture Options:"), ICON_TEXTURE_DATA);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "active_uv_only", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "export_texture_type_selection", 0, "", ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "use_texture_copies", 1, NULL, ICON_NONE);


	/* Armature options */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Armature Options:"), ICON_ARMATURE_DATA);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "deform_bones_only", 0, NULL, ICON_NONE);
	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "open_sim", 0, NULL, ICON_NONE);

	/* Collada options: */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Collada Options:"), ICON_MODIFIER);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "triangulate", 1, NULL, ICON_NONE);
	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "use_object_instantiation", 1, NULL, ICON_NONE);
	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "use_blender_profile", 1, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	split = uiLayoutSplit(row, 0.6f, UI_LAYOUT_ALIGN_RIGHT);
	uiItemL(split, IFACE_("Transformation Type"), ICON_NONE);
	uiItemR(split, imfptr, "export_transformation_type_selection", 0, "", ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "sort_by_name", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "keep_bind_info", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "limit_precision", 0, NULL, ICON_NONE);

}

static void wm_collada_export_draw(bContext *UNUSED(C), wmOperator *op)
{
	PointerRNA ptr;

	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
	uiCollada_exportSettings(op->layout, &ptr);
}

static bool wm_collada_export_check(bContext *UNUSED(C), wmOperator *op)
{
	char filepath[FILE_MAX];
	RNA_string_get(op->ptr, "filepath", filepath);

	if (!BLI_testextensie(filepath, ".dae")) {
		BLI_ensure_extension(filepath, FILE_MAX, ".dae");
		RNA_string_set(op->ptr, "filepath", filepath);
		return true;
	}

	return false;
}

void WM_OT_collada_export(wmOperatorType *ot)
{
	struct StructRNA *func = ot->srna;

	static EnumPropertyItem prop_bc_export_mesh_type[] = {
		{BC_MESH_TYPE_VIEW, "view", 0, "View", "Apply modifier's view settings"},
		{BC_MESH_TYPE_RENDER, "render", 0, "Render", "Apply modifier's render settings"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_bc_export_transformation_type[] = {
		{ BC_TRANSFORMATION_TYPE_MATRIX, "matrix", 0, "Matrix", "Use <matrix> to specify transformations" },
		{ BC_TRANSFORMATION_TYPE_TRANSROTLOC, "transrotloc", 0, "TransRotLoc", "Use <translate>, <rotate>, <scale> to specify transformations" },
		{ 0, NULL, 0, NULL, NULL }
	};

	static EnumPropertyItem prop_bc_export_texture_type[] = {
		{ BC_TEXTURE_TYPE_MAT, "mat", 0, "Materials", "Export Materials" },
		{ BC_TEXTURE_TYPE_UV, "uv", 0, "UV Textures", "Export UV Textures (Face textures) as materials" },
		{ 0, NULL, 0, NULL, NULL }
	};

	ot->name = "Export COLLADA";
	ot->description = "Save a Collada file";
	ot->idname = "WM_OT_collada_export";

	ot->invoke = wm_collada_export_invoke;
	ot->exec = wm_collada_export_exec;
	ot->poll = WM_operator_winactive;
	ot->check = wm_collada_export_check;

	ot->flag |= OPTYPE_PRESET;

	ot->ui = wm_collada_export_draw;

	WM_operator_properties_filesel(
	        ot, FILE_TYPE_FOLDER | FILE_TYPE_COLLADA, FILE_BLENDER, FILE_SAVE,
	        WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);

	RNA_def_boolean(func,
	                "apply_modifiers", 0, "Apply Modifiers",
	                "Apply modifiers to exported mesh (non destructive))");

	RNA_def_int(func, "export_mesh_type", 0, INT_MIN, INT_MAX,
	            "Resolution", "Modifier resolution for export", INT_MIN, INT_MAX);

	RNA_def_enum(func, "export_mesh_type_selection", prop_bc_export_mesh_type, 0,
	             "Resolution", "Modifier resolution for export");

	RNA_def_boolean(func, "selected", 0, "Selection Only",
	                "Export only selected elements");

	RNA_def_boolean(func, "include_children", 0, "Include Children",
	                "Export all children of selected objects (even if not selected)");

	RNA_def_boolean(func, "include_armatures", 0, "Include Armatures",
	                "Export related armatures (even if not selected)");

	RNA_def_boolean(func, "include_shapekeys", 1, "Include Shape Keys",
	                "Export all Shape Keys from Mesh Objects");

	RNA_def_boolean(func, "deform_bones_only", 0, "Deform Bones only",
	                "Only export deforming bones with armatures");

	RNA_def_boolean(func, "active_uv_only", 0, "Only Selected UV Map",
	                "Export only the selected UV Map");

	RNA_def_boolean(func, "use_texture_copies", 1, "Copy",
	                "Copy textures to same folder where the .dae file is exported");


	RNA_def_boolean(func, "triangulate", 1, "Triangulate",
	                "Export Polygons (Quads & NGons) as Triangles");

	RNA_def_boolean(func, "use_object_instantiation", 1, "Use Object Instances",
		"Instantiate multiple Objects from same Data");

	RNA_def_boolean(func, "use_blender_profile", 1, "Use Blender Profile",
		"Export additional Blender specific information (for material, shaders, bones, etc.)");

	RNA_def_boolean(func, "sort_by_name", 0, "Sort by Object name",
	                "Sort exported data by Object name");


	RNA_def_int(func, "export_transformation_type", 0, INT_MIN, INT_MAX,
		"Transform", "Transformation type for translation, scale and rotation", INT_MIN, INT_MAX);

	RNA_def_enum(func, "export_transformation_type_selection", prop_bc_export_transformation_type, 0,
		"Transform", "Transformation type for translation, scale and rotation");


	RNA_def_int(func, "export_texture_type", 0, INT_MIN, INT_MAX,
		"Texture Type", "Type for exported Textures (UV or MAT)", INT_MIN, INT_MAX);

	RNA_def_enum(func, "export_texture_type_selection", prop_bc_export_texture_type, 0,
		"Texture Type", "Type for exported Textures (UV or MAT)");


	RNA_def_boolean(func, "open_sim", 0, "Export to SL/OpenSim",
	                "Compatibility mode for SL, OpenSim and other compatible online worlds");

	RNA_def_boolean(func, "limit_precision", 0,
		"Limit Precision", "Reduce the precision of the exported data to 6 digits");

	RNA_def_boolean(func, "keep_bind_info", 0,
		"Keep Bind Info", "Store Bindpose information in custom bone properties for later use during Collada export");

}


/* function used for WM_OT_save_mainfile too */
static int wm_collada_import_exec(bContext *C, wmOperator *op)
{
	char filename[FILE_MAX];
	int import_units;
	int find_chains;
	int auto_connect;
	int fix_orientation;
	int min_chain_length;

	int keep_bind_info;

	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}

	/* Options panel */
	import_units     = RNA_boolean_get(op->ptr, "import_units");
	find_chains      = RNA_boolean_get(op->ptr, "find_chains");
	auto_connect     = RNA_boolean_get(op->ptr, "auto_connect");
	fix_orientation  = RNA_boolean_get(op->ptr, "fix_orientation");

	keep_bind_info = RNA_boolean_get(op->ptr, "keep_bind_info");

	min_chain_length = RNA_int_get(op->ptr, "min_chain_length");

	RNA_string_get(op->ptr, "filepath", filename);
	if (collada_import(
	        C, filename,
	        import_units,
	        find_chains,
	        auto_connect,
	        fix_orientation,
	        min_chain_length,
	        keep_bind_info) )
	{
		return OPERATOR_FINISHED;
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "Errors found during parsing COLLADA document (see console for details)");
		return OPERATOR_CANCELLED;
	}
}

static void uiCollada_importSettings(uiLayout *layout, PointerRNA *imfptr)
{
	uiLayout *box, *row;

	/* Import Options: */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Import Data Options:"), ICON_MESH_DATA);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "import_units", 0, NULL, ICON_NONE);

	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Armature Options:"), ICON_MESH_DATA);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "fix_orientation", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "find_chains", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "auto_connect", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "min_chain_length", 0, NULL, ICON_NONE);

	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "keep_bind_info", 0, NULL, ICON_NONE);

}

static void wm_collada_import_draw(bContext *UNUSED(C), wmOperator *op)
{
	PointerRNA ptr;

	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
	uiCollada_importSettings(op->layout, &ptr);
}

void WM_OT_collada_import(wmOperatorType *ot)
{
	ot->name = "Import COLLADA";
	ot->description = "Load a Collada file";
	ot->idname = "WM_OT_collada_import";

	ot->invoke = WM_operator_filesel;
	ot->exec = wm_collada_import_exec;
	ot->poll = WM_operator_winactive;

	//ot->flag |= OPTYPE_PRESET;

	ot->ui = wm_collada_import_draw;

	WM_operator_properties_filesel(
	        ot, FILE_TYPE_FOLDER | FILE_TYPE_COLLADA, FILE_BLENDER, FILE_OPENFILE,
	        WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);

	RNA_def_boolean(ot->srna,
		"import_units", 0, "Import Units",
		"If disabled match import to Blender's current Unit settings, "
		"otherwise use the settings from the Imported scene");

	RNA_def_boolean(ot->srna,
		"fix_orientation", 0, "Fix Leaf Bones",
		"Fix Orientation of Leaf Bones (Collada does only support Joints)");

	RNA_def_boolean(ot->srna,
		"find_chains", 0, "Find Bone Chains",
		"Find best matching Bone Chains and ensure bones in chain are connected");

	RNA_def_boolean(ot->srna,
		"auto_connect", 0, "Auto Connect",
		"Set use_connect for parent bones which have exactly one child bone");

	RNA_def_int(ot->srna,
		"min_chain_length",
		0,
		0,
		INT_MAX,
		"Minimum Chain Length",
		"When searching Bone Chains disregard chains of length below this value",
		0,
		INT_MAX);

	RNA_def_boolean(ot->srna, 
		"keep_bind_info", 0, "Keep Bind Info", 
		"Store Bindpose information in custom bone properties for later use during Collada export");

}
#endif
