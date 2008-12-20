/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_markers.h"

/* ********************** frame change operator ***************************/

static int change_frame_init(bContext *C, wmOperator *op)
{
	SpaceTime *stime= (SpaceTime*)CTX_wm_space_data(C);

	stime->flag |= TIME_CFRA_NUM;
	
	return 1;
}

static void change_frame_apply(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	int cfra;

	cfra= RNA_int_get(op->ptr, "frame");

	if(cfra < MINFRAME)
		cfra= MINFRAME;

#if 0
	if( cfra!=CFRA || first )
	{
		first= 0;
		CFRA= cfra;
		update_for_newframe_nodraw(0);  // 1= nosound
		timeline_force_draw(stime->redraws);
	}
	else PIL_sleep_ms(30);
#endif

	if(cfra!=scene->r.cfra)
		scene->r.cfra= cfra;
	
	WM_event_add_notifier(C, WM_NOTE_WINDOW_REDRAW, 0, NULL);
	/* XXX: add WM_NOTE_TIME_CHANGED? */
}

static void change_frame_exit(bContext *C, wmOperator *op)
{
	SpaceTime *stime= (SpaceTime*)CTX_wm_space_data(C);

	stime->flag &= ~TIME_CFRA_NUM;
}

static int change_frame_exec(bContext *C, wmOperator *op)
{
	if(!change_frame_init(C, op))
		return OPERATOR_CANCELLED;
	
	change_frame_apply(C, op);
	change_frame_exit(C, op);
	return OPERATOR_FINISHED;
}

static int frame_from_event(bContext *C, wmEvent *event)
{
	ARegion *region= CTX_wm_region(C);
	int x, y;
	float viewx;
	
	x= event->x - region->winrct.xmin;
	y= event->y - region->winrct.ymin;
	UI_view2d_region_to_view(&region->v2d, x, y, &viewx, NULL);

	return (int)floor(viewx+0.5f);
}

static int change_frame_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	RNA_int_set(op->ptr, "frame", frame_from_event(C, event));
	change_frame_init(C, op);
	change_frame_apply(C, op);

	/* add temp handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);

	return OPERATOR_RUNNING_MODAL;
}

static int change_frame_cancel(bContext *C, wmOperator *op)
{
	change_frame_exit(C, op);
	return OPERATOR_CANCELLED;
}

static int change_frame_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			RNA_int_set(op->ptr, "frame", frame_from_event(C, event));
			change_frame_apply(C, op);
			break;
			
		case LEFTMOUSE:
			if(event->val==0) {
				change_frame_exit(C, op);
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

void ED_TIME_OT_change_frame(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Change frame";
	ot->idname= "ED_TIME_OT_change_frame";
	
	/* api callbacks */
	ot->exec= change_frame_exec;
	ot->invoke= change_frame_invoke;
	ot->cancel= change_frame_cancel;
	ot->modal= change_frame_modal;

	/* rna */
	prop= RNA_def_property(ot->srna, "frame", PROP_INT, PROP_NONE);
}

/* ****************** time display toggle operator ****************************/

static int toggle_time_exec(bContext *C, wmOperator *op)
{
	SpaceTime *stime= (SpaceTime *)CTX_wm_space_data(C);
	ScrArea *curarea= CTX_wm_area(C);
	
	if (ELEM(NULL, curarea, stime))
		return OPERATOR_CANCELLED;
	
	/* simply toggle draw frames flag for now */
	// in past, this asked user to choose in a menu beforehand, but that is clumsy
	stime->flag ^= TIME_DRAWFRAMES;
	
	ED_area_tag_redraw(curarea);
	
	return OPERATOR_FINISHED;
}

void ED_TIME_OT_toggle_time(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Frames/Seconds";
	ot->idname= "ED_TIME_OT_toggle_time";
	
	/* api callbacks */
	ot->exec= toggle_time_exec;
}

/* ************************** registration **********************************/

void time_operatortypes(void)
{
	WM_operatortype_append(ED_TIME_OT_change_frame);
	WM_operatortype_append(ED_TIME_OT_toggle_time);
}

void time_keymap(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "TimeLine", SPACE_TIME, 0);
	
	WM_keymap_verify_item(keymap, "ED_TIME_OT_change_frame", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "ED_TIME_OT_toggle_time", TKEY, KM_PRESS, 0, 0);
}

