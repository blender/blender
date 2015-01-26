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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_toolbar.c
 *  \ingroup spview3d
 */


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_screen.h"


#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_screen.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h"  /* own include */


/* ******************* view3d space & buttons ************** */

static void view3d_panel_operator_redo_buts(const bContext *C, Panel *pa, wmOperator *op)
{
	uiLayoutOperatorButs(C, pa->layout, op, NULL, 'V', 0);
}

static void view3d_panel_operator_redo_header(const bContext *C, Panel *pa)
{
	wmOperator *op = WM_operator_last_redo(C);

	if (op)
		BLI_strncpy(pa->drawname, RNA_struct_ui_name(op->type->srna), sizeof(pa->drawname));
	else
		BLI_strncpy(pa->drawname, IFACE_("Operator"), sizeof(pa->drawname));
}

static void view3d_panel_operator_redo_operator(const bContext *C, Panel *pa, wmOperator *op)
{
	if (op->type->flag & OPTYPE_MACRO) {
		for (op = op->macro.first; op; op = op->next) {
			uiItemL(pa->layout, RNA_struct_ui_name(op->type->srna), ICON_NONE);
			view3d_panel_operator_redo_operator(C, pa, op);
		}
	}
	else {
		view3d_panel_operator_redo_buts(C, pa, op);
	}
}

/* TODO de-duplicate redo panel functions - campbell */
static void view3d_panel_operator_redo(const bContext *C, Panel *pa)
{
	wmOperator *op = WM_operator_last_redo(C);
	ARegion *ar;
	ARegion *ar1;

	if (op == NULL) {
		return;
	}

	/* keep in sync with logic in ED_undo_operator_repeat() */
	ar = CTX_wm_region(C);
	ar1 = BKE_area_find_region_active_win(CTX_wm_area(C));
	if (ar1)
		CTX_wm_region_set((bContext *)C, ar1);

	if (WM_operator_poll((bContext *)C, op->type)) {
		uiBlock *block = uiLayoutGetBlock(pa->layout);

		if (!WM_operator_check_ui_enabled(C, op->type->name))
			uiLayoutSetEnabled(pa->layout, false);

		/* note, blockfunc is a default but->func, use Handle func to allow button callbacks too */
		UI_block_func_handle_set(block, ED_undo_operator_repeat_cb_evt, op);

		view3d_panel_operator_redo_operator(C, pa, op);
	}

	/* set region back */
	CTX_wm_region_set((bContext *)C, ar);
}

/* ******************* */

typedef struct CustomTool {
	struct CustomTool *next, *prev;
	char opname[OP_MAX_TYPENAME];
	char context[OP_MAX_TYPENAME];
} CustomTool;

static void operator_call_cb(struct bContext *C, void *arg_listbase, void *arg2)
{
	wmOperatorType *ot = arg2;
	
	if (ot) {
		CustomTool *ct = MEM_callocN(sizeof(CustomTool), "CustomTool");
		
		BLI_addtail(arg_listbase, ct);
		BLI_strncpy(ct->opname, ot->idname, OP_MAX_TYPENAME);
		BLI_strncpy(ct->context, CTX_data_mode_string(C), OP_MAX_TYPENAME);
	}
		
}

static void operator_search_cb(const struct bContext *C, void *UNUSED(arg), const char *str, uiSearchItems *items)
{
	GHashIterator iter;

	for (WM_operatortype_iter(&iter); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		wmOperatorType *ot = BLI_ghashIterator_getValue(&iter);

		if (BLI_strcasestr(ot->name, str)) {
			if (WM_operator_poll((bContext *)C, ot)) {
				
				if (false == UI_search_item_add(items, ot->name, ot, 0))
					break;
			}
		}
	}
}


/* ID Search browse menu, open */
static uiBlock *tool_search_menu(bContext *C, ARegion *ar, void *arg_listbase)
{
	static char search[OP_MAX_TYPENAME];
	wmEvent event;
	wmWindow *win = CTX_wm_window(C);
	uiBlock *block;
	uiBut *but;
	
	/* clear initial search string, then all items show */
	search[0] = 0;
	
	block = UI_block_begin(C, ar, "_popup", UI_EMBOSS);
	UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_SEARCH_MENU);
	
	/* fake button, it holds space for search items */
	uiDefBut(block, UI_BTYPE_LABEL, 0, "", 10, 15, UI_searchbox_size_x(), UI_searchbox_size_y(), NULL, 0, 0, 0, 0, NULL);
	
	but = uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 10, 0, 150, 19, 0, 0, "");
	UI_but_func_search_set(but, operator_search_cb, arg_listbase, operator_call_cb, NULL);
	
	UI_block_bounds_set_normal(block, 6);
	UI_block_direction_set(block, UI_DIR_DOWN);
	UI_block_end(C, block);
	
	wm_event_init_from_window(win, &event);
	event.type = EVT_BUT_OPEN;
	event.val = KM_PRESS;
	event.customdata = but;
	event.customdatafree = false;
	wm_event_add(win, &event);
	
	return block;
}


static void view3d_panel_tool_shelf(const bContext *C, Panel *pa)
{
	SpaceLink *sl = CTX_wm_space_data(C);
	SpaceType *st = NULL;
	uiLayout *col;
	const char *context = CTX_data_mode_string(C);
	
	if (sl)
		st = BKE_spacetype_from_id(sl->spacetype);
	
	if (st && st->toolshelf.first) {
		CustomTool *ct;
		
		for (ct = st->toolshelf.first; ct; ct = ct->next) {
			if (STREQLEN(context, ct->context, OP_MAX_TYPENAME)) {
				col = uiLayoutColumn(pa->layout, true);
				uiItemFullO(col, ct->opname, NULL, ICON_NONE, NULL, WM_OP_INVOKE_REGION_WIN, 0);
			}
		}
	}
	col = uiLayoutColumn(pa->layout, true);
	uiDefBlockBut(uiLayoutGetBlock(pa->layout), tool_search_menu, &st->toolshelf, "Add Tool", 0, 0, UI_UNIT_X, UI_UNIT_Y, "Add Tool in shelf, gets saved in files");
}


void view3d_toolshelf_register(ARegionType *art)
{
	PanelType *pt;

	pt = MEM_callocN(sizeof(PanelType), "spacetype view3d panel tools");
	strcpy(pt->idname, "VIEW3D_PT_tool_shelf");
	strcpy(pt->label, N_("Tool Shelf"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = view3d_panel_tool_shelf;
	BLI_addtail(&art->paneltypes, pt);
}

void view3d_tool_props_register(ARegionType *art)
{
	PanelType *pt;
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype view3d panel last operator");
	strcpy(pt->idname, "VIEW3D_PT_last_operator");
	strcpy(pt->label, N_("Operator"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw_header = view3d_panel_operator_redo_header;
	pt->draw = view3d_panel_operator_redo;
	BLI_addtail(&art->paneltypes, pt);
}

/* ********** operator to open/close toolshelf region */

static int view3d_toolshelf_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = view3d_has_tools_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_toolshelf(wmOperatorType *ot)
{
	ot->name = "Tool Shelf";
	ot->description = "Toggles tool shelf display";
	ot->idname = "VIEW3D_OT_toolshelf";
	
	ot->exec = view3d_toolshelf_toggle_exec;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = 0;
}

