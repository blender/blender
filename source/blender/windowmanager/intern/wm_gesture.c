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

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_global.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm_event_system.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"


wmGesture *wm_gesture_find(ListBase *list, int type)
{
	wmGesture *gt= list->first;
	while(gt) {
		if(gt->type==type)
			return(gt);
		gt= gt->next;
	}
	return(NULL);
}

wmGesture *wm_gesture_new(int type)
{
	wmGesture *gesture= NULL;
	wmGestureRect *rect;

	if(type==GESTURE_RECT) {
		rect= MEM_mallocN(sizeof(wmGestureRect), "gesture rect new");
		gesture= (wmGesture*) rect;
		gesture->type= type;
		rect->x1= 0;
		rect->y1= 0;
		rect->x2= 1;
		rect->y2= 1;
	}
	return(gesture);
}

void WM_gesture_init(bContext *C, int type)
{
	wmGesture *gt= NULL;

	if(C->window) {
		gt= wm_gesture_find(&C->window->gesture, type);
		if(!gt) {
			gt= wm_gesture_new(type);
			BLI_addtail(&C->window->gesture, gt);
		}
	}
}

void wm_gesture_rect_copy(wmGestureRect *to, wmGestureRect *from)
{
	to->x1= from->x1;
	to->x2= from->x2;
	to->y1= from->y1;
	to->y2= from->y2;
}

void WM_gesture_update(bContext *C, wmGesture *from)
{
	wmGesture *to;

	if(!C->window)
		return;

	to= wm_gesture_find(&C->window->gesture, from->type);
	if(!to)
		return;

	printf("found gesture!!\n");
	if(to->type==GESTURE_RECT)
		wm_gesture_rect_copy((wmGestureRect*)to, (wmGestureRect*)from);
}

void WM_gesture_free(wmWindow *win)
{
	/* Now don't have multiple struct so
	 * a simple BLI_freelistN is what we need.
	 */
	BLI_freelistN(&win->gesture);
}

void WM_gesture_end(bContext *C, int type)
{
	wmGesture *gt;
	wmGestureRect *rect;
	wmBorderSelect *wmbor;
	wmEvent event;

	if(!C->window)
		return;

	gt= wm_gesture_find(&C->window->gesture, type);
	if(!gt)
		return;

	if(gt->type==GESTURE_RECT) {
		rect= (wmGestureRect*)gt;

		wmbor= MEM_mallocN(sizeof(wmBorderSelect), "border select");
		wmbor->x1= rect->x1;
		wmbor->y1= rect->y1;
		wmbor->x2= rect->x2;
		wmbor->y2= rect->y2;

		event.type= BORDERSELECT;
		event.custom= EVT_GESTURE;
		event.customdata= wmbor;
		wm_event_add(C->window, &event);
	}
}
