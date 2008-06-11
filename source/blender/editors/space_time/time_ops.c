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

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"

#include "BKE_global.h"

#include "BIF_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

/* ********************** frame change operator ***************************/

static int change_frame_init(bContext *C, wmOperator *op)
{
	SpaceTime *stime= C->area->spacedata.first;
	int cfra;

	if(!OP_get_int(op, "frame", &cfra))
		return 0;
	
	stime->flag |= TIME_CFRA_NUM;
	
	return 1;
}

static void change_frame_apply(bContext *C, wmOperator *op)
{
	int cfra;

	OP_get_int(op, "frame", &cfra);

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

	if(cfra!=CFRA)
		CFRA= cfra;
	
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	/* XXX: add WM_NOTE_TIME_CHANGED? */
}

static void change_frame_exit(bContext *C, wmOperator *op)
{
	SpaceTime *stime= C->area->spacedata.first;

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
	SpaceTime *stime= C->area->spacedata.first;
	ARegion *region= C->region;
	int x, y;
	float viewx;

	/* XXX region->winrect isn't updated on window changes */
	x= event->x - region->winrct.xmin;
	y= event->y - region->winrct.ymin;
	BIF_view2d_region_to_view(&stime->v2d, x, y, &viewx, NULL);

	return (int)(viewx+0.5f);
}

static int change_frame_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	OP_verify_int(op, "frame", frame_from_event(C, event), NULL);
	change_frame_init(C, op);
	change_frame_apply(C, op);

	/* add temp handler */
	WM_event_add_modal_handler(&C->region->handlers, op);

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
			OP_set_int(op, "frame", frame_from_event(C, event));
			change_frame_apply(C, op);
			break;
			
		case LEFTMOUSE:
			if(event->val==0) {
				change_frame_exit(C, op);
				WM_event_remove_modal_handler(&C->region->handlers, op);				
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

/* Operator for joining two areas (space types) */
void ED_TIME_OT_change_frame(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Change frame";
	ot->idname= "ED_TIME_OT_change_frame";
	
	/* api callbacks */
	ot->exec= change_frame_exec;
	ot->invoke= change_frame_invoke;
	ot->cancel= change_frame_cancel;
	ot->modal= change_frame_modal;
}

/* ************************** registration **********************************/

void time_operatortypes(void)
{
	WM_operatortype_append(ED_TIME_OT_change_frame);
}

void time_keymap(wmWindowManager *wm)
{
	WM_keymap_verify_item(&wm->timekeymap, "ED_TIME_OT_change_frame", LEFTMOUSE, KM_PRESS, 0, 0);
}

