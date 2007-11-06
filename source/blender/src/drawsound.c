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

#include <math.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BIF_gl.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_editsound.h"
#include "BIF_resources.h"

#include "BSE_drawipo.h"
#include "BSE_time.h"
#include "BMF_Api.h"

#include "blendef.h"

/* local */
void drawsoundspace(ScrArea *sa, void *spacedata);

/*implementation */
static void draw_wave(int startsamp, int endsamp, short sampdx, short offset, short *sp, float sampfac, float y)
{
	float min, max, v1[2], v2[3];
	int i, j, deltasp, value; /*deltasp, value: were both shorts but for music files 5min, zooming out cased a crash */
	
	sp+= offset*startsamp;

	deltasp= offset*sampdx;
	
	glBegin(GL_LINES);
	for(i=startsamp; i<endsamp; i+=sampdx, sp+=deltasp) {
	
		/* filter */
		min= max= 0.0;
		for(j=0; j<sampdx; j++) {
			value= sp[offset*j];
			if(value < min) min= value;
			else if(value > max) max= value;
		}
		v1[1]= y + 0.002*min;
		v2[1]= y + 0.002*max;
		
		v1[0]=v2[0]= sampfac*i;

		glVertex2fv(v1);
		glVertex2fv(v2);
	}
	glEnd();
}

static void draw_sample(bSample *sample)
{
	float sampxlen, sampfac;
	int samples, startsamp, endsamp;
	short *sp, sampdx;
	
	/* one sample is where in v2d space? (v2d space in frames!) */
	sampfac= FPS/(sample->rate);

	/* how many samples? */
	samples= sample->len/(sample->channels*(sample->bits/8));
	/* total len in v2d space */
	sampxlen= sampfac*samples;

	/* one pixel is how many samples? */
	sampdx= (samples*((G.v2d->cur.xmax-G.v2d->cur.xmin)/sampxlen))/curarea->winx;

	if(sampdx==0) sampdx= 1;
	
	/* start and and */
	startsamp = G.v2d->cur.xmin/sampfac;
	CLAMP(startsamp, 0, samples-1);
	endsamp= G.v2d->cur.xmax/sampfac;
	CLAMP(endsamp, 0, samples-1);
	endsamp-= sampdx;
	
	/* set 'tot' for sliders */
	G.v2d->tot.xmax= sampfac*samples;

	/* channels? */
	if(sample->channels==2) {
		
		cpack(0x905050);
		sp= (short *)(sample->data);
		draw_wave(startsamp, endsamp, sampdx, 2, sp, sampfac, 85.0);

		cpack(0x506890);
		sp++;
		draw_wave(startsamp, endsamp, sampdx, 2, sp, sampfac, 190.0);
	}
	else {
		cpack(0x905050);
		sp= (short *)(sample->data);

		draw_wave(startsamp, endsamp, sampdx, 1, sp, sampfac, 128.0);		
	}
}

static void draw_cfra_sound(SpaceSound *ssound)
{
	float vec[2];
	
	if(ssound->flag & SND_CFRA_NUM) {
		short mval[2];
		float x,  y;
		char str[32];
		/* little box with frame */
		
		getmouseco_areawin(mval);
		
		if(mval[1]>curarea->winy-10) mval[1]= curarea->winy - 13;
			
		if (curarea->winy < 25) {	
			if (mval[1]<17) mval[1]= 17;
		} else if (mval[1]<22) mval[1]= 22;
		
		areamouseco_to_ipoco(G.v2d, mval, &x, &y);
		
		if(ssound->flag & SND_DRAWFRAMES) 
			sprintf(str, "   %d\n", CFRA);
		else sprintf(str, "   %.2f\n", FRA2TIME(CFRA));
		
		glRasterPos2f(x, y);
		glColor3ub(0, 0, 0);
		BMF_DrawString(G.font, str);

	}

	vec[0]=  (G.scene->r.cfra);
	vec[0]*= G.scene->r.framelen;

	vec[1]= G.v2d->cur.ymin;
	glColor3ub(0x20, 0x90, 0x20);
	glLineWidth(4.0);

	glBegin(GL_LINE_STRIP);
		glVertex2fv(vec);
		vec[1]= G.v2d->cur.ymax;
		glVertex2fv(vec);
	glEnd();
	
	glLineWidth(1.0);
	
}

void drawsoundspace(ScrArea *sa, void *spacedata)
{
	float col[3];
	short ofsx, ofsy;
	
	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	calc_scrollrcts(sa, G.v2d, curarea->winx, curarea->winy);

	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {	
			ofsx= curarea->winrct.xmin;	/* because mywin */
			ofsy= curarea->winrct.ymin;
			glViewport(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1); 
			glScissor(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
		}
	}

	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);

	/* boundbox_seq(); */
	calc_ipogrid();	
	draw_ipogrid();

	if (G.ssound->sound) {
		sound_initialize_sample(G.ssound->sound);
		draw_sample(G.ssound->sound->sample);
	}
	
	draw_cfra_sound(spacedata);
	draw_markers_timespace(0);

	/* restore viewport */
	mywinset(curarea->win);

	/* ortho at pixel level curarea */
	myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);

	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {
			drawscroll(0);
		}
	}
	
	myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);
	draw_area_emboss(sa);
	curarea->win_swap= WIN_BACK_OK;
}
