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

	int include_uv_textures;
	int include_material_textures;
	int use_texture_copies;
	int active_uv_only;

	int triangulate;
	int use_object_instantiation;
	int sort_by_name;
	int export_transformation_type;
	int open_sim; 

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

	include_uv_textures      = RNA_boolean_get(op->ptr, "include_uv_textures");
	include_material_textures = RNA_boolean_get(op->ptr, "include_material_textures");
	use_texture_copies       = RNA_boolean_get(op->ptr, "use_texture_copies");
	active_uv_only           = RNA_boolean_get(op->ptr, "active_uv_only");

	triangulate                = RNA_boolean_get(op->ptr, "triangulate");
	use_object_instantiation   = RNA_boolean_get(op->ptr, "use_object_instantiation");
	sort_by_name               = RNA_boolean_get(op->ptr, "sort_by_name");
	export_transformation_type = RNA_enum_get(op->ptr,    "export_transformation_type_selection");
	open_sim                   = RNA_boolean_get(op->ptr, "open_sim");

	/* get editmode results */
	ED_object_editmode_load(CTX_data_edit_object(C));



	if (collada_export(CTX_data_scene(C),
	                   filepath,
	                   apply_modifiers,
	                   export_mesh_type,
	                   selected,
	                   include_children,
	                   include_armatures,
	                   include_shapekeys,
	                   deform_bones_only,

	                   active_uv_only,
	                   include_uv_textures,
	                   include_material_textures,
	                   use_texture_copies,

	                   triangulate,
	                   use_object_instantiation,
	                   sort_by_name,
	                   export_transformation_type,
	                   open_sim))
	{
		return OPERATOR_FINISHED;
	}
	else {
		BKE_report(op->reports, RPT_WARNING, "Export file not created");
		return OPERATOR_CANCELLED;
	}
}

static void uiCollada_exportSettings(uiLayout *layout, PointerRNA *imfptr)
{
	uiLayout *box, *row, *col, *split;

	/* Export Options: */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, FALSE);
	uiItemL(row, IFACE_("Export Data Options:"), ICON_MESH_DATA);

	row = uiLayoutRow(box, FALSE);
	split = uiLayoutSplit(row, 0.6f, UI_LAYOUT_ALIGN_RIGHT);
	col   = uiLayoutColumn(split, FALSE);
	uiItemR(col, imfptr, "apply_modifiers", 0, NULL, ICON_NONE);
	col   = uiLayoutColumn(split, FALSE);
	uiItemR(col, imfptr, "export_mesh_type_selection", 0, "", ICON_NONE);
	uiLayoutSetEnabled(col, RNA_boolean_get(imfptr, "apply_modifiers"));

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "selected", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "include_children", 0, NULL, ICON_NONE);
	uiLayoutSetEnabled(row, RNA_boolean_get(imfptr, "selected"));

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "include_armatures", 0, NULL, ICON_NONE);
	uiLayoutSetEnabled(row, RNA_boolean_get(imfptr, "selected"));

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "include_shapekeys", 0, NULL, ICON_NONE);
	uiLayoutSetEnabled(row, RNA_boolean_get(imfptr, "selected"));

	/* Texture options */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, FALSE);
	uiItemL(row, IFACE_("Texture Options:"), ICON_TEXTURE_DATA);

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "active_uv_only", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "include_uv_textures", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "include_material_textures", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "use_texture_copies", 1, NULL, ICON_NONE);


	/* Armature options */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, FALSE);
	uiItemL(row, IFACE_("Armature Options:"), ICON_ARMATURE_DATA);

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "deform_bones_only", 0, NULL, ICON_NONE);
	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "open_sim", 0, NULL, ICON_NONE);

	/* Collada options: */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, FALSE);
	uiItemL(row, IFACE_("Collada Options:"), ICON_MODIFIER);

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "triangulate", 0, NULL, ICON_NONE);
	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "use_object_instantiation", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, FALSE);
	split = uiLayoutSplit(row, 0.6f, UI_LAYOUT_ALIGN_RIGHT);
    uiItemL(split, IFACE_("Transformation Type"), ICON_NONE);
	uiItemR(split, imfptr, "export_transformation_type_selection", 0, "", ICON_NONE);

	row = uiLayoutRow(box, FALSE);
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

	static EnumPropertyItem prop_bc_export_transformation_type[] = {
		{BC_TRANSFORMATION_TYPE_MATRIX, "matrix", 0, "Matrix", "Use <matrix> to specify transformations"},
		{BC_TRANSFORMATION_TYPE_TRANSROTLOC, "transrotloc", 0, "TransRotLoc", "Use <translate>, <rotate>, <scale> to specify transformations"},
		{BC_TRANSFORMATION_TYPE_BOTH, "both", 0, "Both", "Use <matrix> AND <translate>, <rotate>, <scale> to specify transformations"},
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

	WM_operator_properties_filesel(ot, FOLDERFILE | COLLADAFILE, FILE_BLENDER, FILE_SAVE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);

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

	RNA_def_boolean(ot->srna, "include_shapekeys", 1, "Include Shape Keys",
	                "Export all Shape Keys from Mesh Objects");

	RNA_def_boolean(ot->srna, "deform_bones_only", 0, "Deform Bones only",
	                "Only export deforming bones with armatures");


	RNA_def_boolean(ot->srna, "active_uv_only", 0, "Only Active UV layer",
	                "Export textures assigned to the object UV maps");

	RNA_def_boolean(ot->srna, "include_uv_textures", 0, "Include UV Textures",
	                "Export textures assigned to the object UV maps");

	RNA_def_boolean(ot->srna, "include_material_textures", 0, "Include Material Textures",
	                "Export textures assigned to the object Materials");

	RNA_def_boolean(ot->srna, "use_texture_copies", 1, "Copy",
	                "Copy textures to same folder where the .dae file is exported");


	RNA_def_boolean(ot->srna, "triangulate", 1, "Triangulate",
	                "Export Polygons (Quads & NGons) as Triangles");

	RNA_def_boolean(ot->srna, "use_object_instantiation", 1, "Use Object Instances",
	                "Instantiate multiple Objects from same Data");

	RNA_def_boolean(ot->srna, "sort_by_name", 0, "Sort by Object name",
	                "Sort exported data by Object name");

	RNA_def_int(ot->srna, "export_transformation_type", 0, INT_MIN, INT_MAX,
	            "Transform", "Transformation type for translation, scale and rotation", INT_MIN, INT_MAX);

	RNA_def_enum(ot->srna, "export_transformation_type_selection", prop_bc_export_transformation_type, 0,
	             "Transform", "Transformation type for translation, scale and rotation");

	RNA_def_boolean(ot->srna, "open_sim", 0, "Export for OpenSim",
	                "Compatibility mode for OpenSim and compatible online worlds");
}


/* function used for WM_OT_save_mainfile too */
static int wm_collada_import_exec(bContext *C, wmOperator *op)
{
	char filename[FILE_MAX];
	int import_units;

	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}

	/* Options panel */
	import_units = RNA_boolean_get(op->ptr, "import_units");

	RNA_string_get(op->ptr, "filepath", filename);
	if (collada_import(C, filename, import_units)) {
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
	row = uiLayoutRow(box, FALSE);
	uiItemL(row, IFACE_("Import Data Options:"), ICON_MESH_DATA);

	row = uiLayoutRow(box, FALSE);
	uiItemR(row, imfptr, "import_units", 0, NULL, ICON_NONE);
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

	WM_operator_properties_filesel(ot, FOLDERFILE | COLLADAFILE, FILE_BLENDER, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);

	RNA_def_boolean(ot->srna,
	                "import_units", 0, "Import Units",
	                "If disabled match import to Blender's current Unit settings, "
	                "otherwise use the settings from the Imported scene");

}
#endif
