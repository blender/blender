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

#include <stdlib.h>
#include <string.h>
#include <GL/glew.h>

#include "DNA_listBase.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_window.h"
#include "wm_event_system.h"

/* swap */
#define WIN_NONE_OK		0
#define WIN_BACK_OK     1
#define WIN_FRONT_OK    2
#define WIN_BOTH_OK		3

/* draw method */
#define USER_DRAW_ALL			0
#define USER_DRAW_OVERLAP_ALL	1
#define USER_DRAW_OVERLAP		2
#define USER_DRAW_TRIPLE		3

/* ********************* drawing, swap ****************** */

static void wm_paintcursor_draw(bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	
	if(wm->paintcursors.first) {
		wmWindow *win= CTX_wm_window(C);
		wmPaintCursor *pc;
		
		for(pc= wm->paintcursors.first; pc; pc= pc->next) {
			if(pc->poll(C)) {
				ARegion *ar= CTX_wm_region(C);
				pc->draw(C, win->eventstate->x - ar->winrct.xmin, win->eventstate->y - ar->winrct.ymin);
			}
		}
	}
}

/********************** draw all **************************/
/* - reference method, draw all each time                 */

static void wm_method_draw_all(bContext *C, wmWindow *win)
{
	bScreen *screen= win->screen;
	ScrArea *sa;
	ARegion *ar;

	/* draw area regions */
	for(sa= screen->areabase.first; sa; sa= sa->next) {
		CTX_wm_area_set(C, sa);

		for(ar=sa->regionbase.first; ar; ar= ar->next) {
			if(ar->swinid) {
				CTX_wm_region_set(C, ar);
				ED_region_do_draw(C, ar);
				if(screen->subwinactive==ar->swinid)
					wm_paintcursor_draw(C);
				ED_area_overdraw_flush(C, sa, ar);
				CTX_wm_region_set(C, NULL);
			}
		}
		
		CTX_wm_area_set(C, NULL);
	}

	ED_screen_draw(win);
	ED_area_overdraw(C);

	/* draw overlapping regions */
	for(ar=screen->regionbase.first; ar; ar= ar->next) {
		if(ar->swinid) {
			CTX_wm_region_set(C, ar);
			ED_region_do_draw(C, ar);
			CTX_wm_region_set(C, NULL);
		}
	}

	if(screen->do_gesture)
		wm_gesture_draw(win);
}

/****************** draw overlap all **********************/
/* - redraw marked areas, and anything that overlaps it   */
/* - it also handles swap exchange optionally, assuming   */
/*   that on swap no clearing happens and we get back the */
/*   same buffer as we swapped to the front               */
/* - TODO for swap exchange in full screen mode, and then */
/*   switching to another window seems to invalidate the  */
/*   swap flags, probably best to clear then?             */

/* mark area-regions to redraw if overlapped with rect */
static void wm_overlap_regions_down(bScreen *screen, rcti *dirty)
{
	ScrArea *sa;
	ARegion *ar;

	for(sa= screen->areabase.first; sa; sa= sa->next) {
		for(ar= sa->regionbase.first; ar; ar= ar->next) {
			if(BLI_isect_rcti(dirty, &ar->winrct, NULL)) {
				ar->do_draw= 1;
				ar->swap= WIN_NONE_OK;
			}
		}
	}
}

/* mark menu-regions to redraw if overlapped with rect */
static void wm_overlap_regions_up(bScreen *screen, rcti *dirty)
{
	ARegion *ar;
	
	for(ar= screen->regionbase.first; ar; ar= ar->next) {
		if(BLI_isect_rcti(dirty, &ar->winrct, NULL)) {
			ar->do_draw= 1;
			ar->swap= WIN_NONE_OK;
		}
	}
}

static void wm_method_draw_overlap_all(bContext *C, wmWindow *win)
{
	bScreen *screen= win->screen;
	ScrArea *sa;
	ARegion *ar;
	int exchange= (G.f & G_SWAP_EXCHANGE);

	/* flush overlapping regions */
	if(screen->regionbase.first) {
		/* flush redraws of area regions up to overlapping regions */
		for(sa= screen->areabase.first; sa; sa= sa->next)
			for(ar= sa->regionbase.first; ar; ar= ar->next)
				if(ar->swinid && ar->do_draw)
					wm_overlap_regions_up(screen, &ar->winrct);
		
		/* flush between overlapping regions */
		for(ar= screen->regionbase.last; ar; ar= ar->prev)
			if(ar->swinid && ar->do_draw)
				wm_overlap_regions_up(screen, &ar->winrct);
		
		/* flush redraws of overlapping regions down to area regions */
		for(ar= screen->regionbase.last; ar; ar= ar->prev)
			if(ar->swinid && ar->do_draw)
				wm_overlap_regions_down(screen, &ar->winrct);
	}

	/* draw marked area regions */
	for(sa= screen->areabase.first; sa; sa= sa->next) {
		CTX_wm_area_set(C, sa);

		for(ar=sa->regionbase.first; ar; ar= ar->next) {
			if(ar->swinid) {
				if(ar->do_draw) {
					CTX_wm_region_set(C, ar);
					ED_region_do_draw(C, ar);
					if(screen->subwinactive==ar->swinid)
						wm_paintcursor_draw(C);
					ED_area_overdraw_flush(C, sa, ar);
					CTX_wm_region_set(C, NULL);

					if(exchange)
						ar->swap= WIN_FRONT_OK;
				}
				else if(exchange) {
					if(ar->swap == WIN_FRONT_OK) {
						CTX_wm_region_set(C, ar);
						ED_region_do_draw(C, ar);
						if(screen->subwinactive==ar->swinid)
							wm_paintcursor_draw(C);
						ED_area_overdraw_flush(C, sa, ar);
						CTX_wm_region_set(C, NULL);

						ar->swap= WIN_BOTH_OK;
						printf("draws swap exchange %d\n", ar->swinid);
					}
					else if(ar->swap == WIN_BACK_OK)
						ar->swap= WIN_FRONT_OK;
					else if(ar->swap == WIN_BOTH_OK)
						ar->swap= WIN_BOTH_OK;
				}
			}
		}
		
		CTX_wm_area_set(C, NULL);
	}

	/* after area regions so we can do area 'overlay' drawing */
	if(screen->do_draw) {
		ED_screen_draw(win);

		if(exchange)
			screen->swap= WIN_FRONT_OK;
	}
	else if(exchange) {
		if(screen->swap==WIN_FRONT_OK) {
			ED_screen_draw(win);
			screen->swap= WIN_BOTH_OK;
		}
		else if(screen->swap==WIN_BACK_OK)
			screen->swap= WIN_FRONT_OK;
		else if(screen->swap==WIN_BOTH_OK)
			screen->swap= WIN_BOTH_OK;
	}

	ED_area_overdraw(C);

	/* draw marked overlapping regions */
	for(ar=screen->regionbase.first; ar; ar= ar->next) {
		if(ar->swinid && ar->do_draw) {
			CTX_wm_region_set(C, ar);
			ED_region_do_draw(C, ar);
			CTX_wm_region_set(C, NULL);
		}
	}

	if(screen->do_gesture)
		wm_gesture_draw(win);
}

/******************** draw overlap ************************/
/* - not implemented                                      */

static void wm_method_draw_overlap(bContext *C, wmWindow *win)
{
	wm_method_draw_all(C, win);
}

/****************** draw triple buffer ********************/
/* - area regions are written into a texture, without any */
/*   of the overlapping menus, brushes, gestures. these   */
/*   are redrawn each time.                               */
/* - work in progress still ..                            */
/* - TODO glDeleteTextures ..                             */
/* - TODO handle window resize                            */
/* - TODO avoid region redraw for brush and gestures..    */
/* - TODO use multiple smaller textures for cards without */
/*   non power of two support                             */

static int is_pow2(int n)
{
	return ((n)&(n-1))==0;
}

static int larger_pow2(int n)
{
	if (is_pow2(n))
		return n;

	while(!is_pow2(n))
		n= n&(n-1);

	return n*2;
}

static void wm_method_draw_triple(bContext *C, wmWindow *win)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	bScreen *screen= win->screen;
	ScrArea *sa;
	ARegion *ar;
	float halfx, halfy, ratiox, ratioy;
	int copytex= 0;

	if(win->drawtex) {
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

		wmSubWindowSet(win, screen->mainwin);

		/* wmOrtho for the screen has this same offset */
		ratiox= win->sizex/(float)win->drawtexw;
		ratioy= win->sizey/(float)win->drawtexh;
		halfx= 0.375f/win->drawtexw;
		halfy= 0.375f/win->drawtexh;

		glBindTexture(GL_TEXTURE_2D, win->drawtex);
		glEnable(GL_TEXTURE_2D);

		glColor3f(1.0f, 1.0f, 1.0f);
		glBegin(GL_QUADS);
			glTexCoord2f(halfx, halfy);
			glVertex2f(0.0f, 0.0f);
			glTexCoord2f(ratiox+halfx, halfy);
			glVertex2f(win->sizex, 0.0f);
			glTexCoord2f(ratiox+halfx, ratioy+halfy);
			glVertex2f(win->sizex, win->sizey);
			glTexCoord2f(halfx, ratioy+halfy);
			glVertex2f(0.0f, win->sizey);
		glEnd();

		glDisable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	else {
		GLint format;

		glGenTextures(1, (GLuint *)&win->drawtex);

		if(!win->drawtex) {
			/* not the typical failure case but we handle it anyway */
			win->drawmethod= USER_DRAW_OVERLAP_ALL;
			wm_method_draw_overlap_all(C, win);

			printf("failed to allocate texture for triple buffer drawing (generate).\n");
			return;
		}

		win->drawtexw= win->sizex;
		win->drawtexh= win->sizey;

		if(!GLEW_ARB_texture_non_power_of_two) {
			win->drawtexw= larger_pow2(win->drawtexw);
			win->drawtexh= larger_pow2(win->drawtexh);
		}

		glTexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGB8, win->drawtexw, win->drawtexh, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);

		if(format != GL_RGB8) {
			/* proxy texture is only guaranteed to test for the cases that
			 * there is only one texture in use, which may not be the case */
			glDeleteTextures(1, (GLuint *)&win->drawtex);

			win->drawmethod= USER_DRAW_OVERLAP_ALL;
			wm_method_draw_overlap_all(C, win);

			printf("failed to allocate texture for triple buffer drawing (proxy test).\n");
			return;
		}

		glBindTexture(GL_TEXTURE_2D, win->drawtex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, win->drawtexw, win->drawtexh, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);

		if(glGetError() == GL_OUT_OF_MEMORY) {
			/* not sure if this works everywhere .. */
			glDeleteTextures(1, (GLuint *)&win->drawtex);

			win->drawmethod= USER_DRAW_OVERLAP_ALL;
			wm_method_draw_overlap_all(C, win);

			printf("failed to allocate texture for triple buffer drawing (out of memory).\n");
			return;
		}
	}

	/* draw marked area regions */
	for(sa= screen->areabase.first; sa; sa= sa->next) {
		CTX_wm_area_set(C, sa);

		for(ar=sa->regionbase.first; ar; ar= ar->next) {
			if(ar->swinid && ar->do_draw) {
				CTX_wm_region_set(C, ar);
				ED_region_do_draw(C, ar);
				CTX_wm_region_set(C, NULL);
				copytex= 1;
			}
		}
		
		CTX_wm_area_set(C, NULL);
	}

	if(copytex) {
		wmSubWindowSet(win, screen->mainwin);
		ED_area_overdraw(C);

		glBindTexture(GL_TEXTURE_2D, win->drawtex);
		glReadBuffer(GL_BACK);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, win->sizex, win->sizey);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	/* after area regions so we can do area 'overlay' drawing */
	ED_screen_draw(win);

	/* draw overlapping regions */
	for(ar=screen->regionbase.first; ar; ar= ar->next) {
		if(ar->swinid) {
			CTX_wm_region_set(C, ar);
			ED_region_do_draw(C, ar);
			CTX_wm_region_set(C, NULL);
		}
	}

	if(win->screen->do_gesture)
		wm_gesture_draw(win);

	if(wm->paintcursors.first) {
		for(sa= screen->areabase.first; sa; sa= sa->next) {
			for(ar=sa->regionbase.first; ar; ar= ar->next) {
				if(ar->swinid == screen->subwinactive) {
					CTX_wm_area_set(C, sa);
					CTX_wm_region_set(C, ar);

					wmSubWindowSet(win, ar->swinid);
					ED_region_pixelspace(ar);
					wm_paintcursor_draw(C);

					CTX_wm_region_set(C, NULL);
					CTX_wm_area_set(C, NULL);
				}
			}
		}

		wmSubWindowSet(win, screen->mainwin);
	}
}

/****************** main update call **********************/

/* quick test to prevent changing window drawable */
static int wm_draw_update_test_window(wmWindow *win)
{
	ScrArea *sa;
	ARegion *ar;
	
	if(win->screen->do_refresh)
		return 1;
	if(win->screen->do_draw)
		return 1;
	if(win->screen->do_gesture)
		return 1;
	
	for(ar= win->screen->regionbase.first; ar; ar= ar->next)
		if(ar->swinid && ar->do_draw)
			return 1;
		
	for(sa= win->screen->areabase.first; sa; sa= sa->next)
		for(ar=sa->regionbase.first; ar; ar= ar->next)
			if(ar->swinid && ar->do_draw)
				return 1;

	return 0;
}

void wm_draw_update(bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmWindow *win;
	
	for(win= wm->windows.first; win; win= win->next) {
		win->drawmethod= USER_DRAW_OVERLAP_ALL;

		if(wm_draw_update_test_window(win)) {
			CTX_wm_window_set(C, win);
			
			/* sets context window+screen */
			wm_window_make_drawable(C, win);

			/* notifiers for screen redraw */
			if(win->screen->do_refresh)
				ED_screen_refresh(wm, win);

			if(win->drawmethod == USER_DRAW_ALL)
				wm_method_draw_all(C, win);
			else if(win->drawmethod == USER_DRAW_OVERLAP_ALL)
				wm_method_draw_overlap_all(C, win);
			else if(win->drawmethod == USER_DRAW_OVERLAP)
				wm_method_draw_overlap(C, win);
			else if(win->drawmethod == USER_DRAW_TRIPLE)
				wm_method_draw_triple(C, win);
		
			wm_window_swap_buffers(win);

			CTX_wm_window_set(C, NULL);
		}
	}
}

