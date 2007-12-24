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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef WM_EVENT_SYSTEM_H
#define WM_EVENT_SYSTEM_H

/* return value of handler-operator call */
#define WM_HANDLER_CONTINUE	0
#define WM_HANDLER_BREAK	1


/* each event should have full modifier state */
/* event comes from eventmanager and from keymap */
typedef struct wmEvent {
	struct wmEvent *next, *prev;
	
	short type;		/* event code itself (short, is also in keymap) */
	short val;		/* press, release, scrollvalue */
	short x, y;		/* mouse pointer position */
	short unicode;	/* future, ghost? */
	char ascii;		/* from ghost */
	char pad1;		
	
	/* modifier states */
	short shift, ctrl, alt, oskey;	/* oskey is apple or windowskey, value denotes order of pressed */
	short keymodifier;				/* rawkey modifier */
	
	/* keymap item, set by handler (weak?) */
	const char *keymap_idname;
	
	/* custom data */
	short custom;	/* custom data type, stylus, 6dof, see wm_event_types.h */
	void *customdata;	/* ascii, unicode, mouse coords, angles, vectors, dragdrop info */
	
} wmEvent;

/* wmKeyMap is in DNA_windowmanager.h, it's savable */

typedef struct wmEventHandler {
	struct wmEventHandler *next, *prev;
	
	int type, flag;	/* type default=0, rest is custom */
	
	ListBase *keymap;	/* pointer to builtin/custom keymaps */
	
	rctf boundbox;	/* float, in bContext space (window, area, region) */
	
	wmOperator *op;	/* for derived handlers */
	
} wmEventHandler;

/* handler flag */
		/* after this handler all others are ignored */
#define WM_HANDLER_BLOCKING		1

/* custom types for handlers, for signalling, freeing */
enum {
	WM_HANDLER_DEFAULT,
	WM_HANDLER_TRANSFORM
};


void	wm_event_free_all	(wmWindow *win);
wmEvent *wm_event_next		(wmWindow *win);
void wm_event_free_handlers (ListBase *lb);

/* goes over entire hierarchy:  events -> window -> screen -> area -> region */
void wm_event_do_handlers(bContext *C);

void wm_event_add_ghostevent(wmWindow *win, int type, void *customdata);

#endif /* WM_EVENT_SYSTEM_H */

