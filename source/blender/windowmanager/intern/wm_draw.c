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

/** \file blender/windowmanager/intern/wm_draw.c
 *  \ingroup wm
 *
 * Handle OpenGL buffers for windowing, also paint cursor.
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_listBase.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BIF_gl.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_main.h"

#include "GHOST_C-api.h"

#include "ED_node.h"
#include "ED_view3d.h"
#include "ED_screen.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_basic_shader.h"

#include "RE_engine.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_draw.h"
#include "wm_window.h"
#include "wm_event_system.h"

#ifdef WITH_OPENSUBDIV
#  include "BKE_subsurf.h"
#endif

/* swap */
#define WIN_NONE_OK     0
#define WIN_BACK_OK     1
#define WIN_FRONT_OK    2
#define WIN_BOTH_OK     3

/* ******************* drawing, overlays *************** */


static void wm_paintcursor_draw(bContext *C, ARegion *ar)
{
	wmWindowManager *wm = CTX_wm_manager(C);

	if (wm->paintcursors.first) {
		wmWindow *win = CTX_wm_window(C);
		bScreen *screen = win->screen;
		wmPaintCursor *pc;

		if (ar->swinid && screen->subwinactive == ar->swinid) {
			for (pc = wm->paintcursors.first; pc; pc = pc->next) {
				if (pc->poll == NULL || pc->poll(C)) {
					ARegion *ar_other = CTX_wm_region(C);
					if (ELEM(win->grabcursor, GHOST_kGrabWrap, GHOST_kGrabHide)) {
						int x = 0, y = 0;
						wm_get_cursor_position(win, &x, &y);
						pc->draw(C,
						         x - ar_other->winrct.xmin,
						         y - ar_other->winrct.ymin,
						         pc->customdata);
					}
					else {
						pc->draw(C,
						         win->eventstate->x - ar_other->winrct.xmin,
						         win->eventstate->y - ar_other->winrct.ymin,
						         pc->customdata);
					}
				}
			}
		}
	}
}

/* ********************* drawing, swap ****************** */

static void wm_area_mark_invalid_backbuf(ScrArea *sa)
{
	if (sa->spacetype == SPACE_VIEW3D)
		((View3D *)sa->spacedata.first)->flag |= V3D_INVALID_BACKBUF;
}

static bool wm_area_test_invalid_backbuf(ScrArea *sa)
{
	if (sa->spacetype == SPACE_VIEW3D)
		return (((View3D *)sa->spacedata.first)->flag & V3D_INVALID_BACKBUF) != 0;
	else
		return true;
}

static void wm_region_test_render_do_draw(const bScreen *screen, ScrArea *sa, ARegion *ar)
{
	/* tag region for redraw from render engine preview running inside of it */
	if (sa->spacetype == SPACE_VIEW3D) {
		RegionView3D *rv3d = ar->regiondata;
		RenderEngine *engine = (rv3d) ? rv3d->render_engine : NULL;

		if (engine && (engine->flag & RE_ENGINE_DO_DRAW)) {
			Scene *scene = screen->scene;
			View3D *v3d = sa->spacedata.first;
			rcti border_rect;

			/* do partial redraw when possible */
			if (ED_view3d_calc_render_border(scene, v3d, ar, &border_rect))
				ED_region_tag_redraw_partial(ar, &border_rect);
			else
				ED_region_tag_redraw(ar);

			engine->flag &= ~RE_ENGINE_DO_DRAW;
		}
	}
}

/********************** draw all **************************/
/* - reference method, draw all each time                 */

typedef struct WindowDrawCB {
	struct WindowDrawCB *next, *prev;

	void(*draw)(const struct wmWindow *, void *);
	void *customdata;

} WindowDrawCB;

void *WM_draw_cb_activate(
        wmWindow *win,
        void(*draw)(const struct wmWindow *, void *),
        void *customdata)
{
	WindowDrawCB *wdc = MEM_callocN(sizeof(*wdc), "WindowDrawCB");

	BLI_addtail(&win->drawcalls, wdc);
	wdc->draw = draw;
	wdc->customdata = customdata;

	return wdc;
}

void WM_draw_cb_exit(wmWindow *win, void *handle)
{
	for (WindowDrawCB *wdc = win->drawcalls.first; wdc; wdc = wdc->next) {
		if (wdc == (WindowDrawCB *)handle) {
			BLI_remlink(&win->drawcalls, wdc);
			MEM_freeN(wdc);
			return;
		}
	}
}

static void wm_draw_callbacks(wmWindow *win)
{
	for (WindowDrawCB *wdc = win->drawcalls.first; wdc; wdc = wdc->next) {
		wdc->draw(win, wdc->customdata);
	}
}

static void wm_method_draw_full(bContext *C, wmWindow *win)
{
	bScreen *screen = win->screen;
	ScrArea *sa;
	ARegion *ar;

	/* draw area regions */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid) {
				CTX_wm_region_set(C, ar);
				ED_region_do_draw(C, ar);
				ar->do_draw = false;
				wm_paintcursor_draw(C, ar);
				CTX_wm_region_set(C, NULL);
			}
		}

		wm_area_mark_invalid_backbuf(sa);
		CTX_wm_area_set(C, NULL);
	}

	ED_screen_draw_edges(win);
	screen->do_draw = false;
	wm_draw_callbacks(win);

	/* draw overlapping regions */
	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (ar->swinid) {
			CTX_wm_menu_set(C, ar);
			ED_region_do_draw(C, ar);
			ar->do_draw = false;
			CTX_wm_menu_set(C, NULL);
		}
	}

	if (screen->do_draw_gesture)
		wm_gesture_draw(win);
}

/****************** draw overlap all **********************/
/* - redraw marked areas, and anything that overlaps it   */
/* - it also handles swap exchange optionally, assuming   */
/*   that on swap no clearing happens and we get back the */
/*   same buffer as we swapped to the front               */

/* mark area-regions to redraw if overlapped with rect */
static void wm_flush_regions_down(bScreen *screen, rcti *dirty)
{
	ScrArea *sa;
	ARegion *ar;

	for (sa = screen->areabase.first; sa; sa = sa->next) {
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (BLI_rcti_isect(dirty, &ar->winrct, NULL)) {
				ar->do_draw = RGN_DRAW;
				memset(&ar->drawrct, 0, sizeof(ar->drawrct));
				ar->swap = WIN_NONE_OK;
			}
		}
	}
}

/* mark menu-regions to redraw if overlapped with rect */
static void wm_flush_regions_up(bScreen *screen, rcti *dirty)
{
	ARegion *ar;

	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (BLI_rcti_isect(dirty, &ar->winrct, NULL)) {
			ar->do_draw = RGN_DRAW;
			memset(&ar->drawrct, 0, sizeof(ar->drawrct));
			ar->swap = WIN_NONE_OK;
		}
	}
}

static void wm_method_draw_overlap_all(bContext *C, wmWindow *win, int exchange)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	bScreen *screen = win->screen;
	ScrArea *sa;
	ARegion *ar;
	static rcti rect = {0, 0, 0, 0};

	/* after backbuffer selection draw, we need to redraw */
	for (sa = screen->areabase.first; sa; sa = sa->next)
		for (ar = sa->regionbase.first; ar; ar = ar->next)
			if (ar->swinid && !wm_area_test_invalid_backbuf(sa))
				ED_region_tag_redraw(ar);

	/* flush overlapping regions */
	if (screen->regionbase.first) {
		/* flush redraws of area regions up to overlapping regions */
		for (sa = screen->areabase.first; sa; sa = sa->next)
			for (ar = sa->regionbase.first; ar; ar = ar->next)
				if (ar->swinid && ar->do_draw)
					wm_flush_regions_up(screen, &ar->winrct);

		/* flush between overlapping regions */
		for (ar = screen->regionbase.last; ar; ar = ar->prev)
			if (ar->swinid && ar->do_draw)
				wm_flush_regions_up(screen, &ar->winrct);

		/* flush redraws of overlapping regions down to area regions */
		for (ar = screen->regionbase.last; ar; ar = ar->prev)
			if (ar->swinid && ar->do_draw)
				wm_flush_regions_down(screen, &ar->winrct);
	}

	/* flush drag item */
	if (rect.xmin != rect.xmax) {
		wm_flush_regions_down(screen, &rect);
		rect.xmin = rect.xmax = 0;
	}
	if (wm->drags.first) {
		/* doesnt draw, fills rect with boundbox */
		wm_drags_draw(C, win, &rect);
	}

	/* draw marked area regions */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid) {
				if (ar->do_draw) {
					CTX_wm_region_set(C, ar);
					ED_region_do_draw(C, ar);
					ar->do_draw = false;
					wm_paintcursor_draw(C, ar);
					CTX_wm_region_set(C, NULL);

					if (exchange)
						ar->swap = WIN_FRONT_OK;
				}
				else if (exchange) {
					if (ar->swap == WIN_FRONT_OK) {
						CTX_wm_region_set(C, ar);
						ED_region_do_draw(C, ar);
						ar->do_draw = false;
						wm_paintcursor_draw(C, ar);
						CTX_wm_region_set(C, NULL);

						ar->swap = WIN_BOTH_OK;
					}
					else if (ar->swap == WIN_BACK_OK)
						ar->swap = WIN_FRONT_OK;
					else if (ar->swap == WIN_BOTH_OK)
						ar->swap = WIN_BOTH_OK;
				}
			}
		}

		wm_area_mark_invalid_backbuf(sa);
		CTX_wm_area_set(C, NULL);
	}

	/* after area regions so we can do area 'overlay' drawing */
	if (screen->do_draw) {
		ED_screen_draw_edges(win);
		screen->do_draw = false;
		wm_draw_callbacks(win);

		if (exchange)
			screen->swap = WIN_FRONT_OK;
	}
	else if (exchange) {
		if (screen->swap == WIN_FRONT_OK) {
			ED_screen_draw_edges(win);
			screen->do_draw = false;
			screen->swap = WIN_BOTH_OK;
			wm_draw_callbacks(win);
		}
		else if (screen->swap == WIN_BACK_OK)
			screen->swap = WIN_FRONT_OK;
		else if (screen->swap == WIN_BOTH_OK)
			screen->swap = WIN_BOTH_OK;
	}

	/* draw marked overlapping regions */
	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (ar->swinid && ar->do_draw) {
			CTX_wm_menu_set(C, ar);
			ED_region_do_draw(C, ar);
			ar->do_draw = false;
			CTX_wm_menu_set(C, NULL);
		}
	}

	if (screen->do_draw_gesture)
		wm_gesture_draw(win);

	/* needs pixel coords in screen */
	if (wm->drags.first) {
		wm_drags_draw(C, win, NULL);
	}
}

/****************** draw triple buffer ********************/
/* - area regions are written into a texture, without any */
/*   of the overlapping menus, brushes, gestures. these   */
/*   are redrawn each time.                               */

static void wm_draw_triple_free(wmDrawTriple *triple)
{
	if (triple) {
		glDeleteTextures(1, &triple->bind);
		MEM_freeN(triple);
	}
}

static void wm_draw_triple_fail(bContext *C, wmWindow *win)
{
	wm_draw_window_clear(win);

	win->drawfail = 1;
	wm_method_draw_overlap_all(C, win, 0);
}

static int wm_triple_gen_textures(wmWindow *win, wmDrawTriple *triple)
{
	const int winsize_x = WM_window_pixels_x(win);
	const int winsize_y = WM_window_pixels_y(win);

	GLint maxsize;

	/* compute texture sizes */
	if (GLEW_ARB_texture_rectangle || GLEW_NV_texture_rectangle || GLEW_EXT_texture_rectangle) {
		triple->target = GL_TEXTURE_RECTANGLE_ARB;
	}
	else {
		triple->target = GL_TEXTURE_2D;
	}

	triple->x = winsize_x;
	triple->y = winsize_y;

	/* generate texture names */
	glGenTextures(1, &triple->bind);

	if (!triple->bind) {
		/* not the typical failure case but we handle it anyway */
		printf("WM: failed to allocate texture for triple buffer drawing (glGenTextures).\n");
		return 0;
	}

	/* proxy texture is only guaranteed to test for the cases that
	 * there is only one texture in use, which may not be the case */
	maxsize = GPU_max_texture_size();

	if (triple->x > maxsize || triple->y > maxsize) {
		glBindTexture(triple->target, 0);
		printf("WM: failed to allocate texture for triple buffer drawing "
			   "(texture too large for graphics card).\n");
		return 0;
	}

	/* setup actual texture */
	glBindTexture(triple->target, triple->bind);
	glTexImage2D(triple->target, 0, GL_RGB8, triple->x, triple->y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(triple->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(triple->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(triple->target, 0);

	/* not sure if this works everywhere .. */
	if (glGetError() == GL_OUT_OF_MEMORY) {
		printf("WM: failed to allocate texture for triple buffer drawing (out of memory).\n");
		return 0;
	}

	return 1;
}

void wm_triple_draw_textures(wmWindow *win, wmDrawTriple *triple, float alpha, bool is_interlace)
{
	const int sizex = WM_window_pixels_x(win);
	const int sizey = WM_window_pixels_y(win);

	float halfx, halfy, ratiox, ratioy;

	/* wmOrtho for the screen has this same offset */
	ratiox = sizex;
	ratioy = sizey;
	halfx = GLA_PIXEL_OFS;
	halfy = GLA_PIXEL_OFS;

	/* texture rectangle has unnormalized coordinates */
	if (triple->target == GL_TEXTURE_2D) {
		ratiox /= triple->x;
		ratioy /= triple->y;
		halfx /= triple->x;
		halfy /= triple->y;
	}

	/* interlace stereo buffer bind the shader before calling wm_triple_draw_textures */
	if (is_interlace) {
		glEnable(triple->target);
	}
	else {
		GPU_basic_shader_bind((triple->target == GL_TEXTURE_2D) ? GPU_SHADER_TEXTURE_2D : GPU_SHADER_TEXTURE_RECT);
	}

	glBindTexture(triple->target, triple->bind);

	glColor4f(1.0f, 1.0f, 1.0f, alpha);
	glBegin(GL_QUADS);
	glTexCoord2f(halfx, halfy);
	glVertex2f(0, 0);

	glTexCoord2f(ratiox + halfx, halfy);
	glVertex2f(sizex, 0);

	glTexCoord2f(ratiox + halfx, ratioy + halfy);
	glVertex2f(sizex, sizey);

	glTexCoord2f(halfx, ratioy + halfy);
	glVertex2f(0, sizey);
	glEnd();

	glBindTexture(triple->target, 0);

	if (is_interlace) {
		glDisable(triple->target);
	}
	else {
		GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
	}
}

static void wm_triple_copy_textures(wmWindow *win, wmDrawTriple *triple)
{
	const int sizex = WM_window_pixels_x(win);
	const int sizey = WM_window_pixels_y(win);

	glBindTexture(triple->target, triple->bind);
	glCopyTexSubImage2D(triple->target, 0, 0, 0, 0, 0, sizex, sizey);

	glBindTexture(triple->target, 0);
}

static void wm_draw_region_blend(wmWindow *win, ARegion *ar, wmDrawTriple *triple)
{
	float fac = ED_region_blend_factor(ar);

	/* region blend always is 1, except when blend timer is running */
	if (fac < 1.0f) {
		wmSubWindowScissorSet(win, win->screen->mainwin, &ar->winrct, true);

		glEnable(GL_BLEND);
		wm_triple_draw_textures(win, triple, 1.0f - fac, false);
		glDisable(GL_BLEND);
	}
}

static void wm_method_draw_triple(bContext *C, wmWindow *win)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmDrawData *dd, *dd_next, *drawdata = win->drawdata.first;
	bScreen *screen = win->screen;
	ScrArea *sa;
	ARegion *ar;
	int copytex = false;

	if (drawdata && drawdata->triple) {
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		wmSubWindowSet(win, screen->mainwin);

		wm_triple_draw_textures(win, drawdata->triple, 1.0f, false);
	}
	else {
		/* we run it when we start OR when we turn stereo on */
		if (drawdata == NULL) {
			drawdata = MEM_callocN(sizeof(wmDrawData), "wmDrawData");
			BLI_addhead(&win->drawdata, drawdata);
		}

		drawdata->triple = MEM_callocN(sizeof(wmDrawTriple), "wmDrawTriple");

		if (!wm_triple_gen_textures(win, drawdata->triple)) {
			wm_draw_triple_fail(C, win);
			return;
		}
	}

	/* it means stereo was just turned off */
	/* note: we are removing all drawdatas that are not the first */
	for (dd = drawdata->next; dd; dd = dd_next) {
		dd_next = dd->next;

		BLI_remlink(&win->drawdata, dd);
		wm_draw_triple_free(dd->triple);
		MEM_freeN(dd);
	}

	wmDrawTriple *triple = drawdata->triple;

	/* draw marked area regions */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid && ar->do_draw) {
				if (ar->overlap == false) {
					CTX_wm_region_set(C, ar);
					ED_region_do_draw(C, ar);
					ar->do_draw = false;
					CTX_wm_region_set(C, NULL);
					copytex = true;
				}
			}
		}

		wm_area_mark_invalid_backbuf(sa);
		CTX_wm_area_set(C, NULL);
	}

	if (copytex) {
		wmSubWindowSet(win, screen->mainwin);

		wm_triple_copy_textures(win, triple);
	}

	if (wm->paintcursors.first) {
		for (sa = screen->areabase.first; sa; sa = sa->next) {
			for (ar = sa->regionbase.first; ar; ar = ar->next) {
				if (ar->swinid && ar->swinid == screen->subwinactive) {
					CTX_wm_area_set(C, sa);
					CTX_wm_region_set(C, ar);

					/* make region ready for draw, scissor, pixelspace */
					ED_region_set(C, ar);
					wm_paintcursor_draw(C, ar);

					CTX_wm_region_set(C, NULL);
					CTX_wm_area_set(C, NULL);
				}
			}
		}

		wmSubWindowSet(win, screen->mainwin);
	}

	/* draw overlapping area regions (always like popups) */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid && ar->overlap) {
				CTX_wm_region_set(C, ar);
				ED_region_do_draw(C, ar);
				ar->do_draw = false;
				CTX_wm_region_set(C, NULL);

				wm_draw_region_blend(win, ar, triple);
			}
		}

		CTX_wm_area_set(C, NULL);
	}

	/* after area regions so we can do area 'overlay' drawing */
	ED_screen_draw_edges(win);
	win->screen->do_draw = false;
	wm_draw_callbacks(win);

	/* draw floating regions (menus) */
	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (ar->swinid) {
			CTX_wm_menu_set(C, ar);
			ED_region_do_draw(C, ar);
			ar->do_draw = false;
			CTX_wm_menu_set(C, NULL);
		}
	}

	/* always draw, not only when screen tagged */
	if (win->gesture.first)
		wm_gesture_draw(win);

	/* needs pixel coords in screen */
	if (wm->drags.first) {
		wm_drags_draw(C, win, NULL);
	}
}

static void wm_method_draw_triple_multiview(bContext *C, wmWindow *win, eStereoViews sview)
{
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmDrawData *drawdata;
	wmDrawTriple *triple_data, *triple_all;
	bScreen *screen = win->screen;
	ScrArea *sa;
	ARegion *ar;
	int copytex = false;
	int id;

	/* we store the triple_data in sequence to triple_all */
	for (id = 0; id < 2; id++) {
		drawdata = BLI_findlink(&win->drawdata, (sview * 2) + id);

		if (drawdata && drawdata->triple) {
			if (id == 0) {
				glClearColor(0, 0, 0, 0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				wmSubWindowSet(win, screen->mainwin);

				wm_triple_draw_textures(win, drawdata->triple, 1.0f, false);
			}
		}
		else {
			/* we run it when we start OR when we turn stereo on */
			if (drawdata == NULL) {
				drawdata = MEM_callocN(sizeof(wmDrawData), "wmDrawData");
				BLI_addtail(&win->drawdata, drawdata);
			}

			drawdata->triple = MEM_callocN(sizeof(wmDrawTriple), "wmDrawTriple");

			if (!wm_triple_gen_textures(win, drawdata->triple)) {
				wm_draw_triple_fail(C, win);
				return;
			}
		}
	}

	triple_data = ((wmDrawData *) BLI_findlink(&win->drawdata, sview * 2))->triple;
	triple_all  = ((wmDrawData *) BLI_findlink(&win->drawdata, (sview * 2) + 1))->triple;

	/* draw marked area regions */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		switch (sa->spacetype) {
			case SPACE_IMAGE:
			{
				SpaceImage *sima = sa->spacedata.first;
				sima->iuser.multiview_eye = sview;
				break;
			}
			case SPACE_VIEW3D:
			{
				View3D *v3d = sa->spacedata.first;
				BGpic *bgpic = v3d->bgpicbase.first;
				v3d->multiview_eye = sview;
				if (bgpic) bgpic->iuser.multiview_eye = sview;
				break;
			}
			case SPACE_NODE:
			{
				SpaceNode *snode = sa->spacedata.first;
				if ((snode->flag & SNODE_BACKDRAW) && ED_node_is_compositor(snode)) {
					Image *ima = BKE_image_verify_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
					ima->eye = sview;
				}
				break;
			}
			case SPACE_SEQ:
			{
				SpaceSeq *sseq = sa->spacedata.first;
				sseq->multiview_eye = sview;
				break;
			}
		}

		/* draw marked area regions */
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid && ar->do_draw) {

				if (ar->overlap == false) {
					CTX_wm_region_set(C, ar);
					ED_region_do_draw(C, ar);

					if (sview == STEREO_RIGHT_ID)
						ar->do_draw = false;

					CTX_wm_region_set(C, NULL);
					copytex = true;
				}
			}
		}

		wm_area_mark_invalid_backbuf(sa);
		CTX_wm_area_set(C, NULL);
	}

	if (copytex) {
		wmSubWindowSet(win, screen->mainwin);

		wm_triple_copy_textures(win, triple_data);
	}

	if (wm->paintcursors.first) {
		for (sa = screen->areabase.first; sa; sa = sa->next) {
			for (ar = sa->regionbase.first; ar; ar = ar->next) {
				if (ar->swinid && ar->swinid == screen->subwinactive) {
					CTX_wm_area_set(C, sa);
					CTX_wm_region_set(C, ar);

					/* make region ready for draw, scissor, pixelspace */
					ED_region_set(C, ar);
					wm_paintcursor_draw(C, ar);

					CTX_wm_region_set(C, NULL);
					CTX_wm_area_set(C, NULL);
				}
			}
		}

		wmSubWindowSet(win, screen->mainwin);
	}

	/* draw overlapping area regions (always like popups) */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid && ar->overlap) {
				CTX_wm_region_set(C, ar);
				ED_region_do_draw(C, ar);
				if (sview == STEREO_RIGHT_ID)
					ar->do_draw = false;
				CTX_wm_region_set(C, NULL);

				wm_draw_region_blend(win, ar, triple_data);
			}
		}

		CTX_wm_area_set(C, NULL);
	}

	/* after area regions so we can do area 'overlay' drawing */
	ED_screen_draw_edges(win);
	if (sview == STEREO_RIGHT_ID)
		win->screen->do_draw = false;
	wm_draw_callbacks(win);

	/* draw floating regions (menus) */
	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (ar->swinid) {
			CTX_wm_menu_set(C, ar);
			ED_region_do_draw(C, ar);
			if (sview == STEREO_RIGHT_ID)
				ar->do_draw = false;
			CTX_wm_menu_set(C, NULL);
		}
	}

	/* always draw, not only when screen tagged */
	if (win->gesture.first)
		wm_gesture_draw(win);

	/* needs pixel coords in screen */
	if (wm->drags.first) {
		wm_drags_draw(C, win, NULL);
	}

	/* copy the ui + overlays */
	wmSubWindowSet(win, screen->mainwin);
	wm_triple_copy_textures(win, triple_all);
}

/****************** main update call **********************/

/* quick test to prevent changing window drawable */
static bool wm_draw_update_test_window(wmWindow *win)
{
	const bScreen *screen = win->screen;
	ScrArea *sa;
	ARegion *ar;
	bool do_draw = false;

	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (ar->do_draw_overlay) {
			wm_tag_redraw_overlay(win, ar);
			ar->do_draw_overlay = false;
		}
		if (ar->swinid && ar->do_draw)
			do_draw = true;
	}

	for (sa = screen->areabase.first; sa; sa = sa->next) {
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			wm_region_test_render_do_draw(screen, sa, ar);

			if (ar->swinid && ar->do_draw)
				do_draw = true;
		}
	}

	if (do_draw)
		return true;

	if (screen->do_refresh)
		return true;
	if (screen->do_draw)
		return true;
	if (screen->do_draw_gesture)
		return true;
	if (screen->do_draw_paintcursor)
		return true;
	if (screen->do_draw_drag)
		return true;

	return false;
}

static int wm_automatic_draw_method(wmWindow *win)
{
	/* We assume all supported GPUs now support triple buffer well. */
	if (win->drawmethod == USER_DRAW_AUTOMATIC) {
		return USER_DRAW_TRIPLE;
	}
	else {
		return win->drawmethod;
	}
}

bool WM_is_draw_triple(wmWindow *win)
{
	/* function can get called before this variable is set in drawing code below */
	if (win->drawmethod != U.wmdrawmethod)
		win->drawmethod = U.wmdrawmethod;
	return (USER_DRAW_TRIPLE == wm_automatic_draw_method(win));
}

void wm_tag_redraw_overlay(wmWindow *win, ARegion *ar)
{
	/* for draw triple gestures, paint cursors don't need region redraw */
	if (ar && win) {
		if (wm_automatic_draw_method(win) != USER_DRAW_TRIPLE)
			ED_region_tag_redraw(ar);
		win->screen->do_draw_paintcursor = true;
	}
}

void WM_paint_cursor_tag_redraw(wmWindow *win, ARegion *ar)
{
	win->screen->do_draw_paintcursor = true;
	wm_tag_redraw_overlay(win, ar);
}

void wm_draw_update(bContext *C)
{
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win;

#ifdef WITH_OPENSUBDIV
	BKE_subsurf_free_unused_buffers();
#endif

	GPU_free_unused_buffers(bmain);

	for (win = wm->windows.first; win; win = win->next) {
#ifdef WIN32
		GHOST_TWindowState state = GHOST_GetWindowState(win->ghostwin);

		if (state == GHOST_kWindowStateMinimized) {
			/* do not update minimized windows, gives issues on Intel (see T33223)
			 * and AMD (see T50856). it seems logical to skip update for invisible
			 * window anyway.
			 */
			continue;
		}
#endif
		if (win->drawmethod != U.wmdrawmethod) {
			wm_draw_window_clear(win);
			win->drawmethod = U.wmdrawmethod;
		}

		if (wm_draw_update_test_window(win)) {
			bScreen *screen = win->screen;

			CTX_wm_window_set(C, win);

			/* sets context window+screen */
			wm_window_make_drawable(wm, win);

			/* notifiers for screen redraw */
			if (screen->do_refresh)
				ED_screen_refresh(wm, win);

			int drawmethod = wm_automatic_draw_method(win);

			if (win->drawfail)
				wm_method_draw_overlap_all(C, win, 0);
			else if (drawmethod == USER_DRAW_FULL)
				wm_method_draw_full(C, win);
			else if (drawmethod == USER_DRAW_OVERLAP)
				wm_method_draw_overlap_all(C, win, 0);
			else if (drawmethod == USER_DRAW_OVERLAP_FLIP)
				wm_method_draw_overlap_all(C, win, 1);
			else { /* USER_DRAW_TRIPLE */
				if ((WM_stereo3d_enabled(win, false)) == false) {
					wm_method_draw_triple(C, win);
				}
				else {
					wm_method_draw_triple_multiview(C, win, STEREO_LEFT_ID);
					wm_method_draw_triple_multiview(C, win, STEREO_RIGHT_ID);
					wm_method_draw_stereo3d(C, win);
				}
			}

			screen->do_draw_gesture = false;
			screen->do_draw_paintcursor = false;
			screen->do_draw_drag = false;

			wm_window_swap_buffers(win);

			CTX_wm_window_set(C, NULL);
		}
	}
}

void wm_draw_data_free(wmWindow *win)
{
	wmDrawData *dd;

	for (dd = win->drawdata.first; dd; dd = dd->next) {
		wm_draw_triple_free(dd->triple);
	}
	BLI_freelistN(&win->drawdata);
}

void wm_draw_window_clear(wmWindow *win)
{
	bScreen *screen = win->screen;
	ScrArea *sa;
	ARegion *ar;

	wm_draw_data_free(win);

	/* clear screen swap flags */
	if (screen) {
		for (sa = screen->areabase.first; sa; sa = sa->next)
			for (ar = sa->regionbase.first; ar; ar = ar->next)
				ar->swap = WIN_NONE_OK;

		screen->swap = WIN_NONE_OK;
	}
}

void wm_draw_region_clear(wmWindow *win, ARegion *ar)
{
	int drawmethod = wm_automatic_draw_method(win);

	if (ELEM(drawmethod, USER_DRAW_OVERLAP, USER_DRAW_OVERLAP_FLIP))
		wm_flush_regions_down(win->screen, &ar->winrct);

	win->screen->do_draw = true;
}

void WM_redraw_windows(bContext *C)
{
	wmWindow *win_prev = CTX_wm_window(C);
	ScrArea *area_prev = CTX_wm_area(C);
	ARegion *ar_prev = CTX_wm_region(C);

	wm_draw_update(C);

	CTX_wm_window_set(C, win_prev);
	CTX_wm_area_set(C, area_prev);
	CTX_wm_region_set(C, ar_prev);
}
