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
 * The Original Code is Copyright (C) 2004 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/util/undo.c
 *  \ingroup edutil
 */



#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"

#include "ED_armature.h"
#include "ED_particle.h"
#include "ED_curve.h"
#include "ED_gpencil.h"
#include "ED_mball.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_util.h"
#include "ED_text.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "util_intern.h"

#define MAXUNDONAME 64 /* XXX, make common define */

/* ***************** generic undo system ********************* */

void ED_undo_push(bContext *C, const char *str)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	Object *obedit = CTX_data_edit_object(C);
	Object *obact = CTX_data_active_object(C);

	if (G.debug & G_DEBUG)
		printf("undo push %s\n", str);
	
	if (obedit) {
		if (U.undosteps == 0) return;
		
		if (obedit->type == OB_MESH)
			undo_push_mesh(C, str);
		else if (ELEM(obedit->type, OB_CURVE, OB_SURF))
			undo_push_curve(C, str);
		else if (obedit->type == OB_FONT)
			undo_push_font(C, str);
		else if (obedit->type == OB_MBALL)
			undo_push_mball(C, str);
		else if (obedit->type == OB_LATTICE)
			undo_push_lattice(C, str);
		else if (obedit->type == OB_ARMATURE)
			undo_push_armature(C, str);
	}
	else if (obact && obact->mode & OB_MODE_PARTICLE_EDIT) {
		if (U.undosteps == 0) return;
		
		PE_undo_push(CTX_data_scene(C), str);
	}
	else {
		if (U.uiflag & USER_GLOBALUNDO) 
			BKE_write_undo(C, str);
	}
	
	if (wm->file_saved) {
		wm->file_saved = 0;
		/* notifier that data changed, for save-over warning or header */
		WM_event_add_notifier(C, NC_WM | ND_DATACHANGED, NULL);
	}
}

/* note: also check undo_history_exec() in bottom if you change notifiers */
static int ed_undo_step(bContext *C, int step, const char *undoname)
{	
	Object *obedit = CTX_data_edit_object(C);
	Object *obact = CTX_data_active_object(C);
	ScrArea *sa = CTX_wm_area(C);

	/* undo during jobs are running can easily lead to freeing data using by jobs,
	 * or they can just lead to freezing job in some other cases */
	if (WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C))) {
		return OPERATOR_CANCELLED;
	}

	/* grease pencil can be can be used in plenty of spaces, so check it first */
	if (ED_gpencil_session_active()) {
		return ED_undo_gpencil_step(C, step, undoname);
	}

	if (sa && (sa->spacetype == SPACE_IMAGE)) {
		SpaceImage *sima = (SpaceImage *)sa->spacedata.first;
		
		if ((obact && (obact->mode & OB_MODE_TEXTURE_PAINT)) || (sima->mode == SI_MODE_PAINT)) {
			if (!ED_undo_paint_step(C, UNDO_PAINT_IMAGE, step, undoname) && undoname)
				if (U.uiflag & USER_GLOBALUNDO)
					BKE_undo_name(C, undoname);
			
			WM_event_add_notifier(C, NC_WINDOW, NULL);
			return OPERATOR_FINISHED;
		}
	}

	if (sa && (sa->spacetype == SPACE_TEXT)) {
		ED_text_undo_step(C, step);
	}
	else if (obedit) {
		if (OB_TYPE_SUPPORT_EDITMODE(obedit->type)) {
			if (undoname)
				undo_editmode_name(C, undoname);
			else
				undo_editmode_step(C, step);
			
			WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
		}
	}
	else {
		int do_glob_undo = FALSE;
		
		if (obact && obact->mode & OB_MODE_TEXTURE_PAINT) {
			if (!ED_undo_paint_step(C, UNDO_PAINT_IMAGE, step, undoname))
				do_glob_undo = TRUE;
		}
		else if (obact && obact->mode & OB_MODE_SCULPT) {
			if (!ED_undo_paint_step(C, UNDO_PAINT_MESH, step, undoname))
				do_glob_undo = TRUE;
		}
		else if (obact && obact->mode & OB_MODE_PARTICLE_EDIT) {
			if (step == 1)
				PE_undo(CTX_data_scene(C));
			else
				PE_redo(CTX_data_scene(C));
		}
		else {
			do_glob_undo = TRUE;
		}
		
		if (do_glob_undo) {
			if (U.uiflag & USER_GLOBALUNDO) {
				// note python defines not valid here anymore.
				//#ifdef WITH_PYTHON
				// XXX		BPY_scripts_clear_pyobjects();
				//#endif
				if (undoname)
					BKE_undo_name(C, undoname);
				else
					BKE_undo_step(C, step);
				
				WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, CTX_data_scene(C));
			}
			
		}
	}
	
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	
	return OPERATOR_FINISHED;
}

void ED_undo_pop(bContext *C)
{
	ed_undo_step(C, 1, NULL);
}
void ED_undo_redo(bContext *C)
{
	ed_undo_step(C, -1, NULL);
}

void ED_undo_push_op(bContext *C, wmOperator *op)
{
	/* in future, get undo string info? */
	ED_undo_push(C, op->type->name);
}

void ED_undo_pop_op(bContext *C, wmOperator *op)
{
	/* search back a couple of undo's, in case something else added pushes */
	ed_undo_step(C, 0, op->type->name);
}

/* name optionally, function used to check for operator redo panel */
int ED_undo_valid(const bContext *C, const char *undoname)
{
	Object *obedit = CTX_data_edit_object(C);
	Object *obact = CTX_data_active_object(C);
	ScrArea *sa = CTX_wm_area(C);
	
	if (sa && sa->spacetype == SPACE_IMAGE) {
		SpaceImage *sima = (SpaceImage *)sa->spacedata.first;
		
		if ((obact && (obact->mode & OB_MODE_TEXTURE_PAINT)) || (sima->mode == SI_MODE_PAINT)) {
			return 1;
		}
	}
	
	if (sa && (sa->spacetype == SPACE_TEXT)) {
		return 1;
	}
	else if (obedit) {
		if (OB_TYPE_SUPPORT_EDITMODE(obedit->type)) {
			return undo_editmode_valid(undoname);
		}
	}
	else {
		
		/* if below tests fail, global undo gets executed */
		
		if (obact && obact->mode & OB_MODE_TEXTURE_PAINT) {
			if (ED_undo_paint_valid(UNDO_PAINT_IMAGE, undoname))
				return 1;
		}
		else if (obact && obact->mode & OB_MODE_SCULPT) {
			if (ED_undo_paint_valid(UNDO_PAINT_MESH, undoname))
				return 1;
		}
		else if (obact && obact->mode & OB_MODE_PARTICLE_EDIT) {
			return PE_undo_valid(CTX_data_scene(C));
		}
		
		if (U.uiflag & USER_GLOBALUNDO) {
			return BKE_undo_valid(undoname);
		}
	}
	return 0;
}

static int ed_undo_exec(bContext *C, wmOperator *UNUSED(op))
{
	/* "last operator" should disappear, later we can tie this with undo stack nicer */
	WM_operator_stack_clear(CTX_wm_manager(C));
	return ed_undo_step(C, 1, NULL);
}

static int ed_undo_push_exec(bContext *C, wmOperator *op)
{
	char str[MAXUNDONAME];
	RNA_string_get(op->ptr, "message", str);
	ED_undo_push(C, str);
	return OPERATOR_FINISHED;
}

static int ed_redo_exec(bContext *C, wmOperator *UNUSED(op))
{
	return ed_undo_step(C, -1, NULL);
}


/* ********************** */

void ED_OT_undo(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Undo";
	ot->description = "Undo previous action";
	ot->idname = "ED_OT_undo";
	
	/* api callbacks */
	ot->exec = ed_undo_exec;
	ot->poll = ED_operator_screenactive;
}

void ED_OT_undo_push(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Undo Push";
	ot->description = "Add an undo state (internal use only)";
	ot->idname = "ED_OT_undo_push";
	
	/* api callbacks */
	ot->exec = ed_undo_push_exec;

	ot->flag = OPTYPE_INTERNAL;

	RNA_def_string(ot->srna, "message", "Add an undo step *function may be moved*", MAXUNDONAME, "Undo Message", "");
}

void ED_OT_redo(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Redo";
	ot->description = "Redo previous action";
	ot->idname = "ED_OT_redo";
	
	/* api callbacks */
	ot->exec = ed_redo_exec;
	ot->poll = ED_operator_screenactive;
}


/* ui callbacks should call this rather than calling WM_operator_repeat() themselves */
int ED_undo_operator_repeat(bContext *C, struct wmOperator *op)
{
	int ret = 0;

	if (op) {
		wmWindowManager *wm = CTX_wm_manager(C);
		struct Scene *scene = CTX_data_scene(C);

		ARegion *ar = CTX_wm_region(C);
		ARegion *ar1 = BKE_area_find_region_type(CTX_wm_area(C), RGN_TYPE_WINDOW);

		if (ar1)
			CTX_wm_region_set(C, ar1);

		if ( (WM_operator_repeat_check(C, op)) &&
		     (WM_operator_poll(C, op->type)) &&
		     /* note, undo/redo cant run if there are jobs active,
		      * check for screen jobs only so jobs like material/texture/world preview
		      * (which copy their data), wont stop redo, see [#29579]],
		      *
		      * note, - WM_operator_check_ui_enabled() jobs test _must_ stay in sync with this */
		     (WM_jobs_test(wm, scene) == 0))
		{
			int retval;

			if (G.debug & G_DEBUG)
				printf("redo_cb: operator redo %s\n", op->type->name);
			ED_undo_pop_op(C, op);

			if (op->type->check) {
				op->type->check(C, op); /* ignore return value since its running again anyway */
			}

			retval = WM_operator_repeat(C, op);
			if ((retval & OPERATOR_FINISHED) == 0) {
				if (G.debug & G_DEBUG)
					printf("redo_cb: operator redo failed: %s, return %d\n", op->type->name, retval);
				ED_undo_redo(C);
			}
			else {
				ret = 1;
			}
		}
		else {
			if (G.debug & G_DEBUG) {
				printf("redo_cb: WM_operator_repeat_check returned false %s\n", op->type->name);
			}
		}

		/* set region back */
		CTX_wm_region_set(C, ar);
	}
	else {
		if (G.debug & G_DEBUG) {
			printf("redo_cb: ED_undo_operator_repeat called with NULL 'op'\n");
		}
	}

	return ret;
}


void ED_undo_operator_repeat_cb(bContext *C, void *arg_op, void *UNUSED(arg_unused))
{
	ED_undo_operator_repeat(C, (wmOperator *)arg_op);
}

void ED_undo_operator_repeat_cb_evt(bContext *C, void *arg_op, int UNUSED(arg_event))
{
	ED_undo_operator_repeat(C, (wmOperator *)arg_op);
}


/* ************************** */

enum {
	UNDOSYSTEM_GLOBAL   = 1,
	UNDOSYSTEM_EDITMODE = 2,
	UNDOSYSTEM_PARTICLE = 3
};

static int get_undo_system(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	
	/* find out which undo system */
	if (obedit) {
		if (OB_TYPE_SUPPORT_EDITMODE(obedit->type)) {
			return UNDOSYSTEM_EDITMODE;
		}
	}
	else {
		Object *obact = CTX_data_active_object(C);
		
		if (obact && obact->mode & OB_MODE_PARTICLE_EDIT)
			return UNDOSYSTEM_PARTICLE;
		else if (U.uiflag & USER_GLOBALUNDO)
			return UNDOSYSTEM_GLOBAL;
	}
	
	return 0;
}

/* create enum based on undo items */
static EnumPropertyItem *rna_undo_itemf(bContext *C, int undosys, int *totitem)
{
	EnumPropertyItem item_tmp = {0}, *item = NULL;
	int active, i = 0;
	
	while (TRUE) {
		const char *name = NULL;
		
		if (undosys == UNDOSYSTEM_PARTICLE) {
			name = PE_undo_get_name(CTX_data_scene(C), i, &active);
		}
		else if (undosys == UNDOSYSTEM_EDITMODE) {
			name = undo_editmode_get_name(C, i, &active);
		}
		else {
			name = BKE_undo_get_name(i, &active);
		}
		
		if (name) {
			item_tmp.identifier = name;
			/* XXX This won't work with non-default contexts (e.g. operators) :/ */
			item_tmp.name = IFACE_(name);
			if (active)
				item_tmp.icon = ICON_RESTRICT_VIEW_OFF;
			else 
				item_tmp.icon = ICON_NONE;
			item_tmp.value = i++;
			RNA_enum_item_add(&item, totitem, &item_tmp);
		}
		else
			break;
	}
	
	RNA_enum_item_end(&item, totitem);
	
	return item;
}


static int undo_history_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	int undosys, totitem = 0;
	
	undosys = get_undo_system(C);
	
	if (undosys) {
		EnumPropertyItem *item = rna_undo_itemf(C, undosys, &totitem);
		
		if (totitem > 0) {
			uiPopupMenu *pup = uiPupMenuBegin(C, RNA_struct_ui_name(op->type->srna), ICON_NONE);
			uiLayout *layout = uiPupMenuLayout(pup);
			uiLayout *split = uiLayoutSplit(layout, 0.0f, FALSE);
			uiLayout *column = NULL;
			int i, c;
			
			for (c = 0, i = totitem - 1; i >= 0; i--, c++) {
				if ( (c % 20) == 0)
					column = uiLayoutColumn(split, FALSE);
				if (item[i].identifier)
					uiItemIntO(column, item[i].name, item[i].icon, op->type->idname, "item", item[i].value);
				
			}
			
			MEM_freeN(item);
			
			uiPupMenuEnd(C, pup);
		}		
		
	}
	return OPERATOR_CANCELLED;
}

/* note: also check ed_undo_step() in top if you change notifiers */
static int undo_history_exec(bContext *C, wmOperator *op)
{
	if (RNA_struct_property_is_set(op->ptr, "item")) {
		int undosys = get_undo_system(C);
		int item = RNA_int_get(op->ptr, "item");
		
		if (undosys == UNDOSYSTEM_PARTICLE) {
			PE_undo_number(CTX_data_scene(C), item);
		}
		else if (undosys == UNDOSYSTEM_EDITMODE) {
			undo_editmode_number(C, item + 1);
			WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
		}
		else {
			BKE_undo_number(C, item);
			WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, CTX_data_scene(C));
		}
		WM_event_add_notifier(C, NC_WINDOW, NULL);
		
		return OPERATOR_FINISHED;
	}
	return OPERATOR_CANCELLED;
}

void ED_OT_undo_history(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Undo History";
	ot->description = "Redo specific action in history";
	ot->idname = "ED_OT_undo_history";
	
	/* api callbacks */
	ot->invoke = undo_history_invoke;
	ot->exec = undo_history_exec;
	ot->poll = ED_operator_screenactive;
	
	RNA_def_int(ot->srna, "item", 0, 0, INT_MAX, "Item", "", 0, INT_MAX);

}


