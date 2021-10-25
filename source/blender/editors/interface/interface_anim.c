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

#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_nla.h"

#include "ED_keyframing.h"

#include "UI_interface.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

static FCurve *ui_but_get_fcurve(uiBut *but, AnimData **adt, bAction **action, bool *r_driven, bool *r_special)
{
	/* for entire array buttons we check the first component, it's not perfect
	 * but works well enough in typical cases */
	int rnaindex = (but->rnaindex == -1) ? 0 : but->rnaindex;

	return rna_get_fcurve_context_ui(but->block->evil_C, &but->rnapoin, but->rnaprop, rnaindex, adt, action, r_driven, r_special);
}

void ui_but_anim_flag(uiBut *but, float cfra)
{
	AnimData *adt;
	bAction *act;
	FCurve *fcu;
	bool driven;
	bool special;
	
	but->flag &= ~(UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN);
	
	/* NOTE: "special" is reserved for special F-Curves stored on the animation data
	 *        itself (which are used to animate properties of the animation data).
	 *        We count those as "animated" too for now
	 */
	fcu = ui_but_get_fcurve(but, &adt, &act, &driven, &special);
	
	if (fcu) {
		if (!driven) {
			but->flag |= UI_BUT_ANIMATED;
			
			/* T41525 - When the active action is a NLA strip being edited, 
			 * we need to correct the frame number to "look inside" the
			 * remapped action
			 */
			if (adt)
				cfra = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);
			
			if (fcurve_frame_has_keyframe(fcu, cfra, 0))
				but->flag |= UI_BUT_ANIMATED_KEY;
		}
		else {
			but->flag |= UI_BUT_DRIVEN;
		}
	}
}

/**
 * \a str can be NULL to only perform check if \a but has an expression at all.
 * \return if button has an expression.
 */
bool ui_but_anim_expression_get(uiBut *but, char *str, size_t maxlen)
{
	FCurve *fcu;
	ChannelDriver *driver;
	bool driven, special;
	
	fcu = ui_but_get_fcurve(but, NULL, NULL, &driven, &special);
	
	if (fcu && driven) {
		driver = fcu->driver;

		if (driver && driver->type == DRIVER_TYPE_PYTHON) {
			if (str) {
				BLI_strncpy(str, driver->expression, maxlen);
			}
			return true;
		}
	}

	return false;
}

bool ui_but_anim_expression_set(uiBut *but, const char *str)
{
	FCurve *fcu;
	ChannelDriver *driver;
	bool driven, special;

	fcu = ui_but_get_fcurve(but, NULL, NULL, &driven, &special);

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
			printf("ERROR: create expression failed - invalid data-block for adding drivers (%p)\n", id);
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
			DAG_relations_tag_update(CTX_data_main(C));
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
	bool special;

	fcu = ui_but_get_fcurve(but, NULL, &action, &driven, &special);
	
	if (fcu == NULL)
		return;
	
	if (special) {
		/* NLA Strip property */
		if (IS_AUTOKEY_ON(scene)) {
			ReportList *reports = CTX_wm_reports(C);
			ToolSettings *ts = scene->toolsettings;
			
			insert_keyframe_direct(reports, but->rnapoin, but->rnaprop, fcu, cfra, ts->keyframe_type, 0);
			WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
		}
	}
	else if (driven) {
		/* Driver - Try to insert keyframe using the driver's input as the frame,
		 * making it easier to set up corrective drivers
		 */
		if (IS_AUTOKEY_ON(scene)) {
			ReportList *reports = CTX_wm_reports(C);
			ToolSettings *ts = scene->toolsettings;
			
			insert_keyframe_direct(reports, but->rnapoin, but->rnaprop, fcu, cfra, ts->keyframe_type, INSERTKEY_DRIVER);
			WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
		}
	}
	else {
		id = but->rnapoin.id.data;
		
		/* TODO: this should probably respect the keyingset only option for anim */
		if (autokeyframe_cfra_can_key(scene, id)) {
			ReportList *reports = CTX_wm_reports(C);
			ToolSettings *ts = scene->toolsettings;
			short flag = ANIM_get_keyframing_flags(scene, 1);
			
			fcu->flag &= ~FCURVE_SELECTED;
			
			/* Note: We use but->rnaindex instead of fcu->array_index,
			 *       because a button may control all items of an array at once.
			 *       E.g., color wheels (see T42567). */
			BLI_assert((fcu->array_index == but->rnaindex) || (but->rnaindex == -1));
			insert_keyframe(reports, id, action, ((fcu->grp) ? (fcu->grp->name) : (NULL)),
			                fcu->rna_path, but->rnaindex, cfra, ts->keyframe_type, flag);
			
			WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
		}
	}
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
