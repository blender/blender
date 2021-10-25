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

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_blender_undo.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#include "ED_armature.h"
#include "ED_particle.h"
#include "ED_curve.h"
#include "ED_gpencil.h"
#include "ED_mball.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_paint.h"
#include "ED_util.h"
#include "ED_text.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "util_intern.h"

/* ***************** generic undo system ********************* */

void ED_undo_push(bContext *C, const char *str)
{
	Object *obedit = CTX_data_edit_object(C);
	Object *obact = CTX_data_active_object(C);

	if (G.debug & G_DEBUG)
		printf("%s: %s\n", __func__, str);

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
	else if (obact && obact->mode & OB_MODE_SCULPT) {
		/* do nothing for now */
	}
	else {
		BKE_undo_write(C, str);
	}

	WM_file_tag_modified(C);
}

/* note: also check undo_history_exec() in bottom if you change notifiers */
static int ed_undo_step(bContext *C, int step, const char *undoname)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Object *obact = CTX_data_active_object(C);
	ScrArea *sa = CTX_wm_area(C);

	/* undo during jobs are running can easily lead to freeing data using by jobs,
	 * or they can just lead to freezing job in some other cases */
	if (WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY)) {
		return OPERATOR_CANCELLED;
	}

	/* grease pencil can be can be used in plenty of spaces, so check it first */
	if (ED_gpencil_session_active()) {
		return ED_undo_gpencil_step(C, step, undoname);
	}

	if (sa && (sa->spacetype == SPACE_IMAGE)) {
		SpaceImage *sima = (SpaceImage *)sa->spacedata.first;
		
		if ((obact && (obact->mode & OB_MODE_TEXTURE_PAINT)) || (sima->mode == SI_MODE_PAINT)) {
			if (!ED_undo_paint_step(C, UNDO_PAINT_IMAGE, step, undoname) && undoname) {
				if (U.uiflag & USER_GLOBALUNDO) {
					ED_viewport_render_kill_jobs(wm, bmain, true);
					BKE_undo_name(C, undoname);
				}
			}
			
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
		/* Note: we used to do a fall-through here where if the
		 * mode-specific undo system had no more steps to undo (or
		 * redo), the global undo would run.
		 *
		 * That was inconsistent with editmode, and also makes for
		 * unecessarily tricky interaction with the other undo
		 * systems. */
		if (obact && obact->mode & OB_MODE_TEXTURE_PAINT) {
			ED_undo_paint_step(C, UNDO_PAINT_IMAGE, step, undoname);
		}
		else if (obact && obact->mode & OB_MODE_SCULPT) {
			ED_undo_paint_step(C, UNDO_PAINT_MESH, step, undoname);
		}
		else if (obact && obact->mode & OB_MODE_PARTICLE_EDIT) {
			if (step == 1)
				PE_undo(scene);
			else
				PE_redo(scene);
		}
		else if (U.uiflag & USER_GLOBALUNDO) {
			// note python defines not valid here anymore.
			//#ifdef WITH_PYTHON
			// XXX		BPY_scripts_clear_pyobjects();
			//#endif
			
			/* for global undo/redo we should just clear the editmode stack */
			/* for example, texface stores image pointers */
			undo_editmode_clear();
			
			ED_viewport_render_kill_jobs(wm, bmain, true);

			if (undoname)
				BKE_undo_name(C, undoname);
			else
				BKE_undo_step(C, step);

			scene = CTX_data_scene(C);
				
			WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
		}
	}
	
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	WM_event_add_notifier(C, NC_WM | ND_UNDO, NULL);

	if (win) {
		win->addmousemove = true;
	}
	
	return OPERATOR_FINISHED;
}

void ED_undo_grouped_push(bContext *C, const char *str)
{
	/* do nothing if previous undo task is the same as this one (or from the same undo group) */
	const char *last_undo = BKE_undo_get_name_last();

	if (last_undo && STREQ(str, last_undo)) {
		return;
	}

	/* push as usual */
	ED_undo_push(C, str);
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

void ED_undo_grouped_push_op(bContext *C, wmOperator *op)
{
	if (op->type->undo_group[0] != '\0') {
		ED_undo_grouped_push(C, op->type->undo_group);
	}
	else {
		ED_undo_grouped_push(C, op->type->name);
	}
}

void ED_undo_pop_op(bContext *C, wmOperator *op)
{
	/* search back a couple of undo's, in case something else added pushes */
	ed_undo_step(C, 0, op->type->name);
}

/* name optionally, function used to check for operator redo panel */
bool ED_undo_is_valid(const bContext *C, const char *undoname)
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
			return undo_editmode_is_valid(undoname);
		}
	}
	else {
		
		/* if below tests fail, global undo gets executed */
		
		if (obact && obact->mode & OB_MODE_TEXTURE_PAINT) {
			if (ED_undo_paint_is_valid(UNDO_PAINT_IMAGE, undoname))
				return 1;
		}
		else if (obact && obact->mode & OB_MODE_SCULPT) {
			if (ED_undo_paint_is_valid(UNDO_PAINT_MESH, undoname))
				return 1;
		}
		else if (obact && obact->mode & OB_MODE_PARTICLE_EDIT) {
			return PE_undo_is_valid(CTX_data_scene(C));
		}
		
		if (U.uiflag & USER_GLOBALUNDO) {
			return BKE_undo_is_valid(undoname);
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
	char str[BKE_UNDO_STR_MAX];
	RNA_string_get(op->ptr, "message", str);
	ED_undo_push(C, str);
	return OPERATOR_FINISHED;
}

static int ed_redo_exec(bContext *C, wmOperator *UNUSED(op))
{
	return ed_undo_step(C, -1, NULL);
}

static int ed_undo_redo_exec(bContext *C, wmOperator *UNUSED(op))
{
	wmOperator *last_op = WM_operator_last_redo(C);
	const int ret = ED_undo_operator_repeat(C, last_op);
	return ret ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int ed_undo_redo_poll(bContext *C)
{
	wmOperator *last_op = WM_operator_last_redo(C);
	return last_op && ED_operator_screenactive(C) && 
		WM_operator_check_ui_enabled(C, last_op->type->name);
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

	RNA_def_string(ot->srna, "message", "Add an undo step *function may be moved*", BKE_UNDO_STR_MAX, "Undo Message", "");
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

void ED_OT_undo_redo(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Undo and Redo";
	ot->description = "Undo and redo previous action";
	ot->idname = "ED_OT_undo_redo";
	
	/* api callbacks */
	ot->exec = ed_undo_redo_exec;
	ot->poll = ed_undo_redo_poll;
}

/* ui callbacks should call this rather than calling WM_operator_repeat() themselves */
int ED_undo_operator_repeat(bContext *C, struct wmOperator *op)
{
	int ret = 0;

	if (op) {
		wmWindowManager *wm = CTX_wm_manager(C);
		struct Scene *scene = CTX_data_scene(C);

		/* keep in sync with logic in view3d_panel_operator_redo() */
		ARegion *ar = CTX_wm_region(C);
		ARegion *ar1 = BKE_area_find_region_active_win(CTX_wm_area(C));

		if (ar1)
			CTX_wm_region_set(C, ar1);

		if ((WM_operator_repeat_check(C, op)) &&
		    (WM_operator_poll(C, op->type)) &&
		     /* note, undo/redo cant run if there are jobs active,
		      * check for screen jobs only so jobs like material/texture/world preview
		      * (which copy their data), wont stop redo, see [#29579]],
		      *
		      * note, - WM_operator_check_ui_enabled() jobs test _must_ stay in sync with this */
		    (WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY) == 0))
		{
			int retval;

			ED_viewport_render_kill_jobs(wm, CTX_data_main(C), true);

			if (G.debug & G_DEBUG)
				printf("redo_cb: operator redo %s\n", op->type->name);

			WM_operator_free_all_after(wm, op);

			ED_undo_pop_op(C, op);

			if (op->type->check) {
				if (op->type->check(C, op)) {
					/* check for popup and re-layout buttons */
					ARegion *ar_menu = CTX_wm_menu(C);
					if (ar_menu) {
						ED_region_tag_refresh_ui(ar_menu);
					}
				}
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
	UNDOSYSTEM_PARTICLE = 3,
	UNDOSYSTEM_IMAPAINT = 4,
	UNDOSYSTEM_SCULPT   = 5,
};

static int get_undo_system(bContext *C)
{
	Object *obact = CTX_data_active_object(C);
	Object *obedit = CTX_data_edit_object(C);
	ScrArea *sa = CTX_wm_area(C);

	/* first check for editor undo */
	if (sa && (sa->spacetype == SPACE_IMAGE)) {
		SpaceImage *sima = (SpaceImage *)sa->spacedata.first;

		if ((obact && (obact->mode & OB_MODE_TEXTURE_PAINT)) || (sima->mode == SI_MODE_PAINT)) {
			if (!ED_undo_paint_empty(UNDO_PAINT_IMAGE))
				return UNDOSYSTEM_IMAPAINT;
		}
	}
	/* find out which undo system */
	if (obedit) {
		if (OB_TYPE_SUPPORT_EDITMODE(obedit->type)) {
			return UNDOSYSTEM_EDITMODE;
		}
	}
	else {
		if (obact) {
			if (obact->mode & OB_MODE_PARTICLE_EDIT)
				return UNDOSYSTEM_PARTICLE;
			else if (obact->mode & OB_MODE_TEXTURE_PAINT) {
				if (!ED_undo_paint_empty(UNDO_PAINT_IMAGE))
					return UNDOSYSTEM_IMAPAINT;
			}
			else if (obact->mode & OB_MODE_SCULPT) {
				if (!ED_undo_paint_empty(UNDO_PAINT_MESH))
					return UNDOSYSTEM_SCULPT;
			}
		}
		if (U.uiflag & USER_GLOBALUNDO)
			return UNDOSYSTEM_GLOBAL;
	}
	
	return 0;
}

/* create enum based on undo items */
static EnumPropertyItem *rna_undo_itemf(bContext *C, int undosys, int *totitem)
{
	EnumPropertyItem item_tmp = {0}, *item = NULL;
	int i = 0;
	bool active;
	
	while (true) {
		const char *name = NULL;
		
		if (undosys == UNDOSYSTEM_PARTICLE) {
			name = PE_undo_get_name(CTX_data_scene(C), i, &active);
		}
		else if (undosys == UNDOSYSTEM_EDITMODE) {
			name = undo_editmode_get_name(C, i, &active);
		}
		else if (undosys == UNDOSYSTEM_IMAPAINT) {
			name = ED_undo_paint_get_name(C, UNDO_PAINT_IMAGE, i, &active);
		}
		else if (undosys == UNDOSYSTEM_SCULPT) {
			name = ED_undo_paint_get_name(C, UNDO_PAINT_MESH, i, &active);
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


static int undo_history_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	int undosys, totitem = 0;
	
	undosys = get_undo_system(C);
	
	if (undosys) {
		EnumPropertyItem *item = rna_undo_itemf(C, undosys, &totitem);
		
		if (totitem > 0) {
			uiPopupMenu *pup = UI_popup_menu_begin(C, RNA_struct_ui_name(op->type->srna), ICON_NONE);
			uiLayout *layout = UI_popup_menu_layout(pup);
			uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
			uiLayout *column = NULL;
			const int col_size = 20 + totitem / 12;
			int i, c;
			bool add_col = true;
			
			for (c = 0, i = totitem; i--;) {
				if (add_col && !(c % col_size)) {
					column = uiLayoutColumn(split, false);
					add_col = false;
				}
				if (item[i].identifier) {
					uiItemIntO(column, item[i].name, item[i].icon, op->type->idname, "item", item[i].value);
					++c;
					add_col = true;
				}
			}
			
			MEM_freeN(item);
			
			UI_popup_menu_end(C, pup);
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
		else if (undosys == UNDOSYSTEM_IMAPAINT) {
			ED_undo_paint_step_num(C, UNDO_PAINT_IMAGE, item);
		}
		else if (undosys == UNDOSYSTEM_SCULPT) {
			ED_undo_paint_step_num(C, UNDO_PAINT_MESH, item);
		}
		else {
			ED_viewport_render_kill_jobs(CTX_wm_manager(C), CTX_data_main(C), true);
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


