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

/** \file blender/makesrna/intern/rna_workspace_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"

#include "DNA_object_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_enum_types.h"  /* own include */

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME

#include "BKE_paint.h"

#include "ED_screen.h"

static void rna_WorkspaceTool_setup(
        ID *id,
        bToolRef *tref,
        bContext *C,
        const char *name,
        /* Args for: 'bToolRef_Runtime'. */
        int cursor,
        const char *keymap,
        const char *gizmo_group,
        const char *data_block,
        const char *operator,
        int index)
{
	bToolRef_Runtime tref_rt = {0};

	tref_rt.cursor = cursor;
	STRNCPY(tref_rt.keymap, keymap);
	STRNCPY(tref_rt.gizmo_group, gizmo_group);
	STRNCPY(tref_rt.data_block, data_block);
	STRNCPY(tref_rt.operator, operator);
	tref_rt.index = index;

	WM_toolsystem_ref_set_from_runtime(C, (WorkSpace *)id, tref, &tref_rt, name);
}

static void rna_WorkspaceTool_refresh_from_context(
        ID *id,
        bToolRef *tref,
        Main *bmain)
{
	bToolRef_Runtime *tref_rt = tref->runtime;
	if ((tref_rt == NULL) || (tref_rt->data_block[0] == '\0')) {
		return;
	}
	wmWindowManager *wm = bmain->wm.first;
	for (wmWindow *win = wm->windows.first; win; win = win->next) {
		WorkSpace *workspace = WM_window_get_active_workspace(win);
		if (&workspace->id == id) {
			Scene *scene = WM_window_get_active_scene(win);
			ToolSettings *ts = scene->toolsettings;
			ViewLayer *view_layer = WM_window_get_active_view_layer(win);
			Object *ob = OBACT(view_layer);
			if (ob == NULL) {
				/* pass */
			}
			else if (ob->mode & OB_MODE_PARTICLE_EDIT) {
				const EnumPropertyItem *items = rna_enum_particle_edit_hair_brush_items;
				const int i = RNA_enum_from_value(items, ts->particle.brushtype);
				const EnumPropertyItem *item = &items[i];
				if (!STREQ(tref_rt->data_block, item->identifier)) {
					STRNCPY(tref_rt->data_block, item->identifier);
					STRNCPY(tref->idname, item->name);
				}
			}
			else {
				Paint *paint = BKE_paint_get_active(scene, view_layer);
				if (paint) {
					const ID *brush = (ID *)paint->brush;
					if (brush) {
						if (!STREQ(tref_rt->data_block, brush->name + 2)) {
							STRNCPY(tref_rt->data_block, brush->name + 2);
							STRNCPY(tref->idname, brush->name + 2);
						}
					}
				}
			}
		}
	}
}

static PointerRNA rna_WorkspaceTool_operator_properties(
        bToolRef *tref,
        const char *idname)
{
	wmOperatorType *ot = WM_operatortype_find(idname, true);

	if (ot != NULL) {
		PointerRNA ptr;
		WM_toolsystem_ref_properties_ensure(tref, ot, &ptr);
		return ptr;
	}
	return PointerRNA_NULL;
}

#else

void RNA_api_workspace(StructRNA *srna)
{
	FunctionRNA *func;

	func = RNA_def_function(srna, "status_text_set", "ED_workspace_status_text");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Set the status bar text, typically key shortcuts for modal operators");
	RNA_def_string(func, "text", NULL, 0, "Text", "New string for the status bar, no argument clears the text");
}

void RNA_api_workspace_tool(StructRNA *srna)
{
	PropertyRNA *parm;
	FunctionRNA *func;

	func = RNA_def_function(srna, "setup", "rna_WorkspaceTool_setup");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Set the tool settings");

	parm = RNA_def_string(func, "name", NULL, KMAP_MAX_NAME, "Name", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	/* 'bToolRef_Runtime' */
	parm = RNA_def_property(func, "cursor", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(parm, rna_enum_window_cursor_items);
	RNA_def_string(func, "keymap", NULL, KMAP_MAX_NAME, "Key Map", "");
	RNA_def_string(func, "gizmo_group", NULL, MAX_NAME, "Gizmo Group", "");
	RNA_def_string(func, "data_block", NULL, MAX_NAME, "Data Block", "");
	RNA_def_string(func, "operator", NULL, MAX_NAME, "Operator", "");
	RNA_def_int(func, "index", 0, INT_MIN, INT_MAX, "Index", "", INT_MIN, INT_MAX);

	/* Access tool operator options (optionally create). */
	func = RNA_def_function(srna, "operator_properties", "rna_WorkspaceTool_operator_properties");
	parm = RNA_def_string(func, "operator", NULL, 0, "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	/* return */
	parm = RNA_def_pointer(func, "result", "OperatorProperties", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR);
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "refresh_from_context", "rna_WorkspaceTool_refresh_from_context");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
}

#endif
