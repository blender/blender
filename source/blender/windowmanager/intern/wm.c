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

#include "string.h"

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_report.h"
#include "BKE_global.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm_draw.h"
#include "wm.h"

#include "ED_screen.h"

#include "RNA_types.h"

/* ****************************************************** */

#define MAX_OP_REGISTERED	32

void WM_operator_free(wmOperator *op)
{
	if(op->ptr) {
		op->properties= op->ptr->data;
		MEM_freeN(op->ptr);
	}

	if(op->properties) {
		IDP_FreeProperty(op->properties);
		MEM_freeN(op->properties);
	}

	if(op->reports && (op->reports->flag & RPT_FREE)) {
		BKE_reports_clear(op->reports);
		MEM_freeN(op->reports);
	}

	if(op->macro.first) {
		wmOperator *opm, *opmnext;
		for(opm= op->macro.first; opm; opm= opmnext) {
			opmnext = opm->next;
			WM_operator_free(opm);
		}
	}
	
	MEM_freeN(op);
}

/* all operations get registered in the windowmanager here */
/* called on event handling by event_system.c */
void wm_operator_register(bContext *C, wmOperator *op)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	int tot;
	char *buf;

	BLI_addtail(&wm->operators, op);
	tot= BLI_countlist(&wm->operators);
	
	while(tot>MAX_OP_REGISTERED) {
		wmOperator *opt= wm->operators.first;
		BLI_remlink(&wm->operators, opt);
		WM_operator_free(opt);
		tot--;
	}
	
	
	/* Report the string representation of the operator */
	buf = WM_operator_pystring(C, op->type, op->ptr, 1);
	BKE_report(CTX_wm_reports(C), RPT_OPERATOR, buf);
	MEM_freeN(buf);
	
	/* so the console is redrawn */
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_CONSOLE_REPORT, NULL);
}


void WM_operator_stack_clear(bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmOperator *op;
	
	while((op= wm->operators.first)) {
		BLI_remlink(&wm->operators, op);
		WM_operator_free(op);
	}
	
}

/* ****************************************** */

static ListBase menutypes = {NULL, NULL}; /* global menutype list */

MenuType *WM_menutype_find(const char *idname, int quiet)
{
	MenuType* mt;

	if (idname[0]) {
		for(mt=menutypes.first; mt; mt=mt->next)
			if(strcmp(idname, mt->idname)==0)
				return mt;
	}

	if(!quiet)
		printf("search for unknown menutype %s\n", idname);

	return NULL;
}

int WM_menutype_add(MenuType* mt)
{
	BLI_addtail(&menutypes, mt);
	return 1;
}

void WM_menutype_freelink(MenuType* mt)
{
	BLI_freelinkN(&menutypes, mt);
}

void WM_menutype_free(void)
{
	MenuType* mt= menutypes.first, *mt_next;

	while(mt) {
		mt_next= mt->next;

		if(mt->ext.free)
			mt->ext.free(mt->ext.data);

		WM_menutype_freelink(mt);

		mt= mt_next;
	}
}

/* ****************************************** */

void WM_keymap_init(bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);

	if(!wm->defaultconf)
		wm->defaultconf= WM_keyconfig_add(wm, "Blender");
	
	if(wm && CTX_py_init_get(C) && (wm->initialized & WM_INIT_KEYMAP) == 0) {
		/* create default key config */
		wm_window_keymap(wm->defaultconf);
		ED_spacetypes_keymap(wm->defaultconf);
		WM_keyconfig_userdef(wm);

		wm->initialized |= WM_INIT_KEYMAP;
	}
}

void WM_check(bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	
	/* wm context */
	if(wm==NULL) {
		wm= CTX_data_main(C)->wm.first;
		CTX_wm_manager_set(C, wm);
	}
	if(wm==NULL) return;
	if(wm->windows.first==NULL) return;

	if (!G.background) {
		/* case: fileread */
		if((wm->initialized & WM_INIT_WINDOW) == 0) {
			WM_keymap_init(C);
			WM_autosave_init(wm);
		}

		/* case: no open windows at all, for old file reads */
		wm_window_add_ghostwindows(wm);

		/* case: fileread */
		if((wm->initialized & WM_INIT_WINDOW) == 0) {
			ED_screens_initialize(wm);
			wm->initialized |= WM_INIT_WINDOW;
		}
	}
}

void wm_clear_default_size(bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmWindow *win;
	
	/* wm context */
	if(wm==NULL) {
		wm= CTX_data_main(C)->wm.first;
		CTX_wm_manager_set(C, wm);
	}
	if(wm==NULL) return;
	if(wm->windows.first==NULL) return;
	
	for(win= wm->windows.first; win; win= win->next) {
		win->sizex = 0;
		win->sizey = 0;
		win->posx = 0;
		win->posy = 0;
	}

}

/* on startup, it adds all data, for matching */
void wm_add_default(bContext *C)
{
	wmWindowManager *wm= alloc_libblock(&CTX_data_main(C)->wm, ID_WM, "WinMan");
	wmWindow *win;
	bScreen *screen= CTX_wm_screen(C); /* XXX from file read hrmf */
	
	CTX_wm_manager_set(C, wm);
	win= wm_window_new(C);
	win->screen= screen;
	screen->winid= win->winid;
	BLI_strncpy(win->screenname, screen->id.name+2, 21);
	
	wm->winactive= win;
	wm->file_saved= 1;
	wm_window_make_drawable(C, win); 
}


/* context is allowed to be NULL, do not free wm itself (library.c) */
void wm_close_and_free(bContext *C, wmWindowManager *wm)
{
	wmWindow *win;
	wmOperator *op;
	wmKeyConfig *keyconf;

	if(wm->autosavetimer)
		wm_autosave_timer_ended(wm);

	while((win= wm->windows.first)) {
		BLI_remlink(&wm->windows, win);
		win->screen= NULL; /* prevent draw clear to use screen */
		wm_draw_window_clear(win);
		wm_window_free(C, wm, win);
	}
	
	while((op= wm->operators.first)) {
		BLI_remlink(&wm->operators, op);
		WM_operator_free(op);
	}

	while((keyconf=wm->keyconfigs.first)) {
		BLI_remlink(&wm->keyconfigs, keyconf);
		WM_keyconfig_free(keyconf);
	}

	BLI_freelistN(&wm->queue);
	
	BLI_freelistN(&wm->paintcursors);
	BLI_freelistN(&wm->drags);
	BKE_reports_clear(&wm->reports);
	
	if(C && CTX_wm_manager(C)==wm) CTX_wm_manager_set(C, NULL);
}

void wm_close_and_free_all(bContext *C, ListBase *wmlist)
{
	wmWindowManager *wm;
	
	while((wm=wmlist->first)) {
		wm_close_and_free(C, wm);
		BLI_remlink(wmlist, wm);
		MEM_freeN(wm);
	}
}

void WM_main(bContext *C)
{
	while(1) {
		
		/* get events from ghost, handle window events, add to window queues */
		wm_window_process_events(C); 
		
		/* per window, all events to the window, screen, area and region handlers */
		wm_event_do_handlers(C);
		
		/* events have left notes about changes, we handle and cache it */
		wm_event_do_notifiers(C);
		
		/* execute cached changes draw */
		wm_draw_update(C);
	}
}


