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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
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
#include "BLI_threads.h"

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "DNA_image_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_vec_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"
#include "BKE_writeavi.h"	/* movie handle */

#include "BIF_drawimage.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_graphics.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_writeimage.h"

#include "BDR_sculptmode.h"
#include "BDR_editobject.h"
#include "BPY_extern.h" /* for BPY_do_all_scripts */

#include "BSE_view.h"
#include "BSE_drawview.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"

#include "RE_pipeline.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "GPU_draw.h"

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


typedef struct {
	Window *win;

	int rectx, recty;	/* size of image */
	
	float zoom, zoomofs[2];
	int active;
	
	int mbut[5];
	int lmouse[2];
	
	unsigned int flags;
	
	float pan_mouse_start[2], pan_ofs_start[2];

	char *info_text;
	
} RenderWin;

typedef struct RenderSpare {
	ImBuf *ibuf;
	
	short storespare, showspare;
	char *render_text_spare;
} RenderSpare;

static RenderWin *render_win= NULL;
static RenderSpare *render_spare= NULL;
static char *render_text= NULL;

/* --------------- help functions for RenderWin struct ---------------------------- */

static RenderSpare *renderspare_alloc()
{
	RenderSpare *rspare= MEM_callocN(sizeof(*rspare), "RenderSpare");
	rspare->render_text_spare= MEM_callocN(RW_MAXTEXT, "rendertext spare");

	return rspare;
}

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

	rw->lmouse[0]= rw->lmouse[1]= 0;
	rw->mbut[0]= rw->mbut[1]= rw->mbut[2]= rw->mbut[3] = rw->mbut[4] = 0;

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
		
		str= BIF_render_text();
		
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
	Image *ima;
	ImBuf *ibuf;
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
		RenderSpare *rspare= render_spare;
		
		if(rspare && rspare->showspare) {
			ibuf= rspare->ibuf;
		}
		else {
			ima= BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");
			ibuf= BKE_image_get_ibuf(ima, NULL);
		}
		
		if(ibuf) {
			if(!ibuf->rect)
				IMB_rect_from_float(ibuf);
			
			glPixelZoom(rw->zoom, rw->zoom);
			if(rw->flags & RW_FLAGS_ALPHA) {
				if(ibuf->rect) {
					/* swap bytes, so alpha is most significant one, then just draw it as luminance int */
					if(G.order==B_ENDIAN)
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
					glaDrawPixelsSafe(fullrect[0][0], fullrect[0][1], ibuf->x, ibuf->y, ibuf->x, GL_LUMINANCE, GL_UNSIGNED_INT, ibuf->rect);
					glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
				}
				else {
					float *trectf= MEM_mallocN(ibuf->x*ibuf->y*4, "temp");
					int a, b;
					
					for(a= ibuf->x*ibuf->y -1, b= 4*a+3; a>=0; a--, b-=4)
						trectf[a]= ibuf->rect_float[b];
					
					glaDrawPixelsSafe(fullrect[0][0], fullrect[0][1], ibuf->x, ibuf->y, ibuf->x, GL_LUMINANCE, GL_FLOAT, trectf);
					MEM_freeN(trectf);
				}
			}
			else {
				if(ibuf->rect)
					glaDrawPixelsSafe(fullrect[0][0], fullrect[0][1], ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
				else if(ibuf->rect_float)
					glaDrawPixelsSafe_to32(fullrect[0][0], fullrect[0][1], ibuf->x, ibuf->y, ibuf->x, ibuf->rect_float);
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
static void renderwin_zoom(RenderWin *rw, int ZoomIn) {
	if (ZoomIn) {
		if (rw->zoom>0.26) {
			if(rw->zoom>1.0 && rw->zoom<2.0) rw->zoom= 1.0;
			else rw->zoom*= 0.5;
		}
	} else {
		if (rw->zoom<15.9) {
			if(rw->zoom>0.5 && rw->zoom<1.0) rw->zoom= 1.0;
			else rw->zoom*= 2.0;
		}
	}
	if (rw->zoom>1.0) rw->flags |= RW_FLAGS_OLDZOOM;
	if (rw->zoom==1.0) rw->flags &= ~RW_FLAGS_OLDZOOM;
	renderwin_queue_redraw(rw);
}

#define FTOCHAR(val) val<=0.0f? 0 : (val>=(1.0f-0.5f/255.0f)? 255 :(char)((255.0f*val)+0.5f))

static void renderwin_mouse_moved(RenderWin *rw)
{
	Image *ima;
	ImBuf *ibuf;
	RenderSpare *rspare= render_spare;
		
	if(rspare && rspare->showspare) {
		ibuf= rspare->ibuf;
	}
	else {
		ima= BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");
		ibuf= BKE_image_get_ibuf(ima, NULL);
	}

	if(!ibuf)
		return;

	if (rw->flags & RW_FLAGS_PIXEL_EXAMINING) {
		int imgco[2], ofs=0;
		char buf[128];
		char *pxl;

		if (renderwin_win_to_image_co(rw, rw->lmouse, imgco)) {
			ofs= sprintf(buf, "X: %d Y: %d ", imgco[0], imgco[1]);
			if (ibuf->rect) {
				pxl= (char*) &ibuf->rect[ibuf->x*imgco[1] + imgco[0]];
				ofs+= sprintf(buf+ofs, " | R: %d G: %d B: %d A: %d", pxl[0], pxl[1], pxl[2], pxl[3]);	
			}
			if (ibuf->rect_float) {
				float *pxlf= ibuf->rect_float + 4*(ibuf->x*imgco[1] + imgco[0]);
				if(!ibuf->rect) {
					ofs+= sprintf(buf+ofs, " | R: %d G: %d B: %d A: %d", FTOCHAR(pxlf[0]), FTOCHAR(pxlf[1]), FTOCHAR(pxlf[2]), FTOCHAR(pxlf[3]));
				}
				ofs+= sprintf(buf+ofs, " | R: %.3f G: %.3f B: %.3f A: %.3f ", pxlf[0], pxlf[1], pxlf[2], pxlf[3]);
			}
			if (ibuf->zbuf_float) {
				float *pxlz= &ibuf->zbuf_float[ibuf->x*imgco[1] + imgco[0]];			
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
		rw->zoomofs[0]= CLAMPIS(rw->zoomofs[0], -ibuf->x/2, ibuf->x/2);
		rw->zoomofs[1]= CLAMPIS(rw->zoomofs[1], -ibuf->y/2, ibuf->y/2);

		renderwin_queue_redraw(rw);
	} 
	else if (rw->flags & RW_FLAGS_OLDZOOM) {
		float ndc[2];
		int w, h;

		window_get_size(rw->win, &w, &h);
		h-= RW_HEADERY;
		renderwin_win_to_ndc(rw, rw->lmouse, ndc);

		rw->zoomofs[0]= -0.5*ndc[0]*(w-ibuf->x*rw->zoom)/rw->zoom;
		rw->zoomofs[1]= -0.5*ndc[1]*(h-ibuf->y*rw->zoom)/rw->zoom;

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
	} else if (rw->mbut[3]) {
		renderwin_zoom(rw, 0);
		rw->mbut[3]=0;
	} else if (rw->mbut[4]) {
		renderwin_zoom(rw, 1);
		rw->mbut[4]=0;
	} else {
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
	else if (ELEM(evt, WHEELUPMOUSE, WHEELDOWNMOUSE)) {
		int which=(evt==WHEELUPMOUSE?3:4); 
		rw->mbut[which]=val;
		renderwin_mousebut_changed(rw);
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
		else if (ELEM(evt,PADPLUSKEY,PAGEUPKEY))  {
			renderwin_zoom(rw, 0);
		} 
		else if (ELEM(evt,PADMINUS,PAGEDOWNKEY)) {
			renderwin_zoom(rw, 1);
		} 
		else if (evt==PADENTER || evt==HOMEKEY) {
			if (rw->flags&RW_FLAGS_OLDZOOM) {
				rw->flags&= ~RW_FLAGS_OLDZOOM;
			}
			renderwin_reset_view(rw);
		} 
		else if (evt==F3KEY) {
			if(G.rendering==0) {
				mainwindow_raise();
				mainwindow_make_active();
				rw->active= 0;
				areawinset(find_biggest_area()->win);
				BIF_save_rendered_image_fs();
			}
		} 
		else if (evt==F11KEY) {
			BIF_toggle_render_display();
		} 
		else if (evt==F12KEY) {
			if(G.rendering==0) 
				BIF_do_render(0);
		}
	}
}

static char *renderwin_get_title()
{
	char *title="";
	
	if(BIF_show_render_spare()) {
		if (G.scene->r.renderer==R_YAFRAY) title = "YafRay:Render (previous)";
		else title = "Blender:Render (previous)";
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
	
	title= renderwin_get_title();
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
	GPU_state_init();
	
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
static void renderwin_progress(RenderWin *rw, RenderResult *rr, volatile rcti *renrect)
{
	rcti win_rct;
	float *rectf= NULL, fullrect[2][2];
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
	
	/* renderwindow cruft */
	win_rct.xmin= win_rct.ymin= 0;
	window_get_size(rw->win, &win_rct.xmax, &win_rct.ymax);
	win_rct.ymax-= RW_HEADERY;
	renderwin_get_fullrect(rw, fullrect);
	
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
	fullrect[0][0] += (rr->tilerect.xmin + rr->crop + xmin)*rw->zoom;
	fullrect[0][1] += (rr->tilerect.ymin + rr->crop + ymin)*rw->zoom;

	glEnable(GL_SCISSOR_TEST);
	glaDefine2DArea(&win_rct);

#ifdef __APPLE__
#else
	glDrawBuffer(GL_FRONT);
#endif
	glPixelZoom(rw->zoom, rw->zoom);

	if(rect32)
		glaDrawPixelsSafe(fullrect[0][0], fullrect[0][1], xmax, ymax, rr->rectx, GL_RGBA, GL_UNSIGNED_BYTE, rect32);
	else
		glaDrawPixelsSafe_to32(fullrect[0][0], fullrect[0][1], xmax, ymax, rr->rectx, rectf);
	
	glPixelZoom(1.0, 1.0);
	
#ifdef __APPLE__
	window_swap_buffers(render_win->win);
#else
	glFlush();
	glDrawBuffer(GL_BACK);
#endif	
}


/* in render window; display a couple of scanlines of rendered image */
static void renderwin_progress_display_cb(RenderResult *rr, volatile rcti *rect)
{
	if (render_win) {
		renderwin_progress(render_win, rr, rect);
	}
}

/* -------------- callbacks for render loop: interactivity ----------------------- */

/* string is RW_MAXTEXT chars min */
void make_renderinfo_string(RenderStats *rs, char *str)
{
	extern char info_time_str[32];	// header_info.c
	uintptr_t mem_in_use, mmap_in_use;
	float megs_used_memory, mmap_used_memory;
	char *spos= str;
	
	mem_in_use= MEM_get_memory_in_use();
	mmap_in_use= MEM_get_mapped_memory_in_use();

	megs_used_memory= (mem_in_use-mmap_in_use)/(1024.0*1024.0);
	mmap_used_memory= (mmap_in_use)/(1024.0*1024.0);
	
	if(G.scene->lay & 0xFF000000)
		spos+= sprintf(spos, "Localview | ");
	else if(G.scene->r.scemode & R_SINGLE_LAYER)
		spos+= sprintf(spos, "Single Layer | ");
	
	spos+= sprintf(spos, "Fra:%d  Ve:%d Fa:%d ", (G.scene->r.cfra), rs->totvert, rs->totface);
	if(rs->tothalo) spos+= sprintf(spos, "Ha:%d ", rs->tothalo);
	if(rs->totstrand) spos+= sprintf(spos, "St:%d ", rs->totstrand);
	spos+= sprintf(spos, "La:%d Mem:%.2fM (%.2fM) ", rs->totlamp, megs_used_memory, mmap_used_memory);

	if(rs->curfield)
		spos+= sprintf(spos, "Field %d ", rs->curfield);
	if(rs->curblur)
		spos+= sprintf(spos, "Blur %d ", rs->curblur);
	
	BLI_timestr(rs->lastframetime, info_time_str);
	spos+= sprintf(spos, "Time:%s ", info_time_str);
	
	if(rs->infostr)
		spos+= sprintf(spos, "| %s ", rs->infostr);
	
	/* very weak... but 512 characters is quite safe... we cannot malloc during thread render */
	if(spos >= str+RW_MAXTEXT)
		printf("WARNING! renderwin text beyond limit \n");
	
}

/* callback for print info in top header of renderwin */
static void renderwin_renderinfo_cb(RenderStats *rs)
{
	
	if(render_win) {
		
		BIF_make_render_text(rs);
		
#ifdef __APPLE__
#else
		glDrawBuffer(GL_FRONT);
#endif
		renderwin_draw_render_info(render_win);
		
#ifdef __APPLE__
		window_swap_buffers(render_win->win);
#else
		glFlush();
		glDrawBuffer(GL_BACK);
#endif
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
/* all other OS's support signal(SIGVTALRM/SIGALRM) */

/* XXX The ESC problem: some unix users reported that ESC doesn't cancel
 * renders anymore. Most complaints came from linux, but it's not
 * general, not all linux users have the problem.
 *
 * From tests, the systems that do have it are not signalling SIGVTALRM
 * interrupts (an issue with signals and threads). Using SIGALRM instead
 * fixes the problem, at least while we investigate better.
 *
 * ITIMER_REAL (SIGALRM): timer that counts real system time
 * ITIMER_VIRTUAL (SIGVTALRM): only counts time spent in its owner process
 *
 * Addendum: now SIGVTALRM is used on Solaris again, because SIGALRM can
 * kill the process there! */

/* POSIX: this function goes in the signal() callback */
static void interruptESC(int sig)
{

	if(G.afbreek==0) G.afbreek= 2;	/* code for read queue */

	/* call again, timer was reset */
#ifdef __sun
	signal(SIGVTALRM, interruptESC);
#else
	signal(SIGALRM, interruptESC);
#endif
}

/* POSIX: initialize timer and signal */
static void init_test_break_callback()
{

	struct itimerval tmevalue;

	tmevalue.it_interval.tv_sec = 0;
	tmevalue.it_interval.tv_usec = 250000;
	/* when the first ? */
	tmevalue.it_value.tv_sec = 0;
	tmevalue.it_value.tv_usec = 10000;

#ifdef __sun
	signal(SIGVTALRM, interruptESC);
	setitimer(ITIMER_VIRTUAL, &tmevalue, 0);
#else
	signal(SIGALRM, interruptESC);
	setitimer(ITIMER_REAL, &tmevalue, 0);
#endif
}

/* POSIX: stop timer and callback */
static void end_test_break_callback()
{
	struct itimerval tmevalue;

	memset(&tmevalue, 0, sizeof(struct itimerval));

#ifdef __sun
	setitimer(ITIMER_VIRTUAL, &tmevalue, 0);
	signal(SIGVTALRM, SIG_IGN);
#else
	setitimer(ITIMER_REAL, &tmevalue, 0);
	signal(SIGALRM, SIG_IGN);
#endif
}


#endif



/* -------------- callbacks for render loop: init & run! ----------------------- */


/* - initialize displays
   - set callbacks
   - cleanup
*/

static void do_render(int anim)
{
	Image *ima;
	Render *re= RE_NewRender(G.scene->id.name);
	unsigned int lay= G.scene->lay;
	int scemode= G.scene->r.scemode;
	int sculptmode= G.f & G_SCULPTMODE;
	
	/* UGLY! we set this flag to prevent renderwindow queue to execute another render */
	/* is reset in RE_BlenderFrame */
	G.rendering= 1;

	/* set render callbacks, also starts ESC timer */
	BIF_init_render_callbacks(re, 1);
	
	waitcursor(1);
	if(render_win) 
		window_set_cursor(render_win->win, CURSOR_WAIT);
	
	if(G.obedit)
		exit_editmode(0);	/* 0 = no free data */

	if(sculptmode) set_sculptmode();

	/* allow localview render for objects with lights in normal layers */
	if(curarea->spacetype==SPACE_VIEW3D) {
		/* if view is defined (might not be if called from script), check and set layers. */
		if(G.vd) {
			if(G.vd->lay & 0xFF000000) {
				G.scene->lay |= G.vd->lay;
				G.scene->r.scemode |= R_SINGLE_LAYER;
			}
			else G.scene->lay= G.vd->lay;
		}
	}
	
	if(anim)
		RE_BlenderAnim(re, G.scene, G.scene->r.sfra, G.scene->r.efra);
	else
		RE_BlenderFrame(re, G.scene, G.scene->r.cfra);

	/* restore local view exception */
	G.scene->lay= lay;
	G.scene->r.scemode= scemode;
	
	if(render_win) window_set_cursor(render_win->win, CURSOR_STD);
	
	free_filesel_spec(G.scene->r.pic);

	G.afbreek= 0;
	BIF_end_render_callbacks();
	
	/* after an envmap creation...  */
//		if(R.flag & R_REDRAW_PRV) {
//			BIF_preview_changed(ID_TE);
//		}
		
	scene_update_for_newframe(G.scene, G.scene->lay);	// no redraw needed, this restores to view as we left it
	
	/* get a render result image, and make sure it is clean */
	ima= BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");
	BKE_image_signal(ima, NULL, IMA_SIGNAL_FREE);
	
	if(sculptmode) set_sculptmode();
	
	waitcursor(0);
}

/* called before render, store old render in spare buffer */
static int render_store_spare(void)
{
	RenderResult rres;
	RenderSpare *rspare= render_spare;
	
	if(rspare==0 || rspare->storespare==0)
		return 0;
	
	/* only store when it does not show spare */
	if(rspare->showspare==0)
		return 0;
	
	rspare->showspare= 0;

	if(rspare->ibuf) {
		IMB_freeImBuf(rspare->ibuf);
		rspare->ibuf= NULL;
	}
	
	RE_GetResultImage(RE_GetRender(G.scene->id.name), &rres);

	rspare->ibuf= IMB_allocImBuf(rres.rectx, rres.recty, 32, 0, 0);
	rspare->ibuf->dither= G.scene->r.dither_intensity;
	
	if(rres.rect32) {
		rspare->ibuf->rect= MEM_dupallocN(rres.rect32);
		rspare->ibuf->flags |= IB_rect;
		rspare->ibuf->mall |= IB_rect;
	}
	if(rres.rectf) {
		rspare->ibuf->rect_float= MEM_dupallocN(rres.rectf);
		rspare->ibuf->flags |= IB_rectfloat;
		rspare->ibuf->mall |= IB_rectfloat;
	}
	if(rres.rectz) {
		rspare->ibuf->zbuf_float= MEM_dupallocN(rres.rectz);
		rspare->ibuf->flags |= IB_zbuffloat;
		rspare->ibuf->mall |= IB_zbuffloat;
	}

	return 1;
}

/* -------------- API: externally called --------------- */

static void error_cb(char *str){error(str);}
static int esc_timer_set= 0;

/* set callbacks, exported to sequence render too. 
   Only call in foreground (UI) renders. */

void BIF_init_render_callbacks(Render *re, int do_display)
{
	if(do_display) {
		if(G.displaymode!=R_DISPLAYWIN) {
			if(render_win)
				BIF_close_render_display();
			imagewindow_render_callbacks(re);
		}
		else {
			RE_display_init_cb(re, renderwin_init_display_cb);
			RE_display_draw_cb(re, renderwin_progress_display_cb);
			RE_display_clear_cb(re, renderwin_clear_display_cb);
			RE_stats_draw_cb(re, renderwin_renderinfo_cb);
		}
	}
	
	RE_error_cb(re, error_cb);
	
	G.afbreek= 0;
	if(render_win)
		render_win->flags &= ~RW_FLAGS_ESCAPE;

	/* start esc timer. ensure it happens once only */
	if(esc_timer_set==0)
		init_test_break_callback();
	esc_timer_set++;
	
	RE_test_break_cb(re, test_break);
	RE_timecursor_cb(re, set_timecursor);
	
}

/* the init/end callbacks can be called multiple times (sequence render) */
void BIF_end_render_callbacks(void)
{
	esc_timer_set--;
	if(esc_timer_set==0) {
		end_test_break_callback();
		
		if(render_win)
			mainwindow_make_active();
	}
}

void BIF_store_spare(void)
{
	if(render_store_spare()) {
		if(render_text)
			BLI_strncpy(render_spare->render_text_spare, render_text, RW_MAXTEXT);

		if(render_win)
			window_set_title(render_win->win, renderwin_get_title());
		allqueue(REDRAWIMAGE, 0);
	}
}

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
	
	BIF_store_spare();

	do_render(anim);

	if(G.scene->use_nodes) {
		allqueue(REDRAWNODE, 1);
		allqueue(REDRAWIMAGE, 1);
	}
	if(G.scene->r.dither_intensity != 0.0f)
		BIF_redraw_render_rect();
	if (slink_flag) G.f |= G_DOSCRIPTLINKS;
	if (G.f & G_DOSCRIPTLINKS) BPY_do_all_scripts(SCRIPT_POSTRENDER);
}

void do_ogl_view3d_render(Render *re, View3D *v3d, int winx, int winy)
{
	float winmat[4][4];

	update_for_newframe_muted();	/* here, since camera can be animated */

	if(v3d->persp==V3D_CAMOB && v3d->camera) {
		/* in camera view, use actual render winmat */
		RE_GetCameraWindow(re, v3d->camera, CFRA, winmat);
		drawview3d_render(v3d, NULL, winx, winy, winmat, 0);
	}
	else
		drawview3d_render(v3d, NULL, winx, winy, NULL, 0);
}

/* set up display, render the current area view in an image */
/* the RE_Render is only used to make sure we got the picture in the result */
void BIF_do_ogl_render(View3D *v3d, int anim)
{
	Render *re= RE_NewRender(G.scene->id.name);
	RenderResult *rr;
	int winx, winy;
	
	G.afbreek= 0;
	init_test_break_callback();
	
	winx= (G.scene->r.size*G.scene->r.xsch)/100;
	winy= (G.scene->r.size*G.scene->r.ysch)/100;
	
	RE_InitState(re, NULL, &G.scene->r, winx, winy, NULL);

	/* for now, result is defaulting to floats still... */
	rr= RE_GetResult(re);
	if(rr->rect32==NULL)
		rr->rect32= MEM_mallocN(sizeof(int)*winx*winy, "32 bits rects");
	
	/* open window */
	renderwin_init_display_cb(rr);
	if(render_win)
		render_win->flags &= ~RW_FLAGS_ESCAPE;

	GPU_state_init();
	
	waitcursor(1);

	if(anim) {
		bMovieHandle *mh= BKE_get_movie_handle(G.scene->r.imtype);
		int cfrao= CFRA;
		
		if(BKE_imtype_is_movie(G.scene->r.imtype))
			mh->start_movie(&G.scene->r, winx, winy);
		
		for(CFRA= SFRA; CFRA<=EFRA; CFRA++) {
			/* user event can close window */
			if(render_win==NULL)
				break;

			do_ogl_view3d_render(re, v3d, winx, winy);
			glReadPixels(0, 0, winx, winy, GL_RGBA, GL_UNSIGNED_BYTE, rr->rect32);
			if((G.scene->r.scemode & R_STAMP_INFO) && (G.scene->r.stamp & R_STAMP_DRAW)) {
				BKE_stamp_buf((unsigned char *)rr->rect32, rr->rectf, rr->rectx, rr->recty, 3);
			}
			window_swap_buffers(render_win->win);
			
			if(BKE_imtype_is_movie(G.scene->r.imtype)) {
				mh->append_movie(CFRA, rr->rect32, winx, winy);
				printf("Append frame %d", G.scene->r.cfra);
			}
			else {
				ImBuf *ibuf= IMB_allocImBuf(winx, winy, G.scene->r.planes, 0, 0);
				char name[FILE_MAXDIR+FILE_MAXFILE];
				int ok;
				
				BKE_makepicstring(name, G.scene->r.pic, G.scene->r.cfra, G.scene->r.imtype);

				ibuf->rect= (unsigned int *)rr->rect32;
				ok= BKE_write_ibuf(ibuf, name, G.scene->r.imtype, G.scene->r.subimtype, G.scene->r.quality);
				
				if(ok==0) {
					printf("Write error: cannot save %s\n", name);
					break;
				}
				else printf("Saved: %s", name);
				
                /* imbuf knows which rects are not part of ibuf */
				IMB_freeImBuf(ibuf);	
			}
			/* movie stats prints have no line break */
			printf("\n");
			
			if(test_break()) break;
		}
		
		if(BKE_imtype_is_movie(G.scene->r.imtype))
			mh->end_movie();
		
		CFRA= cfrao;
	}
	else {
		do_ogl_view3d_render(re, v3d, winx, winy);
		glReadPixels(0, 0, winx, winy, GL_RGBA, GL_UNSIGNED_BYTE, rr->rect32);
		if((G.scene->r.scemode & R_STAMP_INFO) && (G.scene->r.stamp & R_STAMP_DRAW)) {
			BKE_stamp_buf((unsigned char *)rr->rect32, rr->rectf, rr->rectx, rr->recty, 3);
		}
		window_swap_buffers(render_win->win);
	}
	
	if(render_win)
		renderwin_draw(render_win, 0);

	mainwindow_make_active();
	
	if(anim)
		scene_update_for_newframe(G.scene, G.scene->lay);	// no redraw needed, this restores to view as we left it
	
	end_test_break_callback();
	waitcursor(0);
}

void BIF_redraw_render_rect(void)
{
	/* redraw */
	if(render_win)
		renderwin_queue_redraw(render_win);
	allqueue(REDRAWIMAGE, 0);
}	

void BIF_swap_render_rects(void)
{
	RenderResult rres;
	RenderSpare *rspare;
	ImBuf *ibuf;
	
	if(!render_spare)
		render_spare= renderspare_alloc();

	rspare= render_spare;
	rspare->storespare= 1;
	rspare->showspare ^= 1;
		
	RE_GetResultImage(RE_GetRender(G.scene->id.name), &rres);
	
	ibuf= rspare->ibuf;
	if(ibuf && (ibuf->x!=rres.rectx || ibuf->y!=rres.recty)) {
		IMB_freeImBuf(ibuf);
		rspare->ibuf= NULL;
	}
	
	if(render_win)
		window_set_title(render_win->win, renderwin_get_title());
	
	/* redraw */
	BIF_redraw_render_rect();
}				

ImBuf *BIF_render_spare_imbuf()
{
	return (render_spare)? render_spare->ibuf: NULL;
}

int BIF_show_render_spare()
{
	return (render_spare && render_spare->showspare);
}

char *BIF_render_text()
{
	if(render_spare && render_spare->showspare)
		return render_spare->render_text_spare;
	else
		return render_text;
}

void BIF_make_render_text(RenderStats *rs)
{
	if(!render_text)
		render_text= MEM_callocN(RW_MAXTEXT, "rendertext");
	make_renderinfo_string(rs, render_text);
}

/* called from usiblender.c too, to free and close renderwin */

void BIF_free_render_spare()
{
	RenderSpare *rspare= render_spare;

	if(render_text) {
		MEM_freeN(render_text);
		render_text= NULL;
	}

	if(rspare) {
		if (rspare->render_text_spare) MEM_freeN(rspare->render_text_spare);
		if (rspare->ibuf) IMB_freeImBuf(rspare->ibuf);
		
		MEM_freeN(rspare);
		render_spare= NULL;
	}
}
	
void BIF_close_render_display(void)
{
	if (render_win) {
		if (render_win->info_text) MEM_freeN(render_win->info_text);
		window_destroy(render_win->win); /* ghost close window */
		MEM_freeN(render_win);

		render_win= NULL;
	}
}


/* typical with F11 key, show image or hide/close */
void BIF_toggle_render_display(void) 
{
	
	if (G.displaymode!=R_DISPLAYWIN) {
		imagewindow_toggle_render();
	}
	else {
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
			RenderResult *rr= RE_GetResult(RE_GetRender(G.scene->id.name));
			if(rr) renderwin_init_display_cb(rr);
		}
	}
}

void BIF_renderwin_set_custom_cursor(unsigned char mask[16][2], unsigned char bitmap[16][2])
{
	if (render_win) {
		window_set_custom_cursor(render_win->win, mask, bitmap, 7, 7);
	}
}
