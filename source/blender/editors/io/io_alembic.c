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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifdef WITH_ALEMBIC

/* needed for directory lookup */
#ifndef WIN32
#  include <dirent.h>
#else
#  include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "io_alembic.h"

#include "ABC_alembic.h"

static int wm_alembic_export_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		char filepath[FILE_MAX];
		BLI_strncpy(filepath, G.main->name, sizeof(filepath));
		BLI_replace_extension(filepath, sizeof(filepath), ".abc");
		RNA_string_set(op->ptr, "filepath", filepath);
	}

	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;

	UNUSED_VARS(event);
}

static int wm_alembic_export_exec(bContext *C, wmOperator *op)
{
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}

	char filename[FILE_MAX];
	RNA_string_get(op->ptr, "filepath", filename);

	const struct AlembicExportParams params = {
	    .frame_start = RNA_int_get(op->ptr, "start"),
	    .frame_end = RNA_int_get(op->ptr, "end"),

	    .frame_step_xform = 1.0 / (double)RNA_int_get(op->ptr, "xsamples"),
	    .frame_step_shape = 1.0 / (double)RNA_int_get(op->ptr, "gsamples"),

	    .shutter_open = RNA_float_get(op->ptr, "sh_open"),
	    .shutter_close = RNA_float_get(op->ptr, "sh_close"),

	    .selected_only = RNA_boolean_get(op->ptr, "selected"),
	    .uvs = RNA_boolean_get(op->ptr, "uvs"),
	    .normals = RNA_boolean_get(op->ptr, "normals"),
	    .vcolors = RNA_boolean_get(op->ptr, "vcolors"),
	    .apply_subdiv = RNA_boolean_get(op->ptr, "apply_subdiv"),
	    .flatten_hierarchy = RNA_boolean_get(op->ptr, "flatten"),
	    .visible_layers_only = RNA_boolean_get(op->ptr, "visible_layers_only"),
	    .renderable_only = RNA_boolean_get(op->ptr, "renderable_only"),
	    .face_sets = RNA_boolean_get(op->ptr, "face_sets"),
	    .use_subdiv_schema = RNA_boolean_get(op->ptr, "subdiv_schema"),
	    .compression_type = RNA_enum_get(op->ptr, "compression_type"),
	    .packuv = RNA_boolean_get(op->ptr, "packuv"),

	    .global_scale = RNA_float_get(op->ptr, "global_scale"),
	};

	ABC_export(CTX_data_scene(C), C, filename, &params);

	return OPERATOR_FINISHED;
}

static void ui_alembic_export_settings(uiLayout *layout, PointerRNA *imfptr)
{
	uiLayout *box = uiLayoutBox(layout);
	uiLayout *row;

#ifdef WITH_ALEMBIC_HDF5
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Archive Options:"), ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "compression_type", 0, NULL, ICON_NONE);
#endif

	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Manual Transform:"), ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "global_scale", 0, NULL, ICON_NONE);

	/* Scene Options */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Scene Options:"), ICON_SCENE_DATA);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "start", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "end", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "xsamples", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "gsamples", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "sh_open", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "sh_close", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "selected", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "renderable_only", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "visible_layers_only", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "flatten", 0, NULL, ICON_NONE);

	/* Object Data */
	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Object Options:"), ICON_OBJECT_DATA);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "uvs", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "packuv", 0, NULL, ICON_NONE);
	uiLayoutSetEnabled(row, RNA_boolean_get(imfptr, "uvs"));

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "normals", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "vcolors", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "face_sets", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "subdiv_schema", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "apply_subdiv", 0, NULL, ICON_NONE);
}

static void wm_alembic_export_draw(bContext *UNUSED(C), wmOperator *op)
{
	PointerRNA ptr;

	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
	ui_alembic_export_settings(op->layout, &ptr);
}

void WM_OT_alembic_export(wmOperatorType *ot)
{
	ot->name = "Export Alembic Archive";
	ot->idname = "WM_OT_alembic_export";

	ot->invoke = wm_alembic_export_invoke;
	ot->exec = wm_alembic_export_exec;
	ot->poll = WM_operator_winactive;
	ot->ui = wm_alembic_export_draw;

	WM_operator_properties_filesel(ot, FILE_TYPE_FOLDER | FILE_TYPE_ALEMBIC,
	                               FILE_BLENDER, FILE_SAVE, WM_FILESEL_FILEPATH,
	                               FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);

	RNA_def_int(ot->srna, "start", 1, INT_MIN, INT_MAX,
	            "Start Frame", "Start Frame", INT_MIN, INT_MAX);

	RNA_def_int(ot->srna, "end", 1, INT_MIN, INT_MAX,
	            "End Frame", "End Frame", INT_MIN, INT_MAX);

	RNA_def_int(ot->srna, "xsamples", 1, 1, 128,
	            "Transform Samples", "Number of times per frame transformations are sampled", 1, 128);

	RNA_def_int(ot->srna, "gsamples", 1, 1, 128,
	            "Geometry Samples", "Number of times per frame object datas are sampled", 1, 128);

	RNA_def_float(ot->srna, "sh_open", 0.0f, -1.0f, 1.0f,
	              "Shutter Open", "Time at which the shutter is open", -1.0f, 1.0f);

	RNA_def_float(ot->srna, "sh_close", 1.0f, -1.0f, 1.0f,
	              "Shutter Close", "Time at which the shutter is closed", -1.0f, 1.0f);

	RNA_def_boolean(ot->srna, "selected", 0,
	                "Selected Objects Only", "Export only selected objects");

	RNA_def_boolean(ot->srna, "renderable_only", 1,
	                "Renderable Objects Only",
	                "Export only objects marked renderable in the outliner");

	RNA_def_boolean(ot->srna, "visible_layers_only", 0,
	                "Visible Layers Only", "Export only objects in visible layers");

	RNA_def_boolean(ot->srna, "flatten", 0,
	                "Flatten Hierarchy",
	                "Do not preserve objects' parent/children relationship");

	RNA_def_boolean(ot->srna, "uvs", 1, "UVs", "Export UVs");

	RNA_def_boolean(ot->srna, "packuv", 1, "Pack UV Islands",
	                "Export UVs with packed island");

	RNA_def_boolean(ot->srna, "normals", 1, "Normals", "Export normals");

	RNA_def_boolean(ot->srna, "vcolors", 0, "Vertex colors", "Export vertex colors");

	RNA_def_boolean(ot->srna, "face_sets", 0, "Face Sets", "Export per face shading group assignments");

	RNA_def_boolean(ot->srna, "subdiv_schema", 0,
	                "Use Subdivision Schema",
	                "Export meshes using Alembic's subdivision schema");

	RNA_def_boolean(ot->srna, "apply_subdiv", 0,
	                "Apply Subsurf", "Export subdivision surfaces as meshes");

	RNA_def_enum(ot->srna, "compression_type", rna_enum_abc_compression_items,
	             ABC_ARCHIVE_OGAWA, "Compression", "");

	RNA_def_float(ot->srna, "global_scale", 1.0f, 0.0001f, 1000.0f, "Scale",
	              "Value by which to enlarge or shrink the objects with respect to the world's origin",
	              0.0001f, 1000.0f);
}

/* ************************************************************************** */

/* TODO(kevin): check on de-duplicating all this with code in image_ops.c */

typedef struct CacheFrame {
	struct CacheFrame *next, *prev;
	int framenr;
} CacheFrame;

static int cmp_frame(const void *a, const void *b)
{
	const CacheFrame *frame_a = a;
	const CacheFrame *frame_b = b;

	if (frame_a->framenr < frame_b->framenr) return -1;
	if (frame_a->framenr > frame_b->framenr) return 1;
	return 0;
}

static int get_sequence_len(char *filename, int *ofs)
{
	int frame;
	int numdigit;

	if (!BLI_path_frame_get(filename, &frame, &numdigit)) {
		return 1;
	}

	char path[FILE_MAX];
	BLI_split_dir_part(filename, path, FILE_MAX);

	DIR *dir = opendir(path);

	const char *ext = ".abc";
	const char *basename = BLI_path_basename(filename);
	const int len = strlen(basename) - (numdigit + strlen(ext));

	ListBase frames;
	BLI_listbase_clear(&frames);

	struct dirent *fname;
	while ((fname = readdir(dir)) != NULL) {
		/* do we have the right extension? */
		if (!strstr(fname->d_name, ext)) {
			continue;
		}

		if (!STREQLEN(basename, fname->d_name, len)) {
			continue;
		}

		CacheFrame *cache_frame = MEM_callocN(sizeof(CacheFrame), "abc_frame");

		BLI_path_frame_get(fname->d_name, &cache_frame->framenr, &numdigit);

		BLI_addtail(&frames, cache_frame);
	}

	closedir(dir);

	BLI_listbase_sort(&frames, cmp_frame);

	CacheFrame *cache_frame = frames.first;

	if (cache_frame) {
		int frame_curr = cache_frame->framenr;
		(*ofs) = frame_curr;

		while (cache_frame && (cache_frame->framenr == frame_curr)) {
			++frame_curr;
			cache_frame = cache_frame->next;
		}

		BLI_freelistN(&frames);

		return frame_curr - (*ofs);
	}

	return 1;
}

/* ************************************************************************** */

static void ui_alembic_import_settings(uiLayout *layout, PointerRNA *imfptr)
{
	uiLayout *box = uiLayoutBox(layout);
	uiLayout *row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Manual Transform:"), ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "scale", 0, NULL, ICON_NONE);

	box = uiLayoutBox(layout);
	row = uiLayoutRow(box, false);
	uiItemL(row, IFACE_("Options:"), ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "set_frame_range", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "is_sequence", 0, NULL, ICON_NONE);

	row = uiLayoutRow(box, false);
	uiItemR(row, imfptr, "validate_meshes", 0, NULL, ICON_NONE);
}

static void wm_alembic_import_draw(bContext *UNUSED(C), wmOperator *op)
{
	PointerRNA ptr;

	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
	ui_alembic_import_settings(op->layout, &ptr);
}

static int wm_alembic_import_exec(bContext *C, wmOperator *op)
{
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}

	char filename[FILE_MAX];
	RNA_string_get(op->ptr, "filepath", filename);

	const float scale = RNA_float_get(op->ptr, "scale");
	const bool is_sequence = RNA_boolean_get(op->ptr, "is_sequence");
	const bool set_frame_range = RNA_boolean_get(op->ptr, "set_frame_range");
	const bool validate_meshes = RNA_boolean_get(op->ptr, "validate_meshes");

	int offset = 0;
	int sequence_len = 1;

	if (is_sequence) {
		sequence_len = get_sequence_len(filename, &offset);
	}

	ABC_import(C, filename, scale, is_sequence, set_frame_range, sequence_len, offset, validate_meshes);

	return OPERATOR_FINISHED;
}

void WM_OT_alembic_import(wmOperatorType *ot)
{
	ot->name = "Import Alembic Archive";
	ot->idname = "WM_OT_alembic_import";

	ot->invoke = WM_operator_filesel;
	ot->exec = wm_alembic_import_exec;
	ot->poll = WM_operator_winactive;
	ot->ui = wm_alembic_import_draw;

	WM_operator_properties_filesel(ot, FILE_TYPE_FOLDER | FILE_TYPE_ALEMBIC,
	                               FILE_BLENDER, FILE_SAVE, WM_FILESEL_FILEPATH,
	                               FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);

	RNA_def_float(ot->srna, "scale", 1.0f, 0.0001f, 1000.0f, "Scale",
	              "Value by which to enlarge or shrink the objects with respect to the world's origin",
	              0.0001f, 1000.0f);

	RNA_def_boolean(ot->srna, "set_frame_range", true,
	                "Set Frame Range",
	                "If checked, update scene's start and end frame to match those of the Alembic archive");

	RNA_def_boolean(ot->srna, "validate_meshes", 0,
	                "Validate Meshes", "Check imported mesh objects for invalid data (slow)");

	RNA_def_boolean(ot->srna, "is_sequence", false, "Is Sequence",
	                "Set to true if the cache is split into separate files");
}

#endif
