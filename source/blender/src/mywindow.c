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
 *
 *
 * for compatibility with old iris code, replacement of swinopen, winset, etc
 * btw: subwindows in X are way too slow, tried it, and choose for my own system... (ton)
 * 
 */

#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_gsqueue.h"

#include "DNA_screen_types.h"

#include "BKE_global.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"

#include "mydevice.h"
#include "blendef.h"

#include "winlay.h"

typedef struct {
	struct bWindow *next, *prev;
	int id, pad;
	
	int xmin, xmax, ymin, ymax;
	float viewmat[4][4], winmat[4][4];

	GSQueue *qevents;
} bWindow;

/* globals */
static Window *winlay_mainwindow;
static int curswin=0;
static bWindow *swinarray[MAXWIN]= {0};
static bWindow mainwindow, renderwindow;
static int mainwin_color_depth;

void mywindow_init_mainwin(Window *win, int orx, int ory, int sizex, int sizey)
{
	GLint r, g, b;
	
	winlay_mainwindow= win;
	
	swinarray[1]= &mainwindow;
	curswin= 1;

	mainwindow.xmin= orx;
	mainwindow.ymin= ory;
	mainwindow.xmax= orx+sizex-1;
	mainwindow.ymax= ory+sizey-1;
	mainwindow.qevents= NULL;

	myortho2(-0.5, (float)sizex-0.5, -0.6, (float)sizey-0.6);
	glLoadIdentity();
		
	glGetFloatv(GL_PROJECTION_MATRIX, (float *)mainwindow.winmat);
	glGetFloatv(GL_MODELVIEW_MATRIX, (float *)mainwindow.viewmat);
	
	glGetIntegerv(GL_RED_BITS, &r);
	glGetIntegerv(GL_GREEN_BITS, &g);
	glGetIntegerv(GL_BLUE_BITS, &b);

	mainwin_color_depth= r + g + b;
}

/* XXXXXXXXXXXXXXXX very hacky, not allowed to release
 * again after 2.24
 */
void mywindow_build_and_set_renderwin(void)
{
	glGetFloatv(GL_PROJECTION_MATRIX, (float *)renderwindow.winmat);
	glGetFloatv(GL_MODELVIEW_MATRIX, (float *)renderwindow.viewmat);
	
	swinarray[2]= &renderwindow;
	renderwindow.qevents= NULL;

	curswin= 2;
}

/* ------------------------------------------------------------------------- */

	/* XXXXX, remove later */
static bWindow *bwin_from_winid(int winid)
{
	bWindow *bwin= swinarray[winid];
	if (!bwin) {
		printf("bwin_from_winid: Internal error, bad winid: %d\n", winid);
	}
	return bwin;
}

int bwin_qtest(int winid)
{
	return !BLI_gsqueue_is_empty(bwin_from_winid(winid)->qevents);
}
int bwin_qread(int winid, BWinEvent *evt_r)
{
	if (bwin_qtest(winid)) {
		BLI_gsqueue_pop(bwin_from_winid(winid)->qevents, evt_r);
		return 1;
	} else {
		return 0;
	}
}
void bwin_qadd(int winid, BWinEvent *evt)
{
	BLI_gsqueue_push(bwin_from_winid(winid)->qevents, evt);
}

/* ------------------------------------------------------------------------- */

void bwin_get_rect(int winid, rcti *rect_r)
{
	bWindow *win= bwin_from_winid(winid);

	rect_r->xmin= win->xmin;
	rect_r->ymin= win->ymin;
	rect_r->xmax= win->xmax;
	rect_r->ymax= win->ymax;
}

void bwin_getsize(int win, int *x, int *y) 
{
	if(win<4) {
		if (win==1) {
			window_get_size(winlay_mainwindow, x, y);
		} else {
			printf("bwin_getsize: Internal error, bad winid: %d\n", win);
			*x= *y= 0;
		}
	} else {
		bWindow *bwin= swinarray[win];
		if (bwin) {
			*x= bwin->xmax-bwin->xmin+1;
			*y= bwin->ymax-bwin->ymin+1;
		}
	}
}

void bwin_getsuborigin(int win, int *x, int *y)
{
	if(win<4) {
		*x= *y= 0;	
	} else {
		bWindow *bwin= swinarray[win];
		if (bwin) {
			*x= bwin->xmin;
			*y= bwin->ymin;
		}
	}
}

void bwin_getsinglematrix(int winid, float mat[][4])
{
	bWindow *win;
	float matview[4][4], matproj[4][4];

	win= swinarray[winid];
	if(win==0) {
		glGetFloatv(GL_PROJECTION_MATRIX, (float *)matproj);
		glGetFloatv(GL_MODELVIEW_MATRIX, (float *)matview);
		Mat4MulMat4(mat, matview, matproj);
	}
	else {
		Mat4MulMat4(mat, win->viewmat, win->winmat);
	}
}

/* ------------------------------------------------------------------------- */

void bwin_load_viewmatrix(int winid, float mat[][4])
{
	bWindow *win= bwin_from_winid(winid);
	
	glLoadMatrixf(mat);
	Mat4CpyMat4(win->viewmat, mat);
}
void bwin_load_winmatrix(int winid, float mat[][4])
{
	bWindow *win= bwin_from_winid(winid);

	glLoadMatrixf(mat);
	Mat4CpyMat4(win->winmat, mat);
}

void bwin_get_viewmatrix(int winid, float mat[][4])
{
	bWindow *win= bwin_from_winid(winid);
	Mat4CpyMat4(mat, win->viewmat);
}
void bwin_get_winmatrix(int winid, float mat[][4])
{
	bWindow *win= bwin_from_winid(winid);
	Mat4CpyMat4(mat, win->winmat);
}

void bwin_multmatrix(int winid, float mat[][4])
{
	bWindow *win= bwin_from_winid(winid);

	glMultMatrixf((float*) mat);
	glGetFloatv(GL_MODELVIEW_MATRIX, (float *)win->viewmat);
}

void bwin_clear_viewmat(int swin)
{
	bWindow *win;

	win= swinarray[swin];
	if(win==0) return;

	memset(win->viewmat, 0, sizeof(win->viewmat));
	win->viewmat[0][0]= 1.0;
	win->viewmat[1][1]= 1.0;
	win->viewmat[2][2]= 1.0;
	win->viewmat[3][3]= 1.0;
}


void myloadmatrix(float mat[][4])
{
	if (glaGetOneInteger(GL_MATRIX_MODE)==GL_MODELVIEW) {
		bwin_load_viewmatrix(curswin, mat);
	} else {
		bwin_load_winmatrix(curswin, mat);
	}
}

void mygetmatrix(float mat[][4])
{
	if (glaGetOneInteger(GL_MATRIX_MODE)==GL_MODELVIEW) {
		bwin_get_viewmatrix(curswin, mat);
	} else {
		bwin_get_winmatrix(curswin, mat);
	}
}

void mymultmatrix(float mat[][4])
{
	bwin_multmatrix(curswin, mat);
}

void mygetsingmatrix(float mat[][4])
{
	bwin_getsinglematrix(curswin, mat);
}

int mywinget(void)
{
	return curswin;
}

void mywinset(int wid)
{
	bWindow *win;

	win= swinarray[wid];
	if(win==0) {
		printf("mywinset %d: doesn't exist\n", wid);
		return;
	}

	if (wid == 1) {	/* main window */
		glViewport(0,  0, ( win->xmax-win->xmin)+1, ( win->ymax-win->ymin)+1);
		glScissor(0,  0, ( win->xmax-win->xmin)+1, ( win->ymax-win->ymin)+1);
	}
	else {
		int width= (win->xmax - win->xmin)+1;
		int height= (win->ymax - win->ymin)+1;

			/* CRITICAL, this clamping ensures that
			 * the viewport never goes outside the screen
			 * edges (assuming the x, y coords aren't
			 * outside). This causes a hardware lock
			 * on Matrox cards if it happens.
			 * 
			 * Really Blender should never _ever_ try
			 * to do such a thing, but just to be safe
			 * clamp it anyway (or fix the bScreen
			 * scaling routine, and be damn sure you
			 * fixed it). - zr
			 */
		if (win->xmin + width>G.curscreen->sizex)
			width= G.curscreen->sizex - win->xmin;
		if (win->ymin + height>G.curscreen->sizey)
			height= G.curscreen->sizey - win->ymin;
		
		glViewport(win->xmin, win->ymin, width, height);
		glScissor(win->xmin, win->ymin, width, height);
	}
	
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(&win->winmat[0][0]);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(&win->viewmat[0][0]);

	glFinish();
	
	curswin= wid;
}

int myswinopen(int parentid, int xmin, int xmax, int ymin, int ymax)
{
	bWindow *win= NULL;
	int freewinid;
	
	for (freewinid= 4; freewinid<MAXWIN; freewinid++)
		if (!swinarray[freewinid])
			break;
	
	if (freewinid==MAXWIN) {
		printf("too many windows\n");

		return 0;
	} else {
		win= MEM_callocN(sizeof(*win), "winopen");

		win->id= freewinid;
		swinarray[win->id]= win;

		win->xmin= xmin;
		win->ymin= ymin;
		win->xmax= xmax;
		win->ymax= ymax;
	
		win->qevents= BLI_gsqueue_new(sizeof(BWinEvent));

		Mat4One(win->viewmat);
		Mat4One(win->winmat);
	
		mywinset(win->id);

		return win->id;
	}
}

void mywinclose(int winid)
{
	if (winid<4) {
		if (winid==1) {
			window_destroy(winlay_mainwindow);
			winlay_mainwindow= NULL;
		} else {
			printf("mwinclose: Internal error, bad winid: %d\n", winid);
		}
	} else {
		bWindow *win= swinarray[winid];

		if (win) {
			BLI_gsqueue_free(win->qevents);
			MEM_freeN(win);
		} else {
			printf("mwinclose: Internal error, bad winid: %d\n", winid);
		}
	}

	swinarray[winid]= 0;
	if (curswin==winid) curswin= 0;
}

void mywinposition(int winid, int xmin, int xmax, int ymin, int ymax) /* watch: syntax differs from iris */
{
	bWindow *win= bwin_from_winid(winid);
	
	win->xmin= xmin;
	win->ymin= ymin;
	win->xmax= xmax;
	win->ymax= ymax;
}


void bwin_ortho(int winid, float x1, float x2, float y1, float y2, float n, float f)
{
	bWindow *bwin= bwin_from_winid(winid);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(x1, x2, y1, y2, n, f);

	glGetFloatv(GL_PROJECTION_MATRIX, (float *)bwin->winmat);
	glMatrixMode(GL_MODELVIEW);
}

void bwin_ortho2(int win, float x1, float x2, float y1, float y2)
{
	bwin_ortho(win, x1, x2, y1, y2, -1, 1);
}

void bwin_frustum(int winid, float x1, float x2, float y1, float y2, float n, float f)
{
	bWindow *win= bwin_from_winid(winid);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(x1, x2, y1, y2, n, f);

	glGetFloatv(GL_PROJECTION_MATRIX, (float *)win->winmat);
	glMatrixMode(GL_MODELVIEW);
}

void myortho(float x1, float x2, float y1, float y2, float n, float f)
{
	bwin_ortho(curswin, x1, x2, y1, y2, n, f);
}

void myortho2(float x1, float x2, float y1, float y2)
{
	bwin_ortho(curswin, x1, x2, y1, y2, -1, 1);
}

void mywindow(float x1, float x2, float y1, float y2, float n, float f)
{
	bwin_frustum(curswin, x1, x2, y1, y2, n, f);
}

unsigned int index_to_framebuffer(int index)
{
	unsigned int i= index;

	switch(mainwin_color_depth) {
	case 8:
		i= ((i & 48)<<18) + ((i & 12)<<12) + ((i & 3)<<6);
		i |= 0x3F3F3F;
		break;
	case 12:
		i= ((i & 0xF00)<<12) + ((i & 0xF0)<<8) + ((i & 0xF)<<4);
		/* sometimes dithering subtracts! */
		i |= 0x0F0F0F;
		break;
	case 15:
	case 16:
		i= ((i & 0x7C00)<<9) + ((i & 0x3E0)<<6) + ((i & 0x1F)<<3);
		i |= 0x070707;
		break;
	default:
		i= ((i & 0x3F000)<<6) + ((i & 0xFC0)<<4) + ((i & 0x3F)<<2);
		i |= 0x030303;
		break;
	}
	
	return i;
}

int framebuffer_to_index(unsigned int col)
{
	if (col==0) return 0;

	switch(mainwin_color_depth) {
	case 8:
		return ((col & 0xC00000)>>18) + ((col & 0xC000)>>12) + ((col & 0xC0)>>6);
	case 12:
		return ((col & 0xF00000)>>12) + ((col & 0xF000)>>8) + ((col & 0xF0)>>4);
	case 15:
	case 16:
		return ((col & 0xF80000)>>9) + ((col & 0xF800)>>6) + ((col & 0xF8)>>3);
	default:
		return ((col & 0xFC0000)>>6) + ((col & 0xFC00)>>4) + ((col & 0xFC)>>2);
	}		
}


/* ********** END MY WINDOW ************** */

#ifdef WIN32
static int is_a_really_crappy_nvidia_card(void) {
	static int well_is_it= -1;

		/* Do you understand the implication? Do you? */
	if (well_is_it==-1)
		well_is_it= (strcmp((char*) glGetString(GL_VENDOR), "NVIDIA Corporation") == 0);

	return well_is_it;
}
#endif

void myswapbuffers(void)
{
	ScrArea *sa;
	
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->win_swap==WIN_BACK_OK) sa->win_swap= WIN_FRONT_OK;
		if(sa->head_swap==WIN_BACK_OK) sa->head_swap= WIN_FRONT_OK;
		
		sa= sa->next;
	}

	/* HACK, some windows drivers feel they should honor the scissor
	 * test when swapping buffers, disable the test while swapping
	 * on WIN32. (namely Matrox and NVidia's new drivers around Oct 1 2001)
	 * - zr
	 */

#ifdef WIN32
		/* HACK, in some NVidia driver release some kind of
		 * fancy optimiziation (I presume) was put in which for
		 * some reason causes parts of the buffer not to be
		 * swapped. One way to defeat it is the following wierd
		 * code (which we only do for nvidia cards). This should
		 * be removed if NVidia fixes their drivers. - zr
		 */
	if (is_a_really_crappy_nvidia_card()) {
		glDrawBuffer(GL_FRONT);

		glBegin(GL_LINES);
		glEnd();

		glDrawBuffer(GL_BACK);
	}

	glDisable(GL_SCISSOR_TEST);
	window_swap_buffers(winlay_mainwindow);
	glEnable(GL_SCISSOR_TEST);
#else
	window_swap_buffers(winlay_mainwindow);
#endif
}


/* *********************** PATTERNS ETC ***************** */

void setlinestyle(int nr)
{
	if(nr==0) {
		glDisable(GL_LINE_STIPPLE);
	}
	else {
		
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(nr, 0xAAAA);
	}
}

/*******************/
/*******************/
/*  Menu utilities */

static int *frontbuffer_save= NULL;
static int ov_x, ov_y, ov_sx, ov_sy;

void my_put_frontbuffer_image(void)
{
	if (frontbuffer_save) {
		glRasterPos2f( (float)ov_x -0.5,  (float)ov_y - 0.5 );
		glDrawPixels(ov_sx, ov_sy, GL_RGBA, GL_UNSIGNED_BYTE, frontbuffer_save);
		MEM_freeN(frontbuffer_save);
		frontbuffer_save= NULL;
	}
}

void my_get_frontbuffer_image(int x, int y, int sx, int sy)
{
	if(frontbuffer_save) return;

	ov_x= x;
	ov_y= y;
	ov_sx= sx;
	ov_sy= sy;
	
	if(sx>1 && sy>1) {
		frontbuffer_save= MEM_mallocN(sx*sy*4, "temp_frontbuffer_image");
		glReadPixels(x, y, sx, sy, GL_RGBA, GL_UNSIGNED_BYTE, frontbuffer_save);
	}

	#ifdef WIN32
	/* different coord system! */
	y= (G.curscreen->sizey-y);
	
	if(curswin>3) {
		y -= curarea->winrct.ymin;
	}
	#endif
}

int mywin_inmenu(void) {
	return frontbuffer_save?1:0;
}

void mywin_getmenu_rect(int *x, int *y, int *sx, int *sy) {
	*x= ov_x;
	*sx= ov_sx;
	*sy= ov_sy;

#if defined(WIN32) || defined (__BeOS)
	*y= ov_y;
#else
	*y= (G.curscreen->sizey - ov_y) - ov_sy;
#endif	
}
