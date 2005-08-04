/**
 * $Id:
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
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

#include "DNA_action_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BKE_ipo.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BIF_gl.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_resources.h"

#include "BSE_drawipo.h"
#include "BSE_view.h"
#include "BMF_Api.h"

#include "blendef.h"

/* ---- prototypes ------ */
void drawtimespace(ScrArea *, void *);


static void draw_cfra_time(SpaceTime *stime)
{
	float vec[2];
	
	vec[0]=  (G.scene->r.cfra);
	vec[0]*= G.scene->r.framelen;

	vec[1]= G.v2d->cur.ymin;
	glColor3ub(0x60, 0xc0, 0x40);	// no theme, should be global color once...
	glLineWidth(3.0);

	glBegin(GL_LINES);
		glVertex2fv(vec);
		vec[1]= G.v2d->cur.ymax;
		glVertex2fv(vec);
	glEnd();
	
	glLineWidth(1.0);
	
	if(stime->flag & TIME_CFRA_NUM) {
		short mval[2];
		float x,  y;
		char str[32];
		/* little box with frame */
		
		glFlush();	// huhh... without this glColor won't work for the text...
		getmouseco_areawin(mval);
		
		if(mval[1]>curarea->winy-10) mval[1]= curarea->winy - 13;
		
		if (curarea->winy < 25) {	
			if (mval[1]<17) mval[1]= 17;
		} else if (mval[1]<22) mval[1]= 22;
		
		areamouseco_to_ipoco(G.v2d, mval, &x, &y);
		
		if(stime->flag & TIME_DRAWFRAMES) 
			sprintf(str, "   %d\n", (G.scene->r.cfra));
		else sprintf(str, "   %.2f\n", (G.scene->r.cfra/(float)G.scene->r.frs_sec));
		
		/* HACK! somehow the green color won't go away... */
		glColor4ub(0, 0, 0, 0);
		BIF_ThemeColor(TH_TEXT);
		
		glRasterPos2f(x, y);
		BMF_DrawString(G.fonts, str);
		
	}
	
}

static void draw_marker(TimeMarker *marker)
{
	float xpos, col[3];
	float xspace, yspace, xpixels, ypixels;

	xpos = marker->frame;
	/* no time correction for framelen! space is drawn with old values */
	
	xspace= G.v2d->cur.xmax - G.v2d->cur.xmin;
	yspace= G.v2d->cur.ymax - G.v2d->cur.ymin;
	xpixels= G.v2d->mask.xmax-G.v2d->mask.xmin;
	ypixels= G.v2d->mask.ymax-G.v2d->mask.ymin;

	BIF_GetThemeColor3fv(TH_BACK, col);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);			
	
	/* 5 px to offset icon to align properly, space / pixels corrects for zoom */
	if(marker->flag & SELECT)
		BIF_draw_icon_blended(xpos-(5.0*(xspace/xpixels)), 12.0*yspace/ypixels, ICON_MARKER_HLT, (int)col, 0);
	else
		BIF_draw_icon_blended(xpos-(5.0*(xspace/xpixels)), 12.0*yspace/ypixels, ICON_MARKER, (int)col, 0);
	
	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);		

	/* and the marker name too, shifted slightly to the top-right */
	BIF_ThemeColor(TH_TEXT);
	glRasterPos2f(xpos+(4.0*(xspace/xpixels)), 17.0*yspace/ypixels);
		
	BMF_DrawString(G.font, marker->name);
}

static void draw_markers_time(SpaceTime *stime)
{
	TimeMarker *marker;

	/* unselected markers are drawn at the first time */
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		if(!(marker->flag & SELECT)) draw_marker(marker);
	}

	/* selected markers are drawn later ... selected markers have to cover unselected
	 * markers laying at the same position as selected markers
	 * (jiri: it is hack, it could be solved better) */
	for(marker= G.scene->markers.first; marker; marker= marker->next) {
		if(marker->flag & SELECT) draw_marker(marker);
	}
}

static void draw_sfra_efra()
{
	BIF_ThemeColorShade(TH_BACK, -25);
	
	if (G.scene->r.sfra < G.scene->r.efra) {
		glRectf(G.v2d->cur.xmin, G.v2d->cur.ymin, G.scene->r.sfra, G.v2d->cur.ymax);
		
		glRectf(G.scene->r.efra, G.v2d->cur.ymin, G.v2d->cur.xmax, G.v2d->cur.ymax);	
	} else {
		glRectf(G.v2d->cur.xmin, G.v2d->cur.ymin, G.v2d->cur.xmax, G.v2d->cur.ymax);
	}
	
	BIF_ThemeColorShade(TH_BACK, -60);
	/* thin lines where the actual frames are */
	fdrawline(G.scene->r.sfra, G.v2d->cur.ymin, G.scene->r.sfra, G.v2d->cur.ymax);
	fdrawline(G.scene->r.efra, G.v2d->cur.ymin, G.scene->r.efra, G.v2d->cur.ymax);
	
	glDisable(GL_BLEND);
}

/*draw all the keys in a list (elems) as lines */
static void draw_key_list(ListBase elems, char col[3]) 
{
	CfraElem *ce;
	float drawframe;

	ce= elems.first;
	while(ce) {
		drawframe = ce->cfra; //not correct for G.scene->r.framelen;
		glColor3ub(col[0], col[1], col[2]);

		fdrawline(drawframe, G.v2d->cur.ymin, drawframe, G.v2d->cur.ymax);
		
		ce= ce->next;
	}
}

static void draw_ob_keys()
{
	/*mostly copied from drawobject.c, draw_object() */
	Object *ob;
	bActionChannel *achan;
	bAction *act;
	ListBase elems;
	int a;
	char col[3];

	if (OBACT) {
		ob = OBACT;
		
		if(ob) {
			if(ob!=G.obedit) {
				if(ob->ipo) {
					/* convert the ipo to a list of 'current frame elements' */
						
					elems.first= elems.last= NULL;
					make_cfra_list(ob->ipo, &elems);
					
					/* draw the list of current frame elements */
					col[0] = 0xDD; col[1] = 0xD7; col[2] = 0x00;
					draw_key_list(elems, col);
					
					BLI_freelistN(&elems);
				}
				
				if(ob->action) {
					act = ob->action;

					/* go through each channel in the action */
					for (achan=act->chanbase.first; achan; achan=achan->next){
						/* convert the ipo to a list of 'current frame elements' */
						
						elems.first= elems.last= NULL;
						make_cfra_list(achan->ipo, &elems);

						col[0] = 0x00; col[1] = 0x82; col[2] = 0x8B;
						draw_key_list(elems, col);
						
						BLI_freelistN(&elems);
					}
				}
				
				for(a=0; a<ob->totcol; a++) {
					Material *ma= give_current_material(ob, a+1);
					
					if(ma && ma->ipo) {
						elems.first= elems.last= NULL;
						make_cfra_list(ma->ipo, &elems);
						
						col[0] = 0xDD; col[1] = 0xA7; col[2] = 0x00;
						draw_key_list(elems, col);
						
						BLI_freelistN(&elems);
					}
				}
				
			}
		}
	}
}

void drawtimespace(ScrArea *sa, void *spacedata)
{
	SpaceTime *stime= sa->spacedata.first;
	float col[3];
	
	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	calc_scrollrcts(sa, &(stime->v2d), curarea->winx, curarea->winy);
	
	myortho2(stime->v2d.cur.xmin, stime->v2d.cur.xmax, stime->v2d.cur.ymin, stime->v2d.cur.ymax);

	/* draw darkened area outside of active timeline */
	draw_sfra_efra();
	
	/* boundbox_seq(); */
	calc_ipogrid();	
	draw_ipogrid();

	draw_cfra_time(spacedata);
	draw_ob_keys();
	draw_markers_time(spacedata);

	/* restore viewport */
	mywinset(curarea->win);

	/* ortho at pixel level curarea */
	myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);
	
	/* the bottom with time values */
	BIF_ThemeColor(TH_HEADER);
	glRectf(0.0f, 0.0f, (float)curarea->winx, 12.0f);
	BIF_ThemeColorShade(TH_HEADER, 50);
	fdrawline(0.0f, 12.0f, (float)curarea->winx, 12.0f);
	draw_view2d_numbers_horiz(stime->flag & TIME_DRAWFRAMES);
	
	draw_area_emboss(sa);
	curarea->win_swap= WIN_BACK_OK;
}
