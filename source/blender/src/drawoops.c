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

#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_oops_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_language.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_outliner.h"
#include "BIF_resources.h"
#include "BIF_screen.h"

/*  #include "BIF_drawoops.h" bad name :(*/
#include "BIF_oops.h"

#include "BSE_drawipo.h"
#include "BSE_drawoops.h"

float aspect;
void *font;


static float icon_width();
float center_oops_text(char *str);


void boundbox_oops(short sel)
{
	Oops *oops;
	float min[2], max[2];
	int ok= 0;
	
	if(G.soops==0) return;
	
	min[0]= 1000.0;
	max[0]= -1000.0;
	min[1]= 1000.0;
	max[1]= -1000.0;
	
	oops= G.soops->oops.first;
	while(oops) {
		if ((oops->hide==0 && !sel) || (sel && oops->flag & SELECT )) {
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
	float vec[4][3], dist, spline_step;
	short curve_res;
	
	if(oops->type==ID_SCE || oops->type==ID_GR) {
		if(oops->flag & SELECT) {
			/* when using python Mesh to make meshes a file was saved
			that had an oops with no ID, stops a segfault when looking for lib */
			if(oops->id && oops->id->lib) cpack(0x4080A0);
			else cpack(0x808080);
		}
		else cpack(0x606060);
	}
	else {
		if(oops->flag & SELECT) {
			if(oops->id && oops->id->lib) cpack(0x11AAFF);
			else cpack(0xFFFFFF);
		}
		else cpack(0x0);
	}
	
	glEnable(GL_MAP1_VERTEX_3);
	vec[0][2]= vec[1][2]= vec[2][2]= vec[3][2]= 0.0; /* only 2d spline, set the Z to 0*/
	
	ol= oops->link.first;
	while(ol) {
		if(ol->to && ol->to->hide==0) {
			
			give_oopslink_line(oops, ol, vec[0], vec[3]);
			
			dist= 0.5*VecLenf(vec[0], vec[3]);

			/* check ol->xof and yof for direction */
			if(ol->xof == 0.0) {
				vec[1][0]= vec[0][0]-dist;
				vec[1][1]= vec[0][1];
			}
			else if(ol->xof==OOPSX) {
				vec[1][0]= vec[0][0]+dist;
				vec[1][1]= vec[0][1];
			}
			else {
				vec[1][0]= vec[0][0];
				vec[1][1]= vec[0][1]+dist;
			}
			
			/* v3 is always pointing down */
			vec[2][0]= vec[3][0];
			vec[2][1]= vec[3][1] - dist;
			
			if( MIN4(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) > G.v2d->cur.xmax); /* clipped */	
				else if ( MAX4(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) < G.v2d->cur.xmin); /* clipped */
					else {
						/* calculate a curve resolution to use based on the length of the curve.*/
						curve_res = MIN2(40, MAX2(2, 2*dist/aspect));
						
						/* we can reuse the dist variable here to increment the GL curve eval amount*/
						dist = (float)1/curve_res;
						spline_step = 0.0;
						
						glMap1f(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 4, vec[0]);
						glBegin(GL_LINE_STRIP);
						while (spline_step < 1.000001) {
							glEvalCoord1f(spline_step);
							spline_step += dist;
						}
						glEnd();
					}
			
		}
		ol= ol->next;
	}
}

static float icon_width()
{
    /* change it in *one place* when you mess around */
    return 0.8*OOPSY;
}

void draw_icon_oops(float *co, short type)
{
	BIFIconID icon;
    float ofs;
	
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
	case ID_GR:	icon= ICON_CIRCLE_DEHLT; break;
	case ID_CA:	icon= ICON_CAMERA_DEHLT; break;
	case ID_AR:	icon= ICON_ARMATURE; break;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); 

    /* height of box is OOPSY...icon is centered vertically */
    ofs = (OOPSY - icon_width())/2.0;
	BIF_icon_draw_aspect(co[0], co[1]+ofs, icon, icon_width()*aspect*ICON_DEFAULT_HEIGHT/OOPSY);

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
	case ID_GR:
		body= 0x507050; break;
	case ID_CA:
		body= 0x7570A0; break;
	case ID_LT:
		body= 0xA08090; break;
	case ID_AR:
		body= 0x70B0C0; break;
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


float center_oops_text(char *str)
/* gives x offset at which to draw oops text -- takes icon into account */
{
    int len;
    float width;

	len= strlen(str);
    if(len < 1) return 0;

    /* center at box width of OOPSX */
	width= aspect*BIF_GetStringWidth(font, str, 0) + icon_width();

    while(width > OOPSX && len >= 0) {
        str[len] = 0;
        width= aspect*BIF_GetStringWidth(font, str, 0) + icon_width();
        len--;
    }
    return (OOPSX - width)/2;
}

void draw_oops(Oops *oops)
{
	OopsLink *ol;
	float v1[2], x1, y1, x2, y2, f1, f2;
	unsigned int body, border;
	short line= 0;
	char str[FILE_MAXDIR+FILE_MAXFILE+5];

	x1= oops->x; 
	x2= oops->x+OOPSX;
	y1= oops->y; 
	y2= oops->y+OOPSY;

    /* do clip */
	if(x2 < G.v2d->cur.xmin || x1 > G.v2d->cur.xmax) return;
	if(y2 < G.v2d->cur.ymin || y1 > G.v2d->cur.ymax) return;

	body= give_oops_color(oops->type, oops->flag & SELECT, &border);
	if(oops->id== (ID *)((G.scene->basact) ? (G.scene->basact->object) : 0)) line= 1;
	else if(oops->id== (ID *)G.scene) line= 1;

	if (!oops->id) return;

	if(oops->id->us) {
		cpack(body);

		glRectf(x1, y1, x2, y2);
	}

	if(oops->id->lib) { 
		if(oops->id->flag & LIB_INDIRECT) cpack(0x1144FF);
		else cpack(0x11AAFF);

		glRectf(x2-0.2*OOPSX,  y2-0.2*OOPSX,  x2-0.1*OOPSX,  y2-0.1*OOPSX);
	}

	v1[0]= x1; 
	v1[1] = y1;

	if(oops->type==ID_LI) {
		sprintf(str, " %s", ((Library *)oops->id)->name);
	}
	else {
		sprintf(str, " %s", oops->id->name+2);
	}

    BIF_SetScale(aspect);
    v1[0] += center_oops_text(str);

    draw_icon_oops(v1, oops->type);
    v1[0] += icon_width();

    v1[1] = y1+(y2-y1)/3.0;
	if(oops->flag & SELECT) BIF_ThemeColor(TH_TEXT_HI);
	else BIF_ThemeColor(TH_TEXT);
	glRasterPos2f(v1[0],  v1[1]);
    BIF_RasterPos(v1[0], v1[1]);
    BIF_SetScale(aspect);
	BIF_DrawString(font, str, 0);
	
    
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
	SpaceOops *soops= spacedata;
	Oops *oops;
	float col[3];
	
	BIF_GetThemeColor3fv(TH_BACK, col);

	if(soops==0) return;	
	
	/* darker background for oops */
	if(soops->type!=SO_OUTLINER) {
		col[0] = col[0] * 0.75; col[1] = col[1] * 0.75; col[2] = col[2] * 0.75;
	}
	
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	if(soops->type==SO_OUTLINER) draw_outliner(sa, soops);
	else {
		build_oops();	/* changed to become first call... */
		
		boundbox_oops(0);
		calc_scrollrcts(sa, G.v2d, curarea->winx, curarea->winy);

        myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
        bwin_clear_viewmat(sa->win);	/* clear buttons view */
        glLoadIdentity();

        aspect= (G.v2d->cur.xmax - G.v2d->cur.xmin)/((float)sa->winx);
        font= uiSetCurFont_ext(30*aspect);

		calc_ipogrid();	/* for scrollvariables */
		
        /* drop shadow */
		BIF_ThemeColorShade(TH_BACK, -96); /* drop shadow color */
        glRectf(G.v2d->tot.xmin-1, G.v2d->tot.ymin-3, G.v2d->tot.xmax+3,
                G.v2d->tot.ymax+1);
        
        /* light square in the center */
		BIF_GetThemeColor3fv(TH_BACK, col);
		glColor3fv(col);
        glRectf(G.v2d->tot.xmin-2, G.v2d->tot.ymin-2,  G.v2d->tot.xmax+2,
                G.v2d->tot.ymax+2);

		/* box around the oops. */
		cpack(0x0);
        mysbox(G.v2d->tot.xmin-2, G.v2d->tot.ymin-2,  G.v2d->tot.xmax+2,
                G.v2d->tot.ymax+2);
	

		/* Draw unselected oops links */
        for(oops= soops->oops.first; oops; oops = oops->next) {
			if(oops->hide==0 && (oops->flag & SELECT)); else {
				draw_oopslink(oops);
			}
		}
		
		/* Draw selected oops links */
        for(oops= soops->oops.first; oops; oops = oops->next) {
			if(oops->hide==0 && (oops->flag & SELECT)) {
				draw_oopslink(oops);
			}
		}			
		
        for(oops= soops->oops.first; oops; oops = oops->next) {
			if(oops->hide==0) {
				if(oops->flag & SELECT); else draw_oops(oops);
			}
		}
		
        for(oops= soops->oops.first; oops; oops = oops->next) {
			if(oops->hide==0) {
				if(oops->flag & SELECT) draw_oops(oops);
			}
		}
	}
	
	/* restore viewport */
	mywinset(curarea->win);
	
	/* ortho at pixel level curarea */
	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);

	if(soops->type==SO_OUTLINER) {
		if(sa->winx>SCROLLB+10 && sa->winy>SCROLLH+10) {
			if(G.v2d->scroll) drawscroll(0);		
		}
	}
	draw_area_emboss(sa);	
	
	curarea->win_swap= WIN_BACK_OK;
}




