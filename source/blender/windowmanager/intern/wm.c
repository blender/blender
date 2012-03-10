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

/** \file blender/windowmanager/intern/wm.c
 *  \ingroup wm
 */


#include <string.h>
#include <stddef.h>

#include "BLO_sys_types.h"

#include "DNA_windowmanager_types.h"

#include "GHOST_C-api.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"

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

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

/* ****************************************************** */

#define MAX_OP_REGISTERED	32

void WM_operator_free(wmOperator *op)
{

#ifdef WITH_PYTHON
	if(op->py_instance) {
		/* do this first in case there are any __del__ functions or
		 * similar that use properties */
		BPY_DECREF(op->py_instance);
	}
#endif

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

static void wm_reports_free(wmWindowManager *wm)
{
	BKE_reports_clear(&wm->reports);
	WM_event_remove_timer(wm, NULL, wm->reports.reporttimer);
}

/* all operations get registered in the windowmanager here */
/* called on event handling by event_system.c */
void wm_operator_register(bContext *C, wmOperator *op)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	int tot;

	BLI_addtail(&wm->operators, op);
	tot= BLI_countlist(&wm->operators);
	
	while(tot>MAX_OP_REGISTERED) {
		wmOperator *opt= wm->operators.first;
		BLI_remlink(&wm->operators, opt);
		WM_operator_free(opt);
		tot--;
	}
	
	/* so the console is redrawn */
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_INFO_REPORT, NULL);
	WM_event_add_notifier(C, NC_WM|ND_HISTORY, NULL);
}


void WM_operator_stack_clear(wmWindowManager *wm)
{
	wmOperator *op;
	
	while((op= wm->operators.first)) {
		BLI_remlink(&wm->operators, op);
		WM_operator_free(op);
	}
	
	WM_main_add_notifier(NC_WM|ND_HISTORY, NULL);
}

/* ****************************************** */

static GHash *menutypes_hash= NULL;

MenuType *WM_menutype_find(const char *idname, int quiet)
{
	MenuType* mt;

	if (idname[0]) {
		mt= BLI_ghash_lookup(menutypes_hash, idname);
		if(mt)
			return mt;
	}

	if(!quiet)
		printf("search for unknown menutype %s\n", idname);

	return NULL;
}

int WM_menutype_add(MenuType* mt)
{
	BLI_ghash_insert(menutypes_hash, (void *)mt->idname, mt);
	return 1;
}

void WM_menutype_freelink(MenuType* mt)
{
	BLI_ghash_remove(menutypes_hash, mt->idname, NULL, (GHashValFreeFP)MEM_freeN);
}

/* called on initialize WM_init() */
void WM_menutype_init(void)
{
	menutypes_hash= BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp, "menutypes_hash gh");
}

void WM_menutype_free(void)
{
	GHashIterator *iter= BLI_ghashIterator_new(menutypes_hash);

	for( ; !BLI_ghashIterator_isDone(iter); BLI_ghashIterator_step(iter)) {
		MenuType *mt= BLI_ghashIterator_getValue(iter);
		if(mt->ext.free) {
			mt->ext.free(mt->ext.data);
		}
	}
	BLI_ghashIterator_free(iter);

	BLI_ghash_free(menutypes_hash, NULL, (GHashValFreeFP)MEM_freeN);
	menutypes_hash= NULL;
}

/* ****************************************** */

void WM_keymap_init(bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);

	/* create standard key configs */
	if(!wm->defaultconf)
		wm->defaultconf= WM_keyconfig_new(wm, "Blender");
	if(!wm->addonconf)
		wm->addonconf= WM_keyconfig_new(wm, "Blender Addon");
	if(!wm->userconf)
		wm->userconf= WM_keyconfig_new(wm, "Blender User");
	
	/* initialize only after python init is done, for keymaps that
	 * use python operators */
	if(CTX_py_init_get(C) && (wm->initialized & WM_INIT_KEYMAP) == 0) {
		/* create default key config, only initialize once,
		 * it's persistent across sessions */
		if(!(wm->defaultconf->flag & KEYCONF_INIT_DEFAULT)) {
			wm_window_keymap(wm->defaultconf);
			ED_spacetypes_keymap(wm->defaultconf);

			wm->defaultconf->flag |= KEYCONF_INIT_DEFAULT;
		}

		WM_keyconfig_update_tag(NULL, NULL);
		WM_keyconfig_update(wm);

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
	}

	/* case: fileread */
	/* note: this runs in bg mode to set the screen context cb */
	if((wm->initialized & WM_INIT_WINDOW) == 0) {
		ED_screens_initialize(wm);
		wm->initialized |= WM_INIT_WINDOW;
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
	BLI_strncpy(win->screenname, screen->id.name+2, sizeof(win->screenname));
	
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
	
	wm_reports_free(wm);
	
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


