/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
 
#ifdef WIN32
/* for the multimedia timer */
#include <windows.h>
#include <mmsystem.h>
#endif

#include <string.h>
#include <stdarg.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#else
 /* for signal callback, not (fully) supported at windows */
#include <sys/time.h>
#include <signal.h>

#endif

#include <limits.h>

#include "BLI_blenlib.h"

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"

#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"
#include "BKE_writeavi.h"	/* movie handle */

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_graphics.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_toets.h"
#include "BIF_writeimage.h"

#include "BDR_editobject.h"
#include "BPY_extern.h" /* for BPY_do_all_scripts */

#include "BSE_view.h"
#include "BSE_drawview.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"

#include "RE_pipeline.h"

#include "blendef.h"
#include "mydevice.h"
#include "winlay.h"

/* ------------ renderwin struct, to prevent too much global vars --------- */
/* ------------ only used for display in a 2nd window  --------- */


/* flags escape presses during event handling
* so we can test for user break later.
*/
#define RW_FLAGS_ESCAPE		(1<<0)
/* old zoom style (2x, locked to mouse, exits
* when mouse leaves window), to be removed
* at some point.
*/
#define RW_FLAGS_OLDZOOM		(1<<1)
/* on when image is being panned with middlemouse
*/
#define RW_FLAGS_PANNING		(1<<2)
/* on when the mouse is dragging over the image
* to examine pixel values.
*/
#define RW_FLAGS_PIXEL_EXAMINING	(1<<3)

/* forces draw of alpha */
#define RW_FLAGS_ALPHA		(1<<4)

/* space for info text */
#define RW_HEADERY		18

typedef struct {
	Window *win;

	int rectx, recty;	/* size of image */
	
	int sparex, sparey;	/* spare rect size */
	unsigned int *rectspare;
	float *rectsparef;
	
	float zoom, zoomofs[2];
	int active;
	short storespare, showspare;
	
	int mbut[3];
	int lmouse[2];
	
	unsigned int flags;
	
	float pan_mouse_start[2], pan_ofs_start[2];

	char *info_text;
	char *render_text, *render_text_spare;
	
} RenderWin;

static RenderWin *render_win= NULL;

/* --------------- help functions for RenderWin struct ---------------------------- */


/* only called in function open_renderwin */
static RenderWin *renderwin_alloc(Window *win)
{
	RenderWin *rw= MEM_callocN(sizeof(*rw), "RenderWin");
	rw->win= win;
	rw->zoom= 1.0;
	rw->active= 0;
	rw->flags= 0;
	rw->zoomofs[0]= rw->zoomofs[1]= 0;
	rw->info_text= NULL;
	rw->render_text= rw->render_text_spare= NULL;

	rw->lmouse[0]= rw->lmouse[1]= 0;
	rw->mbut[0]= rw->mbut[1]= rw->mbut[2]= 0;

	return rw;
}


static void renderwin_queue_redraw(RenderWin *rw)
{
	window_queue_redraw(rw->win); // to ghost
}

static void renderwin_reshape(RenderWin *rw)
{
	;
}

static void renderwin_get_fullrect(RenderWin *rw, float fullrect_r[2][2])
{
	float display_w, display_h;
	float cent_x, cent_y;
	int w, h;

	window_get_size(rw->win, &w, &h);
	h-= RW_HEADERY;

	display_w= rw->rectx*rw->zoom;
	display_h= rw->recty*rw->zoom;
	cent_x= (rw->zoomofs[0] + rw->rectx/2)*rw->zoom;
	cent_y= (rw->zoomofs[1] + rw->recty/2)*rw->zoom;
	
	fullrect_r[0][0]= w/2 - cent_x;
	fullrect_r[0][1]= h/2 - cent_y;
	fullrect_r[1][0]= fullrect_r[0][0] + display_w;
	fullrect_r[1][1]= fullrect_r[0][1] + display_h;
}

	/** 
	 * Project window coordinate to image pixel coordinate.
	 * Returns true if resulting coordinate is within image.
	 */
static int renderwin_win_to_image_co(RenderWin *rw, int winco[2], int imgco_r[2])
{
	float fullrect[2][2];
	
	renderwin_get_fullrect(rw, fullrect);
	
	imgco_r[0]= (int) ((winco[0]-fullrect[0][0])/rw->zoom);
	imgco_r[1]= (int) ((winco[1]-fullrect[0][1])/rw->zoom);
	
	return (imgco_r[0]>=0 && imgco_r[1]>=0 && imgco_r[0]<rw->rectx && imgco_r[1]<rw->recty);
}

	/**
	 * Project window coordinates to normalized device coordinates
	 * Returns true if resulting coordinate is within window.
	 */
static int renderwin_win_to_ndc(RenderWin *rw, int win_co[2], float ndc_r[2])
{
	int w, h;

	window_get_size(rw->win, &w, &h);
	h-= RW_HEADERY;

	ndc_r[0]=  ((float)(win_co[0]*2)/(w-1) - 1.0f);
	ndc_r[1]=  ((float)(win_co[1]*2)/(h-1) - 1.0f);

	return (fabs(ndc_r[0])<=1.0 && fabs(ndc_r[1])<=1.0);
}

static void renderwin_set_infotext(RenderWin *rw, char *info_text)
{
	if (rw->info_text) MEM_freeN(rw->info_text);
	rw->info_text= info_text?BLI_strdup(info_text):NULL;
}

static void renderwin_reset_view(RenderWin *rw)
{
	int w, h;

	if (rw->info_text) renderwin_set_infotext(rw, NULL);

	/* now calculate a zoom for when image is larger than window */
	window_get_size(rw->win, &w, &h);
	h-= RW_HEADERY;

	if(rw->rectx>w || rw->recty>h) {
		if(rw->rectx-w > rw->recty-h) rw->zoom= ((float)w)/((float)rw->rectx);
		else rw->zoom= ((float)h)/((float)rw->recty);
	}
	else rw->zoom= 1.0;

	rw->zoomofs[0]= rw->zoomofs[1]= 0;
	renderwin_queue_redraw(rw);
}

static void renderwin_draw_render_info(RenderWin *rw)
{
	/* render text is added to top */
	if(RW_HEADERY) {
		float colf[3];
		rcti rect;
		char *str;
		
		window_get_size(rw->win, &rect.xmax, &rect.ymax);
		rect.xmin= 0;
		rect.ymin= rect.ymax-RW_HEADERY;
		glEnable(GL_SCISSOR_TEST);
		glaDefine2DArea(&rect);
		
		/* clear header rect */
		BIF_SetTheme(NULL);	// sets view3d theme by default
		BIF_GetThemeColor3fv(TH_HEADER, colf);
		glClearColor(colf[0], colf[1], colf[2], 1.0); 
		glClear(GL_COLOR_BUFFER_BIT);
		
		if(rw->showspare)
			str= rw->render_text_spare;
		else
			str= rw->render_text;
		
		if(str) {
			BIF_ThemeColor(TH_TEXT);
			glRasterPos2i(12, 5);
			BMF_DrawString(G.fonts, str);
		}
		
		BIF_SetTheme(curarea);	// restore theme
	}	
	
}

static void renderwin_draw(RenderWin *rw, int just_clear)
{
	float fullrect[2][2];
	int set_back_mainwindow;
	rcti rect;

	/* since renderwin uses callbacks (controlled by ghost) it can
		mess up active window output with redraw events after a render. 
		this is patchy, still WIP */
	set_back_mainwindow = (winlay_get_active_window() != rw->win);
	window_make_active(rw->win);
	
	rect.xmin= rect.ymin= 0;
	window_get_size(rw->win, &rect.xmax, &rect.ymax);
	rect.ymax-= RW_HEADERY;
	
	renderwin_get_fullrect(rw, fullrect);
	
	/* do this first, so window ends with correct scissor */
	renderwin_draw_render_info(rw);
	
	glEnable(GL_SCISSOR_TEST);
	glaDefine2DArea(&rect);
	
	glClearColor(.1875, .1875, .1875, 1.0); 
	glClear(GL_COLOR_BUFFER_BIT);

	if (just_clear) {
		glColor3ub(0, 0, 0);
		glRectfv(fullrect[0], fullrect[1]);
	} else {
		RenderResult rres;
		
		if(rw->showspare) {
			rres.rectx= rw->sparex;
			rres.recty= rw->sparey;
			rres.rect32= rw->rectspare;
			rres.rectf= rw->rectsparef;
		}
		else
			RE_GetResultImage(RE_GetRender("Render"), &rres);
		
		if(rres.rectf) {
			
			glPixelZoom(rw->zoom, rw->zoom);
			if(rw->flags & RW_FLAGS_ALPHA) {
				if(rres.rect32) {
					/* swap bytes, so alpha is most significant one, then just draw it as luminance int */
					glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
					glaDrawPixelsSafe(fullrect[0][0], fullrect[0][1], rres.rectx, rres.recty, rres.rectx, GL_LUMINANCE, GL_UNSIGNED_INT, rres.rect32);
					glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
				}
				else {
					float *trectf= MEM_mallocN(rres.rectx*rres.recty*4, "temp");
					int a, b;
					
					for(a= rres.rectx*rres.recty -1, b= 4*a+3; a>=0; a--, b-=4)
						trectf[a]= rres.rectf[b];
					
					glaDrawPixelsSafe(fullrect[0][0], fullrect[0][1], rres.rectx, rres.recty, rres.rectx, GL_LUMINANCE, GL_FLOAT, trectf);
					MEM_freeN(trectf);
				}
			}
			else {
				if(rres.rect32)
					glaDrawPixelsSafe(fullrect[0][0], fullrect[0][1], rres.rectx, rres.recty, rres.rectx, GL_RGBA, GL_UNSIGNED_BYTE, rres.rect32);
				else if(rres.rectf)
					glaDrawPixelsSafe(fullrect[0][0], fullrect[0][1], rres.rectx, rres.recty, rres.rectx, GL_RGBA, GL_FLOAT, rres.rectf);
			}
			glPixelZoom(1.0, 1.0);
		}
	}
	
	/* info text is overlayed on bottom */
	if (rw->info_text) {
		float w;
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		w=186.0*strlen(rw->info_text)/30;
		glColor4f(.5,.5,.5,.25);
		glRectf(0.0,0.0,w,30.0);
		glDisable(GL_BLEND);
		glColor3ub(255, 255, 255);
		glRasterPos2i(10, 10);
		BMF_DrawString(G.font, rw->info_text);
	}
	
	window_swap_buffers(rw->win);
	
	if (set_back_mainwindow) mainwindow_make_active();	
}

/* ------ interactivity calls for RenderWin ------------- */

static void renderwin_mouse_moved(RenderWin *rw)
{
	RenderResult rres;
	
	RE_GetResultImage(RE_GetRender("Render"), &rres);

	if (rw->flags & RW_FLAGS_PIXEL_EXAMINING) {
		int imgco[2], ofs=0;
		char buf[128];
		char *pxl;

		if (renderwin_win_to_image_co(rw, rw->lmouse, imgco)) {
			if (rres.rect32) {
				pxl= (char*) &rres.rect32[rres.rectx*imgco[1] + imgco[0]];
				ofs= sprintf(buf, "R: %d G: %d B: %d A: %d", pxl[0], pxl[1], pxl[2], pxl[3]);	
			}
			if (rres.rectf) {
				float *pxlf= rres.rectf + 4*(rres.rectx*imgco[1] + imgco[0]);
				ofs+= sprintf(buf+ofs, " | R: %.3f G: %.3f B: %.3f A: %.3f ", pxlf[0], pxlf[1], pxlf[2], pxlf[3]);
			}
			if (rres.rectz) {
				float *pxlz= &rres.rectz[rres.rectx*imgco[1] + imgco[0]];			
				sprintf(buf+ofs, "| Z: %.3f", *pxlz );
			}

			renderwin_set_infotext(rw, buf);
			renderwin_queue_redraw(rw);
		} else {
			renderwin_set_infotext(rw, NULL);
			renderwin_queue_redraw(rw);
		}
	} 
	else if (rw->flags & RW_FLAGS_PANNING) {
		int delta_x= rw->lmouse[0] - rw->pan_mouse_start[0];
		int delta_y= rw->lmouse[1] - rw->pan_mouse_start[1];
	
		rw->zoomofs[0]= rw->pan_ofs_start[0] - delta_x/rw->zoom;
		rw->zoomofs[1]= rw->pan_ofs_start[1] - delta_y/rw->zoom;
		rw->zoomofs[0]= CLAMPIS(rw->zoomofs[0], -rres.rectx/2, rres.rectx/2);
		rw->zoomofs[1]= CLAMPIS(rw->zoomofs[1], -rres.recty/2, rres.recty/2);

		renderwin_queue_redraw(rw);
	} 
	else if (rw->flags & RW_FLAGS_OLDZOOM) {
		float ndc[2];
		int w, h;

		window_get_size(rw->win, &w, &h);
		h-= RW_HEADERY;
		renderwin_win_to_ndc(rw, rw->lmouse, ndc);

		rw->zoomofs[0]= -0.5*ndc[0]*(w-rres.rectx*rw->zoom)/rw->zoom;
		rw->zoomofs[1]= -0.5*ndc[1]*(h-rres.recty*rw->zoom)/rw->zoom;

		renderwin_queue_redraw(rw);
	}
}

static void renderwin_mousebut_changed(RenderWin *rw)
{
	if (rw->mbut[0]) {
		rw->flags|= RW_FLAGS_PIXEL_EXAMINING;
	} 
	else if (rw->mbut[1]) {
		rw->flags|= RW_FLAGS_PANNING;
		rw->pan_mouse_start[0]= rw->lmouse[0];
		rw->pan_mouse_start[1]= rw->lmouse[1];
		rw->pan_ofs_start[0]= rw->zoomofs[0];
		rw->pan_ofs_start[1]= rw->zoomofs[1];
	} 
	else {
		if (rw->flags & RW_FLAGS_PANNING) {
			rw->flags &= ~RW_FLAGS_PANNING;
			renderwin_queue_redraw(rw);
		}
		if (rw->flags & RW_FLAGS_PIXEL_EXAMINING) {
			rw->flags&= ~RW_FLAGS_PIXEL_EXAMINING;
			renderwin_set_infotext(rw, NULL);
			renderwin_queue_redraw(rw);
		}
	}
}


/* handler for renderwin, passed on to Ghost */
static void renderwin_handler(Window *win, void *user_data, short evt, short val, char ascii)
{
	RenderWin *rw= user_data;

	// added this for safety, while render it's just creating bezerk results
	if(G.rendering) {
		if(evt==ESCKEY && val) 
			rw->flags|= RW_FLAGS_ESCAPE;
		return;
	}
	
	if (evt==RESHAPE) {
		renderwin_reshape(rw);
	} 
	else if (evt==REDRAW) {
		renderwin_draw(rw, 0);
	} 
	else if (evt==WINCLOSE) {
		BIF_close_render_display();
	} 
	else if (evt==INPUTCHANGE) {
		rw->active= val;

		if (!val && (rw->flags&RW_FLAGS_OLDZOOM)) {
			rw->flags&= ~RW_FLAGS_OLDZOOM;
			renderwin_reset_view(rw);
		}
	} 
	else if (ELEM(evt, MOUSEX, MOUSEY)) {
		rw->lmouse[evt==MOUSEY]= val;
		renderwin_mouse_moved(rw);
	} 
	else if (ELEM3(evt, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE)) {
		int which= (evt==LEFTMOUSE)?0:(evt==MIDDLEMOUSE)?1:2;
		rw->mbut[which]= val;
		renderwin_mousebut_changed(rw);
	} 
	else if (val) {
		if (evt==ESCKEY) {
			if (rw->flags&RW_FLAGS_OLDZOOM) {
				rw->flags&= ~RW_FLAGS_OLDZOOM;
				renderwin_reset_view(rw);
			} 
			else {
				rw->flags|= RW_FLAGS_ESCAPE;
				mainwindow_raise();
				mainwindow_make_active();
				rw->active= 0;
			}
		} 
		else if( evt==AKEY) {
			rw->flags ^= RW_FLAGS_ALPHA;
			renderwin_queue_redraw(render_win);
		}
		else if (evt==JKEY) {
			if(G.rendering==0) BIF_swap_render_rects();
		} 
		else if (evt==ZKEY) {
			if (rw->flags&RW_FLAGS_OLDZOOM) {
				rw->flags&= ~RW_FLAGS_OLDZOOM;
				renderwin_reset_view(rw);
			} else {
				rw->zoom= 2.0;
				rw->flags|= RW_FLAGS_OLDZOOM;
				renderwin_mouse_moved(rw);
			}
		} 
		else if (evt==PADPLUSKEY) {
			if (rw->zoom<15.9) {
				if(rw->zoom>0.5 && rw->zoom<1.0) rw->zoom= 1.0;
				else rw->zoom*= 2.0;
				renderwin_queue_redraw(rw);
			}
		} 
		else if (evt==PADMINUS) {
			if (rw->zoom>0.26) {
				if(rw->zoom>1.0 && rw->zoom<2.0) rw->zoom= 1.0;
				else rw->zoom*= 0.5;
				renderwin_queue_redraw(rw);
			}
		} 
		else if (evt==PADENTER || evt==HOMEKEY) {
			if (rw->flags&RW_FLAGS_OLDZOOM) {
				rw->flags&= ~RW_FLAGS_OLDZOOM;
			}
			renderwin_reset_view(rw);
		} 
		else if (evt==F3KEY) {
//			if(R.flag==0) {
//				mainwindow_raise();
//				mainwindow_make_active();
//				rw->active= 0;
//				areawinset(find_biggest_area()->win);
//				BIF_save_rendered_image_fs();
//			}
		} 
		else if (evt==F11KEY) {
			BIF_toggle_render_display();
		} 
		else if (evt==F12KEY) {
			/* if it's rendering, this flag is set */
//			if(R.flag==0) BIF_do_render(0);
		}
	}
}

static char *renderwin_get_title(int doswap)
{
	static int swap= 0;
	char *title="";
	
	swap+= doswap;
	
	if(swap & 1) {
		if (G.scene->r.renderer==R_YAFRAY) title = "YafRay:Render (previous frame)";
		else title = "Blender:Render (previous frame)";
	}
	else {
		if (G.scene->r.renderer==R_YAFRAY) title = "YafRay:Render";
		else title = "Blender:Render";
	}

	return title;
}

/* opens window and allocs struct */
static void open_renderwin(int winpos[2], int winsize[2], int imagesize[2])
{
	extern void mywindow_build_and_set_renderwin( int orx, int ory, int sizex, int sizey); // mywindow.c
	Window *win;
	char *title;
	
	title= renderwin_get_title(0);	/* 0 = no swap */
	win= window_open(title, winpos[0], winpos[1], winsize[0], winsize[1]+RW_HEADERY, 0);

	render_win= renderwin_alloc(win);
	render_win->rectx= imagesize[0];
	render_win->recty= imagesize[1];
	
	/* Ghost calls handler */
	window_set_handler(win, renderwin_handler, render_win);

	winlay_process_events(0);
	window_make_active(render_win->win);
	winlay_process_events(0);
	
	/* mywindow has to know about it too */
	mywindow_build_and_set_renderwin(winpos[0], winpos[1], winsize[0], winsize[1]+RW_HEADERY);
	/* and we should be able to draw 3d in it */
	init_gl_stuff();
	
	renderwin_draw(render_win, 1);
	renderwin_draw(render_win, 1);
}

/* -------------- callbacks for render loop: Window (RenderWin) ----------------------- */

/* calculations for window size and position */
void calc_renderwin_rectangle(int rectx, int recty, int posmask, int renderpos_r[2], int rendersize_r[2]) 
{
	int scr_w, scr_h, x, y, div= 0;
	float ndc_x= 0.0, ndc_y= 0.0;

	winlay_get_screensize(&scr_w, &scr_h);

	rendersize_r[0]= rectx;
	rendersize_r[1]= recty;

	rendersize_r[0]= CLAMPIS(rendersize_r[0], 0, scr_w);	 
	rendersize_r[1]= CLAMPIS(rendersize_r[1], 0, scr_h-RW_HEADERY);	 
	
 	for (y=-1; y<=1; y++) {
		for (x=-1; x<=1; x++) {
			if (posmask & (1<<((y+1)*3 + (x+1)))) {
				ndc_x+= x;
				ndc_y+= y;
				div++;
			}
		}
	}

	if (div) {
		ndc_x/= div;
		ndc_y/= div;
	}
	
	renderpos_r[0]= (scr_w-rendersize_r[0])*(ndc_x*0.5 + 0.5);
#ifdef __APPLE__
	/* 44 pixels is topbar and window header... awaiting better fixes in ghost :) */
	rendersize_r[1]= CLAMPIS(rendersize_r[1], 0, scr_h-44-RW_HEADERY);	 
	renderpos_r[1]= -44-RW_HEADERY+(scr_h-rendersize_r[1])*(ndc_y*0.5 + 0.5);
#else
	renderpos_r[1]= -RW_HEADERY+(scr_h-rendersize_r[1])*(ndc_y*0.5 + 0.5);
#endif
}
	
/* init renderwin, alloc/open/resize */
static void renderwin_init_display_cb(RenderResult *rr) 
{
	if (G.afbreek != 1) {
		int rendersize[2], renderpos[2], imagesize[2];

		calc_renderwin_rectangle(rr->rectx, rr->recty, G.winpos, renderpos, rendersize);
		
		imagesize[0]= rr->rectx;
		imagesize[1]= rr->recty;

		if (!render_win) {
			open_renderwin(renderpos, rendersize, imagesize);
			renderwin_reset_view(render_win); // incl. autozoom for large images
		} else {
			int win_x, win_y;
			int win_w, win_h;

			window_get_position(render_win->win, &win_x, &win_y);
			window_get_size(render_win->win, &win_w, &win_h);
			win_h-= RW_HEADERY;

				/* XXX, this is nasty and I guess bound to cause problems,
				 * but to ensure the window is at the user specified position
				 * and size we reopen the window all the time... we need
				 * a ghost _set_position to fix this -zr
				 */
				 
				 /* XXX, well... it is nasty yes, and reopens windows each time on
				    subsequent renders. Better rule is to make it reopen only only
				    size change, and use the preferred position only on open_renderwin
					cases (ton)
				 */
			if(rendersize[0]!= win_w || rendersize[1]!= win_h) {
				BIF_close_render_display();
				open_renderwin(renderpos, rendersize, imagesize);
			}
			else {
				window_raise(render_win->win);
				window_make_active(render_win->win);
				
				mywinset(2);	// to assign scissor/viewport again in mywindow.c. is hackish yes, but otherwise it draws in header of button for ogl header
				{
					rcti win_rct;
					win_rct.xmin= win_rct.ymin= 0;
					window_get_size(render_win->win, &win_rct.xmax, &win_rct.ymax);
					win_rct.ymax-= RW_HEADERY;
					glaDefine2DArea(&win_rct);
				}
			}

			renderwin_reset_view(render_win);
			render_win->flags&= ~RW_FLAGS_ESCAPE;
			render_win->active= 1;
		}
		/* make sure we are in normal draw again */
		render_win->flags &= ~RW_FLAGS_ALPHA;
		
		glFinish();
	}
}

/* callback for redraw render win */
static void renderwin_clear_display_cb(RenderResult *rr) 
{
	if (render_win) {
		window_make_active(render_win->win);
		renderwin_draw(render_win, 1);
	}
}

/* XXX, this is not good, we do this without any regard to state
* ... better is to make this an optimization of a more clear
* implementation. the bug shows up when you do something like
* open the window, then draw part of the progress, then get
* a redraw event. whatever can go wrong will. -zr
*
* Note: blocked queue handling while rendering to prevent that (ton)
*/

/* can get as well the full picture, as the parts while rendering */
static void renderwin_progress(RenderWin *rw, RenderResult *rr, rcti *renrect)
{
	rcti win_rct;
	float *rectf, fullrect[2][2];
	int ymin, ymax;
	
	/* if renrect argument, we only display scanlines */
	if(renrect) {
		ymin= renrect->ymin;
		ymax= renrect->ymax-ymin;
		if(ymax<2 || renrect->ymax>=rr->recty) /* if ymax==recty, rendering of layer is ready, we should not draw, other things happen... */
			return;
		renrect->ymin= renrect->ymax;
	}
	else {
		ymin= 0;
		ymax= rr->recty-2*rr->crop;
	}
	
	/* renderwindow cruft */
	win_rct.xmin= win_rct.ymin= 0;
	window_get_size(rw->win, &win_rct.xmax, &win_rct.ymax);
	win_rct.ymax-= RW_HEADERY;
	renderwin_get_fullrect(rw, fullrect);
	
	/* find current float rect for display, first case is after composit... still weak */
	if(rr->rectf)
		rectf= rr->rectf;
	else {
		RenderLayer *rl= rr->renlay;
		if(rl==NULL) return;
		rectf= rl->rectf;
	}
	/* if scanline updates... */
	rectf+= 4*rr->rectx*ymin;
	
	/* when rendering more pixels than needed, we crop away cruft */
	if(rr->crop)
		rectf+= 4*(rr->crop*rr->rectx + rr->crop);
	
	/* tilerect defines drawing offset from (0,0) */
	/* however, tilerect (xmin, ymin) is first pixel */
	fullrect[0][0] += (rr->tilerect.xmin+rr->crop)*rw->zoom;
	fullrect[0][1] += (rr->tilerect.ymin+rr->crop + ymin)*rw->zoom;

	glEnable(GL_SCISSOR_TEST);
	glaDefine2DArea(&win_rct);

	glDrawBuffer(GL_FRONT);
	glPixelZoom(rw->zoom, rw->zoom);
	glaDrawPixelsSafe(fullrect[0][0], fullrect[0][1], rr->rectx-2*rr->crop, ymax, rr->rectx, 
					  GL_RGBA, GL_FLOAT, rectf);
	glPixelZoom(1.0, 1.0);
//	glFlush();
	glFinish();
	glDrawBuffer(GL_BACK);
}


/* in render window; display a couple of scanlines of rendered image */
static void renderwin_progress_display_cb(RenderResult *rr, rcti *rect)
{
	if (render_win) {
		renderwin_progress(render_win, rr, rect);
	}
}

/* -------------- callbacks for render loop: interactivity ----------------------- */


/* callback for print info in top header of renderwin */
static void printrenderinfo_cb(RenderStats *rs)
{
	extern char info_time_str[32];	// header_info.c
	extern int mem_in_use;
	static float megs_used_memory;
	char str[300], *spos= str;
		
	if(render_win) {
		megs_used_memory= mem_in_use/(1024.0*1024.0);
		
		if(rs->tothalo)
			spos+= sprintf(spos, "Fra:%d  Ve:%d Fa:%d Ha:%d La:%d Mem:%.2fM", (G.scene->r.cfra), rs->totvert, rs->totface, rs->tothalo, rs->totlamp, megs_used_memory);
		else 
			spos+= sprintf(spos, "Fra:%d  Ve:%d Fa:%d La:%d Mem:%.2fM", (G.scene->r.cfra), rs->totvert, rs->totface, rs->totlamp, megs_used_memory);

		BLI_timestr(rs->lastframetime, info_time_str);
		spos+= sprintf(spos, " Time:%s ", info_time_str);
		
		if(rs->infostr)
			spos+= sprintf(spos, " | %s", rs->infostr);
		
		if(render_win) {
			if(render_win->render_text) MEM_freeN(render_win->render_text);
			render_win->render_text= BLI_strdup(str);
			glDrawBuffer(GL_FRONT);
			renderwin_draw_render_info(render_win);
			glFlush();
			glDrawBuffer(GL_BACK);
		}
	}
}

/* -------------- callback system to allow ESC from rendering ----------------------- */

/* POSIX & WIN32: this function is called all the time, and should not use cpu or resources */
static int test_break(void)
{

	if(G.afbreek==2) { /* code for testing queue */

		G.afbreek= 0;

		blender_test_break(); /* tests blender interface */

		if (G.afbreek==0 && render_win) { /* tests window */
			winlay_process_events(0);
			// render_win can be closed in winlay_process_events()
			if (render_win == 0 || (render_win->flags & RW_FLAGS_ESCAPE)) {
				G.afbreek= 1;
			}
		}
	}

	if(G.afbreek==1) return 1;
	else return 0;
}



#ifdef _WIN32
/* we use the multimedia time here */
static UINT uRenderTimerId;

void CALLBACK interruptESC(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	if(G.afbreek==0) G.afbreek= 2;	/* code for read queue */
}

/* WIN32: init SetTimer callback */
static void init_test_break_callback()
{
	timeBeginPeriod(50);
	uRenderTimerId = timeSetEvent(250, 1, interruptESC, 0, TIME_PERIODIC);
}

/* WIN32: stop SetTimer callback */
static void end_test_break_callback()
{
	timeEndPeriod(50);
	timeKillEvent(uRenderTimerId);
}

#else
/* all other OS's support signal(SIGVTALRM) */

/* POSIX: this function goes in the signal() callback */
static void interruptESC(int sig)
{

	if(G.afbreek==0) G.afbreek= 2;	/* code for read queue */

	/* call again, timer was reset */
	signal(SIGVTALRM, interruptESC);
}

/* POSIX: initialize timer and signal */
static void init_test_break_callback()
{

	struct itimerval tmevalue;

	tmevalue.it_interval.tv_sec = 0;
	tmevalue.it_interval.tv_usec = 250000;
	/* wanneer de eerste ? */
	tmevalue.it_value.tv_sec = 0;
	tmevalue.it_value.tv_usec = 10000;

	signal(SIGVTALRM, interruptESC);
	setitimer(ITIMER_VIRTUAL, &tmevalue, 0);

}

/* POSIX: stop timer and callback */
static void end_test_break_callback()
{
	struct itimerval tmevalue;

	tmevalue.it_value.tv_sec = 0;
	tmevalue.it_value.tv_usec = 0;
	setitimer(ITIMER_VIRTUAL, &tmevalue, 0);
	signal(SIGVTALRM, SIG_IGN);

}


#endif



/* -------------- callbacks for render loop: init & run! ----------------------- */


/* - initialize displays
   - set callbacks
   - cleanup
*/

static void do_render(int anim)
{
	Render *re= RE_NewRender("Render");
	
	/* we set this flag to prevent renderwindow queue to execute another render */
	G.rendering= 1;
	G.afbreek= 0;

	/* set callbacks */
	RE_display_init_cb(re, renderwin_init_display_cb);
	RE_display_draw_cb(re, renderwin_progress_display_cb);
	RE_display_clear_cb(re, renderwin_clear_display_cb);
	init_test_break_callback();
	RE_test_break_cb(re, test_break);
	RE_timecursor_cb(re, set_timecursor);
	RE_stats_draw_cb(re, printrenderinfo_cb);
	
	if(render_win) window_set_cursor(render_win->win, CURSOR_WAIT);
	waitcursor(1);

	if(G.obedit)
		exit_editmode(0);	/* 0 = no free data */

	if(anim)
		RE_BlenderAnim(re, G.scene, G.scene->r.sfra, G.scene->r.efra);
	else
		RE_BlenderFrame(re, G.scene, G.scene->r.cfra);

	if(render_win) window_set_cursor(render_win->win, CURSOR_STD);

	free_filesel_spec(G.scene->r.pic);

	G.afbreek= 0;
	end_test_break_callback();
	
	mainwindow_make_active();
	
	/* after an envmap creation...  */
//		if(R.flag & R_REDRAW_PRV) {
//			BIF_preview_changed(ID_TE);
//		}
	allqueue(REDRAWBUTSSCENE, 0);	// visualize fbuf for example
	
	// before scene update!
	G.rendering= 0;
	
	scene_update_for_newframe(G.scene, G.scene->lay);	// no redraw needed, this restores to view as we left it
	
	waitcursor(0);	// waitcursor checks rendering R.flag...
}

#if 0
/* used for swapping with spare buffer, when images are different size */
static void scalefastrect(unsigned int *recto, unsigned int *rectn, int oldx, int oldy, int newx, int newy)
{
	unsigned int *rect, *newrect;
	int x, y;
	int ofsx, ofsy, stepx, stepy;

	stepx = (int)((65536.0 * (oldx - 1.0) / (newx - 1.0)) + 0.5);
	stepy = (int)((65536.0 * (oldy - 1.0) / (newy - 1.0)) + 0.5);
	ofsy = 32768;
	newrect= rectn;
	
	for (y = newy; y > 0 ; y--){
		rect = recto;
		rect += (ofsy >> 16) * oldx;
		ofsy += stepy;
		ofsx = 32768;
		for (x = newx ; x>0 ; x--){
			*newrect++ = rect[ofsx >> 16];
			ofsx += stepx;
		}
	}
}
#endif

static void renderwin_store_spare(void)
{
	RenderResult rres;
	
	if(render_win==0 || render_win->storespare==0)
		return;

	if(render_win->showspare) {
		render_win->showspare= 0;
		window_set_title(render_win->win, renderwin_get_title(1));
	}
	
	if(render_win->render_text_spare) MEM_freeN(render_win->render_text_spare);
	render_win->render_text_spare= render_win->render_text;
	render_win->render_text= NULL;
	
	if(render_win->rectspare) MEM_freeN(render_win->rectspare);
	render_win->rectspare= NULL;
	if(render_win->rectsparef) MEM_freeN(render_win->rectsparef);
	render_win->rectsparef= NULL;
	
	RE_GetResultImage(RE_GetRender("Render"), &rres);
	
	if(rres.rect32)
		render_win->rectspare= MEM_dupallocN(rres.rect32);
	else if(rres.rectf)
		render_win->rectsparef= MEM_dupallocN(rres.rectf);

	render_win->sparex= rres.rectx;
	render_win->sparey= rres.recty;
}

/* -------------- API: externally called --------------- */

/* not used anywhere ??? */
#if 0
void BIF_renderwin_make_active(void)
{
	if(render_win) {
		window_make_active(render_win->win);
		mywinset(2);
	}
}
#endif



/* set up display, render an image or scene */
void BIF_do_render(int anim)
{
	int slink_flag = 0;

	if (G.f & G_DOSCRIPTLINKS) {
		BPY_do_all_scripts(SCRIPT_RENDER);
		if (!anim) { /* avoid FRAMECHANGED slink in render callback */
			G.f &= ~G_DOSCRIPTLINKS;
			slink_flag = 1;
		}
	}
	
	renderwin_store_spare();

	do_render(anim);

	if(G.scene->use_nodes) {
		allqueue(REDRAWNODE, 1);
		allqueue(REDRAWIMAGE, 1);
	}
	if (slink_flag) G.f |= G_DOSCRIPTLINKS;
	if (G.f & G_DOSCRIPTLINKS) BPY_do_all_scripts(SCRIPT_POSTRENDER);
}

/* set up display, render the current area view in an image */
/* the RE_Render is only used to make sure we got the picture in the result */
void BIF_do_ogl_render(View3D *v3d, int anim)
{
	Render *re= RE_NewRender("Render");
	RenderResult *rr;
	int winx, winy;
	
	G.afbreek= 0;
	init_test_break_callback();
	
	winx= (G.scene->r.size*G.scene->r.xsch)/100;
	winy= (G.scene->r.size*G.scene->r.ysch)/100;
	
	RE_InitState(re, &G.scene->r, winx, winy, NULL);
	
	/* for now, result is defaulting to floats still... */
	rr= RE_GetResult(re);
	if(rr->rect32==NULL)
		rr->rect32= MEM_mallocN(sizeof(int)*winx*winy, "32 bits rects");
	
	/* open window */
	renderwin_init_display_cb(rr);
	init_gl_stuff();
	
	waitcursor(1);

	if(anim) {
		bMovieHandle *mh= BKE_get_movie_handle(G.scene->r.imtype);
		int cfrao= CFRA;
		
		mh->start_movie(&G.scene->r, winx, winy);
		
		for(CFRA= SFRA; CFRA<=EFRA; CFRA++) {
			drawview3d_render(v3d, winx, winy);
			glReadPixels(0, 0, winx, winy, GL_RGBA, GL_UNSIGNED_BYTE, rr->rect32);
			window_swap_buffers(render_win->win);
			
			mh->append_movie(CFRA, rr->rect32, winx, winy);
			if(test_break()) break;
		}
		mh->end_movie();
		CFRA= cfrao;
	}
	else {
		drawview3d_render(v3d, winx, winy);
		glReadPixels(0, 0, winx, winy, GL_RGBA, GL_UNSIGNED_BYTE, rr->rect32);
		window_swap_buffers(render_win->win);
	}
	
	mainwindow_make_active();
	
	if(anim)
		scene_update_for_newframe(G.scene, G.scene->lay);	// no redraw needed, this restores to view as we left it
	
	end_test_break_callback();
	waitcursor(0);
}

void BIF_redraw_render_rect(void)
{
	/* redraw */
	if (render_win) {
		renderwin_queue_redraw(render_win);
	}
}	

void BIF_swap_render_rects(void)
{
	RenderResult rres;
	
	if (render_win==NULL) return;
	
	render_win->storespare= 1;
	render_win->showspare ^= 1;
		
	RE_GetResultImage(RE_GetRender("Render"), &rres);
		
	if(render_win->sparex!=rres.rectx || render_win->sparey!=rres.recty) {
		if(render_win->rectspare) MEM_freeN(render_win->rectspare);
		render_win->rectspare= NULL;
		if(render_win->rectsparef) MEM_freeN(render_win->rectsparef);
		render_win->rectsparef= NULL;
	}
	
	window_set_title(render_win->win, renderwin_get_title(1));

	/* redraw */
	BIF_redraw_render_rect();

}				

/* called from usiblender.c too, to free and close renderwin */
void BIF_close_render_display(void)
{
	if (render_win) {
		if (render_win->info_text) MEM_freeN(render_win->info_text);
		if (render_win->render_text) MEM_freeN(render_win->render_text);
		if (render_win->render_text_spare) MEM_freeN(render_win->render_text_spare);
		if (render_win->rectspare) MEM_freeN(render_win->rectspare);
		if (render_win->rectsparef) MEM_freeN(render_win->rectsparef);
			
		window_destroy(render_win->win); /* ghost close window */
		MEM_freeN(render_win);

		render_win= NULL;
	}
}


/* typical with F11 key, show image or hide/close */
void BIF_toggle_render_display(void) 
{
	
	if (render_win) {
		if(render_win->active) {
			mainwindow_raise();
			mainwindow_make_active();
			render_win->active= 0;
		}
		else {
			window_raise(render_win->win);
			window_make_active(render_win->win);
			render_win->active= 1;
		}
	} 
	else {
		RenderResult *rr= RE_GetResult(RE_GetRender("Render"));
		if(rr) renderwin_init_display_cb(rr);
	}
}

void BIF_renderwin_set_custom_cursor(unsigned char mask[16][2], unsigned char bitmap[16][2])
{
	if (render_win) {
		window_set_custom_cursor(render_win->win, mask, bitmap, 7, 7);
	}
}
