/* ***************************************
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



    raddisplay.c	nov/dec 1992
					may 1999

    - drawing
    - color calculation for display during solving

    $Id$

 *************************************** */

#include <stdlib.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BLI_blenlib.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BIF_gl.h"

#include "radio.h"

/* cpack has to be endian-insensitive! (old irisgl function) */
#define cpack(x)   glColor3ub( ((x)&0xFF), (((x)>>8)&0xFF), (((x)>>16)&0xFF) )

char calculatecolor(float col)
{
	int b;

	if(RG.gamma==1.0) {
		b= RG.radfactor*col;
	}
	else if(RG.gamma==2.0) {
		b= RG.radfactor*sqrt(col);
	}
	else {
		b= RG.radfactor*pow(col, RG.igamma);
	}
	
	if(b>255) b=255;
	return b;
}

void make_node_display()
{
	RNode *rn, **el;
	int a;
	char *charcol;

	RG.igamma= 1.0/RG.gamma;
	RG.radfactor= RG.radfac*pow(64*64, RG.igamma);

	el= RG.elem;
	for(a=RG.totelem; a>0; a--, el++) {
		rn= *el;
		charcol= (char *)&( rn->col );

		charcol[3]= calculatecolor(rn->totrad[0]);
		charcol[2]= calculatecolor(rn->totrad[1]);
		charcol[1]= calculatecolor(rn->totrad[2]);
		
		/* gouraudcolor */
		*(rn->v1+3)= 0;
		*(rn->v2+3)= 0;
		*(rn->v3+3)= 0;
		if(rn->v4) *(rn->v4+3)= 0;
	}
	
	el= RG.elem;
	for(a=RG.totelem; a>0; a--, el++) {
		rn= *el;
		addaccuweight( (char *)&(rn->col), (char *)(rn->v1+3), 16 );
		addaccuweight( (char *)&(rn->col), (char *)(rn->v2+3), 16 );
		addaccuweight( (char *)&(rn->col), (char *)(rn->v3+3), 16 );
		if(rn->v4) addaccuweight( (char *)&(rn->col), (char *)(rn->v4+3), 16 );
	}
}

void drawnodeWire(RNode *rn)
{

	if(rn->down1) {
		drawnodeWire(rn->down1);
		drawnodeWire(rn->down2);
	}
	else {
		glBegin(GL_LINE_LOOP);
			glVertex3fv(rn->v1);
			glVertex3fv(rn->v2);
			glVertex3fv(rn->v3);
			if(rn->type==4) glVertex3fv(rn->v4);
		glEnd();
	}
}

void drawsingnodeWire(RNode *rn)
{
	
	glBegin(GL_LINE_LOOP);
		glVertex3fv(rn->v1);
		glVertex3fv(rn->v2);
		glVertex3fv(rn->v3);
		if(rn->type==4) glVertex3fv(rn->v4);
	glEnd();
}

void drawnodeSolid(RNode *rn)
{
	char *cp;
	
	if(rn->down1) {
		drawnodeSolid(rn->down1);
		drawnodeSolid(rn->down2);
	}
	else {
		cp= (char *)&rn->col;
		glColor3ub(cp[3], cp[2], cp[1]);
		glBegin(GL_POLYGON);
			glVertex3fv(rn->v1);
			glVertex3fv(rn->v2);
			glVertex3fv(rn->v3);
			if(rn->type==4) glVertex3fv(rn->v4);
		glEnd();
	}
}

void drawnodeGour(RNode *rn)
{
	char *cp;
	
	if(rn->down1) {
		drawnodeGour(rn->down1);
		drawnodeGour(rn->down2);
	}
	else {
		glBegin(GL_POLYGON);
			cp= (char *)(rn->v1+3);
			glColor3ub(cp[3], cp[2], cp[1]);
			glVertex3fv(rn->v1);
			
			cp= (char *)(rn->v2+3);
			glColor3ub(cp[3], cp[2], cp[1]);
			glVertex3fv(rn->v2);
			
			cp= (char *)(rn->v3+3);
			glColor3ub(cp[3], cp[2], cp[1]);
			glVertex3fv(rn->v3);
			
			if(rn->type==4) {
				cp= (char *)(rn->v4+3);
				glColor3ub(cp[3], cp[2], cp[1]);
				glVertex3fv(rn->v4);
			}
		glEnd();
	}
}

void drawpatch_ext(RPatch *patch, unsigned int col)
{
	ScrArea *sa, *oldsa;
	View3D *v3d;
	glDrawBuffer(GL_FRONT);

	cpack(col);

	oldsa= NULL; // XXX curarea;

	sa= G.curscreen->areabase.first;
	while(sa) {
		if (sa->spacetype==SPACE_VIEW3D) {
			v3d= sa->spacedata.first;
			
		 	/* use mywinget() here: otherwise it draws in header */
// XXX		 	if(sa->win != mywinget()) areawinset(sa->win);
// XXX			persp(PERSP_VIEW);
			if(v3d->zbuf) glDisable(GL_DEPTH_TEST);
			drawnodeWire(patch->first);
			if(v3d->zbuf) glEnable(GL_DEPTH_TEST);	// pretty useless?
		}
		sa= sa->next;
	}

// XXX	if(oldsa && oldsa!=curarea) areawinset(oldsa->win);

	glFlush();
	glDrawBuffer(GL_BACK);
}


void drawfaceGour(Face *face)
{
	char *cp;
	
	glBegin(GL_POLYGON);
		cp= (char *)(face->v1+3);
		glColor3ub(cp[3], cp[2], cp[1]);
		glVertex3fv(face->v1);
		
		cp= (char *)(face->v2+3);
		glColor3ub(cp[3], cp[2], cp[1]);
		glVertex3fv(face->v2);
		
		cp= (char *)(face->v3+3);
		glColor3ub(cp[3], cp[2], cp[1]);
		glVertex3fv(face->v3);
		
		if(face->v4) {
			cp= (char *)(face->v4+3);
			glColor3ub(cp[3], cp[2], cp[1]);
			glVertex3fv(face->v4);
		}
	glEnd();
	
}

void drawfaceSolid(Face *face)
{
	char *cp;
	
	cp= (char *)&face->col;
	glColor3ub(cp[3], cp[2], cp[1]);
	
	glBegin(GL_POLYGON);
		glVertex3fv(face->v1);
		glVertex3fv(face->v2);
		glVertex3fv(face->v3);
		if(face->v4) {
			glVertex3fv(face->v4);
		}
	glEnd();
	
}

void drawfaceWire(Face *face)
{
	char *cp;
	
	cp= (char *)&face->col;
	glColor3ub(cp[3], cp[2], cp[1]);

	glBegin(GL_LINE_LOOP);
		glVertex3fv(face->v1);
		glVertex3fv(face->v2);
		glVertex3fv(face->v3);
		if(face->v4) {
			glVertex3fv(face->v4);
		}
	glEnd();
	
}

void drawsquare(float *cent, float size, short cox, short coy)
{
	float vec[3];	

	vec[0]= cent[0];
	vec[1]= cent[1];
	vec[2]= cent[2];
	
	glBegin(GL_LINE_LOOP);
		vec[cox]+= .5*size;
		vec[coy]+= .5*size;
		glVertex3fv(vec);	
		vec[coy]-= size;
		glVertex3fv(vec);
		vec[cox]-= size;
		glVertex3fv(vec);
		vec[coy]+= size;
		glVertex3fv(vec);
	glEnd();	
}

void drawlimits()
{
	/* center around cent */
	short cox=0, coy=1;
	
	if((RG.flag & 3)==2) coy= 2;
	if((RG.flag & 3)==3) {
		cox= 1;	
		coy= 2;	
	}
	
	cpack(0);
	drawsquare(RG.cent, sqrt(RG.patchmax), cox, coy);
	drawsquare(RG.cent, sqrt(RG.patchmin), cox, coy);

	drawsquare(RG.cent, sqrt(RG.elemmax), cox, coy);
	drawsquare(RG.cent, sqrt(RG.elemmin), cox, coy);

	cpack(0xFFFFFF);
	drawsquare(RG.cent, sqrt(RG.patchmax), cox, coy);
	drawsquare(RG.cent, sqrt(RG.patchmin), cox, coy);
	cpack(0xFFFF00);
	drawsquare(RG.cent, sqrt(RG.elemmax), cox, coy);
	drawsquare(RG.cent, sqrt(RG.elemmin), cox, coy);
	
}

void setcolNode(RNode *rn, unsigned int *col)
{

	if(rn->down1) {
		 setcolNode(rn->down1, col);
		 setcolNode(rn->down2, col);
	}
	rn->col= *col;
	
	*((unsigned int *)rn->v1+3)= *col;
	*((unsigned int *)rn->v2+3)= *col;
	*((unsigned int *)rn->v3+3)= *col;
	if(rn->v4) *((unsigned int *)rn->v4+3)= *col;
}

void pseudoAmb()
{
	RPatch *rp;
	float fac;
	char col[4];
	
	/* sets pseudo ambient color in the nodes */

	rp= RG.patchbase.first;
	while(rp) {

		if(rp->emit[0]!=0.0 || rp->emit[1]!=0.0 || rp->emit[2]!=0.0) {
			col[1]= col[2]= col[3]= 255;
		}
		else {
			fac= rp->norm[0]+ rp->norm[1]+ rp->norm[2];
			fac= 225.0*(3+fac)/6.0;
			
			col[3]= fac*rp->ref[0];
			col[2]= fac*rp->ref[1];
			col[1]= fac*rp->ref[2];
		}
		
		setcolNode(rp->first, (unsigned int *)col);
		
		rp= rp->next;
	}
}

void RAD_drawall(int depth_is_on)
{
	/* displays elements or faces */
	Face *face = NULL;
	RNode **el;
	RPatch *rp;
	int a;

	if(!depth_is_on) {
		glEnable(GL_DEPTH_TEST);
		glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
	}
	
	if(RG.totface) {
		if(RG.drawtype==DTGOUR) {
			glShadeModel(GL_SMOOTH);
			for(a=0; a<RG.totface; a++) {
				RAD_NEXTFACE(a);
				
				drawfaceGour(face);
			}
		}
		else if(RG.drawtype==DTSOLID) {
			for(a=0; a<RG.totface; a++) {
				RAD_NEXTFACE(a);
				
				drawfaceSolid(face);
			}
		}
		else {
			cpack(0);
			rp= RG.patchbase.first;
			while(rp) {
				drawsingnodeWire(rp->first);
				rp= rp->next;
			}
		}
	}
	else {
		el= RG.elem;
		if(RG.drawtype==DTGOUR) {
			glShadeModel(GL_SMOOTH);
			for(a=RG.totelem; a>0; a--, el++) {
				drawnodeGour(*el);
			}
		}
		else if(RG.drawtype==DTSOLID) {
			for(a=RG.totelem; a>0; a--, el++) {
				drawnodeSolid(*el);
			}
		}
		else {
			cpack(0);
			for(a=RG.totelem; a>0; a--, el++) {
				drawnodeWire(*el);
			}
		}
	}
	glShadeModel(GL_FLAT);
	
	if(RG.totpatch) {
		if(RG.flag & 3) {
			if(depth_is_on) glDisable(GL_DEPTH_TEST);
			drawlimits();
			if(depth_is_on) glEnable(GL_DEPTH_TEST);
		}
	}	
	if(!depth_is_on) {
		glDisable(GL_DEPTH_TEST);
	}
}

void rad_forcedraw()
{
 	ScrArea *sa, *oldsa;
	
	oldsa= NULL; // XXX curarea;

	sa= G.curscreen->areabase.first;
	while(sa) {
		if (sa->spacetype==SPACE_VIEW3D) {
		 	/* use mywinget() here: othwerwise it draws in header */
// XXX	 	if(sa->win != mywinget()) areawinset(sa->win);
// XXX		 	scrarea_do_windraw(sa);
		}
		sa= sa->next;
	}
// XXX	screen_swapbuffers();
	
// XXX	if(oldsa && oldsa!=curarea) areawinset(oldsa->win);
}

