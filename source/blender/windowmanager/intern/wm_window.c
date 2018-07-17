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
#include "DNA_workspace_types.h"

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_icons.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_draw.h"
#include "wm_window.h"
#include "wm_event_system.h"

#include "ED_anim_api.h"
#include "ED_scene.h"
#include "ED_screen.h"
#include "ED_fileselect.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "PIL_time.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_init_exit.h"
#include "GPU_immediate.h"
#include "GPU_material.h"
#include "GPU_texture.h"
#include "GPU_context.h"
#include "BLF_api.h"

#include "UI_resources.h"

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

static void wm_window_set_drawable(wmWindowManager *wm, wmWindow *win, bool activate);
static void wm_window_clear_drawable(wmWindowManager *wm);

/* XXX this one should correctly check for apple top header...
 * done for Cocoa : returns window contents (and not frame) max size*/
void wm_get_screensize(int *r_width, int *r_height)
{
	unsigned int uiwidth;
	unsigned int uiheight;

	GHOST_GetMainDisplayDimensions(g_system, &uiwidth, &uiheight);
	*r_width = uiwidth;
	*r_height = uiheight;
}

/* size of all screens (desktop), useful since the mouse is bound by this */
void wm_get_desktopsize(int *r_width, int *r_height)
{
	unsigned int uiwidth;
	unsigned int uiheight;

	GHOST_GetAllDisplayDimensions(g_system, &uiwidth, &uiheight);
	*r_width = uiwidth;
	*r_height = uiheight;
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

static void wm_ghostwindow_destroy(wmWindowManager *wm, wmWindow *win)
{
	if (win->ghostwin) {
		/* Prevents non-drawable state of main windows (bugs #22967,
		 * #25071 and possibly #22477 too). Always clear it even if
		 * this window was not the drawable one, because we mess with
		 * drawing context to discard the GW context. */
		wm_window_clear_drawable(wm);

		if (win == wm->winactive) {
			wm->winactive = NULL;
		}

		/* We need this window's opengl context active to discard it. */
		GHOST_ActivateWindowDrawingContext(win->ghostwin);
		GWN_context_active_set(win->gwnctx);

		/* Delete local gawain objects.  */
		GWN_context_discard(win->gwnctx);

		GHOST_DisposeWindow(g_system, win->ghostwin);
		win->ghostwin = NULL;
		win->gwnctx = NULL;

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

	BKE_screen_area_map_free(&win->global_areas);

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

	if (win->cursor_keymap_status) {
		MEM_freeN(win->cursor_keymap_status);
	}

	wm_event_free_all(win);

	wm_ghostwindow_destroy(wm, win);

	BKE_workspace_instance_hook_free(G_MAIN, win->workspace_hook);
	MEM_freeN(win->stereo3d_format);

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
wmWindow *wm_window_new(bContext *C, wmWindow *parent)
{
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = MEM_callocN(sizeof(wmWindow), "window");

	BLI_addtail(&wm->windows, win);
	win->winid = find_free_winid(wm);

	win->parent = (parent && parent->parent) ? parent->parent : parent;
	win->stereo3d_format = MEM_callocN(sizeof(Stereo3dFormat), "Stereo 3D Format (window)");
	win->workspace_hook = BKE_workspace_instance_hook_create(bmain);

	return win;
}

/* part of wm_window.c api */
wmWindow *wm_window_copy(bContext *C, wmWindow *win_src, const bool duplicate_layout, const bool child)
{
	Main *bmain = CTX_data_main(C);
	wmWindow *win_parent = (child) ? win_src : win_src->parent;
	wmWindow *win_dst = wm_window_new(C, win_parent);
	WorkSpace *workspace = WM_window_get_active_workspace(win_src);
	WorkSpaceLayout *layout_old = WM_window_get_active_layout(win_src);
	WorkSpaceLayout *layout_new;

	win_dst->posx = win_src->posx + 10;
	win_dst->posy = win_src->posy;
	win_dst->sizex = win_src->sizex;
	win_dst->sizey = win_src->sizey;

	win_dst->scene = win_src->scene;
	STRNCPY(win_dst->view_layer_name, win_src->view_layer_name);
	BKE_workspace_active_set(win_dst->workspace_hook, workspace);
	layout_new = duplicate_layout ? ED_workspace_layout_duplicate(bmain, workspace, layout_old, win_dst) : layout_old;
	BKE_workspace_hook_layout_for_workspace_set(win_dst->workspace_hook, workspace, layout_new);

	*win_dst->stereo3d_format = *win_src->stereo3d_format;

	return win_dst;
}

/**
 * A higher level version of copy that tests the new window can be added.
 * (called from the operator directly)
 */
wmWindow *wm_window_copy_test(bContext *C, wmWindow *win_src, const bool duplicate_layout, const bool child)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win_dst;

	win_dst = wm_window_copy(C, win_src, duplicate_layout, child);

	WM_check(C);

	if (win_dst->ghostwin) {
		WM_event_add_notifier(C, NC_WINDOW | NA_ADDED, NULL);
		return win_dst;
	}
	else {
		wm_window_close(C, wm, win_dst);
		return NULL;
	}
}


/* -------------------------------------------------------------------- */
/** \name Quit Confirmation Dialog
 * \{ */

/** Cancel quitting and close the dialog */
static void wm_block_confirm_quit_cancel(bContext *C, void *arg_block, void *UNUSED(arg))
{
	wmWindow *win = CTX_wm_window(C);
	UI_popup_block_close(C, win, arg_block);
}

/** Discard the file changes and quit */
static void wm_block_confirm_quit_discard(bContext *C, void *arg_block, void *UNUSED(arg))
{
	wmWindow *win = CTX_wm_window(C);
	UI_popup_block_close(C, win, arg_block);
	WM_exit(C);
}

/* Save changes and quit */
static void wm_block_confirm_quit_save(bContext *C, void *arg_block, void *UNUSED(arg))
{
	PointerRNA props_ptr;
	wmWindow *win = CTX_wm_window(C);

	UI_popup_block_close(C, win, arg_block);

	wmOperatorType *ot = WM_operatortype_find("WM_OT_save_mainfile", false);

	WM_operator_properties_create_ptr(&props_ptr, ot);
	RNA_boolean_set(&props_ptr, "exit", true);
	/* No need for second confirmation popup. */
	RNA_boolean_set(&props_ptr, "check_existing", false);
	WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr);
	WM_operator_properties_free(&props_ptr);
}


/* Build the confirm dialog UI */
static uiBlock *block_create_confirm_quit(struct bContext *C, struct ARegion *ar, void *UNUSED(arg1))
{
	Main *bmain = CTX_data_main(C);

	uiStyle *style = UI_style_get();
	uiBlock *block = UI_block_begin(C, ar, "confirm_quit_popup", UI_EMBOSS);

	UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_LOOP | UI_BLOCK_NO_WIN_CLIP | UI_BLOCK_NUMSELECT);
	UI_block_emboss_set(block, UI_EMBOSS);

	uiLayout *layout = UI_block_layout(
	        block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 10, 2, U.widget_unit * 24, U.widget_unit * 6, 0, style);

	/* Text and some vertical space */
	{
		char *message;
		if (BKE_main_blendfile_path(bmain)[0] == '\0') {
			message = BLI_strdup(IFACE_("This file has not been saved yet. Save before closing?"));
		}
		else {
			const char *basename = BLI_path_basename(BKE_main_blendfile_path(bmain));
			message = BLI_sprintfN(IFACE_("Save changes to \"%s\" before closing?"), basename);
		}
		uiItemL(layout, message, ICON_ERROR);
		MEM_freeN(message);
	}

	uiItemS(layout);
	uiItemS(layout);


	/* Buttons */
	uiBut *but;

	uiLayout *split = uiLayoutSplit(layout, 0.0f, true);

	uiLayout *col = uiLayoutColumn(split, false);

	but = uiDefIconTextBut(
	        block, UI_BTYPE_BUT, 0, ICON_SCREEN_BACK, IFACE_("Cancel"), 0, 0, 0, UI_UNIT_Y,
	        NULL, 0, 0, 0, 0, TIP_("Do not quit"));
	UI_but_func_set(but, wm_block_confirm_quit_cancel, block, NULL);

	/* empty space between buttons */
	col = uiLayoutColumn(split, false);
	uiItemS(col);

	col = uiLayoutColumn(split, 1);
	but = uiDefIconTextBut(
	        block, UI_BTYPE_BUT, 0, ICON_CANCEL, IFACE_("Discard Changes"), 0, 0, 50, UI_UNIT_Y,
	        NULL, 0, 0, 0, 0, TIP_("Discard changes and quit"));
	UI_but_func_set(but, wm_block_confirm_quit_discard, block, NULL);

	col = uiLayoutColumn(split, 1);
	but = uiDefIconTextBut(
	        block, UI_BTYPE_BUT, 0, ICON_FILE_TICK, IFACE_("Save & Quit"), 0, 0, 50, UI_UNIT_Y,
	        NULL, 0, 0, 0, 0, TIP_("Save and quit"));
	UI_but_func_set(but, wm_block_confirm_quit_save, block, NULL);

	UI_block_bounds_set_centered(block, 10);

	return block;
}


/**
 * Call the confirm dialog on quitting. It's displayed in the context window so
 * caller should set it as desired.
 */
static void wm_confirm_quit(bContext *C)
{
	wmWindow *win = CTX_wm_window(C);

	if (GHOST_SupportsNativeDialogs() == 0) {
		UI_popup_block_invoke(C, block_create_confirm_quit, NULL);
	}
	else if (GHOST_confirmQuit(win->ghostwin)) {
		wm_exit_schedule_delayed(C);
	}
}

/**
 * Call the quit confirmation prompt or exit directly if needed. The use can
 * still cancel via the confirmation popup. Also, this may not quit Blender
 * immediately, but rather schedule the closing.
 *
 * \param win The window to show the confirmation popup/window in.
 */
void wm_quit_with_optional_confirmation_prompt(bContext *C, wmWindow *win)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win_ctx = CTX_wm_window(C);

	/* The popup will be displayed in the context window which may not be set
	 * here (this function gets called outside of normal event handling loop). */
	CTX_wm_window_set(C, win);

	if ((U.uiflag & USER_QUIT_PROMPT) && !wm->file_saved && !G.background) {
		wm_confirm_quit(C);
	}
	else {
		wm_exit_schedule_delayed(C);
	}

	CTX_wm_window_set(C, win_ctx);
}

/** \} */

/* this is event from ghost, or exit-blender op */
void wm_window_close(bContext *C, wmWindowManager *wm, wmWindow *win)
{
	/* First check if there is another main window remaining. */
	wmWindow *win_other;
	for (win_other = wm->windows.first; win_other; win_other = win_other->next) {
		if (win_other != win &&
		    win_other->parent == NULL &&
		    !WM_window_is_temp_screen(win_other))
		{
			break;
		}
	}

	if (win->parent == NULL && win_other == NULL) {
		wm_quit_with_optional_confirmation_prompt(C, win);
		return;
	}

	/* close child windows */
	for (wmWindow *win_child = wm->windows.first; win_child; win_child = win_child->next) {
		if (win_child->parent == win) {
			wm_window_close(C, wm, win_child);
		}
	}

	bScreen *screen = WM_window_get_active_screen(win);
	WorkSpace *workspace = WM_window_get_active_workspace(win);
	WorkSpaceLayout *layout = BKE_workspace_active_layout_get(win->workspace_hook);

	BLI_remlink(&wm->windows, win);

	CTX_wm_window_set(C, win);  /* needed by handlers */
	WM_event_remove_handlers(C, &win->handlers);
	WM_event_remove_handlers(C, &win->modalhandlers);

	/* for regular use this will _never_ be NULL,
	 * however we may be freeing an improperly initialized window. */
	if (screen) {
		ED_screen_exit(C, win, screen);
	}

	wm_window_free(C, wm, win);

	/* if temp screen, delete it after window free (it stops jobs that can access it) */
	if (screen && screen->temp) {
		Main *bmain = CTX_data_main(C);

		BLI_assert(BKE_workspace_layout_screen_get(layout) == screen);
		BKE_workspace_layout_remove(bmain, workspace, layout);
	}
}

void wm_window_title(wmWindowManager *wm, wmWindow *win)
{
	if (WM_window_is_temp_screen(win)) {
		/* nothing to do for 'temp' windows,
		 * because WM_window_open_temp always sets window title  */
	}
	else if (win->ghostwin) {
		/* this is set to 1 if you don't have startup.blend open */
		if (G.save_over && BKE_main_blendfile_path_from_global()[0]) {
			char str[sizeof(((Main *)NULL)->name) + 24];
			BLI_snprintf(str, sizeof(str), "Blender%s [%s%s]", wm->file_saved ? "" : "*",
			             BKE_main_blendfile_path_from_global(),
			             G_MAIN->recovered ? " (Recovered)" : "");
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

void WM_window_set_dpi(wmWindow *win)
{
	float auto_dpi = GHOST_GetDPIHint(win->ghostwin);

	/* Clamp auto DPI to 96, since our font/interface drawing does not work well
	 * with lower sizes. The main case we are interested in supporting is higher
	 * DPI. If a smaller UI is desired it is still possible to adjust UI scale. */
	auto_dpi = max_ff(auto_dpi, 96.0f);

	/* Lazily init UI scale size, preserving backwards compatibility by
	 * computing UI scale from ratio of previous DPI and auto DPI */
	if (U.ui_scale == 0) {
		int virtual_pixel = (U.virtual_pixel == VIRTUAL_PIXEL_NATIVE) ? 1 : 2;

		if (U.dpi == 0) {
			U.ui_scale = virtual_pixel;
		}
		else {
			U.ui_scale = (virtual_pixel * U.dpi * 96.0f) / (auto_dpi * 72.0f);
		}

		CLAMP(U.ui_scale, 0.25f, 4.0f);
	}

	/* Blender's UI drawing assumes DPI 72 as a good default following macOS
	 * while Windows and Linux use DPI 96. GHOST assumes a default 96 so we
	 * remap the DPI to Blender's convention. */
	auto_dpi *= GHOST_GetNativePixelSize(win->ghostwin);
	int dpi = auto_dpi * U.ui_scale * (72.0 / 96.0f);

	/* Automatically set larger pixel size for high DPI. */
	int pixelsize = max_ii(1, (int)(dpi / 64));
	/* User adjustment for pixel size. */
	pixelsize = max_ii(1, pixelsize + U.ui_line_width);

	/* Set user preferences globals for drawing, and for forward compatibility. */
	U.pixelsize = pixelsize;
	U.dpi = dpi / pixelsize;
	U.virtual_pixel = (pixelsize == 1) ? VIRTUAL_PIXEL_NATIVE : VIRTUAL_PIXEL_DOUBLE;
	U.widget_unit = (U.pixelsize * U.dpi * 20 + 36) / 72;
	U.dpi_fac = ((U.pixelsize * (float)U.dpi) / 72.0f);

	/* update font drawing */
	BLF_default_dpi(U.pixelsize * U.dpi);
}

static void wm_window_ensure_eventstate(wmWindow *win)
{
	if (win->eventstate) {
		return;
	}

	win->eventstate = MEM_callocN(sizeof(wmEvent), "window event state");
	wm_get_cursor_position(win, &win->eventstate->x, &win->eventstate->y);
}

/* belongs to below */
static void wm_window_ghostwindow_add(wmWindowManager *wm, const char *title, wmWindow *win)
{
	GHOST_WindowHandle ghostwin;
	GHOST_GLSettings glSettings = {0};
	int scr_w, scr_h, posy;

	/* a new window is created when pageflip mode is required for a window */
	if (win->stereo3d_format->display_mode == S3D_DISPLAY_PAGEFLIP)
		glSettings.flags |= GHOST_glStereoVisual;

	if (G.debug & G_DEBUG_GPU) {
		glSettings.flags |= GHOST_glDebugContext;
	}

	wm_get_screensize(&scr_w, &scr_h);
	posy = (scr_h - win->posy - win->sizey);

	ghostwin = GHOST_CreateWindow(g_system, title,
	                              win->posx, posy, win->sizex, win->sizey,
	                              (GHOST_TWindowState)win->windowstate,
	                              GHOST_kDrawingContextTypeOpenGL,
	                              glSettings);

	if (ghostwin) {
		GHOST_RectangleHandle bounds;

		/* Clear drawable so we can set the new window. */
		wm_window_clear_drawable(wm);

		win->gwnctx = GWN_context_create();

		/* needed so we can detect the graphics card below */
		GPU_init();

		/* Set window as drawable upon creation. Note this has already been
		 * it has already been activated by GHOST_CreateWindow. */
		bool activate = false;
		wm_window_set_drawable(wm, win, activate);

		win->ghostwin = ghostwin;
		GHOST_SetWindowUserData(ghostwin, win); /* pointer back */

		wm_window_ensure_eventstate(win);

		/* store actual window size in blender window */
		bounds = GHOST_GetClientBounds(win->ghostwin);

		/* win32: gives undefined window size when minimized */
		if (GHOST_GetWindowState(win->ghostwin) != GHOST_kWindowStateMinimized) {
			win->sizex = GHOST_GetWidthRectangle(bounds);
			win->sizey = GHOST_GetHeightRectangle(bounds);
		}
		GHOST_DisposeRectangle(bounds);

#ifndef __APPLE__
		/* set the state here, so minimized state comes up correct on windows */
		GHOST_SetWindowState(ghostwin, (GHOST_TWindowState)win->windowstate);
#endif
		/* until screens get drawn, make it nice gray */
		glClearColor(0.55, 0.55, 0.55, 0.0);
		/* Crash on OSS ATI: bugs.launchpad.net/ubuntu/+source/mesa/+bug/656100 */
		if (!GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE)) {
			glClear(GL_COLOR_BUFFER_BIT);
		}

		/* needed here, because it's used before it reads userdef */
		WM_window_set_dpi(win);

		wm_window_swap_buffers(win);

		//GHOST_SetWindowState(ghostwin, GHOST_kWindowStateModified);

		/* standard state vars for window */
		GPU_state_init();
	}
}

/**
 * Initialize #wmWindow without ghostwin, open these and clear.
 *
 * window size is read from window, if 0 it uses prefsize
 * called in #WM_check, also inits stuff after file read.
 *
 * \warning
 * After running, 'win->ghostwin' can be NULL in rare cases
 * (where OpenGL driver fails to create a context for eg).
 * We could remove them with #wm_window_ghostwindows_remove_invalid
 * but better not since caller may continue to use.
 * Instead, caller needs to handle the error case and cleanup.
 */
void wm_window_ghostwindows_ensure(wmWindowManager *wm)
{
	wmKeyMap *keymap;
	wmWindow *win;

	BLI_assert(G.background == false);

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

#ifdef WITH_X11 /* X11 */
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

			/* without this, cursor restore may fail, T45456 */
			if (win->cursor == 0) {
				win->cursor = CURSOR_STD;
			}

			wm_window_ghostwindow_add(wm, "Blender", win);
		}
		/* happens after fileread */
		wm_window_ensure_eventstate(win);

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

		/* add topbar */
		if (BLI_listbase_is_empty(&win->global_areas.areabase)) {
			ED_screen_global_areas_create(win);
		}
	}
}

/**
 * Call after #wm_window_ghostwindows_ensure or #WM_check
 * (after loading a new file) in the unlikely event a window couldn't be created.
 */
void wm_window_ghostwindows_remove_invalid(bContext *C, wmWindowManager *wm)
{
	wmWindow *win, *win_next;

	BLI_assert(G.background == false);

	for (win = wm->windows.first; win; win = win_next) {
		win_next = win->next;
		if (win->ghostwin == NULL) {
			wm_window_close(C, wm, win);
		}
	}
}

/**
 * new window, no screen yet, but we open ghostwindow for it,
 * also gets the window level handlers
 * \note area-rip calls this.
 * \return the window or NULL.
 */
wmWindow *WM_window_open(bContext *C, const rcti *rect)
{
	wmWindow *win_prev = CTX_wm_window(C);
	wmWindow *win = wm_window_new(C, win_prev);

	win->posx = rect->xmin;
	win->posy = rect->ymin;
	win->sizex = BLI_rcti_size_x(rect);
	win->sizey = BLI_rcti_size_y(rect);

	WM_check(C);

	if (win->ghostwin) {
		return win;
	}
	else {
		wm_window_close(C, CTX_wm_manager(C), win);
		CTX_wm_window_set(C, win_prev);
		return NULL;
	}
}

/**
 * Uses `screen->temp` tag to define what to do, currently it limits
 * to only one "temp" window for render out, preferences, filewindow, etc...
 *
 * \param type: WM_WINDOW_RENDER, WM_WINDOW_USERPREFS...
 * \return the window or NULL.
 */
wmWindow *WM_window_open_temp(bContext *C, int x, int y, int sizex, int sizey, int type)
{
	Main *bmain = CTX_data_main(C);
	wmWindow *win_prev = CTX_wm_window(C);
	wmWindow *win;
	bScreen *screen;
	ScrArea *sa;
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	const char *title;

	/* convert to native OS window coordinates */
	const float native_pixel_size = GHOST_GetNativePixelSize(win_prev->ghostwin);
	x /= native_pixel_size;
	y /= native_pixel_size;
	sizex /= native_pixel_size;
	sizey /= native_pixel_size;

	/* calculate postition */
	rcti rect;
	rect.xmin = x + win_prev->posx - sizex / 2;
	rect.ymin = y + win_prev->posy - sizey / 2;
	rect.xmax = rect.xmin + sizex;
	rect.ymax = rect.ymin + sizey;

	/* changes rect to fit within desktop */
	wm_window_check_position(&rect);

	/* test if we have a temp screen already */
	for (win = CTX_wm_manager(C)->windows.first; win; win = win->next)
		if (WM_window_is_temp_screen(win))
			break;

	/* add new window? */
	if (win == NULL) {
		win = wm_window_new(C, win_prev);

		win->posx = rect.xmin;
		win->posy = rect.ymin;
	}

	screen = WM_window_get_active_screen(win);

	win->sizex = BLI_rcti_size_x(&rect);
	win->sizey = BLI_rcti_size_y(&rect);

	if (win->ghostwin) {
		wm_window_set_size(win, win->sizex, win->sizey);
		wm_window_raise(win);
	}

	if (WM_window_get_active_workspace(win) == NULL) {
		WorkSpace *workspace = WM_window_get_active_workspace(win_prev);
		BKE_workspace_active_set(win->workspace_hook, workspace);
	}

	if (screen == NULL) {
		/* add new screen layout */
		WorkSpace *workspace = WM_window_get_active_workspace(win);
		WorkSpaceLayout *layout = ED_workspace_layout_add(bmain, workspace, win, "temp");

		screen = BKE_workspace_layout_screen_get(layout);
		WM_window_set_active_layout(win, workspace, layout);
	}

	/* Set scene and view layer to match original window. */
	STRNCPY(win->view_layer_name, view_layer->name);
	if (WM_window_get_active_scene(win) != scene) {
		ED_screen_scene_change(C, win, scene);
	}

	screen->temp = 1;

	/* make window active, and validate/resize */
	CTX_wm_window_set(C, win);
	WM_check(C);

	/* It's possible `win->ghostwin == NULL`.
	 * instead of attempting to cleanup here (in a half finished state),
	 * finish setting up the screen, then free it at the end of the function,
	 * to avoid having to take into account a partially-created window.
	 */

	/* ensure it shows the right spacetype editor */
	sa = screen->areabase.first;
	CTX_wm_area_set(C, sa);

	if (type == WM_WINDOW_RENDER) {
		ED_area_newspace(C, sa, SPACE_IMAGE, false);
	}
	else if (type == WM_WINDOW_DRIVERS) {
		ED_area_newspace(C, sa, SPACE_IPO, false);
	}
	else {
		ED_area_newspace(C, sa, SPACE_USERPREF, false);
	}

	ED_screen_change(C, screen);
	ED_screen_refresh(CTX_wm_manager(C), win); /* test scale */

	/* do additional setup for specific editor type */
	if (type == WM_WINDOW_DRIVERS) {
		ED_drivers_editor_init(C, sa);
	}

	if (sa->spacetype == SPACE_IMAGE)
		title = IFACE_("Blender Render");
	else if (ELEM(sa->spacetype, SPACE_OUTLINER, SPACE_USERPREF))
		title = IFACE_("Blender User Preferences");
	else if (sa->spacetype == SPACE_FILE)
		title = IFACE_("Blender File View");
	else if (sa->spacetype == SPACE_IPO)
		title = IFACE_("Blender Drivers Editor");
	else
		title = "Blender";

	if (win->ghostwin) {
		GHOST_SetTitle(win->ghostwin, title);
		return win;
	}
	else {
		/* very unlikely! but opening a new window can fail */
		wm_window_close(C, CTX_wm_manager(C), win);
		CTX_wm_window_set(C, win_prev);

		return NULL;
	}
}


/* ****************** Operators ****************** */

int wm_window_close_exec(bContext *C, wmOperator *UNUSED(op))
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	wm_window_close(C, wm, win);
	return OPERATOR_FINISHED;
}

int wm_window_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	wmWindow *win_src = CTX_wm_window(C);
	bool ok;

	ok = (wm_window_copy_test(C, win_src, true, true) != NULL);

	return ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

int wm_window_new_main_exec(bContext *C, wmOperator *UNUSED(op))
{
	wmWindow *win_src = CTX_wm_window(C);
	bool ok;

	ok = (wm_window_copy_test(C, win_src, true, false) != NULL);

	return ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
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

void wm_cursor_position_from_ghost(wmWindow *win, int *x, int *y)
{
	float fac = GHOST_GetNativePixelSize(win->ghostwin);

	GHOST_ScreenToClient(win->ghostwin, *x, *y, x, y);
	*x *= fac;

	*y = (win->sizey - 1) - *y;
	*y *= fac;
}

void wm_cursor_position_to_ghost(wmWindow *win, int *x, int *y)
{
	float fac = GHOST_GetNativePixelSize(win->ghostwin);

	*x /= fac;
	*y /= fac;
	*y = win->sizey - *y - 1;

	GHOST_ClientToScreen(win->ghostwin, *x, *y, x, y);
}

void wm_get_cursor_position(wmWindow *win, int *x, int *y)
{
	GHOST_GetCursorPosition(g_system, x, y);
	wm_cursor_position_from_ghost(win, x, y);
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

static void wm_window_set_drawable(wmWindowManager *wm, wmWindow *win, bool activate)
{
	BLI_assert(ELEM(wm->windrawable, NULL, win));

	wm->windrawable = win;
	if (activate) {
		GHOST_ActivateWindowDrawingContext(win->ghostwin);
	}
	GWN_context_active_set(win->gwnctx);
	immActivate();
}

static void wm_window_clear_drawable(wmWindowManager *wm)
{
	if (wm->windrawable) {
		BLF_batch_reset();
		gpu_batch_presets_reset();
		immDeactivate();
		wm->windrawable = NULL;
	}
}

void wm_window_make_drawable(wmWindowManager *wm, wmWindow *win)
{
	BLI_assert(GPU_framebuffer_current_get() == 0);

	if (win != wm->windrawable && win->ghostwin) {
//		win->lmbut = 0;	/* keeps hanging when mousepressed while other window opened */
		wm_window_clear_drawable(wm);

		if (G.debug & G_DEBUG_EVENTS) {
			printf("%s: set drawable %d\n", __func__, win->winid);
		}

		wm_window_set_drawable(wm, win, true);

		/* this can change per window */
		WM_window_set_dpi(win);
	}
}

/* Reset active the current window opengl drawing context. */
void wm_window_reset_drawable(void)
{
	BLI_assert(BLI_thread_is_main());
	BLI_assert(GPU_framebuffer_current_get() == 0);
	wmWindowManager *wm = G_MAIN->wm.first;

	if (wm == NULL)
		return;

	wmWindow *win = wm->windrawable;

	if (win && win->ghostwin) {
		wm_window_clear_drawable(wm);
		wm_window_set_drawable(wm, win, true);
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
		if ((wm->initialized & WM_WINDOW_IS_INITIALIZED) == 0) {
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
				const int keymodifier = ((query_qual(SHIFT)     ? KM_SHIFT : 0) |
				                         (query_qual(CONTROL)   ? KM_CTRL  : 0) |
				                         (query_qual(ALT)       ? KM_ALT   : 0) |
				                         (query_qual(OS)        ? KM_OSKEY : 0));

				/* Win23/GHOST modifier bug, see T40317 */
#ifndef WIN32
//#  define USE_WIN_ACTIVATE
#endif

				wm->winactive = win; /* no context change! c->wm->windrawable is drawable, or for area queues */

				win->active = 1;
//				window_handle(win, INPUTCHANGE, win->active);

				/* bad ghost support for modifier keys... so on activate we set the modifiers again */

				/* TODO: This is not correct since a modifier may be held when a window is activated...
				 * better solve this at ghost level. attempted fix r54450 but it caused bug [#34255]
				 *
				 * For now don't send GHOST_kEventKeyDown events, just set the 'eventstate'.
				 */
				kdata.ascii = '\0';
				kdata.utf8_buf[0] = '\0';

				if (win->eventstate->shift) {
					if ((keymodifier & KM_SHIFT) == 0) {
						kdata.key = GHOST_kKeyLeftShift;
						wm_event_add_ghostevent(wm, win, GHOST_kEventKeyUp, time, &kdata);
					}
				}
#ifdef USE_WIN_ACTIVATE
				else {
					if (keymodifier & KM_SHIFT) {
						win->eventstate->shift = KM_MOD_FIRST;
					}
				}
#endif
				if (win->eventstate->ctrl) {
					if ((keymodifier & KM_CTRL) == 0) {
						kdata.key = GHOST_kKeyLeftControl;
						wm_event_add_ghostevent(wm, win, GHOST_kEventKeyUp, time, &kdata);
					}
				}
#ifdef USE_WIN_ACTIVATE
				else {
					if (keymodifier & KM_CTRL) {
						win->eventstate->ctrl = KM_MOD_FIRST;
					}
				}
#endif
				if (win->eventstate->alt) {
					if ((keymodifier & KM_ALT) == 0) {
						kdata.key = GHOST_kKeyLeftAlt;
						wm_event_add_ghostevent(wm, win, GHOST_kEventKeyUp, time, &kdata);
					}
				}
#ifdef USE_WIN_ACTIVATE
				else {
					if (keymodifier & KM_ALT) {
						win->eventstate->alt = KM_MOD_FIRST;
					}
				}
#endif
				if (win->eventstate->oskey) {
					if ((keymodifier & KM_OSKEY) == 0) {
						kdata.key = GHOST_kKeyOS;
						wm_event_add_ghostevent(wm, win, GHOST_kEventKeyUp, time, &kdata);
					}
				}
#ifdef USE_WIN_ACTIVATE
				else {
					if (keymodifier & KM_OSKEY) {
						win->eventstate->oskey = KM_MOD_FIRST;
					}
				}
#endif

#undef USE_WIN_ACTIVATE


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

				WM_window_set_dpi(win);

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
						const bScreen *screen = WM_window_get_active_screen(win);

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
						BKE_icon_changed(screen->id.icon_id);
						WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
						WM_event_add_notifier(C, NC_WINDOW | NA_EDITED, NULL);

#if defined(__APPLE__) || defined(WIN32)
						/* OSX and Win32 don't return to the mainloop while resize */
						wm_event_do_notifiers(C);
						wm_draw_update(C);

						/* Warning! code above nulls 'C->wm.window', causing BGE to quit, see: T45699.
						 * Further, its easier to match behavior across platforms, so restore the window. */
						CTX_wm_window_set(C, win);
#endif
					}
				}
				break;
			}

			case GHOST_kEventWindowDPIHintChanged:
			{
				WM_window_set_dpi(win);
				/* font's are stored at each DPI level, without this we can easy load 100's of fonts */
				BLF_cache_clear();

				WM_main_add_notifier(NC_WINDOW, NULL);      /* full redraw */
				WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);    /* refresh region sizes */
				break;
			}

			case GHOST_kEventOpenMainFile:
			{
				PointerRNA props_ptr;
				wmWindow *oldWindow;
				const char *path = GHOST_GetEventData(evt);

				if (path) {
					wmOperatorType *ot = WM_operatortype_find("WM_OT_open_mainfile", false);
					/* operator needs a valid window in context, ensures
					 * it is correctly set */
					oldWindow = CTX_wm_window(C);
					CTX_wm_window_set(C, win);

					WM_operator_properties_create_ptr(&props_ptr, ot);
					RNA_string_set(&props_ptr, "filepath", path);
					WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &props_ptr);
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
				event.custom = EVT_DATA_DRAGDROP;
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

						WM_event_start_drag(C, icon, WM_DRAG_PATH, stra->strings[a], 0.0, WM_DRAG_NOP);
						/* void poin should point to string, it makes a copy */
						break; /* only one drop element supported now */
					}
				}

				break;
			}
			case GHOST_kEventNativeResolutionChange:
			{
				// only update if the actual pixel size changes
				float prev_pixelsize = U.pixelsize;
				WM_window_set_dpi(win);

				if (U.pixelsize != prev_pixelsize) {
					BKE_icon_changed(WM_window_get_active_screen(win)->id.icon_id);

					// close all popups since they are positioned with the pixel
					// size baked in and it's difficult to correct them
					wmWindow *oldWindow = CTX_wm_window(C);
					CTX_wm_window_set(C, win);
					UI_popup_handlers_remove_all(C, &win->modalhandlers);
					CTX_wm_window_set(C, oldWindow);

					wm_window_make_drawable(wm, win);

					WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
					WM_event_add_notifier(C, NC_WINDOW | NA_EDITED, NULL);
				}

				break;
			}
			case GHOST_kEventTrackpad:
			{
				GHOST_TEventTrackpadData *pd = data;

				wm_cursor_position_from_ghost(win, &pd->x, &pd->y);
				wm_event_add_ghostevent(wm, win, type, time, data);
				break;
			}
			case GHOST_kEventCursorMove:
			{
				GHOST_TEventCursorData *cd = data;

				wm_cursor_position_from_ghost(win, &cd->x, &cd->y);
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


/**
 * This timer system only gives maximum 1 timer event per redraw cycle,
 * to prevent queues to get overloaded.
 * Timer handlers should check for delta to decide if they just update, or follow real time.
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
				else if (wt->event_type == TIMERNOTIFIER)
					WM_main_add_notifier(GET_UINT_FROM_POINTER(wt->customdata), NULL);
				else if (win) {
					wmEvent event;
					wm_event_init_from_window(win, &event);

					event.type = wt->event_type;
					event.val = KM_NOTHING;
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

/* bContext can be null in background mode because we don't
 * need to event handling. */
void wm_ghost_init(bContext *C)
{
	if (!g_system) {
		GHOST_EventConsumerHandle consumer;

		if (C != NULL) {
			consumer = GHOST_CreateEventConsumer(ghost_event_proc, C);
		}

		g_system = GHOST_CreateSystem();

		if (C != NULL) {
			GHOST_AddEventConsumer(g_system, consumer);
		}

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

wmTimer *WM_event_add_timer_notifier(wmWindowManager *wm, wmWindow *win, unsigned int type, double timestep)
{
	wmTimer *wt = MEM_callocN(sizeof(wmTimer), "window timer");

	wt->event_type = TIMERNOTIFIER;
	wt->ltime = PIL_check_seconds_timer();
	wt->ntime = wt->ltime + timestep;
	wt->stime = wt->ltime;
	wt->timestep = timestep;
	wt->win = win;
	wt->customdata = SET_UINT_IN_POINTER(type);
	wt->flags |= WM_TIMER_NO_FREE_CUSTOM_DATA;

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
		if (wt->customdata != NULL && (wt->flags & WM_TIMER_NO_FREE_CUSTOM_DATA) == 0) {
			MEM_freeN(wt->customdata);
		}
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

void WM_event_remove_timer_notifier(wmWindowManager *wm, wmWindow *win, wmTimer *timer)
{
	timer->customdata = NULL;
	WM_event_remove_timer(wm, win, timer);
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

void wm_window_get_position(wmWindow *win, int *r_pos_x, int *r_pos_y)
{
	*r_pos_x = win->posx;
	*r_pos_y = win->posy;
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
	GPU_texture_orphans_delete(); /* XXX should be done elsewhere. */
	GPU_material_orphans_delete(); /* XXX Amen to that. */
	GHOST_SwapWindowBuffers(win->ghostwin);
}

void wm_window_set_swap_interval (wmWindow *win, int interval)
{
	GHOST_SetSwapInterval(win->ghostwin, interval);
}

bool wm_window_get_swap_interval(wmWindow *win, int *intervalOut)
{
	return GHOST_GetSwapInterval(win->ghostwin, intervalOut);
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
		int oldx = x, oldy = y;

		wm_cursor_position_to_ghost(win, &x, &y);
		GHOST_SetCursorPosition(g_system, x, y);

		win->eventstate->prevx = oldx;
		win->eventstate->prevy = oldy;

		win->eventstate->x = oldx;
		win->eventstate->y = oldy;
	}
}

/**
 * Set x, y to values we can actually position the cursor to.
 */
void WM_cursor_compatible_xy(wmWindow *win, int *x, int *y)
{
	float f = GHOST_GetNativePixelSize(win->ghostwin);
	if (f != 1.0f) {
		*x = (int)(*x / f) * f;
		*y = (int)(*y / f) * f;
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
int WM_window_pixels_x(const wmWindow *win)
{
	float f = GHOST_GetNativePixelSize(win->ghostwin);

	return (int)(f * (float)win->sizex);
}
int WM_window_pixels_y(const wmWindow *win)
{
	float f = GHOST_GetNativePixelSize(win->ghostwin);

	return (int)(f * (float)win->sizey);
}

/**
 * Get boundaries usable by all window contents, including global areas.
 */
void WM_window_rect_calc(const wmWindow *win, rcti *r_rect)
{
	BLI_rcti_init(r_rect, 0, WM_window_pixels_x(win), 0, WM_window_pixels_y(win));
}
/**
 * Get boundaries usable by screen-layouts, excluding global areas.
 * \note Depends on U.dpi_fac. Should that be outdated, call #WM_window_set_dpi first.
 */
void WM_window_screen_rect_calc(const wmWindow *win, rcti *r_rect)
{
	rcti window_rect, screen_rect;

	WM_window_rect_calc(win, &window_rect);
	screen_rect = window_rect;

	/* Substract global areas from screen rectangle. */
	for (ScrArea *global_area = win->global_areas.areabase.first; global_area; global_area = global_area->next) {
		int height = ED_area_global_size_y(global_area) - 1;

		if (global_area->global->flag & GLOBAL_AREA_IS_HIDDEN) {
			continue;
		}

		switch (global_area->global->align) {
			case GLOBAL_AREA_ALIGN_TOP:
				if ((screen_rect.ymax - height) > window_rect.ymin) {
					height += U.pixelsize;
				}
				if (screen_rect.ymax < (window_rect.ymax - 1)) {
					height += U.pixelsize;
				}
				screen_rect.ymax -= height;
				break;
			case GLOBAL_AREA_ALIGN_BOTTOM:
				if (screen_rect.ymin > window_rect.ymin) {
					height += U.pixelsize;
				}
				if ((screen_rect.ymin + height) < (window_rect.ymax - 1)) {
					height += U.pixelsize;
				}
				screen_rect.ymin += height;
				break;
			default:
				BLI_assert(0);
				break;
		}
	}

	BLI_assert(screen_rect.xmin < screen_rect.xmax);
	BLI_assert(screen_rect.ymin < screen_rect.ymax);
	*r_rect = screen_rect;
}

bool WM_window_is_fullscreen(wmWindow *win)
{
	return win->windowstate == GHOST_kWindowStateFullScreen;
}

/**
 * Some editor data may need to be synced with scene data (3D View camera and layers).
 * This function ensures data is synced for editors in visible workspaces and their visible layouts.
 */
void WM_windows_scene_data_sync(const ListBase *win_lb, Scene *scene)
{
	for (wmWindow *win = win_lb->first; win; win = win->next) {
		if (WM_window_get_active_scene(win) == scene) {
			ED_workspace_scene_data_sync(win->workspace_hook, scene);
		}
	}
}

Scene *WM_windows_scene_get_from_screen(const wmWindowManager *wm, const bScreen *screen)
{
	for (wmWindow *win = wm->windows.first; win; win = win->next) {
		if (WM_window_get_active_screen(win) == screen) {
			return WM_window_get_active_scene(win);
		}
	}

	return NULL;
}

WorkSpace *WM_windows_workspace_get_from_screen(const wmWindowManager *wm, const bScreen *screen)
{
	for (wmWindow *win = wm->windows.first; win; win = win->next) {
		if (WM_window_get_active_screen(win) == screen) {
			return WM_window_get_active_workspace(win);
		}
	}
	return NULL;
}

Scene *WM_window_get_active_scene(const wmWindow *win)
{
	return win->scene;
}

/**
 * \warning Only call outside of area/region loops
 */
void WM_window_set_active_scene(Main *bmain, bContext *C, wmWindow *win, Scene *scene)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win_parent = (win->parent) ? win->parent : win;
	bool changed = false;

	/* Set scene in parent and its child windows. */
	if (win_parent->scene != scene) {
		ED_screen_scene_change(C, win_parent, scene);
		changed = true;
	}

	for (wmWindow *win_child = wm->windows.first; win_child; win_child = win_child->next) {
		if (win_child->parent == win_parent && win_child->scene != scene) {
			ED_screen_scene_change(C, win_child, scene);
			changed = true;
		}
	}

	if (changed) {
		/* Update depsgraph and renderers for scene change. */
		ViewLayer *view_layer = WM_window_get_active_view_layer(win_parent);
		ED_scene_change_update(bmain, scene, view_layer);

		/* Complete redraw. */
		WM_event_add_notifier(C, NC_WINDOW, NULL);
	}
}

ViewLayer *WM_window_get_active_view_layer(const wmWindow *win)
{
	Scene *scene = WM_window_get_active_scene(win);
	if (scene == NULL) {
		return NULL;
	}

	ViewLayer *view_layer = BKE_view_layer_find(scene, win->view_layer_name);
	if (view_layer) {
		return view_layer;
	}

	return BKE_view_layer_default_view(scene);
}

void WM_window_set_active_view_layer(wmWindow *win, ViewLayer *view_layer)
{
	BLI_assert(BKE_view_layer_find(WM_window_get_active_scene(win), view_layer->name) != NULL);

	wmWindowManager *wm = G_MAIN->wm.first;
	wmWindow *win_parent = (win->parent) ? win->parent : win;

	/* Set  view layer in parent and child windows. */
	STRNCPY(win->view_layer_name, view_layer->name);

	for (wmWindow *win_child = wm->windows.first; win_child; win_child = win_child->next) {
		if (win_child->parent == win_parent) {
			STRNCPY(win_child->view_layer_name, view_layer->name);
		}
	}
}

void WM_window_ensure_active_view_layer(wmWindow *win)
{
	/* Update layer name is correct after scene changes, load without UI, etc. */
	Scene *scene = WM_window_get_active_scene(win);

	if (scene && BKE_view_layer_find(scene, win->view_layer_name) == NULL) {
		ViewLayer *view_layer = BKE_view_layer_default_view(scene);
		STRNCPY(win->view_layer_name, view_layer->name);
	}
}

WorkSpace *WM_window_get_active_workspace(const wmWindow *win)
{
	return BKE_workspace_active_get(win->workspace_hook);
}

void WM_window_set_active_workspace(bContext *C, wmWindow *win, WorkSpace *workspace)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win_parent = (win->parent) ? win->parent : win;

	ED_workspace_change(workspace, C, wm, win);

	for (wmWindow *win_child = wm->windows.first; win_child; win_child = win_child->next) {
		if (win_child->parent == win_parent) {
			ED_workspace_change(workspace, C, wm, win_child);
		}
	}
}

WorkSpaceLayout *WM_window_get_active_layout(const wmWindow *win)
{
	const WorkSpace *workspace = WM_window_get_active_workspace(win);
	return (LIKELY(workspace != NULL) ? BKE_workspace_active_layout_get(win->workspace_hook) : NULL);
}
void WM_window_set_active_layout(wmWindow *win, WorkSpace *workspace, WorkSpaceLayout *layout)
{
	BKE_workspace_hook_layout_for_workspace_set(win->workspace_hook, workspace, layout);
}

/**
 * Get the active screen of the active workspace in \a win.
 */
bScreen *WM_window_get_active_screen(const wmWindow *win)
{
	const WorkSpace *workspace = WM_window_get_active_workspace(win);
	/* May be NULL in rare cases like closing Blender */
	return (LIKELY(workspace != NULL) ? BKE_workspace_active_screen_get(win->workspace_hook) : NULL);
}
void WM_window_set_active_screen(wmWindow *win, WorkSpace *workspace, bScreen *screen)
{
	BKE_workspace_active_screen_set(win->workspace_hook, workspace, screen);
}

bool WM_window_is_temp_screen(const wmWindow *win)
{
	const bScreen *screen = WM_window_get_active_screen(win);
	return (screen && screen->temp != 0);
}


#ifdef WITH_INPUT_IME
/* note: keep in mind wm_window_IME_begin is also used to reposition the IME window */
void wm_window_IME_begin(wmWindow *win, int x, int y, int w, int h, bool complete)
{
	BLI_assert(win);

	GHOST_BeginIME(win->ghostwin, x, win->sizey - y, w, h, complete);
}

void wm_window_IME_end(wmWindow *win)
{
	BLI_assert(win && win->ime_data);

	GHOST_EndIME(win->ghostwin);
	win->ime_data = NULL;
}
#endif  /* WITH_INPUT_IME */

/* ****** direct opengl context management ****** */

void *WM_opengl_context_create(void)
{
	/* On Windows there is a problem creating contexts that share lists
	 * from one context that is current in another thread.
	 * So we should call this function only on the main thread.
	 */
	BLI_assert(BLI_thread_is_main());
	BLI_assert(GPU_framebuffer_current_get() == 0);
	return GHOST_CreateOpenGLContext(g_system);
}

void WM_opengl_context_dispose(void *context)
{
	BLI_assert(GPU_framebuffer_current_get() == 0);
	GHOST_DisposeOpenGLContext(g_system, (GHOST_ContextHandle)context);
}

void WM_opengl_context_activate(void *context)
{
	BLI_assert(GPU_framebuffer_current_get() == 0);
	GHOST_ActivateOpenGLContext((GHOST_ContextHandle)context);
}

void WM_opengl_context_release(void *context)
{
	BLI_assert(GPU_framebuffer_current_get() == 0);
	GHOST_ReleaseOpenGLContext((GHOST_ContextHandle)context);
}
