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
#include <string.h>
#include <stdarg.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BLI_blenlib.h"

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_vec_types.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_graphics.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"
#include "BIF_renderwin.h"
#include "BIF_toets.h"

#include "BDR_editobject.h"

#include "BSE_view.h"
#include "BSE_drawview.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"

#include "blendertimer.h"

#include "blendef.h"
#include "mydevice.h"
#include "winlay.h"
#include "render.h"

/***/

typedef struct {
	Window *win;

	float zoom, zoomofs[2];
	int active;
	
	int mbut[3];
	int lmouse[2];
	
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
	unsigned int flags;
	
	float pan_mouse_start[2], pan_ofs_start[2];

	char *info_text;
} RenderWin;

static RenderWin *renderwin_new(Window *win)
{
	RenderWin *rw= MEM_mallocN(sizeof(*rw), "RenderWin");
	rw->win= win;
	rw->zoom= 1.0;
	rw->active= 0;
	rw->flags= 0;
	rw->zoomofs[0]= rw->zoomofs[1]= 0;
	rw->info_text= NULL;
	
	rw->lmouse[0]= rw->lmouse[1]= 0;
	rw->mbut[0]= rw->mbut[1]= rw->mbut[2]= 0;

	return rw;
}
static void renderwin_destroy(RenderWin *rw)
{
	if (rw->info_text) MEM_freeN(rw->info_text);
	window_destroy(rw->win);
	MEM_freeN(rw);
}

/***/

static void close_renderwin(void);

static RenderWin *render_win= NULL;

/**/

static void renderwin_queue_redraw(RenderWin *rw)
{
	window_queue_redraw(rw->win);
}

static void renderwin_reshape(RenderWin *rw)
{
	;
}

static void renderwin_get_disprect(RenderWin *rw, float disprect_r[2][2])
{
	float display_w, display_h;
	float cent_x, cent_y;
	int w, h;

	window_get_size(rw->win, &w, &h);

	display_w= R.rectx*rw->zoom;
	display_h= R.recty*rw->zoom;
	cent_x= (rw->zoomofs[0] + R.rectx/2)*rw->zoom;
	cent_y= (rw->zoomofs[1] + R.recty/2)*rw->zoom;
	
	disprect_r[0][0]= w/2 - cent_x;
	disprect_r[0][1]= h/2 - cent_y;
	disprect_r[1][0]= disprect_r[0][0] + display_w;
	disprect_r[1][1]= disprect_r[0][1] + display_h;
}

	/** 
	 * Project window coordinate to image pixel coordinate.
	 * Returns true if resulting coordinate is within image.
	 */
static int renderwin_win_to_image_co(RenderWin *rw, int winco[2], int imgco_r[2])
{
	float disprect[2][2];
	
	renderwin_get_disprect(rw, disprect);
	
	imgco_r[0]= (int) ((winco[0]-disprect[0][0])/rw->zoom);
	imgco_r[1]= (int) ((winco[1]-disprect[0][1])/rw->zoom);
	
	return (imgco_r[0]>=0 && imgco_r[1]>=0 && imgco_r[0]<R.rectx && imgco_r[1]<R.recty);
}

	/**
	 * Project window coordinates to normalized device coordinates
	 * Returns true if resulting coordinate is within window.
	 */
static int renderwin_win_to_ndc(RenderWin *rw, int win_co[2], float ndc_r[2])
{
	int w, h;

	window_get_size(rw->win, &w, &h);

	ndc_r[0]= (float) ((win_co[0]*2)/(w-1) - 1.0);
	ndc_r[1]= (float) ((win_co[1]*2)/(h-1) - 1.0);

	return (fabs(ndc_r[0])<=1.0 && fabs(ndc_r[1])<=1.0);
}

static void renderwin_set_infotext(RenderWin *rw, char *info_text)
{
	if (rw->info_text) MEM_freeN(rw->info_text);
	rw->info_text= info_text?BLI_strdup(info_text):NULL;
}

static void renderwin_draw(RenderWin *rw, int just_clear)
{
	float disprect[2][2];
	rcti rect;

	rect.xmin= rect.ymin= 0;
	window_get_size(rw->win, &rect.xmax, &rect.ymax);
	renderwin_get_disprect(rw, disprect);
	
	window_make_active(rw->win);
	
	glEnable(GL_SCISSOR_TEST);
	glaDefine2DArea(&rect);
	
	glClearColor(.1875, .1875, .1875, 1.0); 
	glClear(GL_COLOR_BUFFER_BIT);

	if (just_clear || !R.rectot) {
		glColor3ub(0, 0, 0);
		glRectfv(disprect[0], disprect[1]);
	} else {
		glPixelZoom(rw->zoom, rw->zoom);
		glaDrawPixelsSafe(disprect[0][0], disprect[0][1], R.rectx, R.recty, R.rectot);
		glPixelZoom(1.0, 1.0);
	}
	
	if (rw->info_text) {
		glColor3ub(255, 255, 255);
		glRasterPos2i(10, 10);
		BMF_DrawString(G.font, rw->info_text);
	}

	window_swap_buffers(rw->win);
}

	/* XXX, this is not good, we do this without any regard to state
	 * ... better is to make this an optimization of a more clear
	 * implementation. the bug shows up when you do something like
	 * open the window, then draw part of the progress, then get
	 * a redraw event. whatever can go wrong will.
	 */
static void renderwin_progress(RenderWin *rw, int start_y, int nlines, int rect_w, int rect_h, unsigned char *rect)
{
	float disprect[2][2];
	rcti win_rct;

	win_rct.xmin= win_rct.ymin= 0;
	window_get_size(rw->win, &win_rct.xmax, &win_rct.ymax);
	renderwin_get_disprect(rw, disprect);

	window_make_active(rw->win);

	glEnable(GL_SCISSOR_TEST);
	glaDefine2DArea(&win_rct);

	glDrawBuffer(GL_FRONT);
	glPixelZoom(rw->zoom, rw->zoom);
	glaDrawPixelsSafe(disprect[0][0], disprect[0][1] + start_y*rw->zoom, rect_w, nlines, &rect[start_y*rect_w*4]);
	glPixelZoom(1.0, 1.0);
	glFlush();
	glDrawBuffer(GL_BACK);
}

static void renderwin_mouse_moved(RenderWin *rw)
{
	if (rw->flags&RW_FLAGS_PIXEL_EXAMINING) {
		int imgco[2];
		char buf[64];

		if (R.rectot && renderwin_win_to_image_co(rw, rw->lmouse, imgco)) {
			unsigned char *pxl= (char*) &R.rectot[R.rectx*imgco[1] + imgco[0]];

			sprintf(buf, "R: %d, G: %d, B: %d, A: %d", pxl[0], pxl[1], pxl[2], pxl[3]);
			renderwin_set_infotext(rw, buf);
			renderwin_queue_redraw(rw);
		} else {
			renderwin_set_infotext(rw, NULL);
			renderwin_queue_redraw(rw);
		}
	} else if (rw->flags&RW_FLAGS_PANNING) {
		int delta_x= rw->lmouse[0] - rw->pan_mouse_start[0];
		int delta_y= rw->lmouse[1] - rw->pan_mouse_start[1];
	
		rw->zoomofs[0]= rw->pan_ofs_start[0] - delta_x/rw->zoom;
		rw->zoomofs[1]= rw->pan_ofs_start[1] - delta_y/rw->zoom;
		rw->zoomofs[0]= CLAMPIS(rw->zoomofs[0], -R.rectx/2, R.rectx/2);
		rw->zoomofs[1]= CLAMPIS(rw->zoomofs[1], -R.recty/2, R.recty/2);

		renderwin_queue_redraw(rw);
	} else if (rw->flags&RW_FLAGS_OLDZOOM) {
		float ndc[2];
		int w, h;

		window_get_size(rw->win, &w, &h);
		renderwin_win_to_ndc(rw, rw->lmouse, ndc);

		rw->zoomofs[0]= -0.5*ndc[0]*(w-R.rectx*rw->zoom)/rw->zoom;
		rw->zoomofs[1]= -0.5*ndc[1]*(h-R.recty*rw->zoom)/rw->zoom;

		renderwin_queue_redraw(rw);
	}
}

static void renderwin_mousebut_changed(RenderWin *rw)
{
	if (rw->mbut[0]) {
		rw->flags|= RW_FLAGS_PIXEL_EXAMINING;
	} else if (rw->mbut[1]) {
		rw->flags|= RW_FLAGS_PANNING;
		rw->pan_mouse_start[0]= rw->lmouse[0];
		rw->pan_mouse_start[1]= rw->lmouse[1];
		rw->pan_ofs_start[0]= rw->zoomofs[0];
		rw->pan_ofs_start[1]= rw->zoomofs[1];
	} else {
		if (rw->flags&RW_FLAGS_PANNING) {
			rw->flags&= ~RW_FLAGS_PANNING;
			renderwin_queue_redraw(rw);
		}
		if (rw->flags&RW_FLAGS_PIXEL_EXAMINING) {
			rw->flags&= ~RW_FLAGS_PIXEL_EXAMINING;
			renderwin_set_infotext(rw, NULL);
			renderwin_queue_redraw(rw);
		}
	}
}

static void renderwin_reset_view(RenderWin *rw)
{
	if (rw->info_text) renderwin_set_infotext(rw, NULL);

	rw->zoom= 1.0;
	rw->zoomofs[0]= rw->zoomofs[1]= 0;
	renderwin_queue_redraw(rw);
}

static void renderwin_handler(Window *win, void *user_data, short evt, short val, char ascii)
{
	RenderWin *rw= user_data;
	
	if (evt==RESHAPE) {
		renderwin_reshape(rw);
	} else if (evt==REDRAW) {
		renderwin_draw(rw, 0);
#ifndef __APPLE__
	} else if (evt==WINCLOSE) {
		close_renderwin();
#endif
	} else if (evt==INPUTCHANGE) {
		rw->active= val;
		
		if (!val && (rw->flags&RW_FLAGS_OLDZOOM)) {
			rw->flags&= ~RW_FLAGS_OLDZOOM;
			renderwin_reset_view(rw);
		}
	} else if (ELEM(evt, MOUSEX, MOUSEY)) {
		rw->lmouse[evt==MOUSEY]= val;
		renderwin_mouse_moved(rw);
	} else if (ELEM3(evt, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE)) {
		int which= (evt==LEFTMOUSE)?0:(evt==MIDDLEMOUSE)?1:2;
		rw->mbut[which]= val;
		renderwin_mousebut_changed(rw);
	} else if (val) {
		if (evt==ESCKEY) {
			if (rw->flags&RW_FLAGS_OLDZOOM) {
				rw->flags&= ~RW_FLAGS_OLDZOOM;
				renderwin_reset_view(rw);
			} else {
				rw->flags|= RW_FLAGS_ESCAPE;
				mainwindow_raise();
			}
		} else if (evt==JKEY) {
			BIF_swap_render_rects();
		} else if (evt==ZKEY) {
			if (rw->flags&RW_FLAGS_OLDZOOM) {
				rw->flags&= ~RW_FLAGS_OLDZOOM;
				renderwin_reset_view(rw);
			} else {
				rw->zoom= 2.0;
				rw->flags|= RW_FLAGS_OLDZOOM;
				renderwin_mouse_moved(rw);
			}
		} else if (evt==PADPLUSKEY) {
			if (rw->zoom<15.9) {
				rw->zoom*= 2.0;
				renderwin_queue_redraw(rw);
			}
		} else if (evt==PADMINUS) {
			if (rw->zoom>0.26) {
				rw->zoom*= 0.5;
				renderwin_queue_redraw(rw);
			}
		} else if (evt==PADENTER || evt==HOMEKEY) {
			if (rw->flags&RW_FLAGS_OLDZOOM) {
				rw->flags&= ~RW_FLAGS_OLDZOOM;
			}
			renderwin_reset_view(rw);
		} else if (evt==F3KEY) {
			mainwindow_raise();
			mainwindow_make_active();
			areawinset(find_biggest_area()->win);
			BIF_save_rendered_image();
		} else if (evt==F11KEY) {
			BIF_toggle_render_display();
		} else if (evt==F12KEY) {
			BIF_do_render(0);
		}
	}
}

/**/

	/* Render window render callbacks */

void calc_renderwin_rectangle(int posmask, int renderpos_r[2], int rendersize_r[2]) 
{
	int scr_w, scr_h, x, y, div= 0;
	float ndc_x= 0.0, ndc_y= 0.0;

		/* XXX, we temporarily hack the screen size and position so
		 * the window is always 30 pixels away from a side, really need
		 * a GHOST_GetMaxWindowBounds or so - zr
		 */
	winlay_get_screensize(&scr_w, &scr_h);
	
	rendersize_r[0]= (G.scene->r.size*G.scene->r.xsch)/100;
	rendersize_r[1]= (G.scene->r.size*G.scene->r.ysch)/100;
	if(G.scene->r.mode & R_PANORAMA) {
		rendersize_r[0]*= G.scene->r.xparts;
		rendersize_r[1]*= G.scene->r.yparts;
	}

	rendersize_r[0]= CLAMPIS(rendersize_r[0], 100, scr_w-90);
	rendersize_r[1]= CLAMPIS(rendersize_r[1], 100, scr_h-90);

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
		
	renderpos_r[0]= 60 + (scr_w-90-rendersize_r[0])*(ndc_x*0.5 + 0.5);
	renderpos_r[1]= 60 + (scr_h-90-rendersize_r[1])*(ndc_y*0.5 + 0.5);
}
	
static void open_renderwin(int winpos[2], int winsize[2])
{
	Window *win;

	win= window_open("Blender:Render", winpos[0], winpos[1], winsize[0], winsize[1], 0);
			
	render_win= renderwin_new(win);
			
	window_set_handler(win, renderwin_handler, render_win);

	winlay_process_events(0);
	window_make_active(render_win->win);
	winlay_process_events(0);
			
	renderwin_draw(render_win, 1);
	renderwin_draw(render_win, 1);
}

static void close_renderwin(void)
{
	if (render_win) {
		renderwin_destroy(render_win);
		render_win= NULL;
	}
}

static void renderwin_init_display_cb(void) 
{
	if (G.afbreek == 0) {
		int rendersize[2], renderpos[2];

		calc_renderwin_rectangle(R.winpos, renderpos, rendersize);

		if (!render_win) {
			open_renderwin(renderpos, rendersize);
		} else {
			int win_x, win_y;
			int win_w, win_h;

			window_get_position(render_win->win, &win_x, &win_y);
			window_get_size(render_win->win, &win_w, &win_h);

				/* XXX, this is nasty and I guess bound to cause problems,
				 * but to ensure the window is at the user specified position
				 * and size we reopen the window all the time... we need
				 * a ghost _set_position to fix this -zr
				 */
			close_renderwin();
			open_renderwin(renderpos, rendersize);

			renderwin_reset_view(render_win);
			render_win->flags&= ~RW_FLAGS_ESCAPE;
		}
	}
}
static void renderwin_clear_display_cb(short ignore) 
{
	if (render_win) {
		window_make_active(render_win->win);	
		renderwin_draw(render_win, 1);
	}
}

static void renderwin_progress_display_cb(int y1, int y2, int w, int h, unsigned int *rect) 
{
	if (render_win) {
		renderwin_progress(render_win, y1, y2-y1+1, w, h, (unsigned char*) rect);
	}
}

	/* Render view render callbacks */

static View3D *render_view3d = NULL;

static void renderview_init_display_cb(void)
{
	ScrArea *sa;

		/* Choose the first view with a persp camera,
		 * if one doesn't exist we will get the first
		 * View3D window.
		 */ 
	render_view3d= NULL;
	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if (sa->win && sa->spacetype==SPACE_VIEW3D) {
			View3D *vd= sa->spacedata.first;
			
			if (vd->persp==2 && vd->camera==G.scene->camera) {
				render_view3d= vd;
				break;
			} else if (!render_view3d) {
				render_view3d= vd;
			}
		}
	}
}

static void renderview_progress_display_cb(int y1, int y2, int w, int h, unsigned int *rect)
{
	if (render_view3d) {
		View3D *v3d= render_view3d;
		int nlines= y2-y1+1;
		float sx, sy, facx, facy;
		rcti win_rct, vb;

		calc_viewborder(v3d, &vb);

		facx= (float) (vb.xmax-vb.xmin)/R.rectx;
		facy= (float) (vb.ymax-vb.ymin)/R.recty;

		bwin_get_rect(v3d->area->win, &win_rct);

		glaDefine2DArea(&win_rct);
	
		glDrawBuffer(GL_FRONT);
		
		sx= vb.xmin;
		sy= vb.ymin + facy*y1;

		glPixelZoom(facx, facy);
		glaDrawPixelsSafe(sx, sy, w, nlines, rect+w*y1);
		glPixelZoom(1.0, 1.0);

		glFlush();
		glDrawBuffer(GL_BACK);
		
		v3d->flag |= V3D_DISPIMAGE;
					
		v3d->area->win_swap= WIN_FRONT_OK;
	}
}

	/* Shared render callbacks */

static int test_break(void)
{
	if (!G.afbreek) {
		if (MISC_test_break()) {
			;
		} else if (render_win) {
			winlay_process_events(0);
			// render_win can be closed in winlay_process_events()
			if (render_win == 0 || (render_win->flags & RW_FLAGS_ESCAPE))
				G.afbreek= 1;
		}
	}
	
	return G.afbreek;
}

static void printrenderinfo_cb(double time, int sample)
{
	extern int mem_in_use;
	float megs_used_memory= mem_in_use/(1024.0*1024.0);
	char str[300], tstr[32], *spos= str;
		
	timestr(time, tstr);
	spos+= sprintf(spos, "RENDER  Fra:%d  Ve:%d Fa:%d La:%d", (G.scene->r.cfra), R.totvert, R.totvlak, R.totlamp);
	spos+= sprintf(spos, "Mem:%.2fM Time:%s ", megs_used_memory, tstr);

	if (R.r.mode & R_FIELDS) {
		spos+= sprintf(spos, "Field %c ", (R.flag&R_SEC_FIELD)?'B':'A');
	}
	if (sample!=-1) {
		spos+= sprintf(spos, "Sample: %d ", sample);
	}
	
	screen_draw_info_text(G.curscreen, str);
}
	
/***/

void BIF_renderwin_set_custom_cursor(unsigned char mask[16][2], unsigned char bitmap[16][2])
{
	if (render_win) {
		window_set_custom_cursor(render_win->win, mask, bitmap);
	}
}

static void do_crap(int force_dispwin)
{
	if (R.displaymode == R_DISPLAYWIN || force_dispwin) {
		RE_set_initrenderdisplay_callback(NULL);
		RE_set_clearrenderdisplay_callback(renderwin_clear_display_cb);	
		RE_set_renderdisplay_callback(renderwin_progress_display_cb);

		renderwin_init_display_cb();
	} else {
		BIF_close_render_display();
		
		RE_set_initrenderdisplay_callback(renderview_init_display_cb);
		RE_set_clearrenderdisplay_callback(NULL);	
		RE_set_renderdisplay_callback(renderview_progress_display_cb);
	}

	RE_set_test_break_callback(test_break);	
	RE_set_timecursor_callback(set_timecursor);
	RE_set_printrenderinfo_callback(printrenderinfo_cb);
}

static void do_render(View3D *ogl_render_view3d, int anim, int force_dispwin)
{
	do_crap(force_dispwin);
	
	if (render_win) window_set_cursor(render_win->win, CURSOR_WAIT);
	waitcursor(1);

	G.afbreek= 0;
	if(G.obedit && !(G.scene->r.scemode & R_OGL)) {
		exit_editmode(0);	/* 0 = geen freedata */
	}

	if(anim) {
		RE_animrender(ogl_render_view3d);
	}
	else {
		RE_initrender(ogl_render_view3d);
	}
	update_for_newframe();
	R.flag= 0;
	
	if (render_win) window_set_cursor(render_win->win, CURSOR_STD);
	waitcursor(0);

	free_filesel_spec(G.scene->r.pic);

	G.afbreek= 0;

	mainwindow_make_active();
}

void BIF_do_render(int anim)
{
	do_render(NULL, anim, 0);
}

void BIF_do_ogl_render(View3D *ogl_render_view3d, int anim)
{
	G.scene->r.scemode |= R_OGL;
	do_render(ogl_render_view3d, anim, 1);
	G.scene->r.scemode &= ~R_OGL;
}

static ScrArea *find_dispimage_v3d(void)
{
	ScrArea *sa;
	
	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if (sa->spacetype==SPACE_VIEW3D) {
			View3D *vd= sa->spacedata.first;
			if (vd->flag & V3D_DISPIMAGE)
				return sa;
		}
	}
	
	return NULL;
}

static void redraw_render_display(void)
{
	if (R.displaymode == R_DISPLAYWIN) {
			// don't open render_win if rendering has been
			// canceled or the render_win has been actively closed
		if (render_win) {
			renderwin_queue_redraw(render_win);
		}
	} else {
		renderview_init_display_cb();
		renderview_progress_display_cb(0, R.recty-1, R.rectx, R.recty, R.rectot);
	}
}

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

void BIF_swap_render_rects(void)
{
	unsigned int *temp;

	if(R.rectspare==0) {
		R.rectspare= (unsigned int *)MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");
		R.sparex= R.rectx;
		R.sparey= R.recty;
	}
	else if(R.sparex!=R.rectx || R.sparey!=R.recty) {
		temp= (unsigned int *)MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");
					
		scalefastrect(R.rectspare, temp, R.sparex, R.sparey, R.rectx, R.recty);
		MEM_freeN(R.rectspare);
		R.rectspare= temp;
					
		R.sparex= R.rectx;
		R.sparey= R.recty;
	}
	
	temp= R.rectot;
	R.rectot= R.rectspare;
	R.rectspare= temp;

	redraw_render_display();
}				

void BIF_toggle_render_display(void)
{
	ScrArea *sa= find_dispimage_v3d();
	
	if (render_win && render_win->active) {
		if (R.displaymode == R_DISPLAYVIEW) {
			BIF_close_render_display();
		}
		mainwindow_raise();
	} else if (sa) {
		View3D *vd= sa->spacedata.first;
		vd->flag &= ~V3D_DISPIMAGE;
		scrarea_queue_winredraw(sa);
	} else {
		if (R.displaymode == R_DISPLAYWIN) {
			renderwin_init_display_cb();
		} else {
			if (render_win) {
				BIF_close_render_display();
			}
			renderview_init_display_cb();
			renderview_progress_display_cb(0, R.recty-1, R.rectx, R.recty, R.rectot);
		}
	}
}

void BIF_close_render_display(void)
{
	close_renderwin();
}
