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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#pragma warning (once : 4761)
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"

#include <stdlib.h>
#include <stdio.h>

#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_action_types.h"
#include "DNA_nla_types.h"
#include "DNA_constraint_types.h"

#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"
#include "BKE_global.h"

#include "BSE_drawnla.h"
#include "BSE_drawipo.h"
#include "BSE_editnla_types.h"
#include "BSE_headerbuttons.h"

#include "BIF_gl.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_interface.h"
#include "BIF_glutil.h"

#include "BDR_drawaction.h"
#include "BDR_editcurve.h"

#include "blendef.h"
#include "mydevice.h"

/* Local function prototypes */
static void draw_nlastrips(SpaceNla *snla);
static void draw_nlatree(void);

int count_nla_levels(void);
int nla_filter (Base* base, int flags);

#define TESTBASE_SAFE(base)	((base)->flag & SELECT)

/* Implementation */
static void draw_nlatree(void)
{

	short ofsx, ofsy = 0; 
	Base *base;
	float	x, y;
	bActionStrip *strip;
	bConstraintChannel *conchan;
	float col[3];

	myortho2(0,	NLAWIDTH, G.v2d->cur.ymin, G.v2d->cur.ymax);	//	Scaling

	/* Blank out the area */
	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {	
			ofsx= curarea->winrct.xmin;	
			ofsy= curarea->winrct.ymin;
			glViewport(ofsx,  ofsy+G.v2d->mask.ymin-SCROLLB, NLAWIDTH, ( ofsy+G.v2d->mask.ymax)-( ofsy+G.v2d->mask.ymin-SCROLLB)); 
			glScissor(ofsx,  ofsy+G.v2d->mask.ymin-SCROLLB, NLAWIDTH, ( ofsy+G.v2d->mask.ymax)-( ofsy+G.v2d->mask.ymin-SCROLLB));
		}
	}
	
	BIF_GetThemeColor3fv(TH_HEADER, col);
	glClearColor(col[0], col[1], col[2], 0.0); 
	glClear(GL_COLOR_BUFFER_BIT);

	/* Clip to the scrollable area */

	glColor3ub(0x00, 0x00, 0x00);
	
	x = 0.0;

	y = count_nla_levels();

	y*= (NLACHANNELHEIGHT+NLACHANNELSKIP);

	
	for (base=G.scene->base.first; base; base=base->next){
		if (nla_filter(base, 0)){		
			BIF_ThemeColorShade(TH_HEADER, 20);
			glRectf(x,  y-NLACHANNELHEIGHT/2,  (float)NLAWIDTH,  y+NLACHANNELHEIGHT/2);

			/* Draw the name / ipo timeline*/
			if (TESTBASE_SAFE(base))
				BIF_ThemeColor(TH_TEXT_HI);
			else
				BIF_ThemeColor(TH_TEXT);
			glRasterPos2f(x+16,  y-4);

			BMF_DrawString(G.font, base->object->id.name+2);
			
			/* Draw the constraint ipos */
			for (conchan = base->object->constraintChannels.first; conchan; conchan=conchan->next){
				y-=NLACHANNELHEIGHT+NLACHANNELSKIP;
				BIF_ThemeColorShade(TH_HEADER, -30);
				glRectf(x+16,  y-NLACHANNELHEIGHT/2,  (float)NLAWIDTH,  y+NLACHANNELHEIGHT/2);
				
				if (conchan->flag & CONSTRAINT_CHANNEL_SELECT)
					BIF_ThemeColor(TH_TEXT_HI);
				else
					BIF_ThemeColor(TH_TEXT);

				glRasterPos2f(x+32,  y-4);
				BMF_DrawString(G.font, conchan->name);
			}
	
			/* Draw the action timeline */
			if (ACTIVE_ARMATURE(base)){
				glRasterPos2f(x,  y-8);				
				BIF_draw_icon(ICON_DOWNARROW_HLT);
				y-=NLACHANNELHEIGHT+NLACHANNELSKIP;
				
				if (base->object->action){
					BIF_ThemeColorShade(TH_HEADER, -30);
					glRectf(x+16,  y-NLACHANNELHEIGHT/2,  (float)NLAWIDTH,  y+NLACHANNELHEIGHT/2);

					if (TESTBASE_SAFE(base))
						BIF_ThemeColor(TH_TEXT_HI);
					else
						BIF_ThemeColor(TH_TEXT);
					glRasterPos2f(x+32,  y-4);
					BMF_DrawString(G.font, base->object->action->id.name+2);
				}
			}
			y-=NLACHANNELHEIGHT+NLACHANNELSKIP;

			/* Draw the nla strips */
			if (base->object->type==OB_ARMATURE){
				for (strip = base->object->nlastrips.first; strip; strip=strip->next){
					BIF_ThemeColorShade(TH_HEADER, -50);
					glRectf(x+32,  y-NLACHANNELHEIGHT/2,  (float)NLAWIDTH,  y+NLACHANNELHEIGHT/2);

					if (TESTBASE_SAFE(base))
						BIF_ThemeColor(TH_TEXT_HI);
					else
						BIF_ThemeColor(TH_TEXT);

	// why this test? check freeing mem when deleting strips? (ton)
					if(strip->act) {
						glRasterPos2f(x+48,  y-4);
						BMF_DrawString(G.font, strip->act->id.name+2);

						y-=(NLACHANNELHEIGHT+NLACHANNELSKIP);
					}
				}
			}
		}
	}
	
	myortho2(0,	NLAWIDTH, 0, ( ofsy+G.v2d->mask.ymax)-( ofsy+G.v2d->mask.ymin-SCROLLB));	//	Scaling
}

static void draw_nlastrips(SpaceNla *snla)
{
	rcti scr_rct;
	gla2DDrawInfo *di;
	Base *base;
	bConstraintChannel *conchan;
	float	y;
	char col1[3], col2[3];
	
	BIF_GetThemeColor3ubv(TH_SHADE2, col2);
	BIF_GetThemeColor3ubv(TH_HILITE, col1);
	
	/* Draw strips */

	scr_rct.xmin= snla->area->winrct.xmin + NLAWIDTH;
	scr_rct.ymin= snla->area->winrct.ymin + snla->v2d.mask.ymin-SCROLLB;
	scr_rct.xmax= snla->area->winrct.xmin + snla->v2d.hor.xmax;
	scr_rct.ymax= snla->area->winrct.ymin + snla->v2d.mask.ymax; 
	di= glaBegin2DDraw(&scr_rct, &G.v2d->cur);
	
	y=count_nla_levels();
	y*= (NLACHANNELHEIGHT+NLACHANNELSKIP);
	
	for (base=G.scene->base.first; base; base=base->next){
		Object *ob;
		bActionStrip *strip;
		int frame1_x, channel_y;
		
		ob=base->object;
		
		if (nla_filter(base, 0)){
			/* Draw the field */
			glEnable (GL_BLEND);
			if (TESTBASE_SAFE(base))
				glColor4b (col1[0], col1[1], col1[2], 0x22);
			else
				glColor4b (col2[0], col2[1], col2[2], 0x22);
			
			gla2DDrawTranslatePt(di, 1, y, &frame1_x, &channel_y);
			glRectf(0,  channel_y-NLACHANNELHEIGHT/2,  frame1_x,  channel_y+NLACHANNELHEIGHT/2);
			
			
			if (TESTBASE_SAFE(base))
				glColor4b (col1[0], col1[1], col1[2], 0x44);
			else
				glColor4b (col2[0], col2[1], col2[2], 0x44);
			glRectf(frame1_x,  channel_y-NLACHANNELHEIGHT/2,   G.v2d->hor.xmax,  channel_y+NLACHANNELHEIGHT/2);
			
			glDisable (GL_BLEND);
			
			/* Draw the ipo */
			draw_object_channel(di, ob, 0, y);
			y-=NLACHANNELHEIGHT+NLACHANNELSKIP;
			
			/* Draw the constraints */
			for (conchan=ob->constraintChannels.first; conchan; conchan=conchan->next){
				glEnable (GL_BLEND);
				if (conchan->flag & CONSTRAINT_CHANNEL_SELECT)
					glColor4b (col1[0], col1[1], col1[2], 0x22);
				else
					glColor4b (col2[0], col2[1], col2[2], 0x22);
				
				gla2DDrawTranslatePt(di, 1, y, &frame1_x, &channel_y);
				glRectf(0,  channel_y-NLACHANNELHEIGHT/2+4,  frame1_x,  channel_y+NLACHANNELHEIGHT/2-4);
				
				
				if (conchan->flag & CONSTRAINT_CHANNEL_SELECT)
					glColor4b (col1[0], col1[1], col1[2], 0x44);
				else
					glColor4b (col2[0], col2[1], col2[2], 0x44);
				glRectf(frame1_x,  channel_y-NLACHANNELHEIGHT/2+4,   G.v2d->hor.xmax,  channel_y+NLACHANNELHEIGHT/2-4);
				
				glDisable (GL_BLEND);
				
				/* Draw the ipo */
				draw_ipo_channel(di, conchan->ipo, 0, y);
				y-=NLACHANNELHEIGHT+NLACHANNELSKIP;

			}
		}
		
		
		
		
		/* Draw the action strip */
		if (ACTIVE_ARMATURE(base)){
			
			/* Draw the field */
			glEnable (GL_BLEND);
			if (TESTBASE_SAFE(base))
				glColor4ub (col1[0], col1[1], col1[2], 0x22);
			else
				glColor4ub (col2[0], col2[1], col2[2], 0x22);
			gla2DDrawTranslatePt(di, 1, y, &frame1_x, &channel_y);
			glRectf(0,  channel_y-NLACHANNELHEIGHT/2+4,  frame1_x,  channel_y+NLACHANNELHEIGHT/2-4);
			
			
			if (TESTBASE_SAFE(base))
				glColor4ub (col1[0], col1[1], col1[2], 0x44);
			else
				glColor4ub (col2[0], col2[1], col2[2], 0x44);
			glRectf(frame1_x,  channel_y-NLACHANNELHEIGHT/2+4,   G.v2d->hor.xmax,  channel_y+NLACHANNELHEIGHT/2-4);
			
			glDisable (GL_BLEND);
			
			/* Draw the action keys */
			draw_action_channel(di, ob->action, 0, y);

			y-=NLACHANNELHEIGHT+NLACHANNELSKIP;
			
		}

		/* Draw the nla strips */
		if (ob->type==OB_ARMATURE){
			for (strip=ob->nlastrips.first; strip; strip=strip->next){
				int stripstart, stripend;
				int blendstart, blendend;
				unsigned char r, g, b;
				
				/* Draw rect */
				if (strip->flag & ACTSTRIP_SELECT){
					r= 0xff; g= 0xff; b= 0xaa;
				}
				else{
					r= 0xe4; g= 0x9c; b= 0xc6;
				}
				
				glColor4ub (r, g, b, 0xFF);
				
				gla2DDrawTranslatePt(di, strip->start+strip->blendin, y, &stripstart, &channel_y);
				gla2DDrawTranslatePt(di, strip->end-strip->blendout, y, &stripend, &channel_y);
				glRectf(stripstart,  channel_y-NLACHANNELHEIGHT/2+3,  stripend,  channel_y+NLACHANNELHEIGHT/2-3);
				
				
				/* Draw blendin */
				if (strip->blendin>0){
					glBegin(GL_TRIANGLES);
					
					gla2DDrawTranslatePt(di, strip->start, y, &blendstart, &channel_y);
					
					glColor4ub (r*0.75, g*0.75, b*0.75, 0xFF);
					glVertex2f(blendstart, channel_y-NLACHANNELHEIGHT/2+3);
					glVertex2f(stripstart, channel_y+NLACHANNELHEIGHT/2-3);
					glVertex2f(stripstart, channel_y-NLACHANNELHEIGHT/2+3);
					
					
					glEnd();
				}
				if (strip->blendout>0){
					glBegin(GL_TRIANGLES);
					gla2DDrawTranslatePt(di, strip->end, y, &blendend, &channel_y);
					glColor4ub (r*0.75, g*0.75, b*0.75, 0xFF);
					glVertex2f(blendend, channel_y-NLACHANNELHEIGHT/2+3);
					glVertex2f(stripend, channel_y+NLACHANNELHEIGHT/2-3);
					glVertex2f(stripend, channel_y-NLACHANNELHEIGHT/2+3);
					glEnd();
				}
				
				/* Draw border */
				glBegin(GL_LINE_STRIP);
				glColor4f(1, 1, 1, 0.5); 
				gla2DDrawTranslatePt(di, strip->start, y, &stripstart, &channel_y);
				gla2DDrawTranslatePt(di, strip->end, y, &stripend, &channel_y);
				
				glVertex2f(stripstart, channel_y-NLACHANNELHEIGHT/2+3);
				glVertex2f(stripstart, channel_y+NLACHANNELHEIGHT/2-3);
				glVertex2f(stripend, channel_y+NLACHANNELHEIGHT/2-3);
				glColor4f(0, 0, 0, 0.5); 
				glVertex2f(stripend, channel_y-NLACHANNELHEIGHT/2+3);
				glVertex2f(stripstart, channel_y-NLACHANNELHEIGHT/2+3);
				glEnd();
				
				glEnable (GL_BLEND);

				/* Show strip extension */
				if (strip->flag & ACTSTRIP_HOLDLASTFRAME){
					glColor4ub (r, g, b, 0x55);
					
					glRectf(stripend,  channel_y-NLACHANNELHEIGHT/2+4,  G.v2d->hor.xmax,  channel_y+NLACHANNELHEIGHT/2-2);
				}
				
				/* Show repeat */
				if (strip->repeat > 1.0 && !(strip->flag & ACTSTRIP_USESTRIDE)){
					float rep = 1;
					glBegin(GL_LINES);
					while (rep<strip->repeat){
						/* Draw line */	
						glColor4f(0, 0, 0, 0.5); 
						gla2DDrawTranslatePt(di, strip->start+(rep*((strip->end-strip->start)/strip->repeat)), y, &frame1_x, &channel_y);
						glVertex2f(frame1_x, channel_y-NLACHANNELHEIGHT/2+4);
						glVertex2f(frame1_x, channel_y+NLACHANNELHEIGHT/2-2);
						
						glColor4f(1.0, 1.0, 1.0, 0.5); 
						gla2DDrawTranslatePt(di, strip->start+(rep*((strip->end-strip->start)/strip->repeat)), y, &frame1_x, &channel_y);
						glVertex2f(frame1_x+1, channel_y-NLACHANNELHEIGHT/2+4);
						glVertex2f(frame1_x+1, channel_y+NLACHANNELHEIGHT/2-2);
						rep+=1.0;
					}
					glEnd();
					
				}
				glDisable (GL_BLEND);
				
				y-=(NLACHANNELHEIGHT+NLACHANNELSKIP);
			}
			
		}
	}
	glaEnd2DDraw(di);
	
}

/* ******* panel *********** */

#define B_NLA_PANEL		121

static bActionStrip *get_active_nlastrip(void)
/* For now just returns the first selected strip */
{
	Base *base;
	bActionStrip *strip;
	
	for (base=G.scene->base.first; base; base=base->next){
		if (nla_filter(base, 0) && base->object->type==OB_ARMATURE){
			for (strip=base->object->nlastrips.first; strip; strip=strip->next){
				if (strip->flag & ACTSTRIP_SELECT)
					return strip;
			}
		}
	}
	
	return NULL;
}

void do_nlabuts(unsigned short event)
{
	bActionStrip *strip;
		
	/* Determine if an nla strip has been selected */
	strip = get_active_nlastrip();
	if (!strip) return;
	
	switch(event) {
	case B_REDR:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWNLA, 0);
		break;
	case B_NLA_PANEL:
		if (strip->end<strip->start)
			strip->end=strip->start;
	
	
		if (strip->blendin>(strip->end-strip->start))
			strip->blendin = strip->end-strip->start;
	
		if (strip->blendout>(strip->end-strip->start))
			strip->blendout = strip->end-strip->start;
	
		if (strip->blendin > (strip->end-strip->start-strip->blendout))
			strip->blendin = (strip->end-strip->start-strip->blendout);
	
		if (strip->blendout > (strip->end-strip->start-strip->blendin))
			strip->blendout = (strip->end-strip->start-strip->blendin);
		
		
		update_for_newframe_muted();
		allqueue (REDRAWNLA, 0);
		allqueue (REDRAWVIEW3D, 0);
		
		break;
	}
}

static void nla_panel_properties(short cntrl)	// NLA_HANDLER_PROPERTIES
{
	bActionStrip *strip;
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "nla_panel_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(NLA_HANDLER_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "Transform Properties", "NLA", 10, 230, 318, 204)==0) return;

	/* Determine if an nla strip has been selected */
	strip = get_active_nlastrip();
	if (!strip) return;
	
	// first labels, for simpler align code :)
	uiDefBut(block, LABEL, 0, "Timeline Range:",	10,180,300,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, LABEL, 0, "Action Range:",		10,140,300,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, LABEL, 0, "Blending:",			10,100,300,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, LABEL, 0, "Options:",			10,60,300,19, 0, 0, 0, 0, 0, "");

	uiBlockBeginAlign(block);
	uiDefBut(block, NUM|FLO, B_REDR, "Strip Start:", 10,160,150,19, &strip->start, 1.0, 18000.0, 100, 0, "First frame in the timeline");
	uiDefBut(block, NUM|FLO, B_REDR, "Strip End:", 	160,160,150,19, &strip->end, 1.0, 18000.0, 100, 0, "Last frame in the timeline");

	uiBlockBeginAlign(block);
	uiDefBut(block, NUM|FLO, B_REDR, "Action Start:", 10,120,150,19, &strip->actstart, 1.0, 18000.0, 100, 0, "First frame of the action to map to the playrange");
	uiDefBut(block, NUM|FLO, B_REDR, "Action End:", 160,120,150,19, &strip->actend, 1.0, 18000.0, 100, 0, "Last frame of the action to map to the playrange");

	uiBlockBeginAlign(block);
	uiDefBut(block, NUM|FLO, B_REDR, "Blendin:", 	10,80,150,19, &strip->blendin, 1.0, 18000.0, 100, 0, "Number of frames of ease-in");
	uiDefBut(block, NUM|FLO, B_REDR, "Blendout:", 	160,80,150,19, &strip->blendout, 1.0, 18000.0, 100, 0, "Number of frames of ease-out");

	uiBlockBeginAlign(block);
	uiDefBut(block, NUM|FLO, B_REDR, "Repeat:", 	10,40,150,19, &strip->repeat, 1.0, 18000.0, 100, 0, "Number of times the action should repeat");
	uiDefBut(block, NUM|FLO, B_REDR, "Stride:", 	160,40,150,19, &strip->stridelen, 1.0, 18000.0, 100, 0, "Distance covered by one complete cycle of the action specified in the Action Range");

	uiBlockBeginAlign(block);
	uiDefBut(block, TOG|SHO|BIT|ACTSTRIP_USESTRIDEBIT, B_REDR, "Use Path",	10,0,100,19, &strip->flag, 0, 0, 0, 0, "Plays action based on path position & stride. Only armatures parented to a path");
	uiDefBut(block, TOG|SHO|BIT|ACTSTRIP_HOLDLASTFRAMEBIT, B_REDR, "Hold",	110,0,100,19, &strip->flag, 0, 0, 0, 0, "Toggles whether to continue displaying the last frame past the end of the strip");
	uiDefBut(block, TOG|SHO, B_REDR, "Add",	210,0,100,19, &strip->mode, 0, 0, 0, 0, "Toggles additive blending mode");
}

static void nla_blockhandlers(ScrArea *sa)
{
	SpaceNla *snla= sa->spacedata.first;
	short a;
	
	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		switch(snla->blockhandler[a]) {

		case NLA_HANDLER_PROPERTIES:
			nla_panel_properties(snla->blockhandler[a+1]);
			break;
		
		}
		/* clear action value for event */
		snla->blockhandler[a+1]= 0;
	}
	uiDrawBlocksPanels(sa, 0);
}


void drawnlaspace(ScrArea *sa, void *spacedata)
{
	float col[3];
	short ofsx = 0, ofsy = 0;
	
	uiFreeBlocksWin(&sa->uiblocks, sa->win);	/* for panel handler to work */

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) ;
	
	calc_scrollrcts(G.v2d, curarea->winx, curarea->winy);
	
	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {	
			ofsx= curarea->winrct.xmin;	
			ofsy= curarea->winrct.ymin;
			glViewport(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1); 
			glScissor(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
		}
	}
	
	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
	bwin_clear_viewmat(sa->win);	/* clear buttons view */
	glLoadIdentity();
	
	/*	Draw backdrop */
	calc_ipogrid();	
	draw_ipogrid();

	/* Draw channel strips */
	draw_nlastrips(G.snla);

	/* Draw current frame */
	glViewport(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1); 
	glScissor(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
	myortho2 (G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
	draw_cfra_action();

	/* Draw scroll */
	mywinset(curarea->win);	// reset scissor too
	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);
		if(G.v2d->scroll) drawscroll(0);
	}

	/* Draw channel names */
	draw_nlatree();

	mywinset(curarea->win);	// reset scissor too
	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);
	draw_area_emboss(sa);
	
	/* it is important to end a view in a transform compatible with buttons */
	bwin_scalematrix(sa->win, G.snla->blockscale, G.snla->blockscale, G.snla->blockscale);
	nla_blockhandlers(sa);

	curarea->win_swap= WIN_BACK_OK;
}

int count_nla_levels(void)
{
	Base *base;
	int y=0;

	for (y=0, base=G.scene->base.first; base; base=base->next)
	{
		if (nla_filter(base,0 )){
			/*	Ipo	*/
			y++;
			/*	Constraint channels */
			y+=BLI_countlist(&base->object->constraintChannels);

			if (base->object->type==OB_ARMATURE){
				/* Action */
				if(base->object->action){
				//	bActionChannel *achan;
					y++;

				//	for (achan=base->object->action->chanbase.first; achan; achan=achan->next){
				//		y+=BLI_countlist(&achan->constraintChannels);
				//	}
				}
				/* Nla strips */
				y+= BLI_countlist(&base->object->nlastrips);
			}
		}
	}

	return y;
}

int nla_filter (Base* base, int flags)
{
	Object *ob = base->object;

	/* Only objects with ipos */
	if (ob->ipo)
		return 1;

	if (ob->constraintChannels.first)
		return 1;

	/* Only armatures */
	if (ob->type==OB_ARMATURE)
		return 1;

	else return 0;
}

