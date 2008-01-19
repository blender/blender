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

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"


wmGesture *WM_gesture_new(int type)
{
	wmGesture *gesture= NULL;
	wmGestureRect *rect;

	if(type==GESTURE_RECT) {
		gesture= rect= MEM_mallocN(sizeof(wmGestureRect), "gesture rect new");
		gesture->type= type;
		rect->x1= 0;
		rect->y1= 0;
		rect->x2= 1;
		rect->y2= 1;
	}
	return(gesture);
}

void wm_gesture_rect_copy(wmGestureRect *to, wmGestureRect *from)
{
	to->x1= from->x1;
	to->x2= from->x2;
	to->y1= from->y1;
	to->y2= from->y2;
}

wmGesture *WM_gesture_dup(wmGesture *from)
{
	wmGesture *to= WM_gesture_new(from->type);

	if(from->type==GESTURE_RECT)
		wm_gesture_rect_copy((wmGestureRect *) to, (wmGestureRect *) from);
	return (to);
}

void WM_gesture_send(wmWindow *win, wmGesture *gesture)
{
	wmGestureRect *rect;
	wmBorderSelect *wmbor;
	wmEvent event;

	if(gesture->type==GESTURE_RECT) {
		rect= (wmGestureRect*)gesture;

		wmbor= MEM_mallocN(sizeof(wmBorderSelect), "border select");
		wmbor->x1= rect->x1;
		wmbor->y1= rect->y1;
		wmbor->x2= rect->x2;
		wmbor->y2= rect->y2;

		event.type= BORDERSELECT;
		event.custom= EVT_GESTURE;
		event.customdata= wmbor;
		wm_event_add(win, &event);
	}
}
