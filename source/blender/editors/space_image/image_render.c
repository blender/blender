/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "DNA_image_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "RE_pipeline.h"

#define HEADER_HEIGHT 18

/* *********************** render callbacks ***************** */

/* set on initialize render, only one render output to imagewindow can exist, so the global isnt dangerous yet :) */
static ScrArea *image_area= NULL;

/* can get as well the full picture, as the parts while rendering */
static void imagewindow_progress(ScrArea *sa, RenderResult *rr, volatile rcti *renrect)
{
	SpaceImage *sima= sa->spacedata.first;
	float x1, y1, *rectf= NULL;
	unsigned int *rect32= NULL;
	int ymin, ymax, xmin, xmax;
	
	/* if renrect argument, we only display scanlines */
	if(renrect) {
		/* if ymax==recty, rendering of layer is ready, we should not draw, other things happen... */
		if(rr->renlay==NULL || renrect->ymax>=rr->recty)
			return;
		
		/* xmin here is first subrect x coord, xmax defines subrect width */
		xmin = renrect->xmin;
		xmax = renrect->xmax - xmin;
		if (xmax<2) return;
		
		ymin= renrect->ymin;
		ymax= renrect->ymax - ymin;
		if(ymax<2)
			return;
		renrect->ymin= renrect->ymax;
	}
	else {
		xmin = ymin = 0;
		xmax = rr->rectx - 2*rr->crop;
		ymax = rr->recty - 2*rr->crop;
	}
	
	/* image window cruft */
	
	/* find current float rect for display, first case is after composit... still weak */
	if(rr->rectf)
		rectf= rr->rectf;
	else {
		if(rr->rect32)
			rect32= (unsigned int *)rr->rect32;
		else {
			if(rr->renlay==NULL || rr->renlay->rectf==NULL) return;
			rectf= rr->renlay->rectf;
		}
	}
	if(rectf) {
		/* if scanline updates... */
		rectf+= 4*(rr->rectx*ymin + xmin);
		
		/* when rendering more pixels than needed, we crop away cruft */
		if(rr->crop)
			rectf+= 4*(rr->crop*rr->rectx + rr->crop);
	}
	
	/* tilerect defines drawing offset from (0,0) */
	/* however, tilerect (xmin, ymin) is first pixel */
	x1 = sima->centx + (rr->tilerect.xmin + rr->crop + xmin)*sima->zoom;
	y1 = sima->centy + (rr->tilerect.ymin + rr->crop + ymin)*sima->zoom;
	
	/* needed for gla draw */
	// XXX { rcti rct= ar->winrct; rct.ymax-= HEADER_HEIGHT; glaDefine2DArea(&rct);}

	glPixelZoom(sima->zoom, sima->zoom);
	
	if(rect32)
		glaDrawPixelsSafe(x1, y1, xmax, ymax, rr->rectx, GL_RGBA, GL_UNSIGNED_BYTE, rect32);
	else
		glaDrawPixelsSafe_to32(x1, y1, xmax, ymax, rr->rectx, rectf);
	
	glPixelZoom(1.0, 1.0);
	
}

/* in render window; display a couple of scanlines of rendered image */
/* NOTE: called while render, so no malloc allowed! */
static void imagewindow_progress_display_cb(RenderResult *rr, volatile rcti *rect)
{
	if (image_area) {
		imagewindow_progress(image_area, rr, rect);

		/* no screen_swapbuffers, prevent any other window to draw */
		// XXX myswapbuffers();
	}
}

/* unused, init_display_cb is called on each render */
static void imagewindow_clear_display_cb(RenderResult *rr)
{
	if (image_area) {
	}
}

/* returns biggest area that is not uv/image editor. Note that it uses buttons */
/* window as the last possible alternative.									   */
static ScrArea *biggest_non_image_area(bContext *C)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa, *big= NULL;
	int size, maxsize= 0, bwmaxsize= 0;
	short foundwin= 0;
	
	for(sa= sc->areabase.first; sa; sa= sa->next) {
		if(sa->winx > 10 && sa->winy > 10) {
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

static ScrArea *biggest_area(bContext *C)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa, *big= NULL;
	int size, maxsize= 0;
	
	for(sa= sc->areabase.first; sa; sa= sa->next) {
		size= sa->winx*sa->winy;
		if(size > maxsize) {
			maxsize= size;
			big= sa;
		}
	}
	return big;
}


/* if R_DISPLAYIMAGE
      use Image Window showing Render Result
	  else: turn largest non-image area into Image Window (not to frustrate texture or composite usage)
	  else: then we use Image Window anyway...
   if R_DISPSCREEN
      make a new temp fullscreen area with Image Window
*/

static ScrArea *find_area_showing_r_result(bContext *C)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa;
	SpaceImage *sima;
	
	/* find an imagewindow showing render result */
	for(sa=sc->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_IMAGE) {
			sima= sa->spacedata.first;
			if(sima->image && sima->image->type==IMA_TYPE_R_RESULT)
				break;
		}
	}
	return sa;
}

static ScrArea *imagewindow_set_render_display(bContext *C)
{
	ScrArea *sa;
	SpaceImage *sima;
	
	sa= find_area_showing_r_result(C);
	
	if(sa==NULL) {
		/* find largest open non-image area */
		sa= biggest_non_image_area(C);
		if(sa) {
			// XXX newspace(sa, SPACE_IMAGE);
			sima= sa->spacedata.first;
			
			/* makes ESC go back to prev space */
			sima->flag |= SI_PREVSPACE;
		}
		else {
			/* use any area of decent size */
			sa= biggest_area(C);
			if(sa->spacetype!=SPACE_IMAGE) {
				// XXX newspace(sa, SPACE_IMAGE);
				sima= sa->spacedata.first;
				
				/* makes ESC go back to prev space */
				sima->flag |= SI_PREVSPACE;
			}
		}
	}
	
	sima= sa->spacedata.first;
	
	/* get the correct image, and scale it */
	sima->image= BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");
	
	if(0) { // XXX G.displaymode==R_DISPLAYSCREEN) {
		if(sa->full==0) {
			sima->flag |= SI_FULLWINDOW;
			/* fullscreen works with lousy curarea */
			// XXX curarea= sa;
			// XXX area_fullscreen();
			// XXX sa= curarea;
		}
	}
	
	return sa;
}

static void imagewindow_init_display_cb(RenderResult *rr)
{
	bContext *C= NULL; // XXX

	image_area= imagewindow_set_render_display(C);
	
	if(image_area) {
		SpaceImage *sima= image_area->spacedata.first;
		
		// XXX areawinset(image_area->win);
		
		/* calc location using original size (tiles don't tell) */
		sima->centx= (image_area->winx - sima->zoom*(float)rr->rectx)/2.0f;
		sima->centy= (image_area->winy - sima->zoom*(float)rr->recty)/2.0f;
		
		sima->centx-= sima->zoom*sima->xof;
		sima->centy-= sima->zoom*sima->yof;
		
		// XXX drawimagespace(image_area, sima);
		// XXX if(image_area->headertype) scrarea_do_headdraw(image_area);

		/* no screen_swapbuffers, prevent any other window to draw */
		// XXX myswapbuffers();
		
		// XXX allqueue(REDRAWIMAGE, 0);	/* redraw in end */
	}
}

/* coming from BIF_toggle_render_display() */
void imagewindow_toggle_render(bContext *C)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa;
	
	/* check if any imagewindow is showing temporal render output */
	for(sa=sc->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_IMAGE) {
			SpaceImage *sima= sa->spacedata.first;
			
			if(sima->image && sima->image->type==IMA_TYPE_R_RESULT)
				if(sima->flag & (SI_PREVSPACE|SI_FULLWINDOW))
					break;
		}
	}

	if(sa) {
		// XXX addqueue(sa->win, ESCKEY, 1);	/* also returns from fullscreen */
	}
	else {
		sa= imagewindow_set_render_display(C);
		// XXX scrarea_queue_headredraw(sa);
		// XXX scrarea_queue_winredraw(sa);
	}
}

/* NOTE: called while render, so no malloc allowed! */
static void imagewindow_renderinfo_cb(RenderStats *rs)
{
	if(image_area) {
		// XXX BIF_make_render_text(rs);

		// XXX sima_draw_render_info(sima, ar);
		
		/* no screen_swapbuffers, prevent any other window to draw */
		// XXX myswapbuffers();
	}
}

void imagewindow_render_callbacks(Render *re)
{
	RE_display_init_cb(re, imagewindow_init_display_cb);
	RE_display_draw_cb(re, imagewindow_progress_display_cb);
	RE_display_clear_cb(re, imagewindow_clear_display_cb);
	RE_stats_draw_cb(re, imagewindow_renderinfo_cb);	
}

