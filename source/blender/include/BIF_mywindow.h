
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
 * These are the protos for mywindow.c -- an emulation of the
 * (obsolete) IrisGL command set 
 */

#ifndef BIF_MYWINDOW_H
#define BIF_MYWINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct rcti;

/*---*/

int mywinget(void);
void mywinclose(int winid);
void mywinposition(int winid, 
				   int xmin, int xmax, 
				   int ymin, int ymax);
/*---*/

int bwin_qtest(int winid);
unsigned short bwin_qread(int winid, short *val_r, char *ascii_r);
void bwin_qadd(int winid, unsigned short event, short val, char ascii);

/*---*/

void bwin_load_viewmatrix(int winid, float mat[][4]);
void bwin_load_winmatrix(int winid, float mat[][4]);

void bwin_get_viewmatrix(int winid, float mat[][4]);
void bwin_get_winmatrix(int winid, float mat[][4]);

void bwin_ortho(int winid, float x1, float x2, float y1, float y2, float n, float f);
void bwin_ortho2(int win, float x1, float x2, float y1, float y2);
void bwin_frustum(int winid, float x1, float x2, float y1, float y2, float n, float f);

void bwin_getsize(int winid, int *x, int *y);
void bwin_getsuborigin(int winid, int *x, int *y);
void bwin_get_rect(int winid, struct rcti *rect_r);
void bwin_getsinglematrix(int winid, float mat[][4]);
void bwin_clear_viewmat(int winid);

int myswinopen(int parentid, int xmin, int xmax, int ymin, int ymax);
void myswapbuffers(void);

void mygetmatrix(float mat[][4]);
void mymultmatrix(float [][4]);
void myloadmatrix(float mat[][4]);
void mywinset(int wid);
void myortho(float x1, float x2, float y1, float y2, float n, float f);
void myortho2(float x1, float x2, float y1, float y2);
void mywindow(float x1, float x2, float y1, float y2, float n, float f);
void mygetsingmatrix(float (*)[4]);

void setlinestyle(int nr);

void BIF_wait_for_statechange(void);

#define L_MOUSE	1
#define M_MOUSE 2
#define R_MOUSE	4
short get_mbut(void);
short get_qual(void);
void getmouse(short *mval);

void warp_pointer(int x, int y);

int framebuffer_to_index(unsigned int col);
unsigned int index_to_framebuffer(int index);

int mywin_inmenu(void);
void mywin_getmenu_rect(int *x, int *y, int *sx, int *sy);

void my_put_frontbuffer_image(void);
void my_get_frontbuffer_image(int x, int y, int sx, int sy);

#endif

