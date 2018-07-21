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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/wm_event_system.h
 *  \ingroup wm
 */

#ifndef __WM_EVENT_SYSTEM_H__
#define __WM_EVENT_SYSTEM_H__

/* return value of handler-operator call */
#define WM_HANDLER_CONTINUE  0
#define WM_HANDLER_BREAK     1
#define WM_HANDLER_HANDLED   2
#define WM_HANDLER_MODAL     4 /* MODAL|BREAK means unhandled */

struct ScrArea;
struct ARegion;

/* wmKeyMap is in DNA_windowmanager.h, it's savable */

struct wmEventHandler_KeymapFn {
	void (*handle_post_fn)(wmKeyMap *keymap, wmKeyMapItem *kmi, void *user_data);
	void  *user_data;
};

typedef struct wmEventHandler {
	struct wmEventHandler *next, *prev;

	char type;                          /* WM_HANDLER_DEFAULT, ... */
	char flag;                          /* WM_HANDLER_BLOCKING, ... */

	/* keymap handler */
	wmKeyMap *keymap;                   /* pointer to builtin/custom keymaps */
	const rcti *bblocal, *bbwin;              /* optional local and windowspace bb */
	/* Run after the keymap item runs. */
	struct wmEventHandler_KeymapFn keymap_callback;

	struct bToolRef *keymap_tool;

	/* modal operator handler */
	wmOperator *op;                     /* for derived/modal handlers */
	struct ScrArea *op_area;            /* for derived/modal handlers */
	struct ARegion *op_region;          /* for derived/modal handlers */
	short           op_region_type;     /* for derived/modal handlers */

	/* ui handler */
	wmUIHandlerFunc ui_handle;          /* callback receiving events */
	wmUIHandlerRemoveFunc ui_remove;    /* callback when handler is removed */
	void *ui_userdata;                  /* user data pointer */
	struct ScrArea *ui_area;            /* for derived/modal handlers */
	struct ARegion *ui_region;          /* for derived/modal handlers */
	struct ARegion *ui_menu;            /* for derived/modal handlers */

	/* drop box handler */
	ListBase *dropboxes;
	/* gizmo handler */
	struct wmGizmoMap *gizmo_map;
} wmEventHandler;

/* custom types for handlers, for signaling, freeing */
enum {
	WM_HANDLER_DEFAULT,
	WM_HANDLER_FILESELECT
};

/* wm_event_system.c */
void        wm_event_free_all       (wmWindow *win);
void        wm_event_free           (wmEvent *event);
void        wm_event_free_handler   (wmEventHandler *handler);

            /* goes over entire hierarchy:  events -> window -> screen -> area -> region */
void        wm_event_do_handlers    (bContext *C);

void        wm_event_add_ghostevent (wmWindowManager *wm, wmWindow *win, int type, int time, void *customdata);

void        wm_event_do_depsgraph(bContext *C);
void        wm_event_do_refresh_wm_and_depsgraph(bContext *C);
void        wm_event_do_notifiers(bContext *C);

/* wm_keymap.c */

/* wm_dropbox.c */
void        wm_dropbox_free(void);
void        wm_drags_check_ops(bContext *C, const wmEvent *event);
void        wm_drags_draw(bContext *C, wmWindow *win, rcti *rect);

#endif /* __WM_EVENT_SYSTEM_H__ */
