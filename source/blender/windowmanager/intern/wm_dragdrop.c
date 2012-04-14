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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_dragdrop.c
 *  \ingroup wm
 */


#include <string.h>

#include "DNA_windowmanager_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_global.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"
#include "wm.h"


/* ****************************************************** */

static ListBase dropboxes = {NULL, NULL};

/* drop box maps are stored global for now */
/* these are part of blender's UI/space specs, and not like keymaps */
/* when editors become configurable, they can add own dropbox definitions */

typedef struct wmDropBoxMap {
	struct wmDropBoxMap *next, *prev;
	
	ListBase dropboxes;
	short spaceid, regionid;
	char idname[KMAP_MAX_NAME];
	
} wmDropBoxMap;

/* spaceid/regionid is zero for window drop maps */
ListBase *WM_dropboxmap_find(const char *idname, int spaceid, int regionid)
{
	wmDropBoxMap *dm;
	
	for (dm = dropboxes.first; dm; dm = dm->next)
		if (dm->spaceid == spaceid && dm->regionid == regionid)
			if (0 == strncmp(idname, dm->idname, KMAP_MAX_NAME))
				return &dm->dropboxes;
	
	dm = MEM_callocN(sizeof(struct wmDropBoxMap), "dropmap list");
	BLI_strncpy(dm->idname, idname, KMAP_MAX_NAME);
	dm->spaceid = spaceid;
	dm->regionid = regionid;
	BLI_addtail(&dropboxes, dm);
	
	return &dm->dropboxes;
}



wmDropBox *WM_dropbox_add(ListBase *lb, const char *idname, int (*poll)(bContext *, wmDrag *, wmEvent *),
                          void (*copy)(wmDrag *, wmDropBox *))
{
	wmDropBox *drop = MEM_callocN(sizeof(wmDropBox), "wmDropBox");
	
	drop->poll = poll;
	drop->copy = copy;
	drop->ot = WM_operatortype_find(idname, 0);
	drop->opcontext = WM_OP_INVOKE_DEFAULT;
	
	if (drop->ot == NULL) {
		MEM_freeN(drop);
		printf("Error: dropbox with unknown operator: %s\n", idname);
		return NULL;
	}
	WM_operator_properties_alloc(&(drop->ptr), &(drop->properties), idname);
	
	BLI_addtail(lb, drop);
	
	return drop;
}

void wm_dropbox_free(void)
{
	wmDropBoxMap *dm;
	
	for (dm = dropboxes.first; dm; dm = dm->next) {
		wmDropBox *drop;
		
		for (drop = dm->dropboxes.first; drop; drop = drop->next) {
			if (drop->ptr) {
				WM_operator_properties_free(drop->ptr);
				MEM_freeN(drop->ptr);
			}
		}
		BLI_freelistN(&dm->dropboxes);
	}
	
	BLI_freelistN(&dropboxes);		
}

/* *********************************** */

/* note that the pointer should be valid allocated and not on stack */
wmDrag *WM_event_start_drag(struct bContext *C, int icon, int type, void *poin, double value)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmDrag *drag = MEM_callocN(sizeof(struct wmDrag), "new drag");
	
	/* keep track of future multitouch drag too, add a mousepointer id or so */
	/* if multiple drags are added, they're drawn as list */
	
	BLI_addtail(&wm->drags, drag);
	drag->icon = icon;
	drag->type = type;
	if (type == WM_DRAG_PATH)
		BLI_strncpy(drag->path, poin, FILE_MAX);
	else
		drag->poin = poin;
	drag->value = value;
	
	return drag;
}

void WM_event_drag_image(wmDrag *drag, ImBuf *imb, float scale, int sx, int sy)
{
	drag->imb = imb;
	drag->scale = scale;
	drag->sx = sx;
	drag->sy = sy;
}


static const char *dropbox_active(bContext *C, ListBase *handlers, wmDrag *drag, wmEvent *event)
{
	wmEventHandler *handler = handlers->first;
	for (; handler; handler = handler->next) {
		if (handler->dropboxes) {
			wmDropBox *drop = handler->dropboxes->first;
			for (; drop; drop = drop->next) {
				if (drop->poll(C, drag, event)) 
					return drop->ot->name;
			}
		}
	}
	return NULL;
}

/* return active operator name when mouse is in box */
static const char *wm_dropbox_active(bContext *C, wmDrag *drag, wmEvent *event)
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	const char *name;
	
	name = dropbox_active(C, &win->handlers, drag, event);
	if (name) return name;
	
	name = dropbox_active(C, &sa->handlers, drag, event);
	if (name) return name;
	
	name = dropbox_active(C, &ar->handlers, drag, event);
	if (name) return name;

	return NULL;
}


static void wm_drop_operator_options(bContext *C, wmDrag *drag, wmEvent *event)
{
	wmWindow *win = CTX_wm_window(C);
	
	/* for multiwin drags, we only do this if mouse inside */
	if (event->x < 0 || event->y < 0 || event->x > win->sizex || event->y > win->sizey)
		return;
	
	drag->opname[0] = 0;
	
	/* check buttons (XXX todo rna and value) */
	if (UI_but_active_drop_name(C) ) {
		strcpy(drag->opname, "Paste name");
	}
	else {
		const char *opname = wm_dropbox_active(C, drag, event);
		
		if (opname) {
			BLI_strncpy(drag->opname, opname, FILE_MAX);
			// WM_cursor_modal(win, CURSOR_COPY);
		}
		// else
		//	WM_cursor_restore(win);
		/* unsure about cursor type, feels to be too much */
	}
}

/* called in inner handler loop, region context */
void wm_drags_check_ops(bContext *C, wmEvent *event)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmDrag *drag;
	
	for (drag = wm->drags.first; drag; drag = drag->next) {
		wm_drop_operator_options(C, drag, event);
	}
}

/* ************** draw ***************** */

static void wm_drop_operator_draw(const char *name, int x, int y)
{
	int width = UI_GetStringWidth(name);
	
	glColor4ub(0, 0, 0, 50);
	
	uiSetRoundBox(UI_CNR_ALL | UI_RB_ALPHA);
	uiRoundBox(x, y, x + width + 8, y + 15, 4);
	
	glColor4ub(255, 255, 255, 255);
	UI_DrawString(x + 4, y + 4, name);
}

static const char *wm_drag_name(wmDrag *drag)
{
	switch (drag->type) {
		case WM_DRAG_ID:
		{
			ID *id = (ID *)drag->poin;
			return id->name + 2;
		}
		case WM_DRAG_PATH:
			return drag->path;
		case WM_DRAG_NAME:
			return (char *)drag->path;
	}
	return "";
}

static void drag_rect_minmax(rcti *rect, int x1, int y1, int x2, int y2)
{
	if (rect->xmin > x1)
		rect->xmin = x1;
	if (rect->xmax < x2)
		rect->xmax = x2;
	if (rect->ymin > y1)
		rect->ymin = y1;
	if (rect->ymax < y2)
		rect->ymax = y2;
}

/* called in wm_draw.c */
/* if rect set, do not draw */
void wm_drags_draw(bContext *C, wmWindow *win, rcti *rect)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmDrag *drag;
	int cursorx, cursory, x, y;
	
	cursorx = win->eventstate->x;
	cursory = win->eventstate->y;
	if (rect) {
		rect->xmin = rect->xmax = cursorx;
		rect->ymin = rect->ymax = cursory;
	}
	
	/* XXX todo, multiline drag draws... but maybe not, more types mixed wont work well */
	glEnable(GL_BLEND);
	for (drag = wm->drags.first; drag; drag = drag->next) {
		
		/* image or icon */
		if (drag->imb) {
			x = cursorx - drag->sx / 2;
			y = cursory - drag->sy / 2;
			
			if (rect)
				drag_rect_minmax(rect, x, y, x + drag->sx, y + drag->sy);
			else {
				glColor4f(1.0, 1.0, 1.0, 0.65); /* this blends texture */
				glaDrawPixelsTexScaled(x, y, drag->imb->x, drag->imb->y, GL_UNSIGNED_BYTE, drag->imb->rect, drag->scale, drag->scale);
			}
		}
		else {
			x = cursorx - 8;
			y = cursory - 2;
			
			/* icons assumed to be 16 pixels */
			if (rect)
				drag_rect_minmax(rect, x, y, x + 16, y + 16);
			else
				UI_icon_draw_aspect(x, y, drag->icon, 1.0, 0.8);
		}
		
		/* item name */
		if (drag->imb) {
			x = cursorx - drag->sx / 2;
			y = cursory - drag->sy / 2 - 16;
		}
		else {
			x = cursorx + 10;
			y = cursory + 1;
		}
		
		if (rect) {
			int w =  UI_GetStringWidth(wm_drag_name(drag));
			drag_rect_minmax(rect, x, y, x + w, y + 16);
		}
		else {
			glColor4ub(255, 255, 255, 255);
			UI_DrawString(x, y, wm_drag_name(drag));
		}
		
		/* operator name with roundbox */
		if (drag->opname[0]) {
			if (drag->imb) {
				x = cursorx - drag->sx / 2;
				y = cursory + drag->sy / 2 + 4;
			}
			else {
				x = cursorx - 8;
				y = cursory + 16;
			}
			
			if (rect) {
				int w =  UI_GetStringWidth(wm_drag_name(drag));
				drag_rect_minmax(rect, x, y, x + w, y + 16);
			}
			else 
				wm_drop_operator_draw(drag->opname, x, y);
			
		}
	}
	glDisable(GL_BLEND);
}
