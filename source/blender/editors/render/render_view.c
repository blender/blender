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

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "wm_window.h"

#include "render_intern.h"

/*********************** utilities for finding areas *************************/

/* returns biggest area that is not uv/image editor. Note that it uses buttons */
/* window as the last possible alternative.									   */
/* would use BKE_screen_find_big_area(...) but this is too specific            */
static ScrArea *biggest_non_image_area(bContext *C)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa, *big= NULL;
	int size, maxsize= 0, bwmaxsize= 0;
	short foundwin= 0;

	for(sa= sc->areabase.first; sa; sa= sa->next) {
		if(sa->winx > 30 && sa->winy > 30) {
			size= sa->winx*sa->winy;
			if(sa->spacetype == SPACE_BUTS) {
				if(foundwin == 0 && size > bwmaxsize) {
					bwmaxsize= size;
					big= sa;
				}
			}
			else if(sa->spacetype != SPACE_IMAGE && size > maxsize) {
				maxsize= size;
				big= sa;
				foundwin= 1;
			}
		}
	}

	return big;
}

static ScrArea *find_area_showing_r_result(bContext *C, wmWindow **win)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	ScrArea *sa = NULL;
	SpaceImage *sima;

	/* find an imagewindow showing render result */
	for(*win=wm->windows.first; *win; *win= (*win)->next) {
		for(sa= (*win)->screen->areabase.first; sa; sa= sa->next) {
			if(sa->spacetype==SPACE_IMAGE) {
				sima= sa->spacedata.first;
				if(sima->image && sima->image->type==IMA_TYPE_R_RESULT)
					break;
			}
		}
		if(sa)
			break;
	}
	
	return sa;
}

static ScrArea *find_area_image_empty(bContext *C)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa;
	SpaceImage *sima;

	/* find an imagewindow showing render result */
	for(sa=sc->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_IMAGE) {
			sima= sa->spacedata.first;
			if(!sima->image)
				break;
		}
	}

	return sa;
}

/********************** open image editor for render *************************/

/* new window uses x,y to set position */
void render_view_open(bContext *C, int mx, int my)
{
	wmWindow *win= CTX_wm_window(C);
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= NULL;
	SpaceImage *sima;
	int area_was_image=0;

	if(scene->r.displaymode==R_OUTPUT_NONE)
		return;
	
	if(scene->r.displaymode==R_OUTPUT_WINDOW) {
		rcti rect;
		int sizex, sizey;

		sizex= 10 + (scene->r.xsch*scene->r.size)/100;
		sizey= 40 + (scene->r.ysch*scene->r.size)/100;

		/* arbitrary... miniature image window views don't make much sense */
		if(sizex < 320) sizex= 320;
		if(sizey < 256) sizey= 256;

		/* XXX some magic to calculate postition */
		rect.xmin= mx + win->posx - sizex/2;
		rect.ymin= my + win->posy - sizey/2;
		rect.xmax= rect.xmin + sizex;
		rect.ymax= rect.ymin + sizey;

		/* changes context! */
		WM_window_open_temp(C, &rect, WM_WINDOW_RENDER);

		sa= CTX_wm_area(C);
	}
	else if(scene->r.displaymode==R_OUTPUT_SCREEN) {
		if (CTX_wm_area(C) && CTX_wm_area(C)->spacetype == SPACE_IMAGE)
			area_was_image = 1;

		/* this function returns with changed context */
		sa= ED_screen_full_newspace(C, CTX_wm_area(C), SPACE_IMAGE);
	}

	if(!sa) {
		sa= find_area_showing_r_result(C, &win); 
		if(sa==NULL)
			sa= find_area_image_empty(C);
		
		/* if area found in other window, we make that one show in front */
		if(win && win!=CTX_wm_window(C))
			wm_window_raise(win);

		if(sa==NULL) {
			/* find largest open non-image area */
			sa= biggest_non_image_area(C);
			if(sa) {
				ED_area_newspace(C, sa, SPACE_IMAGE);
				sima= sa->spacedata.first;

				/* makes ESC go back to prev space */
				sima->flag |= SI_PREVSPACE;
			}
			else {
				/* use any area of decent size */
				sa= BKE_screen_find_big_area(CTX_wm_screen(C), -1, 0);
				if(sa->spacetype!=SPACE_IMAGE) {
					// XXX newspace(sa, SPACE_IMAGE);
					sima= sa->spacedata.first;

					/* makes ESC go back to prev space */
					sima->flag |= SI_PREVSPACE;
				}
			}
		}
	}
	sima= sa->spacedata.first;

	/* get the correct image, and scale it */
	sima->image= BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");


	/* if we're rendering to full screen, set appropriate hints on image editor
	 * so it can restore properly on pressing esc */
	if(sa->full) {
		sima->flag |= SI_FULLWINDOW;

		/* Tell the image editor to revert to previous space in space list on close
		 * _only_ if it wasn't already an image editor when the render was invoked */
		if (area_was_image == 0)
			sima->flag |= SI_PREVSPACE;
		else {
			/* Leave it alone so the image editor will just go back from
			 * full screen to the original tiled setup */
			;
		}
	}
}

/*************************** cancel render viewer **********************/

static int render_view_cancel_exec(bContext *C, wmOperator *UNUSED(op))
{
	wmWindow *win= CTX_wm_window(C);
	ScrArea *sa= CTX_wm_area(C);
	SpaceImage *sima= sa->spacedata.first;

	/* test if we have a temp screen in front */
	if(CTX_wm_window(C)->screen->temp) {
		wm_window_lower(CTX_wm_window(C));
		return OPERATOR_FINISHED;
	}
	/* determine if render already shows */
	else if(sima->flag & SI_PREVSPACE) {
		sima->flag &= ~SI_PREVSPACE;

		if(sima->flag & SI_FULLWINDOW) {
			sima->flag &= ~SI_FULLWINDOW;
			ED_screen_full_prevspace(C, sa);
		}
		else
			ED_area_prevspace(C, sa);

		return OPERATOR_FINISHED;
	}
	else if(sima->flag & SI_FULLWINDOW) {
		sima->flag &= ~SI_FULLWINDOW;
		ED_screen_full_toggle(C, win, sa);
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

static int render_view_show_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	wmWindow *wincur = CTX_wm_window(C);
	
	/* test if we have currently a temp screen active */
	if(wincur->screen->temp) {
		wm_window_lower(wincur);
	}
	else { 
		wmWindow *win, *winshow;
		ScrArea *sa= find_area_showing_r_result(C, &winshow);
		
		/* is there another window showing result? */
		for(win= CTX_wm_manager(C)->windows.first; win; win= win->next) {
			if(win->screen->temp || (win==winshow && winshow!=wincur)) {
				wm_window_raise(win);
				return OPERATOR_FINISHED;
			}
		}
		
		/* determine if render already shows */
		if(sa) {
			/* but don't close it when rendering */
			if(!G.rendering) {
				SpaceImage *sima= sa->spacedata.first;

				if(sima->flag & SI_PREVSPACE) {
					sima->flag &= ~SI_PREVSPACE;

					if(sima->flag & SI_FULLWINDOW) {
						sima->flag &= ~SI_FULLWINDOW;
						ED_screen_full_prevspace(C, sa);
					}
					else if(sima->next) {
						/* workaround for case of double prevspace, render window
						 * with a file browser on top of it (same as in ED_area_prevspace) */
						if(sima->next->spacetype == SPACE_FILE && sima->next->next)
							ED_area_newspace(C, sa, sima->next->next->spacetype);
						else
							ED_area_newspace(C, sa, sima->next->spacetype);
						ED_area_tag_redraw(sa);
					}
				}
			}
		}
		else {
			render_view_open(C, event->x, event->y);
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

