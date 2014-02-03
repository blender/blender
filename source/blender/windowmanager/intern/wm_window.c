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
 * The Original Code is Copyright (C) 2007 Blender Foundation but based 
 * on ghostwinlay.c (C) 2001-2002 by NaN Holding BV
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2008
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_window.c
 *  \ingroup wm
 *
 * Window management, wrap GHOST.
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

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "BIF_gl.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_draw.h"
#include "wm_window.h"
#include "wm_subwindow.h"
#include "wm_event_system.h"

#include "ED_screen.h"
#include "ED_fileselect.h"

#include "PIL_time.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"

#include "UI_interface.h"

/* for assert */
#ifndef NDEBUG
#  include "BLI_threads.h"
#endif

/* the global to talk to ghost */
static GHOST_SystemHandle g_system = NULL;

typedef enum WinOverrideFlag {
	WIN_OVERRIDE_GEOM     = (1 << 0),
	WIN_OVERRIDE_WINSTATE = (1 << 1)
} WinOverrideFlag;

/* set by commandline */
static struct WMInitStruct {
	/* window geometry */
	int size_x, size_y;
	int start_x, start_y;

	int windowstate;
	WinOverrideFlag override_flag;
	
	bool native_pixels;
} wm_init_state = {0, 0, 0, 0, GHOST_kWindowStateNormal, 0, true};

/* ******** win open & close ************ */

/* XXX this one should correctly check for apple top header...
 * done for Cocoa : returns window contents (and not frame) max size*/
void wm_get_screensize(int *width_r, int *height_r)
{
	unsigned int uiwidth;
	unsigned int uiheight;
	
	GHOST_GetMainDisplayDimensions(g_system, &uiwidth, &uiheight);
	*width_r = uiwidth;
	*height_r = uiheight;
}

/* size of all screens (desktop), useful since the mouse is bound by this */
void wm_get_desktopsize(int *width_r, int *height_r)
{
	unsigned int uiwidth;
	unsigned int uiheight;

	GHOST_GetAllDisplayDimensions(g_system, &uiwidth, &uiheight);
	*width_r = uiwidth;
	*height_r = uiheight;
}

/* keeps offset and size within monitor bounds */
/* XXX solve dual screen... */
static void wm_window_check_position(rcti *rect)
{
	int width, height, d;
	
	wm_get_screensize(&width, &height);
	
	if (rect->xmin < 0) {
		rect->xmax -= rect->xmin;
		rect->xmin  = 0;
	}
	if (rect->ymin < 0) {
		rect->ymax -= rect->ymin;
		rect->ymin  = 0;
	}
	if (rect->xmax > width) {
		d = rect->xmax - width;
		rect->xmax -= d;
		rect->xmin -= d;
	}
	if (rect->ymax > height) {
		d = rect->ymax - height;
		rect->ymax -= d;
		rect->ymin -= d;
	}
	
	if (rect->xmin < 0) rect->xmin = 0;
	if (rect->ymin < 0) rect->ymin = 0;
}


static void wm_ghostwindow_destroy(wmWindow *win) 
{
	if (win->ghostwin) {
		GHOST_DisposeWindow(g_system, win->ghostwin);
		win->ghostwin = NULL;
	}
}

/* including window itself, C can be NULL. 
 * ED_screen_exit should have been called */
void wm_window_free(bContext *C, wmWindowManager *wm, wmWindow *win)
{
	wmTimer *wt, *wtnext;
	
	/* update context */
	if (C) {
		WM_event_remove_handlers(C, &win->handlers);
		WM_event_remove_handlers(C, &win->modalhandlers);

		if (CTX_wm_window(C) == win)
			CTX_wm_window_set(C, NULL);
	}

	/* always set drawable and active to NULL,
	 * prevents non-drawable state of main windows (bugs #22967 and #25071, possibly #22477 too) */
	wm->windrawable = NULL;
	wm->winactive = NULL;

	/* end running jobs, a job end also removes its timer */
	for (wt = wm->timers.first; wt; wt = wtnext) {
		wtnext = wt->next;
		if (wt->win == win && wt->event_type == TIMERJOBS)
			wm_jobs_timer_ended(wm, wt);
	}
	
	/* timer removing, need to call this api function */
	for (wt = wm->timers.first; wt; wt = wtnext) {
		wtnext = wt->next;
		if (wt->win == win)
			WM_event_remove_timer(wm, win, wt);
	}

	if (win->eventstate) MEM_freeN(win->eventstate);
	
	wm_event_free_all(win);
	wm_subwindows_free(win);
	
	if (win->drawdata)
		MEM_freeN(win->drawdata);
	
	wm_ghostwindow_destroy(win);
	
	MEM_freeN(win);
}

static int find_free_winid(wmWindowManager *wm)
{
	wmWindow *win;
	int id = 1;
	
	for (win = wm->windows.first; win; win = win->next)
		if (id <= win->winid)
			id = win->winid + 1;
	
	return id;
}

/* don't change context itself */
wmWindow *wm_window_new(bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = MEM_callocN(sizeof(wmWindow), "window");
	
	BLI_addtail(&wm->windows, win);
	win->winid = find_free_winid(wm);

	return win;
}


/* part of wm_window.c api */
wmWindow *wm_window_copy(bContext *C, wmWindow *winorig)
{
	wmWindow *win = wm_window_new(C);
	
	win->posx = winorig->posx + 10;
	win->posy = winorig->posy;
	win->sizex = winorig->sizex;
	win->sizey = winorig->sizey;
	
	/* duplicate assigns to window */
	win->screen = ED_screen_duplicate(win, winorig->screen);
	BLI_strncpy(win->screenname, win->screen->id.name + 2, sizeof(win->screenname));
	win->screen->winid = win->winid;

	win->screen->do_refresh = TRUE;
	win->screen->do_draw = TRUE;

	win->drawmethod = U.wmdrawmethod;
	win->drawdata = NULL;
	
	return win;
}

/* this is event from ghost, or exit-blender op */
void wm_window_close(bContext *C, wmWindowManager *wm, wmWindow *win)
{
	wmWindow *tmpwin;
	bool do_exit = false;
	
	/* first check if we have to quit (there are non-temp remaining windows) */
	for (tmpwin = wm->windows.first; tmpwin; tmpwin = tmpwin->next) {
		if (tmpwin == win)
			continue;
		if (tmpwin->screen->temp == 0)
			break;
	}

	if (tmpwin == NULL)
		do_exit = 1;
	
	if ((U.uiflag & USER_QUIT_PROMPT) && !wm->file_saved) {
		if (do_exit) {
			if (!GHOST_confirmQuit(win->ghostwin))
				return;
		}
	}

	/* let WM_exit do all freeing, for correct quit.blend save */
	if (do_exit) {
		WM_exit(C);
	}
	else {
		bScreen *screen = win->screen;
		
		BLI_remlink(&wm->windows, win);
		
		wm_draw_window_clear(win);
		
		CTX_wm_window_set(C, win);  /* needed by handlers */
		WM_event_remove_handlers(C, &win->handlers);
		WM_event_remove_handlers(C, &win->modalhandlers);
		ED_screen_exit(C, win, win->screen); 
		
		wm_window_free(C, wm, win);
	
		/* if temp screen, delete it after window free (it stops jobs that can access it) */
		if (screen->temp) {
			Main *bmain = CTX_data_main(C);
			BKE_libblock_free(bmain, screen);
		}
	}		
}

void wm_window_title(wmWindowManager *wm, wmWindow *win)
{
	if (win->screen && win->screen->temp) {
		/* nothing to do for 'temp' windows,
		 * because WM_window_open_temp always sets window title  */
	}
	else {
		
		/* this is set to 1 if you don't have startup.blend open */
		if (G.save_over && G.main->name[0]) {
			char str[sizeof(G.main->name) + 24];
			BLI_snprintf(str, sizeof(str), "Blender%s [%s%s]", wm->file_saved ? "" : "*", G.main->name,
			             G.main->recovered ? " (Recovered)" : "");
			GHOST_SetTitle(win->ghostwin, str);
		}
		else
			GHOST_SetTitle(win->ghostwin, "Blender");

		/* Informs GHOST of unsaved changes, to set window modified visual indicator (MAC OS X)
		 * and to give hint of unsaved changes for a user warning mechanism
		 * in case of OS application terminate request (e.g. OS Shortcut Alt+F4, Cmd+Q, (...), or session end) */
		GHOST_SetWindowModifiedState(win->ghostwin, (GHOST_TUns8) !wm->file_saved);
		
	}
}

/* belongs to below */
static void wm_window_add_ghostwindow(const char *title, wmWindow *win)
{
	GHOST_WindowHandle ghostwin;
	static int multisamples = -1;
	int scr_w, scr_h, posy;
	
	/* force setting multisamples only once, it requires restart - and you cannot 
	 * mix it, either all windows have it, or none (tested in OSX opengl) */
	if (multisamples == -1)
		multisamples = U.ogl_multisamples;
	
	wm_get_screensize(&scr_w, &scr_h);
	posy = (scr_h - win->posy - win->sizey);
	
	ghostwin = GHOST_CreateWindow(g_system, title,
	                              win->posx, posy, win->sizex, win->sizey,
	                              (GHOST_TWindowState)win->windowstate,
	                              GHOST_kDrawingContextTypeOpenGL,
	                              0 /* no stereo */,
	                              multisamples /* AA */);
	
	if (ghostwin) {
		GHOST_RectangleHandle bounds;
		
		/* needed so we can detect the graphics card below */
		GPU_extensions_init();
		
		win->ghostwin = ghostwin;
		GHOST_SetWindowUserData(ghostwin, win); /* pointer back */
		
		if (win->eventstate == NULL)
			win->eventstate = MEM_callocN(sizeof(wmEvent), "window event state");
		
		/* set the state */
		GHOST_SetWindowState(ghostwin, (GHOST_TWindowState)win->windowstate);

		/* until screens get drawn, make it nice gray */
		glClearColor(0.55, 0.55, 0.55, 0.0);
		/* Crash on OSS ATI: bugs.launchpad.net/ubuntu/+source/mesa/+bug/656100 */
		if (!GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE)) {
			glClear(GL_COLOR_BUFFER_BIT);
		}
		
		/* displays with larger native pixels, like Macbook. Used to scale dpi with */
		/* needed here, because it's used before it reads userdef */
		U.pixelsize = GHOST_GetNativePixelSize(win->ghostwin);
		BKE_userdef_state();
		
		/* store actual window size in blender window */
		bounds = GHOST_GetClientBounds(win->ghostwin);
		win->sizex = GHOST_GetWidthRectangle(bounds);
		win->sizey = GHOST_GetHeightRectangle(bounds);
		GHOST_DisposeRectangle(bounds);

		
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
	
	/* no commandline prefsize? then we set this.
	 * Note that these values will be used only
	 * when there is no startup.blend yet.
	 */
	if (wm_init_state.size_x == 0) {
		wm_get_screensize(&wm_init_state.size_x, &wm_init_state.size_y);
		
	/* note!, this isnt quite correct, active screen maybe offset 1000s if PX,
	 * we'd need a wm_get_screensize like function that gives offset,
	 * in practice the window manager will likely move to the correct monitor */
	wm_init_state.start_x = 0;
	wm_init_state.start_y = 0;


#if !defined(__APPLE__) && !defined(WIN32)  /* X11 */
		/* X11, start maximized but use default sane size */
		wm_init_state.size_x = min_ii(wm_init_state.size_x, WM_WIN_INIT_SIZE_X);
		wm_init_state.size_y = min_ii(wm_init_state.size_y, WM_WIN_INIT_SIZE_Y);
		/* pad */
		wm_init_state.start_x = WM_WIN_INIT_PAD;
		wm_init_state.start_y = WM_WIN_INIT_PAD;
		wm_init_state.size_x -= WM_WIN_INIT_PAD * 2;
		wm_init_state.size_y -= WM_WIN_INIT_PAD * 2;
#endif
	}
	
	for (win = wm->windows.first; win; win = win->next) {
		if (win->ghostwin == NULL) {
			if ((win->sizex == 0) || (wm_init_state.override_flag & WIN_OVERRIDE_GEOM)) {
				win->posx = wm_init_state.start_x;
				win->posy = wm_init_state.start_y;
				win->sizex = wm_init_state.size_x;
				win->sizey = wm_init_state.size_y;

				win->windowstate = GHOST_kWindowStateNormal;
				wm_init_state.override_flag &= ~WIN_OVERRIDE_GEOM;
			}

			if (wm_init_state.override_flag & WIN_OVERRIDE_WINSTATE) {
				win->windowstate = wm_init_state.windowstate;
				wm_init_state.override_flag &= ~WIN_OVERRIDE_WINSTATE;
			}

			wm_window_add_ghostwindow("Blender", win);
		}
		/* happens after fileread */
		if (win->eventstate == NULL)
			win->eventstate = MEM_callocN(sizeof(wmEvent), "window event state");

		/* add keymap handlers (1 handler for all keys in map!) */
		keymap = WM_keymap_find(wm->defaultconf, "Window", 0, 0);
		WM_event_add_keymap_handler(&win->handlers, keymap);
		
		keymap = WM_keymap_find(wm->defaultconf, "Screen", 0, 0);
		WM_event_add_keymap_handler(&win->handlers, keymap);

		keymap = WM_keymap_find(wm->defaultconf, "Screen Editing", 0, 0);
		WM_event_add_keymap_handler(&win->modalhandlers, keymap);
		
		/* add drop boxes */
		{
			ListBase *lb = WM_dropboxmap_find("Window", 0, 0);
			WM_event_add_dropbox_handler(&win->handlers, lb);
		}
		wm_window_title(wm, win);
	}
}

/* new window, no screen yet, but we open ghostwindow for it */
/* also gets the window level handlers */
/* area-rip calls this */
wmWindow *WM_window_open(bContext *C, const rcti *rect)
{
	wmWindow *win = wm_window_new(C);
	
	win->posx = rect->xmin;
	win->posy = rect->ymin;
	win->sizex = BLI_rcti_size_x(rect);
	win->sizey = BLI_rcti_size_y(rect);

	win->drawmethod = U.wmdrawmethod;
	win->drawdata = NULL;
	
	WM_check(C);
	
	return win;
}

/* uses screen->temp tag to define what to do, currently it limits
 * to only one "temp" window for render out, preferences, filewindow, etc */
/* type is defined in WM_api.h */

void WM_window_open_temp(bContext *C, rcti *position, int type)
{
	wmWindow *win;
	ScrArea *sa;
	Scene *scene = CTX_data_scene(C);
	
	/* changes rect to fit within desktop */
	wm_window_check_position(position);
	
	/* test if we have a temp screen already */
	for (win = CTX_wm_manager(C)->windows.first; win; win = win->next)
		if (win->screen->temp)
			break;
	
	/* add new window? */
	if (win == NULL) {
		win = wm_window_new(C);
		
		win->posx = position->xmin;
		win->posy = position->ymin;
	}
	
	win->sizex = BLI_rcti_size_x(position);
	win->sizey = BLI_rcti_size_y(position);
	
	if (win->ghostwin) {
		wm_window_set_size(win, win->sizex, win->sizey);
		wm_window_raise(win);
	}
	
	if (win->screen == NULL) {
		/* add new screen */
		win->screen = ED_screen_add(win, scene, "temp");
	}
	else {
		/* switch scene for rendering */
		if (win->screen->scene != scene)
			ED_screen_set_scene(C, win->screen, scene);
	}

	win->screen->temp = 1; 
	
	/* make window active, and validate/resize */
	CTX_wm_window_set(C, win);
	WM_check(C);
	
	/* ensure it shows the right spacetype editor */
	sa = win->screen->areabase.first;
	CTX_wm_area_set(C, sa);
	
	if (type == WM_WINDOW_RENDER) {
		ED_area_newspace(C, sa, SPACE_IMAGE);
	}
	else {
		ED_area_newspace(C, sa, SPACE_USERPREF);
	}
	
	ED_screen_set(C, win->screen);
	ED_screen_refresh(CTX_wm_manager(C), win); /* test scale */
	
	if (sa->spacetype == SPACE_IMAGE)
		GHOST_SetTitle(win->ghostwin, IFACE_("Blender Render"));
	else if (ELEM(sa->spacetype, SPACE_OUTLINER, SPACE_USERPREF))
		GHOST_SetTitle(win->ghostwin, IFACE_("Blender User Preferences"));
	else if (sa->spacetype == SPACE_FILE)
		GHOST_SetTitle(win->ghostwin, IFACE_("Blender File View"));
	else
		GHOST_SetTitle(win->ghostwin, "Blender");
}


/* ****************** Operators ****************** */

/* operator callback */
int wm_window_duplicate_exec(bContext *C, wmOperator *UNUSED(op))
{
	wm_window_copy(C, CTX_wm_window(C));
	WM_check(C);
	
	WM_event_add_notifier(C, NC_WINDOW | NA_ADDED, NULL);
	
	return OPERATOR_FINISHED;
}


/* fullscreen operator callback */
int wm_window_fullscreen_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	wmWindow *window = CTX_wm_window(C);
	GHOST_TWindowState state;

	if (G.background)
		return OPERATOR_CANCELLED;

	state = GHOST_GetWindowState(window->ghostwin);
	if (state != GHOST_kWindowStateFullScreen)
		GHOST_SetWindowState(window->ghostwin, GHOST_kWindowStateFullScreen);
	else
		GHOST_SetWindowState(window->ghostwin, GHOST_kWindowStateNormal);

	return OPERATOR_FINISHED;
	
}


/* ************ events *************** */

static void wm_convert_cursor_position(wmWindow *win, int *x, int *y)
{
	float fac = GHOST_GetNativePixelSize(win->ghostwin);
	
	GHOST_ScreenToClient(win->ghostwin, *x, *y, x, y);
	*x *= fac;
	
	*y = (win->sizey - 1) - *y;
	*y *= fac;
}


void wm_get_cursor_position(wmWindow *win, int *x, int *y)
{
	GHOST_GetCursorPosition(g_system, x, y);
	wm_convert_cursor_position(win, x, y);
}

typedef enum {
	SHIFT    = 's',
	CONTROL  = 'c',
	ALT      = 'a',
	OS       = 'C'
} modifierKeyType;

/* check if specified modifier key type is pressed */
static int query_qual(modifierKeyType qual) 
{
	GHOST_TModifierKeyMask left, right;
	int val = 0;
	
	switch (qual) {
		case SHIFT:
			left = GHOST_kModifierKeyLeftShift;
			right = GHOST_kModifierKeyRightShift;
			break;
		case CONTROL:
			left = GHOST_kModifierKeyLeftControl;
			right = GHOST_kModifierKeyRightControl;
			break;
		case OS:
			left = right = GHOST_kModifierKeyOS;
			break;
		case ALT:
		default:
			left = GHOST_kModifierKeyLeftAlt;
			right = GHOST_kModifierKeyRightAlt;
			break;
	}
	
	GHOST_GetModifierKeyState(g_system, left, &val);
	if (!val)
		GHOST_GetModifierKeyState(g_system, right, &val);
	
	return val;
}

void wm_window_make_drawable(wmWindowManager *wm, wmWindow *win) 
{
	if (win != wm->windrawable && win->ghostwin) {
//		win->lmbut = 0;	/* keeps hanging when mousepressed while other window opened */
		
		wm->windrawable = win;
		if (G.debug & G_DEBUG_EVENTS) {
			printf("%s: set drawable %d\n", __func__, win->winid);
		}
		GHOST_ActivateWindowDrawingContext(win->ghostwin);
		
		/* this can change per window */
		U.pixelsize = GHOST_GetNativePixelSize(win->ghostwin);
		BKE_userdef_state();
	}
}

/* called by ghost, here we handle events for windows themselves or send to event system */
/* mouse coordinate converversion happens here */
static int ghost_event_proc(GHOST_EventHandle evt, GHOST_TUserDataPtr C_void_ptr)
{
	bContext *C = C_void_ptr;
	wmWindowManager *wm = CTX_wm_manager(C);
	GHOST_TEventType type = GHOST_GetEventType(evt);
	int time = GHOST_GetEventTime(evt);
	
	if (type == GHOST_kEventQuit) {
		WM_exit(C);
	}
	else {
		GHOST_WindowHandle ghostwin = GHOST_GetEventWindow(evt);
		GHOST_TEventDataPtr data = GHOST_GetEventData(evt);
		wmWindow *win;
		
		/* Ghost now can call this function for life resizes, but it should return if WM didn't initialize yet.
		 * Can happen on file read (especially full size window)  */
		if ((wm->initialized & WM_INIT_WINDOW) == 0) {
			return 1;
		}
		if (!ghostwin) {
			/* XXX - should be checked, why are we getting an event here, and */
			/* what is it? */
			puts("<!> event has no window");
			return 1;
		}
		else if (!GHOST_ValidWindow(g_system, ghostwin)) {
			/* XXX - should be checked, why are we getting an event here, and */
			/* what is it? */
			puts("<!> event has invalid window");
			return 1;
		}
		else {
			win = GHOST_GetWindowUserData(ghostwin);
		}
		
		switch (type) {
			case GHOST_kEventWindowDeactivate:
				wm_event_add_ghostevent(wm, win, type, time, data);
				win->active = 0; /* XXX */
				
				/* clear modifiers for inactive windows */
				win->eventstate->alt = 0;
				win->eventstate->ctrl = 0;
				win->eventstate->shift = 0;
				win->eventstate->oskey = 0;
				win->eventstate->keymodifier = 0;

				break;
			case GHOST_kEventWindowActivate: 
			{
				GHOST_TEventKeyData kdata;
				wmEvent event;
				int wx, wy;
				
				wm->winactive = win; /* no context change! c->wm->windrawable is drawable, or for area queues */
				
				win->active = 1;
//				window_handle(win, INPUTCHANGE, win->active);
				
				/* bad ghost support for modifier keys... so on activate we set the modifiers again */

				/* TODO: This is not correct since a modifier may be held when a window is activated...
				 * better solve this at ghost level. attempted fix r54450 but it caused bug [#34255] */
				kdata.ascii = '\0';
				kdata.utf8_buf[0] = '\0';
				if (win->eventstate->shift && !query_qual(SHIFT)) {
					kdata.key = GHOST_kKeyLeftShift;
					wm_event_add_ghostevent(wm, win, GHOST_kEventKeyUp, time, &kdata);
				}
				if (win->eventstate->ctrl && !query_qual(CONTROL)) {
					kdata.key = GHOST_kKeyLeftControl;
					wm_event_add_ghostevent(wm, win, GHOST_kEventKeyUp, time, &kdata);
				}
				if (win->eventstate->alt && !query_qual(ALT)) {
					kdata.key = GHOST_kKeyLeftAlt;
					wm_event_add_ghostevent(wm, win, GHOST_kEventKeyUp, time, &kdata);
				}
				if (win->eventstate->oskey && !query_qual(OS)) {
					kdata.key = GHOST_kKeyOS;
					wm_event_add_ghostevent(wm, win, GHOST_kEventKeyUp, time, &kdata);
				}
				/* keymodifier zero, it hangs on hotkeys that open windows otherwise */
				win->eventstate->keymodifier = 0;
				
				/* entering window, update mouse pos. but no event */
				wm_get_cursor_position(win,  &wx, &wy);

				win->eventstate->x = wx;
				win->eventstate->y = wy;
				
				win->addmousemove = 1;   /* enables highlighted buttons */
				
				wm_window_make_drawable(wm, win);

				/* window might be focused by mouse click in configuration of window manager
				 * when focus is not following mouse
				 * click could have been done on a button and depending on window manager settings
				 * click would be passed to blender or not, but in any case button under cursor
				 * should be activated, so at max next click on button without moving mouse
				 * would trigger it's handle function
				 * currently it seems to be common practice to generate new event for, but probably
				 * we'll need utility function for this? (sergey)
				 */
				wm_event_init_from_window(win, &event);
				event.type = MOUSEMOVE;
				event.prevx = event.x;
				event.prevy = event.y;

				wm_event_add(win, &event);

				break;
			}
			case GHOST_kEventWindowClose:
			{
				wm_window_close(C, wm, win);
				break;
			}
			case GHOST_kEventWindowUpdate:
			{
				if (G.debug & G_DEBUG_EVENTS) {
					printf("%s: ghost redraw %d\n", __func__, win->winid);
				}
				
				wm_window_make_drawable(wm, win);
				WM_event_add_notifier(C, NC_WINDOW, NULL);

				break;
			}
			case GHOST_kEventWindowSize:
			case GHOST_kEventWindowMove:
			{
				GHOST_TWindowState state;
				state = GHOST_GetWindowState(win->ghostwin);
				win->windowstate = state;

				/* stop screencast if resize */
				if (type == GHOST_kEventWindowSize) {
					WM_jobs_stop(CTX_wm_manager(C), win->screen, NULL);
				}
				
				/* win32: gives undefined window size when minimized */
				if (state != GHOST_kWindowStateMinimized) {
					GHOST_RectangleHandle client_rect;
					int l, t, r, b, scr_w, scr_h;
					int sizex, sizey, posx, posy;
					
					client_rect = GHOST_GetClientBounds(win->ghostwin);
					GHOST_GetRectangle(client_rect, &l, &t, &r, &b);
					
					GHOST_DisposeRectangle(client_rect);
					
					wm_get_desktopsize(&scr_w, &scr_h);
					sizex = r - l;
					sizey = b - t;
					posx = l;
					posy = scr_h - t - win->sizey;

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
						win->sizex = sizex;
						win->sizey = sizey;
						win->posx = posx;
						win->posy = posy;

						/* debug prints */
						if (G.debug & G_DEBUG_EVENTS) {
							const char *state_str;
							state = GHOST_GetWindowState(win->ghostwin);

							if (state == GHOST_kWindowStateNormal) {
								state_str = "normal";
							}
							else if (state == GHOST_kWindowStateMinimized) {
								state_str = "minimized";
							}
							else if (state == GHOST_kWindowStateMaximized) {
								state_str = "maximized";
							}
							else if (state == GHOST_kWindowStateFullScreen) {
								state_str = "fullscreen";
							}
							else {
								state_str = "<unknown>";
							}

							printf("%s: window %d state = %s\n", __func__, win->winid, state_str);

							if (type != GHOST_kEventWindowSize) {
								printf("win move event pos %d %d size %d %d\n",
								       win->posx, win->posy, win->sizex, win->sizey);
							}
						}
					
						wm_window_make_drawable(wm, win);
						wm_draw_window_clear(win);
						WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
						WM_event_add_notifier(C, NC_WINDOW | NA_EDITED, NULL);
						
#if defined(__APPLE__) || defined(WIN32)
						/* OSX and Win32 don't return to the mainloop while resize */
						wm_event_do_handlers(C);
						wm_event_do_notifiers(C);
						wm_draw_update(C);
#endif
					}
				}
				break;
			}
				
			case GHOST_kEventOpenMainFile:
			{
				PointerRNA props_ptr;
				wmWindow *oldWindow;
				char *path = GHOST_GetEventData(evt);
				
				if (path) {
					/* operator needs a valid window in context, ensures
					 * it is correctly set */
					oldWindow = CTX_wm_window(C);
					CTX_wm_window_set(C, win);
					
					WM_operator_properties_create(&props_ptr, "WM_OT_open_mainfile");
					RNA_string_set(&props_ptr, "filepath", path);
					WM_operator_name_call(C, "WM_OT_open_mainfile", WM_OP_EXEC_DEFAULT, &props_ptr);
					WM_operator_properties_free(&props_ptr);
					
					CTX_wm_window_set(C, oldWindow);
				}
				break;
			}
			case GHOST_kEventDraggingDropDone:
			{
				wmEvent event;
				GHOST_TEventDragnDropData *ddd = GHOST_GetEventData(evt);
				int wx, wy;
				
				/* entering window, update mouse pos */
				wm_get_cursor_position(win, &wx, &wy);
				win->eventstate->x = wx;
				win->eventstate->y = wy;
				
				wm_event_init_from_window(win, &event);  /* copy last state, like mouse coords */
				
				/* activate region */
				event.type = MOUSEMOVE;
				event.prevx = event.x;
				event.prevy = event.y;
				
				wm->winactive = win; /* no context change! c->wm->windrawable is drawable, or for area queues */
				win->active = 1;
				
				wm_event_add(win, &event);
				
				
				/* make blender drop event with custom data pointing to wm drags */
				event.type = EVT_DROP;
				event.val = KM_RELEASE;
				event.custom = EVT_DATA_LISTBASE;
				event.customdata = &wm->drags;
				event.customdatafree = 1;
				
				wm_event_add(win, &event);
				
				/* printf("Drop detected\n"); */
				
				/* add drag data to wm for paths: */
				
				if (ddd->dataType == GHOST_kDragnDropTypeFilenames) {
					GHOST_TStringArray *stra = ddd->data;
					int a, icon;
					
					for (a = 0; a < stra->count; a++) {
						printf("drop file %s\n", stra->strings[a]);
						/* try to get icon type from extension */
						icon = ED_file_extension_icon((char *)stra->strings[a]);
						
						WM_event_start_drag(C, icon, WM_DRAG_PATH, stra->strings[a], 0.0);
						/* void poin should point to string, it makes a copy */
						break; /* only one drop element supported now */
					}
				}
				
				break;
			}
			case GHOST_kEventNativeResolutionChange:
				// printf("change, pixel size %f\n", GHOST_GetNativePixelSize(win->ghostwin));
				
				U.pixelsize = GHOST_GetNativePixelSize(win->ghostwin);
				BKE_userdef_state();
				WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
				WM_event_add_notifier(C, NC_WINDOW | NA_EDITED, NULL);

				break;
			case GHOST_kEventTrackpad:
			{
				GHOST_TEventTrackpadData *pd = data;
				
				wm_convert_cursor_position(win, &pd->x, &pd->y);
				wm_event_add_ghostevent(wm, win, type, time, data);
				break;
			}
			case GHOST_kEventCursorMove:
			{
				GHOST_TEventCursorData *cd = data;
				
				wm_convert_cursor_position(win, &cd->x, &cd->y);
				wm_event_add_ghostevent(wm, win, type, time, data);
				break;
			}
			default:
				wm_event_add_ghostevent(wm, win, type, time, data);
				break;
		}

	}
	return 1;
}


/* This timer system only gives maximum 1 timer event per redraw cycle,
 * to prevent queues to get overloaded.
 * Timer handlers should check for delta to decide if they just
 * update, or follow real time.
 * Timer handlers can also set duration to match frames passed
 */
static int wm_window_timer(const bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmTimer *wt, *wtnext;
	wmWindow *win;
	double time = PIL_check_seconds_timer();
	int retval = 0;
	
	for (wt = wm->timers.first; wt; wt = wtnext) {
		wtnext = wt->next; /* in case timer gets removed */
		win = wt->win;

		if (wt->sleep == 0) {
			if (time > wt->ntime) {
				wt->delta = time - wt->ltime;
				wt->duration += wt->delta;
				wt->ltime = time;
				wt->ntime = wt->stime + wt->timestep * ceil(wt->duration / wt->timestep);

				if (wt->event_type == TIMERJOBS)
					wm_jobs_timer(C, wm, wt);
				else if (wt->event_type == TIMERAUTOSAVE)
					wm_autosave_timer(C, wm, wt);
				else if (win) {
					wmEvent event;
					wm_event_init_from_window(win, &event);
					
					event.type = wt->event_type;
					event.val = 0;
					event.keymodifier = 0;
					event.custom = EVT_DATA_TIMER;
					event.customdata = wt;
					wm_event_add(win, &event);

					retval = 1;
				}
			}
		}
	}
	return retval;
}

void wm_window_process_events(const bContext *C) 
{
	int hasevent;

	BLI_assert(BLI_thread_is_main());

	hasevent = GHOST_ProcessEvents(g_system, 0); /* 0 is no wait */

	if (hasevent)
		GHOST_DispatchEvents(g_system);
	
	hasevent |= wm_window_timer(C);

	/* no event, we sleep 5 milliseconds */
	if (hasevent == 0)
		PIL_sleep_ms(5);
}

void wm_window_process_events_nosleep(void) 
{
	if (GHOST_ProcessEvents(g_system, 0))
		GHOST_DispatchEvents(g_system);
}

/* exported as handle callback to bke blender.c */
void wm_window_testbreak(void)
{
	static double ltime = 0;
	double curtime = PIL_check_seconds_timer();

	BLI_assert(BLI_thread_is_main());

	/* only check for breaks every 50 milliseconds
	 * if we get called more often.
	 */
	if ((curtime - ltime) > 0.05) {
		int hasevent = GHOST_ProcessEvents(g_system, 0); /* 0 is no wait */
		
		if (hasevent)
			GHOST_DispatchEvents(g_system);
		
		ltime = curtime;
	}
}

/* **************** init ********************** */

void wm_ghost_init(bContext *C)
{
	if (!g_system) {
		GHOST_EventConsumerHandle consumer = GHOST_CreateEventConsumer(ghost_event_proc, C);
		
		g_system = GHOST_CreateSystem();
		GHOST_AddEventConsumer(g_system, consumer);
		
		if (wm_init_state.native_pixels) {
			GHOST_UseNativePixels();
		}
	}
}

void wm_ghost_exit(void)
{
	if (g_system)
		GHOST_DisposeSystem(g_system);

	g_system = NULL;
}

/* **************** timer ********************** */

/* to (de)activate running timers temporary */
void WM_event_timer_sleep(wmWindowManager *wm, wmWindow *UNUSED(win), wmTimer *timer, bool do_sleep)
{
	wmTimer *wt;
	
	for (wt = wm->timers.first; wt; wt = wt->next)
		if (wt == timer)
			break;

	if (wt)
		wt->sleep = do_sleep;
}

wmTimer *WM_event_add_timer(wmWindowManager *wm, wmWindow *win, int event_type, double timestep)
{
	wmTimer *wt = MEM_callocN(sizeof(wmTimer), "window timer");
	
	wt->event_type = event_type;
	wt->ltime = PIL_check_seconds_timer();
	wt->ntime = wt->ltime + timestep;
	wt->stime = wt->ltime;
	wt->timestep = timestep;
	wt->win = win;
	
	BLI_addtail(&wm->timers, wt);
	
	return wt;
}

void WM_event_remove_timer(wmWindowManager *wm, wmWindow *UNUSED(win), wmTimer *timer)
{
	wmTimer *wt;
	
	/* extra security check */
	for (wt = wm->timers.first; wt; wt = wt->next)
		if (wt == timer)
			break;
	if (wt) {
		wmWindow *win;
		
		if (wm->reports.reporttimer == wt)
			wm->reports.reporttimer = NULL;
		
		BLI_remlink(&wm->timers, wt);
		if (wt->customdata)
			MEM_freeN(wt->customdata);
		MEM_freeN(wt);
		
		/* there might be events in queue with this timer as customdata */
		for (win = wm->windows.first; win; win = win->next) {
			wmEvent *event;
			for (event = win->queue.first; event; event = event->next) {
				if (event->customdata == wt) {
					event->customdata = NULL;
					event->type = EVENT_NONE;	/* timer users customdata, dont want NULL == NULL */
				}
			}
		}
	}
}

/* ******************* clipboard **************** */

static char *wm_clipboard_text_get_ex(bool selection, int *r_len,
                                      bool firstline)
{
	char *p, *p2, *buf, *newbuf;

	if (G.background) {
		*r_len = 0;
		return NULL;
	}

	buf = (char *)GHOST_getClipboard(selection);
	if (!buf) {
		*r_len = 0;
		return NULL;
	}
	
	/* always convert from \r\n to \n */
	p2 = newbuf = MEM_mallocN(strlen(buf) + 1, __func__);

	if (firstline) {
		/* will return an over-alloc'ed value in the case there are newlines */
		for (p = buf; *p; p++) {
			if ((*p != '\n') && (*p != '\r')) {
				*(p2++) = *p;
			}
			else {
				break;
			}
		}
	}
	else {
		for (p = buf; *p; p++) {
			if (*p != '\r') {
				*(p2++) = *p;
			}
		}
	}

	*p2 = '\0';

	free(buf); /* ghost uses regular malloc */
	
	*r_len = (p2 - newbuf);

	return newbuf;
}

/**
 * Return text from the clipboard.
 *
 * \note Caller needs to check for valid utf8 if this is a requirement.
 */
char *WM_clipboard_text_get(bool selection, int *r_len)
{
	return wm_clipboard_text_get_ex(selection, r_len, false);
}

/**
 * Convenience function for pasting to areas of Blender which don't support newlines.
 */
char *WM_clipboard_text_get_firstline(bool selection, int *r_len)
{
	return wm_clipboard_text_get_ex(selection, r_len, true);
}

void WM_clipboard_text_set(const char *buf, bool selection)
{
	if (!G.background) {
#ifdef _WIN32
		/* do conversion from \n to \r\n on Windows */
		const char *p;
		char *p2, *newbuf;
		int newlen = 0;
		
		for (p = buf; *p; p++) {
			if (*p == '\n')
				newlen += 2;
			else
				newlen++;
		}
		
		newbuf = MEM_callocN(newlen + 1, "WM_clipboard_text_set");
	
		for (p = buf, p2 = newbuf; *p; p++, p2++) {
			if (*p == '\n') {
				*(p2++) = '\r'; *p2 = '\n';
			}
			else {
				*p2 = *p;
			}
		}
		*p2 = '\0';
	
		GHOST_putClipboard((GHOST_TInt8 *)newbuf, selection);
		MEM_freeN(newbuf);
#else
		GHOST_putClipboard((GHOST_TInt8 *)buf, selection);
#endif
	}
}

/* ******************* progress bar **************** */

void WM_progress_set(wmWindow *win, float progress)
{
	GHOST_SetProgressBar(win->ghostwin, progress);
}

void WM_progress_clear(wmWindow *win)
{
	GHOST_EndProgressBar(win->ghostwin);
}

/* ************************************ */

void wm_window_get_position(wmWindow *win, int *posx_r, int *posy_r) 
{
	*posx_r = win->posx;
	*posy_r = win->posy;
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

void wm_window_set_swap_interval (wmWindow *win, int interval)
{
	GHOST_SetSwapInterval(win->ghostwin, interval);
}

int wm_window_get_swap_interval (wmWindow *win)
{
	return GHOST_GetSwapInterval(win->ghostwin);
}


/* ******************* exported api ***************** */


/* called whem no ghost system was initialized */
void WM_init_state_size_set(int stax, int stay, int sizx, int sizy)
{
	wm_init_state.start_x = stax; /* left hand pos */
	wm_init_state.start_y = stay; /* bottom pos */
	wm_init_state.size_x = sizx < 640 ? 640 : sizx;
	wm_init_state.size_y = sizy < 480 ? 480 : sizy;
	wm_init_state.override_flag |= WIN_OVERRIDE_GEOM;
}

/* for borderless and border windows set from command-line */
void WM_init_state_fullscreen_set(void)
{
	wm_init_state.windowstate = GHOST_kWindowStateFullScreen;
	wm_init_state.override_flag |= WIN_OVERRIDE_WINSTATE;
}

void WM_init_state_normal_set(void)
{
	wm_init_state.windowstate = GHOST_kWindowStateNormal;
	wm_init_state.override_flag |= WIN_OVERRIDE_WINSTATE;
}

void WM_init_native_pixels(bool do_it)
{
	wm_init_state.native_pixels = do_it;
}

/* This function requires access to the GHOST_SystemHandle (g_system) */
void WM_cursor_warp(wmWindow *win, int x, int y)
{
	if (win && win->ghostwin) {
		float f = GHOST_GetNativePixelSize(win->ghostwin);
		int oldx = x, oldy = y;

		x = x / f;
		y = y / f;
		y = win->sizey - y - 1;

		GHOST_ClientToScreen(win->ghostwin, x, y, &x, &y);
		GHOST_SetCursorPosition(g_system, x, y);

		win->eventstate->prevx = oldx;
		win->eventstate->prevy = oldy;
	}
}

/**
 * Get the cursor pressure, in most cases you'll want to use wmTabletData from the event
 */
float WM_cursor_pressure(const struct wmWindow *win)
{
	const GHOST_TabletData *td = GHOST_GetTabletData(win->ghostwin);
	/* if there's tablet data from an active tablet device then add it */
	if ((td != NULL) && td->Active != GHOST_kTabletModeNone) {
		return td->Pressure;
	}
	else {
		return -1.0f;
	}
}

/* support for native pixel size */
/* mac retina opens window in size X, but it has up to 2 x more pixels */
int WM_window_pixels_x(wmWindow *win)
{
	float f = GHOST_GetNativePixelSize(win->ghostwin);
	
	return (int)(f * (float)win->sizex);
}

int WM_window_pixels_y(wmWindow *win)
{
	float f = GHOST_GetNativePixelSize(win->ghostwin);
	
	return (int)(f * (float)win->sizey);
	
}
