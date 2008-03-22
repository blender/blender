
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

struct rcti;

/*---*/

typedef struct BWinEvent {
	unsigned short	event;
	short			val;
	char			ascii;
} BWinEvent;

/*---*/

int mywinget(void);
void mywinclose(int winid);
void mywinposition(int winid, 
				   int xmin, int xmax, 
				   int ymin, int ymax);
/*---*/

	/** Test if there are events available on a BWin queue.
	 *
	 * @param winid The ID of the window to query.
	 * @return True if there is an event available for _qread'ing.
	 */
int bwin_qtest(int winid);

	/** Read an event off of the BWin queue (if available).
	 *
	 * @param winid The ID of the window to read from.
	 * @param event_r A pointer to return the event in. 
	 * @return True if an event was read and @a event_r filled.
	 */
int bwin_qread(int winid, BWinEvent *event_r);

	/** Add an event to the BWin queue.
	 *
	 * @param winid The ID of the window to add to.
	 * @param event A pointer to copy the event from.
	 */
void bwin_qadd(int winid, BWinEvent *event);

/*---*/

void bwin_load_viewmatrix(int winid, float mat[][4]);
void bwin_load_winmatrix(int winid, float mat[][4]);

void bwin_get_viewmatrix(int winid, float mat[][4]);
void bwin_get_winmatrix(int winid, float mat[][4]);

void bwin_multmatrix(int winid, float mat[][4]);
void bwin_scalematrix(int winid, float x, float y, float z);

void bwin_ortho(int winid, float x1, float x2, float y1, float y2, float n, float f);
void bwin_ortho2(int win, float x1, float x2, float y1, float y2);
void bwin_frustum(int winid, float x1, float x2, float y1, float y2, float n, float f);

void bwin_getsize(int winid, int *x, int *y);
void bwin_getsuborigin(int winid, int *x, int *y);
void bwin_get_rect(int winid, struct rcti *rect_r);
void bwin_getsinglematrix(int winid, float mat[][4]);
void bwin_clear_viewmat(int winid);

int myswinopen(int parentid, int xmin, int xmax, int ymin, int ymax);
int myswinopen_allowed(void);
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

void getndof(float *sbval);
void filterNDOFvalues(float *sbval);

float get_pressure(void);
void get_tilt(float *xtilt, float *ytilt);
#define DEV_MOUSE	0
#define DEV_STYLUS	1
#define DEV_ERASER	2
short get_activedevice(void);

void warp_pointer(int x, int y);

int framebuffer_to_index(unsigned int col);
void set_framebuffer_index_color(int index);

int mywin_inmenu(void);
void mywin_getmenu_rect(int *x, int *y, int *sx, int *sy);

void my_put_frontbuffer_image(void);
void my_get_frontbuffer_image(int x, int y, int sx, int sy);

#endif

