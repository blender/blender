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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/render/render_view.c
 *  \ingroup edrend
 */

#include <string.h>
#include <stddef.h>

#include "BLI_utildefines.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "UI_interface.h"

#include "wm_window.h"

#include "render_intern.h"

/*********************** utilities for finding areas *************************/

/* returns biggest area that is not uv/image editor. Note that it uses buttons */
/* window as the last possible alternative.									   */
/* would use BKE_screen_find_big_area(...) but this is too specific            */
static ScrArea *biggest_non_image_area(bContext *C)
{
	bScreen *sc = CTX_wm_screen(C);
	ScrArea *sa, *big = NULL;
	int size, maxsize = 0, bwmaxsize = 0;
	short foundwin = 0;

	for (sa = sc->areabase.first; sa; sa = sa->next) {
		if (sa->winx > 30 && sa->winy > 30) {
			size = sa->winx * sa->winy;
			if (!sa->full && sa->spacetype == SPACE_BUTS) {
				if (foundwin == 0 && size > bwmaxsize) {
					bwmaxsize = size;
					big = sa;
				}
			}
			else if (sa->spacetype != SPACE_IMAGE && size > maxsize) {
				maxsize = size;
				big = sa;
				foundwin = 1;
			}
		}
	}

	return big;
}

static ScrArea *find_area_showing_r_result(bContext *C, Scene *scene, wmWindow **win)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	ScrArea *sa = NULL;
	SpaceImage *sima;

	/* find an imagewindow showing render result */
	for (*win = wm->windows.first; *win; *win = (*win)->next) {
		if (WM_window_get_active_scene(*win) == scene) {
			const bScreen *screen = WM_window_get_active_screen(*win);

			for (sa = screen->areabase.first; sa; sa = sa->next) {
				if (sa->spacetype == SPACE_IMAGE) {
					sima = sa->spacedata.first;
					if (sima->image && sima->image->type == IMA_TYPE_R_RESULT)
						break;
				}
			}
			if (sa)
				break;
		}
	}

	return sa;
}

static ScrArea *find_area_image_empty(bContext *C)
{
	bScreen *sc = CTX_wm_screen(C);
	ScrArea *sa;
	SpaceImage *sima;

	/* find an imagewindow showing render result */
	for (sa = sc->areabase.first; sa; sa = sa->next) {
		if (sa->spacetype == SPACE_IMAGE) {
			sima = sa->spacedata.first;
			if (!sima->image)
				break;
		}
	}

	return sa;
}

/********************** open image editor for render *************************/

/* new window uses x,y to set position */
ScrArea *render_view_open(bContext *C, int mx, int my, ReportList *reports)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	wmWindow *win = NULL;
	ScrArea *sa = NULL;
	SpaceImage *sima;
	bool area_was_image = false;

	if (scene->r.displaymode == R_OUTPUT_NONE)
		return NULL;

	if (scene->r.displaymode == R_OUTPUT_WINDOW) {
		int sizex = 30 * UI_DPI_FAC + (scene->r.xsch * scene->r.size) / 100;
		int sizey = 60 * UI_DPI_FAC + (scene->r.ysch * scene->r.size) / 100;

		/* arbitrary... miniature image window views don't make much sense */
		if (sizex < 320) sizex = 320;
		if (sizey < 256) sizey = 256;

		/* changes context! */
		if (WM_window_open_temp(C, mx, my, sizex, sizey, WM_WINDOW_RENDER) == NULL) {
			BKE_report(reports, RPT_ERROR, "Failed to open window!");
			return NULL;
		}

		sa = CTX_wm_area(C);
	}
	else if (scene->r.displaymode == R_OUTPUT_SCREEN) {
		sa = CTX_wm_area(C);

		/* if the active screen is already in fullscreen mode, skip this and
		 * unset the area, so that the fullscreen area is just changed later */
		if (sa && sa->full) {
			sa = NULL;
		}
		else {
			if (sa && sa->spacetype == SPACE_IMAGE)
				area_was_image = true;

			/* this function returns with changed context */
			sa = ED_screen_full_newspace(C, sa, SPACE_IMAGE);
		}
	}

	if (!sa) {
		sa = find_area_showing_r_result(C, scene, &win);
		if (sa == NULL)
			sa = find_area_image_empty(C);

		/* if area found in other window, we make that one show in front */
		if (win && win != CTX_wm_window(C))
			wm_window_raise(win);

		if (sa == NULL) {
			/* find largest open non-image area */
			sa = biggest_non_image_area(C);
			if (sa) {
				ED_area_newspace(C, sa, SPACE_IMAGE, true);
				sima = sa->spacedata.first;

				/* makes ESC go back to prev space */
				sima->flag |= SI_PREVSPACE;

				/* we already had a fullscreen here -> mark new space as a stacked fullscreen */
				if (sa->full) {
					sa->flag |= (AREA_FLAG_STACKED_FULLSCREEN | AREA_FLAG_TEMP_TYPE);
				}
			}
			else {
				/* use any area of decent size */
				sa = BKE_screen_find_big_area(CTX_wm_screen(C), SPACE_TYPE_ANY, 0);
				if (sa->spacetype != SPACE_IMAGE) {
					// XXX newspace(sa, SPACE_IMAGE);
					sima = sa->spacedata.first;

					/* makes ESC go back to prev space */
					sima->flag |= SI_PREVSPACE;
				}
			}
		}
	}
	sima = sa->spacedata.first;

	/* get the correct image, and scale it */
	sima->image = BKE_image_verify_viewer(bmain, IMA_TYPE_R_RESULT, "Render Result");


	/* if we're rendering to full screen, set appropriate hints on image editor
	 * so it can restore properly on pressing esc */
	if (sa->full) {
		sima->flag |= SI_FULLWINDOW;

		/* Tell the image editor to revert to previous space in space list on close
		 * _only_ if it wasn't already an image editor when the render was invoked */
		if (area_was_image == 0)
			sima->flag |= SI_PREVSPACE;
		else {
			/* Leave it alone so the image editor will just go back from
			 * full screen to the original tiled setup */
		}
	}

	return sa;
}

/*************************** cancel render viewer **********************/

static int render_view_cancel_exec(bContext *C, wmOperator *UNUSED(op))
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	SpaceImage *sima = sa->spacedata.first;

	/* ensure image editor fullscreen and area fullscreen states are in sync */
	if ((sima->flag & SI_FULLWINDOW) && !sa->full) {
		sima->flag &= ~SI_FULLWINDOW;
	}

	/* test if we have a temp screen in front */
	if (WM_window_is_temp_screen(win)) {
		wm_window_lower(win);
		return OPERATOR_FINISHED;
	}
	/* determine if render already shows */
	else if (sima->flag & SI_PREVSPACE) {
		sima->flag &= ~SI_PREVSPACE;

		if (sima->flag & SI_FULLWINDOW) {
			sima->flag &= ~SI_FULLWINDOW;
			ED_screen_full_prevspace(C, sa);
		}
		else {
			ED_area_prevspace(C, sa);
		}

		return OPERATOR_FINISHED;
	}
	else if (sima->flag & SI_FULLWINDOW) {
		sima->flag &= ~SI_FULLWINDOW;
		ED_screen_state_toggle(C, win, sa, SCREENMAXIMIZED);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_PASS_THROUGH;
}

void RENDER_OT_view_cancel(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Cancel Render View";
	ot->description = "Cancel show render view";
	ot->idname = "RENDER_OT_view_cancel";

	/* api callbacks */
	ot->exec = render_view_cancel_exec;
	ot->poll = ED_operator_image_active;
}

/************************* show render viewer *****************/

static int render_view_show_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	wmWindow *wincur = CTX_wm_window(C);

	/* test if we have currently a temp screen active */
	if (WM_window_is_temp_screen(wincur)) {
		wm_window_lower(wincur);
	}
	else {
		wmWindow *win, *winshow;
		ScrArea *sa = find_area_showing_r_result(C, CTX_data_scene(C), &winshow);

		/* is there another window on current scene showing result? */
		for (win = CTX_wm_manager(C)->windows.first; win; win = win->next) {
			const bScreen *sc = WM_window_get_active_screen(win);

			if ((WM_window_is_temp_screen(win) && ((ScrArea *)sc->areabase.first)->spacetype == SPACE_IMAGE) ||
			    (win == winshow && winshow != wincur))
			{
				wm_window_raise(win);
				return OPERATOR_FINISHED;
			}
		}

		/* determine if render already shows */
		if (sa) {
			/* but don't close it when rendering */
			if (G.is_rendering == false) {
				SpaceImage *sima = sa->spacedata.first;

				if (sima->flag & SI_PREVSPACE) {
					sima->flag &= ~SI_PREVSPACE;

					if (sima->flag & SI_FULLWINDOW) {
						sima->flag &= ~SI_FULLWINDOW;
						ED_screen_full_prevspace(C, sa);
					}
					else {
						ED_area_prevspace(C, sa);
					}
				}
			}
		}
		else {
			render_view_open(C, event->x, event->y, op->reports);
		}
	}

	return OPERATOR_FINISHED;
}

void RENDER_OT_view_show(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show/Hide Render View";
	ot->description = "Toggle show render view";
	ot->idname = "RENDER_OT_view_show";

	/* api callbacks */
	ot->invoke = render_view_show_invoke;
	ot->poll = ED_operator_screenactive;
}
