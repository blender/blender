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

/* 
     a full doc with API notes can be found in bf-blender/blender/doc/interface_API.txt

 */
 

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif   

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BKE_blender.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"

#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_butspace.h"
#include "BIF_language.h"

#include "BSE_view.h"

#include "mydevice.h"
#include "interface.h"
#include "blendef.h"

// globals
extern float UIwinmat[4][4];

// internal prototypes
static void stow_unstow(uiBlock *block);


/* --------- generic helper drawng calls ---------------- */

/* supposes you draw the actual box atop of this. */
void uiSoftShadow(float minx, float miny, float maxx, float maxy, float rad, int alpha)
{

	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	/* quads start left-top, clockwise */
	
	/* left */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( minx-rad, maxy-rad);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( minx+rad, maxy-rad);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( minx+rad, miny+rad);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( minx-rad, miny-rad);
	glEnd();

	/* bottom */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( minx+rad, miny+rad);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( maxx-rad, miny+rad);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( maxx+rad, miny-rad);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( minx-rad, miny-rad);
	glEnd();

	/* right */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( maxx-rad, maxy-rad);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( maxx+rad, maxy-rad);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( maxx+rad, miny-rad);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( maxx-rad, miny+rad);
	glEnd();

	glDisable(GL_BLEND);
	glShadeModel(GL_FLAT);
}


#define UI_RB_ALPHA 16
static int roundboxtype= 15;

void uiSetRoundBox(int type)
{
	roundboxtype= type;
	
	/* flags to set which corners will become rounded:

	1------2
	|      |
	8------4
	*/
	
}

void gl_round_box_topshade(float minx, float miny, float maxx, float maxy, float rad)
{
	float vec[7][2]= {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                  {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	char col[7]= {140, 165, 195, 210, 230, 245, 255};
	int a;
	char alpha=255;
	
	if(roundboxtype & UI_RB_ALPHA) alpha= 128;
	
	/* mult */
	for(a=0; a<7; a++) {
		vec[a][0]*= rad; vec[a][1]*= rad;
	}

	/* shades from grey->white->grey */
	glBegin(GL_LINE_STRIP);
	
	if(roundboxtype & 3) {
		/* corner right-top */
		glColor4ub(140, 140, 140, alpha);
		glVertex2f( maxx, maxy-rad);
		for(a=0; a<7; a++) {
			glColor4ub(col[a], col[a], col[a], alpha);
			glVertex2f( maxx-vec[a][1], maxy-rad+vec[a][0]);
		}
		glColor4ub(225, 225, 225, alpha);
		glVertex2f( maxx-rad, maxy);
	
		
		/* corner left-top */
		glVertex2f( minx+rad, maxy);
		for(a=0; a<7; a++) {
			glColor4ub(col[6-a], col[6-a], col[6-a], alpha);
			glVertex2f( minx+rad-vec[a][0], maxy-vec[a][1]);
		}
		glVertex2f( minx, maxy-rad);
	}
	else {
		glColor4ub(225, 225, 225, alpha);
		glVertex2f( minx, maxy);
		glVertex2f( maxx, maxy);
	}
	
	glEnd();
}


void gl_round_box(float minx, float miny, float maxx, float maxy, float rad)
{
	float vec[7][2]= {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                  {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	int a;
	
	/* mult */
	for(a=0; a<7; a++) {
		vec[a][0]*= rad; vec[a][1]*= rad;
	}

	/* start with corner right-bottom */
	if(roundboxtype & 4) {
		glVertex2f( maxx-rad, miny);
		for(a=0; a<7; a++) {
			glVertex2f( maxx-rad+vec[a][0], miny+vec[a][1]);
		}
		glVertex2f( maxx, miny+rad);
	}
	else glVertex2f( maxx, miny);
	
	/* corner right-top */
	if(roundboxtype & 2) {
		glVertex2f( maxx, maxy-rad);
		for(a=0; a<7; a++) {
			glVertex2f( maxx-vec[a][1], maxy-rad+vec[a][0]);
		}
		glVertex2f( maxx-rad, maxy);
	}
	else glVertex2f( maxx, maxy);
	
	/* corner left-top */
	if(roundboxtype & 1) {
		glVertex2f( minx+rad, maxy);
		for(a=0; a<7; a++) {
			glVertex2f( minx+rad-vec[a][0], maxy-vec[a][1]);
		}
		glVertex2f( minx, maxy-rad);
	}
	else glVertex2f( minx, maxy);
	
	/* corner left-bottom */
	if(roundboxtype & 8) {
		glVertex2f( minx, miny+rad);
		for(a=0; a<7; a++) {
			glVertex2f( minx+vec[a][1], miny+rad-vec[a][0]);
		}
		glVertex2f( minx+rad, miny);
	}
	else glVertex2f( minx, miny);
	
}

/* for headers and floating panels */
void uiRoundBoxEmboss(float minx, float miny, float maxx, float maxy, float rad)
{
	float color[4];
	
	if(roundboxtype & UI_RB_ALPHA) {
		glGetFloatv(GL_CURRENT_COLOR, color);
		color[3]= 0.5;
		glColor4fv(color);
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	
	/* solid part */
	glBegin(GL_POLYGON);
	gl_round_box(minx, miny, maxx, maxy, rad);
	glEnd();
	
	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	gl_round_box_topshade(minx+1, miny+1, maxx-1, maxy-1, rad);

	if(roundboxtype & UI_RB_ALPHA) glColor4ub(0,0,0, 128); else glColor4ub(0,0,0, 255);
	glBegin(GL_LINE_LOOP);
	gl_round_box(minx, miny, maxx, maxy, rad);
	glEnd();
   
	glDisable( GL_BLEND );
	glDisable( GL_LINE_SMOOTH );

}


/* plain antialiased unfilled rectangle */
void uiRoundRect(float minx, float miny, float maxx, float maxy, float rad)
{
	float color[4];
	
	if(roundboxtype & UI_RB_ALPHA) {
		glGetFloatv(GL_CURRENT_COLOR, color);
		color[3]= 0.5;
		glColor4fv(color);
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	
	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glBegin(GL_LINE_LOOP);
	gl_round_box(minx, miny, maxx, maxy, rad);
	glEnd();
   
	glDisable( GL_BLEND );
	glDisable( GL_LINE_SMOOTH );
}



/* plain antialiased filled box */
void uiRoundBox(float minx, float miny, float maxx, float maxy, float rad)
{
	float color[4];
	
	if(roundboxtype & UI_RB_ALPHA) {
		glGetFloatv(GL_CURRENT_COLOR, color);
		color[3]= 0.5;
		glColor4fv(color);
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	
	/* solid part */
	glBegin(GL_POLYGON);
	gl_round_box(minx, miny, maxx, maxy, rad);
	glEnd();
	
	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glBegin(GL_LINE_LOOP);
	gl_round_box(minx, miny, maxx, maxy, rad);
	glEnd();
   
	glDisable( GL_BLEND );
	glDisable( GL_LINE_SMOOTH );
}


/* ************** panels ************* */

static void copy_panel_offset(Panel *pa, Panel *papar)
{
	/* with respect to sizes... papar is parent */

	pa->ofsx= papar->ofsx;
	pa->ofsy= papar->ofsy + papar->sizey-pa->sizey;
}



/* global... but will be NULLed after each 'newPanel' call */
static char *panel_tabbed=NULL, *group_tabbed=NULL;

void uiNewPanelTabbed(char *panelname, char *groupname)
{
	panel_tabbed= panelname;
	group_tabbed= groupname;
}

/* another global... */
static int pnl_control= UI_PNL_TRANSP;

void uiPanelControl(int control)
{
	pnl_control= control;
}

/* another global... */
static int pnl_handler= 0;

void uiSetPanelHandler(int handler)
{
	pnl_handler= handler;
}


/* ofsx/ofsy only used for new panel definitions */
/* return 1 if visible (create buttons!) */
int uiNewPanel(ScrArea *sa, uiBlock *block, char *panelname, char *tabname, int ofsx, int ofsy, int sizex, int sizey)
{
	Panel *pa, *palign;
	
	/* check if Panel exists, then use that one */
	pa= sa->panels.first;
	while(pa) {
		if( strncmp(pa->panelname, panelname, UI_MAX_NAME_STR)==0) {
			if( strncmp(pa->tabname, tabname, UI_MAX_NAME_STR)==0) {
				break;
			}
		}
		pa= pa->next;
	}
	
	if(pa==NULL) {
		
		/* new panel */
		pa= MEM_callocN(sizeof(Panel), "new panel");
		BLI_addtail(&sa->panels, pa);
		strncpy(pa->panelname, panelname, UI_MAX_NAME_STR);
		strncpy(pa->tabname, tabname, UI_MAX_NAME_STR);
	
		pa->ofsx= ofsx & ~(PNL_GRID-1);
		pa->ofsy= ofsy & ~(PNL_GRID-1);
		pa->sizex= sizex;
		pa->sizey= sizey;
		
		/* pre align, for good sorting later on */
		if(sa->spacetype==SPACE_BUTS && pa->prev) {
			SpaceButs *sbuts= sa->spacedata.first;
			
			palign= pa->prev;
			if(sbuts->align==BUT_VERTICAL) {
				pa->ofsy= palign->ofsy - pa->sizey - PNL_HEADER;
			}
			else if(sbuts->align==BUT_HORIZONTAL) {
				pa->ofsx= palign->ofsx + palign->sizex;
			}
		}
		/* make new Panel tabbed? */
		if(panel_tabbed && group_tabbed) {
			Panel *papar;
			for(papar= sa->panels.first; papar; papar= papar->next) {
				if(papar->active && papar->paneltab==NULL) {
					if( strncmp(panel_tabbed, papar->panelname, UI_MAX_NAME_STR)==0) {
						if( strncmp(group_tabbed, papar->tabname, UI_MAX_NAME_STR)==0) {
							pa->paneltab= papar;
							copy_panel_offset(pa, papar);
							break;
						}
					}
				}
			} 
		}
	}
	
	block->panel= pa;
	block->handler= pnl_handler;
	pa->active= 1;
	pa->control= pnl_control;
	
	if(pnl_control & UI_PNL_TO_MOUSE) {
		short mval[2];
		
		Mat4CpyMat4(UIwinmat, block->winmat);	// can be first event here
		uiGetMouse(block->win, mval);		
		pa->ofsx= mval[0]-pa->sizex/2;
		pa->ofsy= mval[1]-pa->sizey/2;
		
		if(pa->flag & PNL_CLOSED) pa->flag &= ~PNL_CLOSED;
	}
	
	if(pnl_control & UI_PNL_UNSTOW) {
		if(pa->flag & PNL_CLOSEDY) {
			pa->flag &= ~PNL_CLOSED;
			stow_unstow(block); // toggles!
		}
	}
	
	/* clear ugly globals */
	panel_tabbed= group_tabbed= NULL;
	pnl_handler= 0;
	pnl_control= UI_PNL_TRANSP; // back to default
	
	if(block->panel->paneltab) return 0;
	if(block->panel->flag & PNL_CLOSED) return 0;

	return 1;
}

void uiFreePanels(ListBase *lb)
{
	Panel *panel;
	
	while( (panel= lb->first) ) {
		BLI_remlink(lb, panel);
		MEM_freeN(panel);
	}
}

void uiNewPanelHeight(uiBlock *block, int sizey)
{
	if(sizey<64) sizey= 64;
	
	if(block->panel) {
		block->panel->ofsy+= (block->panel->sizey - sizey);
		block->panel->sizey= sizey;
	}
}

static int panel_has_tabs(Panel *panel)
{
	Panel *pa= curarea->panels.first;
	
	if(panel==NULL) return 0;
	
	while(pa) {
		if(pa->paneltab==panel) return 1;
		pa= pa->next;
	}
	return 0;
}

static void ui_scale_panel_block(uiBlock *block)
{
	uiBut *but;
	float facx= 1.0, facy= 1.0;
	int centrex= 0, topy=0, tabsy=0;
	
	if(block->panel==NULL) return;

	if(block->autofill) ui_autofill(block);
	/* buttons min/max centered, offset calculated */
	uiBoundsBlock(block, 0);

	if( block->maxx-block->minx > block->panel->sizex - 2*PNL_SAFETY ) {
		facx= (block->panel->sizex - (2*PNL_SAFETY))/( block->maxx-block->minx );
	}
	else centrex= (block->panel->sizex-( block->maxx-block->minx ) - PNL_SAFETY)/2;
	
	// tabsy= PNL_HEADER*panel_has_tabs(block->panel);
	if( (block->maxy-block->miny) > block->panel->sizey - 2*PNL_SAFETY - tabsy) {
		facy= (block->panel->sizey - (2*PNL_SAFETY) - tabsy)/( block->maxy-block->miny );
	}
	else topy= (block->panel->sizey- 2*PNL_SAFETY - tabsy) - ( block->maxy-block->miny ) ;

	but= block->buttons.first;
	while(but) {
		but->x1= PNL_SAFETY+centrex+ facx*(but->x1-block->minx);
		but->y1= PNL_SAFETY+topy   + facy*(but->y1-block->miny);
		but->x2= PNL_SAFETY+centrex+ facx*(but->x2-block->minx);
		but->y2= PNL_SAFETY+topy   + facy*(but->y2-block->miny);
		if(facx!=1.0) ui_check_but(but);	/* for strlen */
		but= but->next;
	}

	block->maxx= block->panel->sizex;
	block->maxy= block->panel->sizey;
	block->minx= block->miny= 0.0;
	
}

// for 'home' key
void uiSetPanel_view2d(ScrArea *sa)
{
	Panel *pa;
	float minx=10000, maxx= -10000, miny=10000, maxy= -10000;
	int done=0;
	
	pa= sa->panels.first;
	while(pa) {
		if(pa->active) {
			done= 1;
			if(pa->ofsx < minx) minx= pa->ofsx;
			if(pa->ofsx+pa->sizex > maxx) maxx= pa->ofsx+pa->sizex;
			if(pa->ofsy < miny) miny= pa->ofsy;
			if(pa->ofsy+pa->sizey+PNL_HEADER > maxy) maxy= pa->ofsy+pa->sizey+PNL_HEADER;
		}
		pa= pa->next;
	}
	if(done) {
		G.v2d->tot.xmin= minx-PNL_DIST;
		G.v2d->tot.xmax= maxx+PNL_DIST;
		G.v2d->tot.ymin= miny-PNL_DIST;
		G.v2d->tot.ymax= maxy+PNL_DIST;
	}
	else {
		G.v2d->tot.xmin= 0;
		G.v2d->tot.xmax= 1280;
		G.v2d->tot.ymin= 0;
		G.v2d->tot.ymax= 228;
	}
	
}

// make sure the panels are not outside 'tot' area
void uiMatchPanel_view2d(ScrArea *sa)
{
	Panel *pa;
	
	pa= sa->panels.first;
	while(pa) {
		if(pa->active) {
			if(pa->ofsx < G.v2d->tot.xmin) G.v2d->tot.xmin= pa->ofsx;
			if(pa->ofsx+pa->sizex > G.v2d->tot.xmax) 
				G.v2d->tot.xmax= pa->ofsx+pa->sizex;
			if(pa->ofsy < G.v2d->tot.ymin) G.v2d->tot.ymin= pa->ofsy;
			if(pa->ofsy+pa->sizey+PNL_HEADER > G.v2d->tot.ymax) 
				G.v2d->tot.ymax= pa->ofsy+pa->sizey+PNL_HEADER;
		}
		pa= pa->next;
	}	
}

/* extern used ny previewrender */
void uiPanelPush(uiBlock *block)
{
	glPushMatrix(); 
	if(block->panel) {
		glTranslatef((float)block->panel->ofsx, (float)block->panel->ofsy, 0.0);
		i_translate((float)block->panel->ofsx, (float)block->panel->ofsy, 0.0, UIwinmat);
	}
}

void uiPanelPop(uiBlock *block)
{
	glPopMatrix();
	Mat4CpyMat4(UIwinmat, block->winmat);
}

uiBlock *uiFindOpenPanelBlockName(ListBase *lb, char *name)
{
	uiBlock *block;
	
	for(block= lb->first; block; block= block->next) {
		if(block->panel && block->panel->active && block->panel->paneltab==NULL) {
			if(block->panel->flag & PNL_CLOSED);
			else if(strncmp(name, block->panel->panelname, UI_MAX_NAME_STR)==0) break;
		}
	}
	return block;
}

static void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3)
{

	// we draw twice, anti polygons not widely supported...

	glBegin(GL_POLYGON);
	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
	glVertex2f(x3, y3);
	glEnd();

	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glBegin(GL_LINE_LOOP);
	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
	glVertex2f(x3, y3);
	glEnd();
	
	glDisable( GL_LINE_SMOOTH );
	glDisable( GL_BLEND );
	
}

/* 'icon' for panel header */
static void ui_draw_tria_icon(float x, float y, float aspect, char dir)
{

	
	glColor3ub(240, 240, 240);
	
	if(dir=='h') {
		ui_draw_anti_tria( x, y, x, y+10.0, x+8.75, y+5.25);
	}
	else {
		ui_draw_anti_tria( x-2, y+8.75,  x+10-2, y+8.75, x+5.25-2, y);	
	}
	
	
}

#if 0
static void ui_set_panel_pattern(char dir)
{
	static int firsttime= 1;
	static GLubyte path[4*32], patv[4*32];
	int a,b,i=0;

	if(firsttime) {
		firsttime= 0;
		for(a=0; a<128; a++) patv[a]= 0x33;
		for(a=0; a<8; a++) {
			for(b=0; b<4; b++) path[i++]= 0xff;	/* 1 scanlines */
			for(b=0; b<12; b++) path[i++]= 0x0;	/* 3 lines */
		}
	}
	glEnable(GL_POLYGON_STIPPLE);
	if(dir=='h') glPolygonStipple(path);	
	else glPolygonStipple(patv);	
}
#endif

static char *ui_block_cut_str(uiBlock *block, char *str, short okwidth)
{
	short width, ofs=strlen(str);
	static char str1[128];
	
	if(ofs>127) return str;
	
	width= block->aspect*BIF_GetStringWidth(block->curfont, str, (U.transopts & TR_BUTTONS));

	if(width <= okwidth) return str;
	strcpy(str1, str);
	
	while(width > okwidth && ofs>0) {
		ofs--;
		str1[ofs]= 0;
		
		width= block->aspect*BIF_GetStringWidth(block->curfont, str1, 0);
		
		if(width < 10) break;
	}
	return str1;
}


#define PNL_ICON 	20
#define PNL_DRAGGER	20


static void ui_draw_panel_header(uiBlock *block)
{
	Panel *pa, *panel= block->panel;
	float width;
	int a, nr= 1, pnl_icons;
	char *str;
	
	/* count */
	pa= curarea->panels.first;
	while(pa) {
		if(pa->active) {
			if(pa->paneltab==panel) nr++;
		}
		pa= pa->next;
	}
	
	pnl_icons= PNL_ICON;
	if(panel->control & UI_PNL_CLOSE) pnl_icons+= PNL_ICON;

	if(nr==1) {
		glColor3ub(255,255,255);
		glRasterPos2f(block->minx+pnl_icons, block->maxy+5);
		BIF_DrawString(block->curfont, block->panel->panelname, (U.transopts & TR_BUTTONS), 0);
		return;
	}
	
	a= 0;
	width= (panel->sizex - 3 - pnl_icons - PNL_ICON)/nr;
	pa= curarea->panels.first;
	while(pa) {
		if(pa->active==0);
		else if(pa==panel) {
			/* active tab */
			uiSetRoundBox(15);
			glColor3ub(140, 140, 147);
			uiRoundBox(2+pnl_icons+a*width, panel->sizey+3, pnl_icons+(a+1)*width, panel->sizey+PNL_HEADER-3, 8);

			glColor3ub(255,255,255);
			glRasterPos2f(10+pnl_icons+a*width, panel->sizey+5);
			str= ui_block_cut_str(block, pa->panelname, (short)(width-10));
			BIF_DrawString(block->curfont, str, (U.transopts & TR_BUTTONS), 0);

			a++;
		}
		else if(pa->paneltab==panel) {
			/* not active tab */
			
			glColor3ub(95,95,95);
			glRasterPos2f(10+pnl_icons+a*width, panel->sizey+5);
			str= ui_block_cut_str(block, pa->panelname, (short)(width-10));
			BIF_DrawString(block->curfont, str, (U.transopts & TR_BUTTONS), 0);
			
			a++;
		}
		pa= pa->next;
	}
	
	// dragger
	uiSetRoundBox(15);
	glColor3ub(140, 140, 147);
	uiRoundBox(panel->sizex-PNL_ICON+5, panel->sizey+5, panel->sizex-5, panel->sizey+PNL_HEADER-5, 5);
	
}

void ui_draw_panel(uiBlock *block)
{
	Panel *panel= block->panel;
	int align=0, ofsx;
	
	if(panel->paneltab) return;

	if(curarea->spacetype==SPACE_BUTS) {
		SpaceButs *sbuts= curarea->spacedata.first;
		align= sbuts->align;
	}
	
	if(panel->flag & PNL_CLOSEDY) {
		uiSetRoundBox(15);
		glColor3ub(160, 160, 167);
		uiRoundBox(block->minx, block->maxy, block->maxx, block->maxy+PNL_HEADER, 10);
		
		// title
		ofsx= 2*PNL_ICON;
		if(panel->control & UI_PNL_CLOSE) ofsx+= PNL_ICON;
		glColor3ub(255,255,255);
		glRasterPos2f(block->minx+40, block->maxy+5);
		BIF_DrawString(block->curfont, panel->panelname, (U.transopts & TR_BUTTONS), 0);

		//  border
		if(panel->flag & PNL_SELECT) {
			glColor3ub(64, 64, 64);
			uiRoundRect(block->minx, block->maxy, block->maxx, block->maxy+PNL_HEADER, 10);
		}
		if(panel->flag & PNL_OVERLAP) {
			glColor3ub(240, 240, 240);
			uiRoundRect(block->minx, block->maxy, block->maxx, block->maxy+PNL_HEADER, 10);
		}
	
	}
	else if(panel->flag & PNL_CLOSEDX) {
		char str[4];
		int a, end, ofs;
		
		uiSetRoundBox(15);
		glColor3ub(160, 160, 167);
		uiRoundBox(block->minx, block->miny, block->minx+PNL_HEADER, block->maxy+PNL_HEADER, 10);
	
		// title, only capitals for now
		glColor3ub(255,255,255);
		str[1]= 0;
		end= strlen(panel->panelname);
		ofs= 20;
		for(a=0; a<end; a++) {
			str[0]= panel->panelname[a];
			if( isupper(str[0]) ) {
				glRasterPos2f(block->minx+5, block->maxy-ofs);
				BIF_DrawString(block->curfont, str, 0, 0);
				ofs+= 15;
			}
		}
		
		//  border
		if(panel->flag & PNL_SELECT) {
			glColor3ub(64, 64, 64);
			uiRoundRect(block->minx, block->miny, block->minx+PNL_HEADER, block->maxy+PNL_HEADER, 10);
		}
		if(panel->flag & PNL_OVERLAP) {
			glColor3ub(240, 240, 240);
			uiRoundRect(block->minx, block->miny, block->minx+PNL_HEADER, block->maxy+PNL_HEADER, 10);
		}
	
	}
	else {
		
		uiSetRoundBox(3);

		if(panel->control & UI_PNL_SOLID) {
			glColor3ub(160, 160, 167);
			uiRoundBox(block->minx, block->maxy, block->maxx, block->maxy+PNL_HEADER, 10);
			// blend now for panels in 3d window, test...
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glEnable(GL_BLEND);
			glColor4ub(198, 198, 198, 100);
			glRectf(block->minx, block->miny, block->maxx, block->maxy);

			//if(align) {
			//	glColor4ub(206, 206, 206, 100);
			//	if(align==BUT_HORIZONTAL) ui_set_panel_pattern('h');
			//	else ui_set_panel_pattern('v');

			//	glRectf(block->minx, block->miny, block->maxx, block->maxy);
			//	glDisable(GL_POLYGON_STIPPLE);
			// }
			glDisable(GL_BLEND);
		}
		else if(panel->control & UI_PNL_TRANSP) {
			glColor3ub(218, 218, 218);
			uiRoundRect(block->minx, block->miny, block->maxx, block->maxy+PNL_HEADER, 10);
		}
		
		
		ui_draw_panel_header(block);

		//  border
		uiSetRoundBox(3);
		if(panel->flag & PNL_SELECT) {
			glColor3ub(64, 64, 64);
			uiRoundRect(block->minx, block->miny, block->maxx, block->maxy+PNL_HEADER, 10);
		}
		if(panel->flag & PNL_OVERLAP) {
			glColor3ub(240, 240, 240);
			uiRoundRect(block->minx, block->miny, block->maxx, block->maxy+PNL_HEADER, 10);
		}
		
		/* and a soft shadow-line for now */
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glColor4ub(0, 0, 0, 50);
		fdrawline(block->maxx, block->miny, block->maxx, block->maxy+PNL_HEADER/2);
		fdrawline(block->minx, block->miny, block->maxx, block->miny);
		glDisable(GL_BLEND);

	}
	
	/* draw optional close icon */
	
	ofsx= 0;
	if(panel->control & UI_PNL_CLOSE) {
		glRasterPos2f(block->minx+2, block->maxy+3);
		if(block->aspect>1.1) glPixelZoom(1.0/block->aspect, 1.0/block->aspect);
		BIF_draw_icon(ICON_X);
		if(block->aspect>1.1) glPixelZoom(1.0, 1.0);
		ofsx= 16;
	}

	/* draw collapse icon */
	
	if(panel->flag & PNL_CLOSEDY)
		ui_draw_tria_icon(block->minx+6+ofsx, block->maxy+5, block->aspect, 'h');
	else if(panel->flag & PNL_CLOSEDX)
		ui_draw_tria_icon(block->minx+4, block->maxy+2, block->aspect, 'h');
	else
		ui_draw_tria_icon(block->minx+6+ofsx, block->maxy+5, block->aspect, 'v');


}

static void ui_redraw_select_panel(ScrArea *sa)
{
	/* only for beauty, make sure the panel thats moved is on top */
	/* better solution later? */
	uiBlock *block;
	
	for(block= sa->uiblocks.first; block; block= block->next) {
		if(block->panel && (block->panel->flag & PNL_SELECT)) {
			uiDrawBlock(block);
		}
	}

}


/* ------------ panel alignment ---------------- */


/* this function is needed because uiBlock and Panel itself dont
change sizey or location when closed */
static int get_panel_real_ofsy(Panel *pa)
{
	if(pa->flag & PNL_CLOSEDY) return pa->ofsy+pa->sizey;
	else if(pa->paneltab && (pa->paneltab->flag & PNL_CLOSEDY)) return pa->ofsy+pa->sizey;
	else return pa->ofsy;
}

static int get_panel_real_ofsx(Panel *pa)
{
	if(pa->flag & PNL_CLOSEDX) return pa->ofsx+PNL_HEADER;
	else if(pa->paneltab && (pa->paneltab->flag & PNL_CLOSEDX)) return pa->ofsx+PNL_HEADER;
	else return pa->ofsx+pa->sizex;
}


typedef struct PanelSort {
	Panel *pa, *orig;
} PanelSort;

static int find_leftmost_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1=a1, *ps2=a2;
	
	if( ps1->pa->ofsx > ps2->pa->ofsx) return 1;
	else if( ps1->pa->ofsx < ps2->pa->ofsx) return -1;

	return 0;
}


static int find_highest_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1=a1, *ps2=a2;
	
	if( ps1->pa->ofsy < ps2->pa->ofsy) return 1;
	else if( ps1->pa->ofsy > ps2->pa->ofsy) return -1;
	
	return 0;
}

/* this doesnt draw */
/* returns 1 when it did something */
int uiAlignPanelStep(ScrArea *sa, float fac)
{
	SpaceButs *sbuts= sa->spacedata.first;
	Panel *pa;
	PanelSort *ps, *panelsort, *psnext;
	int a, tot=0, done;
	
	if(sa->spacetype!=SPACE_BUTS) {
		return 0;
	}
	
	/* count active, not tabbed Panels */
	for(pa= sa->panels.first; pa; pa= pa->next) {
		if(pa->active && pa->paneltab==NULL) tot++;
	}

	if(tot==0) return 0;

	/* extra; change close direction? */
	for(pa= sa->panels.first; pa; pa= pa->next) {
		if(pa->active && pa->paneltab==NULL) {
			if( (pa->flag & PNL_CLOSEDX) && (sbuts->align==BUT_VERTICAL) )
				pa->flag ^= PNL_CLOSED;
			
			else if( (pa->flag & PNL_CLOSEDY) && (sbuts->align==BUT_HORIZONTAL) )
				pa->flag ^= PNL_CLOSED;
			
		}
	}

	panelsort= MEM_callocN( tot*sizeof(PanelSort), "panelsort");
	
	/* fill panelsort array */
	ps= panelsort;
	for(pa= sa->panels.first; pa; pa= pa->next) {
		if(pa->active && pa->paneltab==NULL) {
			ps->pa= MEM_dupallocN(pa);
			ps->orig= pa;
			ps++;
		}
	}
	
	if(sbuts->align==BUT_VERTICAL) 
		qsort(panelsort, tot, sizeof(PanelSort), find_highest_panel);
	else
		qsort(panelsort, tot, sizeof(PanelSort), find_leftmost_panel);

	
	/* no smart other default start loc! this keeps switching f5/f6/etc compatible */
	ps= panelsort;
	ps->pa->ofsx= 0;
	ps->pa->ofsy= 0;
	
	for(a=0 ; a<tot-1; a++, ps++) {
		psnext= ps+1;
		
		if(sbuts->align==BUT_VERTICAL) {
			psnext->pa->ofsx = ps->pa->ofsx;
			psnext->pa->ofsy = get_panel_real_ofsy(ps->pa) - psnext->pa->sizey-PNL_HEADER-PNL_DIST;
		}
		else {
			psnext->pa->ofsx = get_panel_real_ofsx(ps->pa)+PNL_DIST;
			psnext->pa->ofsy = ps->pa->ofsy;
		}
	}
	
	/* we interpolate */
	done= 0;
	ps= panelsort;
	for(a=0; a<tot; a++, ps++) {
		if( (ps->pa->flag & PNL_SELECT)==0) {
			if( (ps->orig->ofsx != ps->pa->ofsx) || (ps->orig->ofsy != ps->pa->ofsy)) {
				ps->orig->ofsx= floor(0.5 + fac*ps->pa->ofsx + (1.0-fac)*ps->orig->ofsx);
				ps->orig->ofsy= floor(0.5 + fac*ps->pa->ofsy + (1.0-fac)*ps->orig->ofsy);
				done= 1;
			}
		}
	}

	/* copy locations to tabs */
	for(pa= sa->panels.first; pa; pa= pa->next) {
		if(pa->paneltab && pa->active) {
			copy_panel_offset(pa, pa->paneltab);
		}
	}

	/* free panelsort array */
	ps= panelsort;
	for(a=0; a<tot; a++, ps++) {
		MEM_freeN(ps->pa);
	}
	MEM_freeN(panelsort);
	
	return done;
}


static void ui_animate_panels(ScrArea *sa)
{
	double time=0, ltime;
	float result= 0.0, fac= 0.2;
	
	ltime = PIL_check_seconds_timer();

	/* for max 1 second, interpolate positions */
	while(TRUE) {
	
		if( uiAlignPanelStep(sa, fac) ) {
			/* warn: this re-allocs uiblocks! */
			scrarea_do_windraw(curarea);
			ui_redraw_select_panel(curarea);
			screen_swapbuffers();
		}
		else {
			addqueue(curarea->win, REDRAW,1 );	// because 'Animate' is also called as redraw
			break;
		}
		
		if(result >= 1.0) break;
		
		if(result==0.0) { // firsttime
			time = PIL_check_seconds_timer()-ltime;
			if(time > 0.5) fac= 0.7;
			else if(time > 0.2) fac= 0.5;
			else if(time > 0.1) fac= 0.4;	
			else if(time > 0.05) fac= 0.3; // 11 steps
		}
		
		result= fac + (1.0-fac)*result;
		
		if(result > 0.98) {
			result= 1.0;
			fac= 1.0;
		}
	}
}

/* only draws blocks with panels */
void uiDrawBlocksPanels(ScrArea *sa, int re_align)
{
	uiBlock *block;
	Panel *panot, *panew, *patest;
	
	/* scaling contents */
	block= sa->uiblocks.first;
	while(block) {
		if(block->panel) ui_scale_panel_block(block);
		block= block->next;
	}

	/* consistancy; are panels not made, whilst they have tabs */
	for(panot= sa->panels.first; panot; panot= panot->next) {
		if(panot->active==0) { // not made

			for(panew= sa->panels.first; panew; panew= panew->next) {
				if(panew->active) {
					if(panew->paneltab==panot) { // panew is tab in notmade pa
						break;
					}
				}
			}
			/* now panew can become the new parent, check all other tabs */
			if(panew) {
				for(patest= sa->panels.first; patest; patest= patest->next) {
					if(patest->paneltab == panot) {
						patest->paneltab= panew;
					}
				}
				panot->paneltab= panew;
				panew->paneltab= NULL;
				addqueue(sa->win, REDRAW, 1);	// the buttons panew were not made
			}
		}	
	}

	/* re-align */
	if(re_align) uiAlignPanelStep(sa, 1.0);
	
	/* clip panels (headers) for non-butspace situations (maybe make optimized event later) */
	if(sa->spacetype!=SPACE_BUTS) {
		SpaceLink *sl= sa->spacedata.first;
		for(block= sa->uiblocks.first; block; block= block->next) {
			if(block->panel && block->panel->active && block->panel->paneltab == NULL) {
				float dx=0.0, dy=0.0, minx, miny, maxx, maxy;
				
				minx= sl->blockscale*block->panel->ofsx;
				maxx= sl->blockscale*(block->panel->ofsx+block->panel->sizex);
				miny= sl->blockscale*(block->panel->ofsy+block->panel->sizey);
				maxy= sl->blockscale*(block->panel->ofsy+block->panel->sizey+PNL_HEADER);
				
				if(minx<0.0) dx= -minx;
				else if(maxx > (float)sa->winx) dx= sa->winx-maxx;
				if(miny<0.0) dy= -miny;
				else if(maxy > (float)sa->winy) dy= sa->winy-maxy;

				block->panel->ofsx+= dx/sl->blockscale;
				block->panel->ofsy+= dy/sl->blockscale;

				/* copy locations */
				for(patest= sa->panels.first; patest; patest= patest->next) {
					if(patest->paneltab==block->panel) copy_panel_offset(patest, block->panel);
				}
				
			}
		}
	}

	/* draw */
	block= sa->uiblocks.first;
	while(block) {
		if(block->panel) uiDrawBlock(block);
		block= block->next;
	}

}



/* ------------ panel merging ---------------- */

static void check_panel_overlap(ScrArea *sa, Panel *panel)
{
	Panel *pa= sa->panels.first;

	/* also called with panel==NULL for clear */
	
	while(pa) {
		pa->flag &= ~PNL_OVERLAP;
		if(panel && (pa != panel)) {
			if(pa->paneltab==NULL && pa->active) {
				float safex= 0.2, safey= 0.2;
				
				if( pa->flag & PNL_CLOSEDX) safex= 0.05;
				else if(pa->flag & PNL_CLOSEDY) safey= 0.05;
				else if( panel->flag & PNL_CLOSEDX) safex= 0.05;
				else if(panel->flag & PNL_CLOSEDY) safey= 0.05;
				
				if( pa->ofsx > panel->ofsx- safex*panel->sizex)
				if( pa->ofsx+pa->sizex < panel->ofsx+ (1.0+safex)*panel->sizex)
				if( pa->ofsy > panel->ofsy- safey*panel->sizey)
				if( pa->ofsy+pa->sizey < panel->ofsy+ (1.0+safey)*panel->sizey)
					pa->flag |= PNL_OVERLAP;
			}
		}
		
		pa= pa->next;
	}
}

static void test_add_new_tabs(ScrArea *sa)
{
	Panel *pa, *pasel=NULL, *palap=NULL;
	/* search selected and overlapped panel */
	
	pa= sa->panels.first;
	while(pa) {
		if(pa->active) {
			if(pa->flag & PNL_SELECT) pasel= pa;
			if(pa->flag & PNL_OVERLAP) palap= pa;
		}
		pa= pa->next;
	}
	
	if(pasel && palap==NULL) {

		/* copy locations */
		pa= sa->panels.first;
		while(pa) {
			if(pa->paneltab==pasel) {
				copy_panel_offset(pa, pasel);
			}
			pa= pa->next;
		}
	}
	
	if(pasel==NULL || palap==NULL) return;
	
	/* the overlapped panel becomes a tab */
	palap->paneltab= pasel;
	
	/* the selected panel gets coords of overlapped one */
	copy_panel_offset(pasel, palap);

	/* and its tabs */
	pa= sa->panels.first;
	while(pa) {
		if(pa->paneltab == pasel) {
			copy_panel_offset(pa, palap);
		}
		pa= pa->next;
	}
	
	/* but, the overlapped panel already can have tabs too! */
	pa= sa->panels.first;
	while(pa) {
		if(pa->paneltab == palap) {
			pa->paneltab = pasel;
		}
		pa= pa->next;
	}
}

/* ------------ panel drag ---------------- */


void ui_drag_panel(uiBlock *block)
{
	Panel *panel= block->panel;
	short align=0, first=1, ofsx, ofsy, dx=0, dy=0, dxo=0, dyo=0, mval[2], mvalo[2];

	if(curarea->spacetype==SPACE_BUTS) {
		SpaceButs *sbuts= curarea->spacedata.first;
		align= sbuts->align;
	}

	uiGetMouse(block->win, mvalo);
	ofsx= block->panel->ofsx;
	ofsy= block->panel->ofsy;

	panel->flag |= PNL_SELECT;
	
	while(TRUE) {
	
		if( !(get_mbut() & L_MOUSE) ) break;	
	
		/* first clip for window, no dragging outside */
		getmouseco_areawin(mval);
		if( mval[0]>0 && mval[0]<curarea->winx && mval[1]>0 && mval[1]<curarea->winy) {
			uiGetMouse(mywinget(), mval);
			dx= (mval[0]-mvalo[0]) & ~(PNL_GRID-1);
			dy= (mval[1]-mvalo[1]) & ~(PNL_GRID-1);
		}
		
		if(dx!=dxo || dy!=dyo || first || align) {
			dxo= dx; dyo= dy;		
			first= 0;
			
			panel->ofsx = ofsx+dx;
			panel->ofsy = ofsy+dy;
			
			check_panel_overlap(curarea, panel);
			
			if(align) uiAlignPanelStep(curarea, 0.2);

			/* warn: this re-allocs blocks! */
			scrarea_do_windraw(curarea);
			ui_redraw_select_panel(curarea);
			screen_swapbuffers();
			
			/* so, we find the new block */
			block= curarea->uiblocks.first;
			while(block) {
				if(block->panel == panel) break;
				block= block->next;
			}
			// temporal debug
			if(block==NULL) {
				printf("block null while panel drag, should not happen\n");
			}
			
			/* restore */
			Mat4CpyMat4(UIwinmat, block->winmat);
			
			/* idle for align */
			if(dx==dxo && dy==dyo) PIL_sleep_ms(30);
		}
		/* idle for this poor code */
		else PIL_sleep_ms(30);
	}

	test_add_new_tabs(curarea);	 // also copies locations of tabs in dragged panel

	panel->flag &= ~PNL_SELECT;
	check_panel_overlap(curarea, NULL);	// clears
	
	if(align==0) addqueue(block->win, REDRAW, 1);
	else ui_animate_panels(curarea);
}


static void ui_panel_untab(uiBlock *block)
{
	Panel *panel= block->panel, *pa, *panew=NULL;
	short nr, mval[2], mvalo[2];
	
	/* while hold mouse, check for movement, then untab */
	
	uiGetMouse(block->win, mvalo);
	while(TRUE) {
	
		if( !(get_mbut() & L_MOUSE) ) break;	
		uiGetMouse(mywinget(), mval);
		
		if( abs(mval[0]-mvalo[0]) + abs(mval[1]-mvalo[1]) > 6 ) {
			/* find new parent panel */
			nr= 0;
			pa= curarea->panels.first;
			while(pa) {
				if(pa->paneltab==panel) {
					panew= pa;
					nr++;
				}
				pa= pa->next;
			}
			
			/* make old tabs point to panew */
			if(panew==NULL) printf("panel untab: shouldnt happen\n");
			panew->paneltab= NULL;
			
			pa= curarea->panels.first;
			while(pa) {
				if(pa->paneltab==panel) {
					pa->paneltab= panew;
				}
				pa= pa->next;
			}
			
			ui_drag_panel(block);
			break;
		
		}
		/* idle for this poor code */
		else PIL_sleep_ms(50);
		
	}

}

/* ------------ panel events ---------------- */


static void panel_clicked_tabs(uiBlock *block,  int mousex)
{
	Panel *pa, *tabsel=NULL, *panel= block->panel;
	int nr= 1, a, width;
	
	/* count */
	pa= curarea->panels.first;
	while(pa) {
		if(pa!=panel) {
			if(pa->paneltab==panel) nr++;
		}
		pa= pa->next;
	}

	if(nr==1) return;
	
	/* find clicked tab, mouse in panel coords */
	a= 0;
	width= (panel->sizex - 3- 2*PNL_ICON)/nr;
	pa= curarea->panels.first;
	while(pa) {
		if(pa==panel || pa->paneltab==panel) {
			if( (mousex > PNL_ICON+a*width) && (mousex < PNL_ICON+(a+1)*width) ) {
				tabsel= pa;
			}
			a++;
		}
		pa= pa->next;
	}

	if(tabsel) {
		
		if(tabsel == panel) {
			ui_panel_untab(block);
		}
		else {
			/* tabsel now becomes parent for all others */
			panel->paneltab= tabsel;
			tabsel->paneltab= NULL;
			
			pa= curarea->panels.first;
			while(pa) {
				if(pa->paneltab == panel) pa->paneltab = tabsel;
				pa= pa->next;
			}
			
			addqueue(curarea->win, REDRAW, 1);
		}
	}
	
}

static void stow_unstow(uiBlock *block)
{
	SpaceLink *sl= curarea->spacedata.first;
	Panel *pa;
	int ok=0, x, y, width;
	
	if(block->panel->flag & PNL_CLOSEDY) {  // flag has been set how it should become!
		
		width= (curarea->winx-320)/sl->blockscale;
		if(width<5) width= 5;
		
		/* find empty spot in bottom */
		for(y=4; y<100; y+= PNL_HEADER+4) {
			for(x=4; x<width; x+= 324) {
				ok= 1;
				/* check overlap with other panels */
				for(pa=curarea->panels.first; pa; pa=pa->next) {
					if(pa!=block->panel && pa->active && pa->paneltab==NULL) {
						if( abs(pa->ofsx-x)<320 ) {
							if( abs(pa->ofsy+pa->sizey-y)<PNL_HEADER+4) ok= 0;
						}
					}
				}
				
				if(ok) break;
			}
			if(ok) break;
		}
		if(ok==0) printf("still primitive code... fix!\n");
		
		block->panel->old_ofsx= block->panel->ofsx;
		block->panel->old_ofsy= block->panel->ofsy;
		
		block->panel->ofsx= x;
		block->panel->ofsy= y-block->panel->sizey;
		
	}
	else {
		block->panel->ofsx= block->panel->old_ofsx;
		block->panel->ofsy= block->panel->old_ofsy;
	
	}
	/* copy locations */
	for(pa= curarea->panels.first; pa; pa= pa->next) {
		if(pa->paneltab==block->panel) copy_panel_offset(pa, block->panel);
	}

}


/* this function is supposed to call general window drawing too */
/* also it supposes a block has panel, and isnt a menu */
void ui_do_panel(uiBlock *block, uiEvent *uevent)
{
	Panel *pa;
	int align= 0;
	
	if(curarea->spacetype==SPACE_BUTS) {
		SpaceButs *sbuts= curarea->spacedata.first;
		align= sbuts->align;
	}

	/* mouse coordinates in panel space! */

	if(uevent->event==LEFTMOUSE && block->panel->paneltab==NULL) {
		int button= 0;
		
		/* check open/collapsed button */
		if(block->panel->flag & PNL_CLOSEDX) {
			if(uevent->mval[1] >= block->maxy) button= 1;
		}
		else if(block->panel->control & UI_PNL_CLOSE) {
			if(uevent->mval[0] <= block->minx+PNL_ICON-2) button= 2;
			else if(uevent->mval[0] <= block->minx+2*PNL_ICON+2) button= 1;
		}
		else if(uevent->mval[0] <= block->minx+PNL_ICON+2) {
			button= 1;
		}
		
		if(button) {
		
			if(button==2) { // close
				rem_blockhandler(curarea, block->handler);
				addqueue(curarea->win, REDRAW, 1);
			}
			else {
		
				if(block->panel->flag & PNL_CLOSED) block->panel->flag &= ~PNL_CLOSED;
				else if(align==BUT_HORIZONTAL) block->panel->flag |= PNL_CLOSEDX;
				else block->panel->flag |= PNL_CLOSEDY;
				
				for(pa= curarea->panels.first; pa; pa= pa->next) {
					if(pa->paneltab==block->panel) {
						if(block->panel->flag & PNL_CLOSED) pa->flag |= PNL_CLOSED;
						else pa->flag &= ~PNL_CLOSED;
					}
				}
				// extra, for non-butspace: open/collapse at window header
				if(curarea->spacetype!=SPACE_BUTS)
					stow_unstow(block);

				
			}
			if(align==0) addqueue(block->win, REDRAW, 1);
			else ui_animate_panels(curarea);
			
		}
		else if(block->panel->flag & PNL_CLOSED) {
			ui_drag_panel(block);
		}
		/* check if clicked in tabbed area */
		else if(uevent->mval[0] < block->maxx-PNL_ICON-3 && panel_has_tabs(block->panel)) {
			panel_clicked_tabs(block, uevent->mval[0]);
		}
		else {
			ui_drag_panel(block);
		}
	}
}


