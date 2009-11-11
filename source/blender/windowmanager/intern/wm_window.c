/**
 * $Id: wm_window.c
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
 * The Original Code is Copyright (C) 2007 Blender Foundation but based 
 * on ghostwinlay.c (C) 2001-2002 by NaN Holding BV
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2008
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "DNA_listBase.h"	
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_draw.h"
#include "wm_window.h"
#include "wm_subwindow.h"
#include "wm_event_system.h"

#include "ED_screen.h"

#include "PIL_time.h"

#include "GPU_draw.h"

/* the global to talk to ghost */
GHOST_SystemHandle g_system= NULL;

/* set by commandline */
static int prefsizx= 0, prefsizy= 0, prefstax= 0, prefstay= 0;

/* ******** win open & close ************ */

/* XXX this one should correctly check for apple top header...
 done for Cocoa : returns window contents (and not frame) max size*/
static void wm_get_screensize(int *width_r, int *height_r) 
{
	unsigned int uiwidth;
	unsigned int uiheight;
	
	GHOST_GetMainDisplayDimensions(g_system, &uiwidth, &uiheight);
	*width_r= uiwidth;
	*height_r= uiheight;
}

/* keeps offset and size within monitor bounds */
/* XXX solve dual screen... */
static void wm_window_check_position(rcti *rect)
{
	int width, height, d;
	
	wm_get_screensize(&width, &height);
	
#if defined(__APPLE__) && !defined(GHOST_COCOA)
	height -= 70;
#endif
	
	if(rect->xmin < 0) {
		d= rect->xmin;
		rect->xmax -= d;
		rect->xmin -= d;
	}
	if(rect->ymin < 0) {
		d= rect->ymin;
		rect->ymax -= d;
		rect->ymin -= d;
	}
	if(rect->xmax > width) {
		d= rect->xmax - width;
		rect->xmax -= d;
		rect->xmin -= d;
	}
	if(rect->ymax > height) {
		d= rect->ymax - height;
		rect->ymax -= d;
		rect->ymin -= d;
	}
	
	if(rect->xmin < 0) rect->xmin= 0;
	if(rect->ymin < 0) rect->ymin= 0;
}


static void wm_ghostwindow_destroy(wmWindow *win) 
{
	if(win->ghostwin) {
		GHOST_DisposeWindow(g_system, win->ghostwin);
		win->ghostwin= NULL;
	}
}

/* including window itself, C can be NULL. 
   ED_screen_exit should have been called */
void wm_window_free(bContext *C, wmWindowManager *wm, wmWindow *win)
{
	wmTimer *wt, *wtnext;
	
	/* update context */
	if(C) {
		WM_event_remove_handlers(C, &win->handlers);
		WM_event_remove_handlers(C, &win->modalhandlers);

		if(CTX_wm_window(C)==win)
			CTX_wm_window_set(C, NULL);
	}	

	if(wm->windrawable==win)
		wm->windrawable= NULL;
	if(wm->winactive==win)
		wm->winactive= NULL;

	/* end running jobs, a job end also removes its timer */
	for(wt= wm->timers.first; wt; wt= wtnext) {
		wtnext= wt->next;
		if(wt->win==win && wt->event_type==TIMERJOBS)
			wm_jobs_timer_ended(wm, wt);
	}
	
	/* timer removing, need to call this api function */
	for(wt= wm->timers.first; wt; wt=wtnext) {
		wtnext= wt->next;
		if(wt->win==win)
			WM_event_remove_timer(wm, win, wt);
	}

	if(win->eventstate) MEM_freeN(win->eventstate);
	
	wm_event_free_all(win);
	wm_subwindows_free(win);
	
	if(win->drawdata)
		MEM_freeN(win->drawdata);
	
	wm_ghostwindow_destroy(win);
	
	MEM_freeN(win);
}

static int find_free_winid(wmWindowManager *wm)
{
	wmWindow *win;
	int id= 1;
	
	for(win= wm->windows.first; win; win= win->next)
		if(id <= win->winid)
			id= win->winid+1;
	
	return id;
}

/* dont change context itself */
wmWindow *wm_window_new(bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmWindow *win= MEM_callocN(sizeof(wmWindow), "window");
	
	BLI_addtail(&wm->windows, win);
	win->winid= find_free_winid(wm);

	return win;
}


/* part of wm_window.c api */
wmWindow *wm_window_copy(bContext *C, wmWindow *winorig)
{
	wmWindow *win= wm_window_new(C);
	
	win->posx= winorig->posx+10;
	win->posy= winorig->posy;
	win->sizex= winorig->sizex;
	win->sizey= winorig->sizey;
	
	/* duplicate assigns to window */
	win->screen= ED_screen_duplicate(win, winorig->screen);
	BLI_strncpy(win->screenname, win->screen->id.name+2, 21);
	win->screen->winid= win->winid;

	win->screen->do_refresh= 1;
	win->screen->do_draw= 1;

	win->drawmethod= -1;
	win->drawdata= NULL;
	
	return win;
}

/* this is event from ghost, or exit-blender op */
void wm_window_close(bContext *C, wmWindowManager *wm, wmWindow *win)
{
	BLI_remlink(&wm->windows, win);
	
	wm_draw_window_clear(win);
	ED_screen_exit(C, win, win->screen);
	wm_window_free(C, wm, win);
	
	/* check remaining windows */
	if(wm->windows.first) {
		for(win= wm->windows.first; win; win= win->next)
			if(win->screen->full!=SCREENTEMP)
				break;
		/* in this case we close all */
		if(win==NULL)
			WM_exit(C);
	}
	else
		WM_exit(C);
}

void wm_window_title(wmWindowManager *wm, wmWindow *win)
{
	/* handle the 'temp' window */
	if(win->screen && win->screen->full==SCREENTEMP) {
		GHOST_SetTitle(win->ghostwin, "Blender");
	}
	else {
		
		/* this is set to 1 if you don't have .B.blend open */
		if(G.save_over) {
			char *str= MEM_mallocN(strlen(G.sce) + 16, "title");
			
			if(wm->file_saved)
				sprintf(str, "Blender [%s]", G.sce);
			else
				sprintf(str, "Blender* [%s]", G.sce);
			
			GHOST_SetTitle(win->ghostwin, str);
			
			MEM_freeN(str);
		}
		else
			GHOST_SetTitle(win->ghostwin, "Blender");

		/* Informs GHOST of unsaved changes, to set window modified visual indicator (MAC OS X)
		 and to give hint of unsaved changes for a user warning mechanism
		 in case of OS application terminate request (e.g. OS Shortcut Alt+F4, Cmd+Q, (...), or session end) */
		GHOST_SetWindowModifiedState(win->ghostwin, (GHOST_TUns8)!wm->file_saved);
		
#if defined(__APPLE__) && !defined(GHOST_COCOA)
		if(wm->file_saved)
			GHOST_SetWindowState(win->ghostwin, GHOST_kWindowStateUnModified);
		else
			GHOST_SetWindowState(win->ghostwin, GHOST_kWindowStateModified);
#endif
	}
}

/* belongs to below */
static void wm_window_add_ghostwindow(wmWindowManager *wm, char *title, wmWindow *win)
{
	GHOST_WindowHandle ghostwin;
	GHOST_TWindowState inital_state;
	int scr_w, scr_h, posy;
	
	wm_get_screensize(&scr_w, &scr_h);
	posy= (scr_h - win->posy - win->sizey);
	
	//		inital_state = GHOST_kWindowStateFullScreen;
	//		inital_state = GHOST_kWindowStateMaximized;
	inital_state = GHOST_kWindowStateNormal;
	
#if defined(__APPLE__) && !defined(GHOST_COCOA)
	{
		extern int macPrefState; /* creator.c */
		inital_state += macPrefState;
	}
#endif
	
	ghostwin= GHOST_CreateWindow(g_system, title, 
								 win->posx, posy, win->sizex, win->sizey, 
								 inital_state, 
								 GHOST_kDrawingContextTypeOpenGL,
								 0 /* no stereo */);
	
	if (ghostwin) {
		
		win->ghostwin= ghostwin;
		GHOST_SetWindowUserData(ghostwin, win);	/* pointer back */
		
		if(win->eventstate==NULL)
			win->eventstate= MEM_callocN(sizeof(wmEvent), "window event state");
		
		/* until screens get drawn, make it nice grey */
		glClearColor(.55, .55, .55, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		wm_window_swap_buffers(win);
		
		//GHOST_SetWindowState(ghostwin, GHOST_kWindowStateModified);
		
		/* standard state vars for window */
		glEnable(GL_SCISSOR_TEST);
		
		GPU_state_init();
	}
}

/* for wmWindows without ghostwin, open these and clear */
/* window size is read from window, if 0 it uses prefsize */
/* called in WM_check, also inits stuff after file read */
void wm_window_add_ghostwindows(wmWindowManager *wm)
{
	wmKeyMap *keymap;
	wmWindow *win;
	
	/* no commandline prefsize? then we set this */
	if (!prefsizx) {
		wm_get_screensize(&prefsizx, &prefsizy);
		
#if defined(__APPLE__) && !defined(GHOST_COCOA)
//Cocoa provides functions to get correct max window size
		{
			extern void wm_set_apple_prefsize(int, int);	/* wm_apple.c */
			
			wm_set_apple_prefsize(prefsizx, prefsizy);
		}
#else
		prefstax= 0;
		prefstay= 0;
		
#endif
	}
	
	for(win= wm->windows.first; win; win= win->next) {
		if(win->ghostwin==NULL) {
			if(win->sizex==0) {
				win->posx= prefstax;
				win->posy= prefstay;
				win->sizex= prefsizx;
				win->sizey= prefsizy;
				win->windowstate= 0;
			}
			wm_window_add_ghostwindow(wm, "Blender", win);
		}
		/* happens after fileread */
		if(win->eventstate==NULL)
		   win->eventstate= MEM_callocN(sizeof(wmEvent), "window event state");

		/* add keymap handlers (1 handler for all keys in map!) */
		keymap= WM_keymap_find(wm->defaultconf, "Window", 0, 0);
		WM_event_add_keymap_handler(&win->handlers, keymap);
		
		keymap= WM_keymap_find(wm->defaultconf, "Screen", 0, 0);
		WM_event_add_keymap_handler(&win->handlers, keymap);

		keymap= WM_keymap_find(wm->defaultconf, "Screen Editing", 0, 0);
		WM_event_add_keymap_handler(&win->modalhandlers, keymap);
		
		wm_window_title(wm, win);
	}
}

/* new window, no screen yet, but we open ghostwindow for it */
/* also gets the window level handlers */
/* area-rip calls this */
wmWindow *WM_window_open(bContext *C, rcti *rect)
{
	wmWindow *win= wm_window_new(C);
	
	win->posx= rect->xmin;
	win->posy= rect->ymin;
	win->sizex= rect->xmax - rect->xmin;
	win->sizey= rect->ymax - rect->ymin;

	win->drawmethod= -1;
	win->drawdata= NULL;
	
	WM_check(C);
	
	return win;
}

/* uses screen->full tag to define what to do, currently it limits
   to only one "temp" window for render out, preferences, filewindow, etc */
/* type is #define in WM_api.h */

void WM_window_open_temp(bContext *C, rcti *position, int type)
{
	wmWindow *win;
	ScrArea *sa;
	
	/* changes rect to fit within desktop */
	wm_window_check_position(position);
	
	/* test if we have a temp screen already */
	for(win= CTX_wm_manager(C)->windows.first; win; win= win->next)
		if(win->screen->full == SCREENTEMP)
			break;
	
	/* add new window? */
	if(win==NULL) {
		win= wm_window_new(C);
		
		win->posx= position->xmin;
		win->posy= position->ymin;
	}
	
	win->sizex= position->xmax - position->xmin;
	win->sizey= position->ymax - position->ymin;
	
	if(win->ghostwin) {
		wm_window_set_size(win, win->sizex, win->sizey) ;
		wm_window_raise(win);
	}
	
	/* add new screen? */
	if(win->screen==NULL)
		win->screen= ED_screen_add(win, CTX_data_scene(C), "temp");
	win->screen->full = SCREENTEMP; 
	
	/* make window active, and validate/resize */
	CTX_wm_window_set(C, win);
	WM_check(C);
	
	/* ensure it shows the right spacetype editor */
	sa= win->screen->areabase.first;
	CTX_wm_area_set(C, sa);
	
	if(type==WM_WINDOW_RENDER) {
		ED_area_newspace(C, sa, SPACE_IMAGE);
	}
	else {
		ED_area_newspace(C, sa, SPACE_USERPREF);
	}
	
	ED_screen_set(C, win->screen);
	
	if(sa->spacetype==SPACE_IMAGE)
		GHOST_SetTitle(win->ghostwin, "Blender Render");
	else if(ELEM(sa->spacetype, SPACE_OUTLINER, SPACE_USERPREF))
		GHOST_SetTitle(win->ghostwin, "Blender User Preferences");
	else if(sa->spacetype==SPACE_FILE)
		GHOST_SetTitle(win->ghostwin, "Blender File View");
	else
		GHOST_SetTitle(win->ghostwin, "Blender");
}


/* ****************** Operators ****************** */

/* operator callback */
int wm_window_duplicate_op(bContext *C, wmOperator *op)
{
	wm_window_copy(C, CTX_wm_window(C));
	WM_check(C);
	
	return OPERATOR_FINISHED;
}


/* fullscreen operator callback */
int wm_window_fullscreen_toggle_op(bContext *C, wmOperator *op)
{
	wmWindow *window= CTX_wm_window(C);
	GHOST_TWindowState state = GHOST_GetWindowState(window->ghostwin);
	if(state!=GHOST_kWindowStateFullScreen)
		GHOST_SetWindowState(window->ghostwin, GHOST_kWindowStateFullScreen);
	else
		GHOST_SetWindowState(window->ghostwin, GHOST_kWindowStateNormal);

	return OPERATOR_FINISHED;
	
}


/* ************ events *************** */

static int query_qual(char qual) 
{
	GHOST_TModifierKeyMask left, right;
	int val= 0;
	
	if (qual=='s') {
		left= GHOST_kModifierKeyLeftShift;
		right= GHOST_kModifierKeyRightShift;
	} else if (qual=='c') {
		left= GHOST_kModifierKeyLeftControl;
		right= GHOST_kModifierKeyRightControl;
	} else if (qual=='C') {
		left= right= GHOST_kModifierKeyCommand;
	} else {
		left= GHOST_kModifierKeyLeftAlt;
		right= GHOST_kModifierKeyRightAlt;
	}
	
	GHOST_GetModifierKeyState(g_system, left, &val);
	if (!val)
		GHOST_GetModifierKeyState(g_system, right, &val);
	
	return val;
}

void wm_window_make_drawable(bContext *C, wmWindow *win) 
{
	wmWindowManager *wm= CTX_wm_manager(C);

	if (win != wm->windrawable && win->ghostwin) {
//		win->lmbut= 0;	/* keeps hanging when mousepressed while other window opened */
		
		wm->windrawable= win;
		if(G.f & G_DEBUG) printf("set drawable %d\n", win->winid);
		GHOST_ActivateWindowDrawingContext(win->ghostwin);
	}
}

/* called by ghost, here we handle events for windows themselves or send to event system */
static int ghost_event_proc(GHOST_EventHandle evt, GHOST_TUserDataPtr private) 
{
	bContext *C= private;
	wmWindowManager *wm= CTX_wm_manager(C);
	GHOST_TEventType type= GHOST_GetEventType(evt);
	
	if (type == GHOST_kEventQuit) {
		WM_exit(C);
	} else {
		GHOST_WindowHandle ghostwin= GHOST_GetEventWindow(evt);
		GHOST_TEventDataPtr data= GHOST_GetEventData(evt);
		wmWindow *win;
		
		if (!ghostwin) {
			// XXX - should be checked, why are we getting an event here, and
			//	what is it?
			
			return 1;
		} else if (!GHOST_ValidWindow(g_system, ghostwin)) {
			// XXX - should be checked, why are we getting an event here, and
			//	what is it?
			
			return 1;
		} else {
			win= GHOST_GetWindowUserData(ghostwin);
		}
		
		switch(type) {
			case GHOST_kEventWindowDeactivate:
				win->active= 0; /* XXX */
				break;
			case GHOST_kEventWindowActivate: 
			{
				GHOST_TEventKeyData kdata;
				int cx, cy, wx, wy;
				
				wm->winactive= win; /* no context change! c->wm->windrawable is drawable, or for area queues */
				
				win->active= 1;
//				window_handle(win, INPUTCHANGE, win->active);
				
				/* bad ghost support for modifier keys... so on activate we set the modifiers again */
				kdata.ascii= 0;
				if (win->eventstate->shift && !query_qual('s')) {
					kdata.key= GHOST_kKeyLeftShift;
					wm_event_add_ghostevent(win, GHOST_kEventKeyUp, &kdata);
				}
				if (win->eventstate->ctrl && !query_qual('c')) {
					kdata.key= GHOST_kKeyLeftControl;
					wm_event_add_ghostevent(win, GHOST_kEventKeyUp, &kdata);
				}
				if (win->eventstate->alt && !query_qual('a')) {
					kdata.key= GHOST_kKeyLeftAlt;
					wm_event_add_ghostevent(win, GHOST_kEventKeyUp, &kdata);
				}
				if (win->eventstate->oskey && !query_qual('C')) {
					kdata.key= GHOST_kKeyCommand;
					wm_event_add_ghostevent(win, GHOST_kEventKeyUp, &kdata);
				}
				/* keymodifier zero, it hangs on hotkeys that open windows otherwise */
				win->eventstate->keymodifier= 0;
				
				/* entering window, update mouse pos. but no event */
				GHOST_GetCursorPosition(g_system, &wx, &wy);
				
				GHOST_ScreenToClient(win->ghostwin, wx, wy, &cx, &cy);
				win->eventstate->x= cx;

#if defined(__APPLE__) && defined(GHOST_COCOA)
				//Cocoa already uses coordinates with y=0 at bottom
				win->eventstate->y= cy;
#else
				win->eventstate->y= (win->sizey-1) - cy;
#endif
				
				wm_window_make_drawable(C, win);
				break;
			}
			case GHOST_kEventWindowClose: {
				wm_window_close(C, wm, win);
				break;
			}
			case GHOST_kEventWindowUpdate: {
				if(G.f & G_DEBUG) printf("ghost redraw\n");
				
				wm_window_make_drawable(C, win);
				WM_event_add_notifier(C, NC_WINDOW, NULL);

				break;
			}
			case GHOST_kEventWindowSize:
			case GHOST_kEventWindowMove: {
				GHOST_TWindowState state;
				state = GHOST_GetWindowState(win->ghostwin);

				 /* win32: gives undefined window size when minimized */
				if(state!=GHOST_kWindowStateMinimized) {
					GHOST_RectangleHandle client_rect;
					int l, t, r, b, scr_w, scr_h;
					int sizex, sizey, posx, posy;
					
					client_rect= GHOST_GetClientBounds(win->ghostwin);
					GHOST_GetRectangle(client_rect, &l, &t, &r, &b);
					
					GHOST_DisposeRectangle(client_rect);
					
					wm_get_screensize(&scr_w, &scr_h);
					sizex= r-l;
					sizey= b-t;
					posx= l;
					posy= scr_h - t - win->sizey;

					/*
					 * Ghost sometimes send size or move events when the window hasn't changed.
					 * One case of this is using compiz on linux. To alleviate the problem
					 * we ignore all such event here.
					 * 
					 * It might be good to eventually do that at Ghost level, but that is for 
					 * another time.
					 */
					if (win->sizex != sizex ||
							win->sizey != sizey ||
							win->posx != posx ||
							win->posy != posy)
					{
						win->sizex= sizex;
						win->sizey= sizey;
						win->posx= posx;
						win->posy= posy;
	
						/* debug prints */
						if(0) {
							state = GHOST_GetWindowState(win->ghostwin);
	
							if(state==GHOST_kWindowStateNormal) {
								if(G.f & G_DEBUG) printf("window state: normal\n");
							}
							else if(state==GHOST_kWindowStateMinimized) {
								if(G.f & G_DEBUG) printf("window state: minimized\n");
							}
							else if(state==GHOST_kWindowStateMaximized) {
								if(G.f & G_DEBUG) printf("window state: maximized\n");
							}
							else if(state==GHOST_kWindowStateFullScreen) {
								if(G.f & G_DEBUG) printf("window state: fullscreen\n");
							}
							
							if(type!=GHOST_kEventWindowSize) {
								if(G.f & G_DEBUG) printf("win move event pos %d %d size %d %d\n", win->posx, win->posy, win->sizex, win->sizey);
							}
							
						}
					
						wm_window_make_drawable(C, win);
						wm_draw_window_clear(win);
						WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
					}
				}
				break;
			}
			default:
				wm_event_add_ghostevent(win, type, data);
				break;
		}

	}
	return 1;
}


/* This timer system only gives maximum 1 timer event per redraw cycle,
   to prevent queues to get overloaded. 
   Timer handlers should check for delta to decide if they just
   update, or follow real time.
   Timer handlers can also set duration to match frames passed
*/
static int wm_window_timer(const bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmTimer *wt, *wtnext;
	wmWindow *win;
	double time= PIL_check_seconds_timer();
	int retval= 0;
	
	for(wt= wm->timers.first; wt; wt= wtnext) {
		wtnext= wt->next; /* in case timer gets removed */
		win= wt->win;

		if(wt->sleep==0) {
			if(time > wt->ntime) {
				wt->delta= time - wt->ltime;
				wt->duration += wt->delta;
				wt->ltime= time;
				wt->ntime= wt->stime + wt->timestep*ceil(wt->duration/wt->timestep);

				if(wt->event_type == TIMERJOBS)
					wm_jobs_timer(C, wm, wt);
				else if(wt->event_type == TIMERAUTOSAVE)
					wm_autosave_timer(C, wm, wt);
				else if(win) {
					wmEvent event= *(win->eventstate);
					
					event.type= wt->event_type;
					event.custom= EVT_DATA_TIMER;
					event.customdata= wt;
					wm_event_add(win, &event);

					retval= 1;
				}
			}
		}
	}
	return retval;
}

void wm_window_process_events(const bContext *C) 
{
	int hasevent= GHOST_ProcessEvents(g_system, 0);	/* 0 is no wait */
	
	if(hasevent)
		GHOST_DispatchEvents(g_system);
	
	hasevent |= wm_window_timer(C);

	/* no event, we sleep 5 milliseconds */
	if(hasevent==0)
		PIL_sleep_ms(5);
}

void wm_window_process_events_nosleep(const bContext *C) 
{
	if(GHOST_ProcessEvents(g_system, 0))
		GHOST_DispatchEvents(g_system);
}

/* exported as handle callback to bke blender.c */
void wm_window_testbreak(void)
{
	static double ltime= 0;
	double curtime= PIL_check_seconds_timer();
	
	/* only check for breaks every 50 milliseconds
		* if we get called more often.
		*/
	if ((curtime-ltime)>.05) {
		int hasevent= GHOST_ProcessEvents(g_system, 0);	/* 0 is no wait */
		
		if(hasevent)
			GHOST_DispatchEvents(g_system);
		
		ltime= curtime;
	}
}

/* **************** init ********************** */

void wm_ghost_init(bContext *C)
{
	if (!g_system) {
		GHOST_EventConsumerHandle consumer= GHOST_CreateEventConsumer(ghost_event_proc, C);
		
		g_system= GHOST_CreateSystem();
		GHOST_AddEventConsumer(g_system, consumer);
	}	
}

void wm_ghost_exit(void)
{
	if(g_system)
		GHOST_DisposeSystem(g_system);

	g_system= NULL;
}

/* **************** timer ********************** */

/* to (de)activate running timers temporary */
void WM_event_timer_sleep(wmWindowManager *wm, wmWindow *win, wmTimer *timer, int dosleep)
{
	wmTimer *wt;
	
	for(wt= wm->timers.first; wt; wt= wt->next)
		if(wt==timer)
			break;

	if(wt)
		wt->sleep= dosleep;
}

wmTimer *WM_event_add_timer(wmWindowManager *wm, wmWindow *win, int event_type, double timestep)
{
	wmTimer *wt= MEM_callocN(sizeof(wmTimer), "window timer");
	
	wt->event_type= event_type;
	wt->ltime= PIL_check_seconds_timer();
	wt->ntime= wt->ltime + timestep;
	wt->stime= wt->ltime;
	wt->timestep= timestep;
	wt->win= win;
	
	BLI_addtail(&wm->timers, wt);
	
	return wt;
}

void WM_event_remove_timer(wmWindowManager *wm, wmWindow *win, wmTimer *timer)
{
	wmTimer *wt;
	
	/* extra security check */
	for(wt= wm->timers.first; wt; wt= wt->next)
		if(wt==timer)
			break;
	if(wt) {
		
		BLI_remlink(&wm->timers, wt);
		if(wt->customdata)
			MEM_freeN(wt->customdata);
		MEM_freeN(wt);
	}
}

/* ******************* clipboard **************** */

char *WM_clipboard_text_get(int selection)
{
	char *p, *p2, *buf, *newbuf;

	buf= (char*)GHOST_getClipboard(selection);
	if(!buf)
		return NULL;
	
	/* always convert from \r\n to \n */
	newbuf= MEM_callocN(strlen(buf)+1, "WM_clipboard_text_get");

	for(p= buf, p2= newbuf; *p; p++) {
		if(*p != '\r')
			*(p2++)= *p;
	}
	*p2= '\0';

	free(buf); /* ghost uses regular malloc */
	
	return newbuf;
}

void WM_clipboard_text_set(char *buf, int selection)
{
#ifdef _WIN32
	/* do conversion from \n to \r\n on Windows */
	char *p, *p2, *newbuf;
	int newlen= 0;
	
	for(p= buf; *p; p++) {
		if(*p == '\n')
			newlen += 2;
		else
			newlen++;
	}
	
	newbuf= MEM_callocN(newlen+1, "WM_clipboard_text_set");

	for(p= buf, p2= newbuf; *p; p++, p2++) {
		if(*p == '\n') { 
			*(p2++)= '\r'; *p2= '\n';
		}
		else *p2= *p;
	}
	*p2= '\0';

	GHOST_putClipboard((GHOST_TInt8*)newbuf, selection);
	MEM_freeN(newbuf);
#else
	GHOST_putClipboard((GHOST_TInt8*)buf, selection);
#endif
}

/* ************************************ */

void wm_window_get_position(wmWindow *win, int *posx_r, int *posy_r) 
{
	*posx_r= win->posx;
	*posy_r= win->posy;
}

void wm_window_get_size(wmWindow *win, int *width_r, int *height_r) 
{
	*width_r= win->sizex;
	*height_r= win->sizey;
}

void wm_window_set_size(wmWindow *win, int width, int height) 
{
	GHOST_SetClientSize(win->ghostwin, width, height);
}

void wm_window_lower(wmWindow *win) 
{
	GHOST_SetWindowOrder(win->ghostwin, GHOST_kWindowOrderBottom);
}

void wm_window_raise(wmWindow *win) 
{
	GHOST_SetWindowOrder(win->ghostwin, GHOST_kWindowOrderTop);
}

void wm_window_swap_buffers(wmWindow *win)
{
	
#ifdef WIN32
	glDisable(GL_SCISSOR_TEST);
	GHOST_SwapWindowBuffers(win->ghostwin);
	glEnable(GL_SCISSOR_TEST);
#else
	GHOST_SwapWindowBuffers(win->ghostwin);
#endif
}

/* ******************* exported api ***************** */


/* called whem no ghost system was initialized */
void WM_setprefsize(int stax, int stay, int sizx, int sizy)
{
	prefstax= stax;
	prefstay= stay;
	prefsizx= sizx;
	prefsizy= sizy;
}

/* This function requires access to the GHOST_SystemHandle (g_system) */
void WM_cursor_warp(wmWindow *win, int x, int y)
{
	if (win && win->ghostwin) {
		int oldx=x, oldy=y;

		y= win->sizey -y - 1;
		GHOST_ClientToScreen(win->ghostwin, x, y, &x, &y);
		GHOST_SetCursorPosition(g_system, x, y);

		win->eventstate->prevx= oldx;
		win->eventstate->prevy= oldy;
	}
}

