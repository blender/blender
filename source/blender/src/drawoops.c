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

#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_oops_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_glutil.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_mywindow.h"

/*  #include "BIF_drawoops.h" bad name :(*/
#include "BIF_oops.h"

#include "BSE_drawipo.h"
#include "BSE_drawoops.h"

float oopscalex;

void boundbox_oops()
{
	Oops *oops;
	float min[2], max[2];
	int ok= 0;
	
	if(G.soops==0) return;
	
	min[0]= 1000.0;
	max[0]= -10000.0;
	min[1]= 1000.0;
	max[1]= -1000.0;
	
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {
			ok= 1;
			
			min[0]= MIN2(min[0], oops->x);
			max[0]= MAX2(max[0], oops->x+OOPSX);
			min[1]= MIN2(min[1], oops->y);
			max[1]= MAX2(max[1], oops->y+OOPSY);
		}
		oops= oops->next;
	}
	
	if(ok==0) return;
	
	G.v2d->tot.xmin= min[0];
	G.v2d->tot.xmax= max[0];
	G.v2d->tot.ymin= min[1];
	G.v2d->tot.ymax= max[1];
	
}

void give_oopslink_line(Oops *oops, OopsLink *ol, float *v1, float *v2)
{

	if(ol->to && ol->to->hide==0) {
		v1[0]= oops->x+ol->xof;
		v1[1]= oops->y+ol->yof;
		v2[0]= ol->to->x+OOPSX/2;
		v2[1]= ol->to->y;
	}
	else if(ol->from && ol->from->hide==0) {
		v1[0]= ol->from->x + ol->xof;
		v1[1]= ol->from->y + ol->xof;
		v2[0]= oops->x+OOPSX/2;
		v2[1]= oops->y;
	}
}

void draw_oopslink(Oops *oops)
{
	OopsLink *ol;
	float vec[4];
	
	if(oops->type==ID_SCE) {
		if(oops->flag & SELECT) {
			if(oops->id->lib) cpack(0x4080A0);
			else cpack(0x808080);
		}
		else cpack(0x606060);
	}
	else {
		if(oops->flag & SELECT) {
			if(oops->id->lib) cpack(0x11AAFF);
			else cpack(0xFFFFFF);
		}
		else cpack(0x0);
	}
	
	ol= oops->link.first;
	while(ol) {
		if(ol->to && ol->to->hide==0) {
			
			give_oopslink_line(oops, ol, vec, vec+2);
			
			glBegin(GL_LINE_STRIP);
			glVertex2fv(vec);
			glVertex2fv(vec+2);
			glEnd();
		}
		ol= ol->next;
	}
}

void draw_icon_oops(float *co, short type)
{
	BIFIconID icon;
	
	switch(type) {
	default: return;

	case ID_OB:	icon= ICON_OBJECT_HLT; break;
	case ID_ME:	icon= ICON_MESH_HLT; break;
	case ID_CU:	icon= ICON_CURVE_HLT; break;
	case ID_MB:	icon= ICON_MBALL_HLT; break;
	case ID_LT:	icon= ICON_LATTICE_HLT; break;
	case ID_LA:	icon= ICON_LAMP_HLT; break;
	case ID_MA:	icon= ICON_MATERIAL_HLT; break;
	case ID_TE:	icon= ICON_TEXTURE_HLT; break;
	case ID_IP:	icon= ICON_IPO_HLT; break;
	case ID_LI:	icon= ICON_LIBRARY_HLT; break;
	case ID_IM:	icon= ICON_IMAGE_HLT; break;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); 

	glRasterPos2f(co[0],  co[1]-0.2);
	BIF_draw_icon(icon);

	glBlendFunc(GL_ONE,  GL_ZERO); 
	glDisable(GL_BLEND);
}

void mysbox(float x1, float y1, float x2, float y2)
{
	float vec[2];
	
	glBegin(GL_LINE_LOOP);
	vec[0]= x1; vec[1]= y1;
	glVertex2fv(vec);
	vec[0]= x2;
	glVertex2fv(vec);
	vec[1]= y2;
	glVertex2fv(vec);
	vec[0]= x1;
	glVertex2fv(vec);
	glEnd();
}

unsigned int give_oops_color(short type, short sel, unsigned int *border)
{
	unsigned int body;
	/* also finds out if a dashed line should be drawn */

	switch(type) {
	case ID_OB:
		body= 0x707070; break;
	case ID_SCE:
		body= 0x608060; break;
	case ID_MA:
		body= 0x808060; break;
	case ID_TE:
		body= 0x7080a0; break;
	case ID_IP:
		body= 0x906050; break;
	case ID_LA:
		body= 0x608080; break;
	case ID_LI:
		body= 0x2198DC; break;
	case ID_IM:
		body= 0x35659F; break;
	default:
		body= 0x606070; break;
	}

	if(sel) {
		if(G.moving) *border= 0xf0f0f0;
		else *border= 0xc0c0c0; 
	}
	else *border= 0x0;

	
	return body;
}

void calc_oopstext(char *str, float *v1)
{
	float f1, f2, size;
	short mval[2], len, flen;
	
	ipoco_to_areaco_noclip(G.v2d, v1, mval);
	f1= mval[0];
	v1[0]+= OOPSX;
	ipoco_to_areaco_noclip(G.v2d, v1, mval);
	f2= mval[0];
	size= f2-f1;

	len= strlen(str);
	
	while( (flen= BMF_GetStringWidth(G.fonts, str)) > size) {
		if(flen < 10 || len<2) break;
		len--;
		str[len]= 0;
	}
	
	mval[0]= (f1+f2-flen+1)/2;
	mval[1]= 1;
	areamouseco_to_ipoco(G.v2d, mval, &f1, &f2);
	
	v1[0]= f1;
	
}

void draw_oops(Oops *oops)
{
	OopsLink *ol;
	float v1[2], x1, y1, x2, y2, f1, f2;
	unsigned int body, border;
	short line= 0;
	char str[32];

	x1= oops->x; 
	x2= oops->x+OOPSX;
	y1= oops->y; 
	y2= oops->y+OOPSY;

	if(x2 < G.v2d->cur.xmin || x1 > G.v2d->cur.xmax) return;
	if(y2 < G.v2d->cur.ymin || y1 > G.v2d->cur.ymax) return;

	body= give_oops_color(oops->type, oops->flag & SELECT, &border);
	if(oops->id== (ID *)((G.scene->basact) ? (G.scene->basact->object) : 0)) line= 1;
	else if(oops->id== (ID *)G.scene) line= 1;

	if(oops->id->us) {
		cpack(body);

		glRectf(x1,  y1,  x2,  y2);
	}
	if(oops->id->lib) {
		if(oops->id->flag & LIB_INDIRECT) cpack(0x1144FF);
		else cpack(0x11AAFF);

		glRectf(x2-0.2*OOPSX,  y2-0.2*OOPSX,  x2-0.1*OOPSX,  y2-0.1*OOPSX);
	}

	v1[0]= x1; 
	v1[1]= (y1+y2)/2 -0.3;
	if(oops->type==ID_LI) {
		sprintf(str, "     %s", ((Library *)oops->id)->name);
	}
	else {
		sprintf(str, "     %s", oops->id->name+2);
	}
	calc_oopstext(str, v1);

	/* ICON */
	if(str[1] && oopscalex>1.1) {
		draw_icon_oops(v1, oops->type);
	}
	if(oops->flag & SELECT) cpack(0xFFFFFF); 
	else cpack(0x0);
	glRasterPos3f(v1[0],  v1[1], 0.0);
	BMF_DrawString(G.fonts, str);

	if(line) setlinestyle(2);
	cpack(border);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glRectf(x1,  y1,  x2,  y2);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if(line) setlinestyle(0);

	/* connection blocks */
	ol= oops->link.first;
	while(ol) {

		f1= x1+ol->xof; 
		f2= y1+ol->yof;

		body= give_oops_color(ol->type, oops->flag & SELECT, &border);
		cpack(body);

		glRectf(f1-.2,  f2-.2,  f1+.2,  f2+.2);
		cpack(border);

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		glRectf(f1-.2,  f2-.2,  f1+.2,  f2+.2);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		ol= ol->next;
	}

	if(oops->flag & OOPS_REFER) {
			/* Draw the little rounded connection point */
		glColor3ub(0, 0, 0);
		glPushMatrix();

		glTranslatef(oops->x + 0.5*OOPSX, oops->y, 0.0);
		glutil_draw_filled_arc(0.0, M_PI, 0.05*OOPSX, 7);

		glPopMatrix();
	}
}

void drawoopsspace(ScrArea *sa, void *spacedata)
{
	Oops *oops;
	int ofsx, ofsy;
	
	glClearColor(0.55, 0.55, 0.55, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	if(G.soops==0) return;	

	boundbox_oops();
	calc_scrollrcts(G.v2d, curarea->winx, curarea->winy);

	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {	
			ofsx= curarea->winrct.xmin;	/* because of mywin */
			ofsy= curarea->winrct.ymin;

			glViewport(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1); 
			glScissor(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
		}
	}

	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);

	oopscalex= .14*((float)curarea->winx)/(G.v2d->cur.xmax-G.v2d->cur.xmin);
	calc_ipogrid();	/* for scrollvariables */
	build_oops();

	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {
			draw_oopslink(oops);
		}
		oops= oops->next;
	}
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {
			if(oops->flag & SELECT); else draw_oops(oops);
		}
		oops= oops->next;
	}
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {
			if(oops->flag & SELECT) draw_oops(oops);
		}
		oops= oops->next;
	}
	
	/* restore viewport */
	mywinset(curarea->win);

	
	/* ortho at pixel level curarea */
	myortho2(-0.5, sa->winx-0.5, -0.5, sa->winy-0.5);
	if(G.v2d->scroll) {	
		drawscroll(0);		
	}
	draw_area_emboss(sa);	
	
	curarea->win_swap= WIN_BACK_OK;
}



