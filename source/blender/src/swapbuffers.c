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


/* HRMS!!... blender has its own swapbuffers method. for sgi only that worked pretty nice.
 * but with porting to linux and win, with mesa and opengl variations, it all grow
 * out of control.
 * with the introduction of Ghost (2002) we really should bring this back to a single
 * method again. (ton)
 */


#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BKE_global.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"

#include "winlay.h"

#if 0
static void copy_back_to_front(void)
{
	int actually_swap= 0;
	int winx, winy;
	char *data;

	winlay_get_winsize(&winx, &winy);
	
	if (actually_swap) {
		data= malloc(4*winx*winy);
		glReadPixels(0, 0, winx, winy, GL_RGBA, GL_UNSIGNED_BYTE, data);
	}
	
	mywinset(1);
	glRasterPos2f(-0.5,-0.5);
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_FRONT);
	glCopyPixels(0, 0, winx, winy, GL_COLOR);
	glDrawBuffer(GL_BACK);
	glFlush();

	if (actually_swap) {
		glRasterPos2f(-0.5,-0.5);
		glDrawPixels(winx, winy, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glFlush();
		free(data);
	}
}
#endif

static void screen_swapbuffers_REDRAW(bScreen *sc)
{
	ScrArea *sa;
	int doswap= 0, swap;
	
	/* a new implementation: only using redraws and a normal swapbuffer */

	/* all areas front ok? */
	sa= sc->areabase.first;
	while(sa) {
		if(sa->win && (sa->win_swap & WIN_FRONT_OK)==0) break;
		if(sa->headertype==0) sa->head_swap= WIN_EQUAL;
		if((sa->head_swap & WIN_FRONT_OK)==0) break;
		sa= sa->next;
	}
	if(sa==0) return;

	sa= sc->areabase.first;
	while(sa) {
		swap= sa->win_swap;
		if( (swap & WIN_BACK_OK) == 0) {
			scrarea_do_windraw(sa);

			doswap= 1;
			sa->win_swap= swap | WIN_BACK_OK;
		}
		else if( sa->win_swap==WIN_BACK_OK) doswap= 1;
		
		swap= sa->head_swap;
		if( (swap & WIN_BACK_OK) == 0) {
			if (sa->headertype) scrarea_do_headdraw(sa);
			doswap= 1;
			sa->head_swap = swap | WIN_BACK_OK;
		}
		else if( sa->head_swap==WIN_BACK_OK) doswap= 1;
	
		sa= sa->next;
	}

	/* the whole backbuffer should now be ok */
	if(doswap) {
		myswapbuffers();
	}
}

#include "BMF_Api.h"
#include <stdio.h>

static void draw_debug_win(int win)
{
	static int drawcounter= 0;
	char buf[64];
	int x, y;
	int w, h;
	
	bwin_getsuborigin(win, &x, &y);
	bwin_getsize(win, &w, &h);

	mywinset(win);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, 0, h, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	glColor3f(0.8, 0.8, 0.8);
	glRecti(0, 0, w, h);

	glColor3f(0.6, 0.6, 0.6);
	glRecti(2, 2, w-4, h-4);
	
	glColor3ub(0, 0, 0);
	glRasterPos2i(5, 5);
	
	sprintf(buf, "win: %d - (%d, %d, %d, %d) %d\n", win, x, y, w, h, drawcounter++);
	BMF_DrawString(G.font, buf);
}

static void screen_swapbuffers_DEBUG(bScreen *sc)
{
	ScrArea *sa;

	for (sa= sc->areabase.first; sa; sa= sa->next) {
		draw_debug_win(sa->win);
		if (sa->headwin) draw_debug_win(sa->headwin);
	}
	
	myswapbuffers();
}

static void screen_swapbuffers_DEBUG_SWAP(bScreen *sc)
{
	ScrArea *sa;
	int doswap= 0, swap;

	/* a new implementation: only using redraws and a normal swapbuffer */

	/* all areas front ok? */
	sa= sc->areabase.first;
	while(sa) {
		if(sa->win && (sa->win_swap & WIN_FRONT_OK)==0) break;
		if(!sa->headertype) sa->head_swap= WIN_EQUAL;
		if((sa->head_swap & WIN_FRONT_OK)==0) break;
		sa= sa->next;
	}
	if(sa==0) return;

	sa= sc->areabase.first;
	while(sa) {
	
		swap= sa->win_swap;
		if( (swap & WIN_BACK_OK) == 0) {
			scrarea_do_windraw(sa);
			draw_debug_win(sa->win);
			
			doswap= 1;
			sa->win_swap= swap | WIN_BACK_OK;
		}
		else if( sa->win_swap==WIN_BACK_OK) doswap= 1;
		
		swap= sa->head_swap;
		if( (swap & WIN_BACK_OK) == 0) {
			if (sa->headertype) {
				scrarea_do_headdraw(sa);
				draw_debug_win(sa->headwin);
			}
			doswap= 1;
			sa->head_swap = swap | WIN_BACK_OK;
		}
		else if( sa->head_swap==WIN_BACK_OK) doswap= 1;
	
		sa= sa->next;
	}

	if(doswap) {
		myswapbuffers();
	}
}

static void screen_swapbuffers_SIMPLE(bScreen *sc)
{
	ScrArea *sa;
	
	mywinset(1);
	glClearColor(0.8, 0.6, 0.7, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	for (sa= sc->areabase.first; sa; sa= sa->next) {
		scrarea_do_windraw(sa);
		if (sa->headertype) scrarea_do_headdraw(sa);
	}
	
	myswapbuffers();
}

static int drawmode_default= 'r';
int debug_swapbuffers_override= 0;

void set_debug_swapbuffers_ovveride(bScreen *sc, int mode)
{
	ScrArea *sa;
	for (sa= sc->areabase.first; sa; sa= sa->next) {
		sa->win_swap= 0;
		sa->head_swap= 0;
	}
	debug_swapbuffers_override= mode;
}

void screen_swapbuffers(void)
{
	ScrArea *tempsa;

	bScreen *sc= G.curscreen;
	int drawmode;
	
	if (debug_swapbuffers_override) {
		drawmode= debug_swapbuffers_override;
	} else {
		drawmode= drawmode_default;
	}
	
	{
		static int count = 3000;

		count = (++count)%5200;

		if (count==51) {
			void mainqenter(unsigned short event, short val);
			markdirty_all();
			mainqenter(0x4001, 1);
		} else if (count<51) {
			extern double BLI_drand(void);
			float aspect = (float) G.curscreen->sizex/G.curscreen->sizey;
			extern signed char monkeyf[][4];
			extern signed char monkeyv[][3];
			extern int monkeyo, monkeynv, monkeynf;
			float fac, x = (BLI_drand()*2-1)*.9, y = (BLI_drand()*2-1)*.9;

			float (*verts)[3] = malloc(sizeof(*verts)*monkeynv*2);
			int i;

			for (i=0; i<monkeynv; i++) {
				float *v = verts[i];
				v[0]= (monkeyv[i][0]+127)/128.0, v[1]= monkeyv[i][1]/128.0, v[2]= monkeyv[i][2]/128.0;
			}

			areawinset(1);

			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(-1, 1, -1, 1, -1, 1);
			glScalef(1, aspect, 1);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glTranslatef(x, y, 0);
			glScalef(.5, .5, .5);

			fac = (BLI_drand()+.1)*.5;
			glScalef(fac, fac, fac);

			glColor3f(BLI_drand(),BLI_drand(),BLI_drand());
			glBegin(GL_QUADS);
			for (i=0; i<monkeynf; i++) {
				int i0 = monkeyf[i][0]+i-monkeyo;
				float *v0 = verts[i0];
				int i1 = monkeyf[i][1]+i-monkeyo;
				float *v1 = verts[i1];
				int i2 = monkeyf[i][2]+i-monkeyo;
				float *v2 = verts[i2];
				int i3 = monkeyf[i][3]+i-monkeyo;
				float *v3 = verts[i3];
				
				glVertex3fv(v0); glVertex3fv(v1); glVertex3fv(v2); glVertex3fv(v3);
				glVertex2f(-v0[0],v0[1]); 
				glVertex2f(-v1[0],v1[1]); 
				glVertex2f(-v2[0],v2[1]); 
				glVertex2f(-v3[0],v3[1]); 
			}
			glEnd();

			free(verts);

			myswapbuffers();
			myswapbuffers();
		}
	}

	tempsa= curarea;
	areawinset(1);
	
	if (drawmode=='s') {
		screen_swapbuffers_SIMPLE(sc);
	} else if (drawmode=='d') {
		screen_swapbuffers_DEBUG(sc);
	} else if (drawmode=='f') {
		screen_swapbuffers_DEBUG_SWAP(sc);
	} else {
		screen_swapbuffers_REDRAW(sc);
	}

	areawinset(tempsa->win);
}
