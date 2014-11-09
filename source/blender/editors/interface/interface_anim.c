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
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_anim.c
 *  \ingroup edinterface
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_animsys.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"

#include "ED_keyframing.h"

#include "UI_interface.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

static FCurve *ui_but_get_fcurve(uiBut *but, bAction **action, bool *r_driven)
{
	/* for entire array buttons we check the first component, it's not perfect
	 * but works well enough in typical cases */
	int rnaindex = (but->rnaindex == -1) ? 0 : but->rnaindex;

	return rna_get_fcurve_context_ui(but->block->evil_C, &but->rnapoin, but->rnaprop, rnaindex, action, r_driven);
}

void ui_but_anim_flag(uiBut *but, float cfra)
{
	FCurve *fcu;
	bool driven;

	but->flag &= ~(UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN);

	fcu = ui_but_get_fcurve(but, NULL, &driven);

	if (fcu) {
		if (!driven) {
			but->flag |= UI_BUT_ANIMATED;
			
			if (fcurve_frame_has_keyframe(fcu, cfra, 0))
				but->flag |= UI_BUT_ANIMATED_KEY;
		}
		else {
			but->flag |= UI_BUT_DRIVEN;
		}
	}
}

bool ui_but_anim_expression_get(uiBut *but, char *str, size_t maxlen)
{
	FCurve *fcu;
	ChannelDriver *driver;
	bool driven;

	fcu = ui_but_get_fcurve(but, NULL, &driven);

	if (fcu && driven) {
		driver = fcu->driver;

		if (driver && driver->type == DRIVER_TYPE_PYTHON) {
			BLI_strncpy(str, driver->expression, maxlen);
			return true;
		}
	}

	return false;
}

bool ui_but_anim_expression_set(uiBut *but, const char *str)
{
	FCurve *fcu;
	ChannelDriver *driver;
	bool driven;

	fcu = ui_but_get_fcurve(but, NULL, &driven);

	if (fcu && driven) {
		driver = fcu->driver;
		
		if (driver && (driver->type == DRIVER_TYPE_PYTHON)) {
			BLI_strncpy_utf8(driver->expression, str, sizeof(driver->expression));
			
			/* tag driver as needing to be recompiled */
			driver->flag |= DRIVER_FLAG_RECOMPILE;
			
			/* clear invalid flags which may prevent this from working */
			driver->flag &= ~DRIVER_FLAG_INVALID;
			fcu->flag &= ~FCURVE_DISABLED;
			
			/* this notifier should update the Graph Editor and trigger depsgraph refresh? */
			WM_event_add_notifier(but->block->evil_C, NC_ANIMATION | ND_KEYFRAME, NULL);
			
			return true;
		}
	}

	return false;
}

/* create new expression for button (i.e. a "scripted driver"), if it can be created... */
bool ui_but_anim_expression_create(uiBut *but, const char *str)
{
	bContext *C = but->block->evil_C;
	ID *id;
	FCurve *fcu;
	char *path;
	bool ok = false;
	
	/* button must have RNA-pointer to a numeric-capable property */
	if (ELEM(NULL, but->rnapoin.data, but->rnaprop)) {
		if (G.debug & G_DEBUG)
			printf("ERROR: create expression failed - button has no RNA info attached\n");
		return false;
	}
	
	if (RNA_property_array_check(but->rnaprop) != 0) {
		if (but->rnaindex == -1) {
			if (G.debug & G_DEBUG)
				printf("ERROR: create expression failed - can't create expression for entire array\n");
			return false;
		}
	}
	
	/* make sure we have animdata for this */
	/* FIXME: until materials can be handled by depsgraph, don't allow drivers to be created for them */
	id = (ID *)but->rnapoin.id.data;
	if ((id == NULL) || (GS(id->name) == ID_MA) || (GS(id->name) == ID_TE)) {
		if (G.debug & G_DEBUG)
			printf("ERROR: create expression failed - invalid id-datablock for adding drivers (%p)\n", id);
		return false;
	}
	
	/* get path */
	path = RNA_path_from_ID_to_property(&but->rnapoin, but->rnaprop);
	if (path == NULL) {
		return false;
	}
	
	/* create driver */
	fcu = verify_driver_fcurve(id, path, but->rnaindex, 1);
	if (fcu) {
		ChannelDriver *driver = fcu->driver;
		
		if (driver) {
			/* set type of driver */
			driver->type = DRIVER_TYPE_PYTHON;

			/* set the expression */
			/* TODO: need some way of identifying variables used */
			BLI_strncpy_utf8(driver->expression, str, sizeof(driver->expression));

			/* updates */
			driver->flag |= DRIVER_FLAG_RECOMPILE;
			WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME, NULL);
			ok = true;
		}
	}
	
	MEM_freeN(path);
	
	return ok;
}

void ui_but_anim_autokey(bContext *C, uiBut *but, Scene *scene, float cfra)
{
	ID *id;
	bAction *action;
	FCurve *fcu;
	bool driven;

	fcu = ui_but_get_fcurve(but, &action, &driven);

	if (fcu && !driven) {
		id = but->rnapoin.id.data;

		/* TODO: this should probably respect the keyingset only option for anim */
		if (autokeyframe_cfra_can_key(scene, id)) {
			ReportList *reports = CTX_wm_reports(C);
			short flag = ANIM_get_keyframing_flags(scene, 1);

			fcu->flag &= ~FCURVE_SELECTED;
			insert_keyframe(reports, id, action, ((fcu->grp) ? (fcu->grp->name) : (NULL)), fcu->rna_path, fcu->array_index, cfra, flag);
			WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
		}
	}
}

void ui_but_anim_insert_keyframe(bContext *C)
{
	/* this operator calls UI_context_active_but_prop_get */
	WM_operator_name_call(C, "ANIM_OT_keyframe_insert_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_delete_keyframe(bContext *C)
{
	/* this operator calls UI_context_active_but_prop_get */
	WM_operator_name_call(C, "ANIM_OT_keyframe_delete_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_clear_keyframe(bContext *C)
{
	/* this operator calls UI_context_active_but_prop_get */
	WM_operator_name_call(C, "ANIM_OT_keyframe_clear_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_add_driver(bContext *C)
{
	/* this operator calls UI_context_active_but_prop_get */
	WM_operator_name_call(C, "ANIM_OT_driver_button_add", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_remove_driver(bContext *C)
{
	/* this operator calls UI_context_active_but_prop_get */
	WM_operator_name_call(C, "ANIM_OT_driver_button_remove", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_copy_driver(bContext *C)
{
	/* this operator calls UI_context_active_but_prop_get */
	WM_operator_name_call(C, "ANIM_OT_copy_driver_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_paste_driver(bContext *C)
{
	/* this operator calls UI_context_active_but_prop_get */
	WM_operator_name_call(C, "ANIM_OT_paste_driver_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_add_keyingset(bContext *C)
{
	/* this operator calls UI_context_active_but_prop_get */
	WM_operator_name_call(C, "ANIM_OT_keyingset_button_add", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_remove_keyingset(bContext *C)
{
	/* this operator calls UI_context_active_but_prop_get */
	WM_operator_name_call(C, "ANIM_OT_keyingset_button_remove", WM_OP_INVOKE_DEFAULT, NULL);
}
