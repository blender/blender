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
#ifndef DNA_WINDOWMANAGER_TYPES_H
#define DNA_WINDOWMANAGER_TYPES_H

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "DNA_ID.h"

/* defined here: */
struct wmWindowManager;
struct wmWindow;

struct wmEvent;
struct wmOperatorType;
struct wmOperator;

/* forwards */
struct bContext;
struct wmLocal;
struct bScreen;
struct uiBlock;
struct wmSubWindow;

/* windowmanager is saved, tag WMAN */
typedef struct wmWindowManager {
	ID id;
	
	struct wmWindow *windrawable, *winactive;		/* separate active from drawable */
	ListBase windows;
	
	int initialized;		/* set on file read */
	int pad;
	
	ListBase operators;		/* operator registry */
	
	ListBase queue;			/* refresh/redraw wmNotifier structs */

	ListBase reports;		/* information and error reports */
	
	/* custom keymaps */
	ListBase windowkeymap;
	ListBase screenkeymap;
	ListBase timekeymap;
	
	
} wmWindowManager;


/* the savable part, rest of data is local in ghostwinlay */
typedef struct wmWindow {
	struct wmWindow *next, *prev;
	
	void *ghostwin;		/* dont want to include ghost.h stuff */
	void *timer;
	int timer_event;
	
	int winid;	/* winid also in screens, is for retrieving this window after read */
	
	struct bScreen *screen;	/* active screen */
	char screenname[32];	/* MAX_ID_NAME for matching window with active screen after file read */
	
	short posx, posy, sizex, sizey;	/* window coords */
	short windowstate;	/* borderless, full */
	short monitor;		/* multiscreen... no idea how to store yet */
	short active;		/* set to 1 if an active window, for quick rejects */
	short cursor;		/* current mouse cursor type */
	
	struct wmEvent *eventstate;	/* storage for event system */
	
	struct wmSubWindow *curswin;	/* internal for wm_subwindow.c only */
	
	ListBase queue;			/* all events (ghost level events were handled) */
	ListBase handlers;		/* window+screen handlers, overriding all queues */
	
	ListBase subwindows;	/* opengl stuff for sub windows, see notes in wm_subwindow.c */
	ListBase gesture;	/* gesture stuff */
} wmWindow;

#
#
typedef struct wmOperatorType {
	struct wmOperatorType *next, *prev;
	
	char *name;		/* text for ui, undo */
	char *idname;	/* unique identifier */
	
	/* this callback executes the operator without any interactive input,
	 * parameters may be provided through operator properties. cannot use
	 * any interface code or input device state.
	 * - see defines below for return values */
	int (*exec)(struct bContext *, struct wmOperator *);

	/* for modal temporary operators, initially invoke is called. then
	 * any further events are handled in modal. if the operation is
	 * cancelled due to some external reason, cancel is called
	 * - see defines below for return values */
	int (*invoke)(struct bContext *, struct wmOperator *, struct wmEvent *);
	int (*cancel)(struct bContext *, struct wmOperator *);
	int (*modal)(struct bContext *, struct wmOperator *, struct wmEvent *);

	/* verify if the operator can be executed in the current context, note
	 * that the operator might still fail to execute even if this return true */
	int (*poll)(struct bContext *);
	
	/* panel for redo and repeat */
	void *(*uiBlock)(struct wmOperator *);
	
	char *customname;	/* dna name */
	void *customdata;	/* defaults */
	
	short flag;

} wmOperatorType;

#define OP_MAX_TYPENAME	64

/* partial copy of the event, for matching by eventhandler */
typedef struct wmKeymapItem {
	struct wmKeymapItem *next, *prev;
	
	char idname[64];				/* used to retrieve operator type pointer */
	
	short type;						/* event code itself */
	short val;						/* 0=any, 1=click, 2=release, or wheelvalue, or... */
	short shift, ctrl, alt, oskey;	/* oskey is apple or windowskey, value denotes order of pressed */
	short keymodifier;				/* rawkey modifier */
	
	short pad;
} wmKeymapItem;


/* this one is the operator itself, stored in files for macros etc */
/* operator + operatortype should be able to redo entirely, but for different contextes */
typedef struct wmOperator {
	struct wmOperator *next, *prev;
	
	wmOperatorType *type;
	char idname[64];		/* used to retrieve type pointer */
	
	/* custom storage, dna pointer */
	void *customdata; 
	/* or IDproperty list */
	IDProperty *properties;

} wmOperator;

/* operator type exec(), invoke() modal(), cancel() return values */
#define OPERATOR_PASS_THROUGH	0
#define OPERATOR_RUNNING_MODAL	1
#define OPERATOR_CANCELLED		2
#define OPERATOR_FINISHED		3

#endif /* DNA_WINDOWMANAGER_TYPES_H */

