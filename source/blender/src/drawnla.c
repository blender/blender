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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#pragma warning (once : 4761)
#endif

#include "BMF_Api.h"

#include <stdlib.h>
#include <string.h>
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

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"

#include "BSE_drawnla.h"
#include "BSE_drawipo.h"
#include "BSE_editnla_types.h"
#include "BSE_headerbuttons.h"
#include "BSE_time.h"

#include "BIF_editnla.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"

#include "BDR_drawaction.h"
#include "BDR_editcurve.h"

#include "blendef.h"
#include "butspace.h"
#include "mydevice.h"

#define TESTBASE_SAFE(base)	((base)->flag & SELECT && ((base)->object->restrictflag & OB_RESTRICT_VIEW)==0)


/* the left hand side with channels only */
static void draw_nla_channels(void)
{
	bActionStrip *strip;
	Base *base;
	Object *ob;
	float	x, y;
	short ofsx, ofsy = 0; 

	myortho2(0,	NLAWIDTH, G.v2d->cur.ymin, G.v2d->cur.ymax);	//	Scaling

	/* Clip to the scrollable area */
	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {	
			ofsx= curarea->winrct.xmin;	
			ofsy= curarea->winrct.ymin;
			glViewport(ofsx,  ofsy+G.v2d->mask.ymin, NLAWIDTH, ( ofsy+G.v2d->mask.ymax)-( ofsy+G.v2d->mask.ymin)); 
			glScissor(ofsx,  ofsy+G.v2d->mask.ymin, NLAWIDTH, ( ofsy+G.v2d->mask.ymax)-( ofsy+G.v2d->mask.ymin));
		}
	}
	
	glColor3ub(0x00, 0x00, 0x00);
	
	x = 0.0;
	y = count_nla_levels();
	y*= (NLACHANNELHEIGHT+NLACHANNELSKIP);
	
	for (base=G.scene->base.first; base; base=base->next){
		if (nla_filter(base)) {
			ob= base->object;
			
			BIF_ThemeColorShade(TH_HEADER, 20);
			glRectf(x,  y-NLACHANNELHEIGHT/2,  (float)NLAWIDTH,  y+NLACHANNELHEIGHT/2);

			/* Draw the name / ipo timeline*/
			if (TESTBASE_SAFE(base))
				BIF_ThemeColor(TH_TEXT_HI);
			else
				BIF_ThemeColor(TH_TEXT);
			glRasterPos2f(x+34,  y-4);
			BMF_DrawString(G.font, ob->id.name+2);
			
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) ;
			
			/* icon to indicate expanded or collapsed */
			if ((ob->nlastrips.first) || (ob->action)) {
				if (ob->nlaflag & OB_NLA_COLLAPSED)
					BIF_icon_draw(x+1, y-8, ICON_TRIA_RIGHT);
				else
					BIF_icon_draw(x+1, y-8, ICON_TRIA_DOWN);
			}
			
			/* icon to indicate nla or action  */
			if(ob->nlastrips.first && ob->action) {
				if(ob->nlaflag & OB_NLA_OVERRIDE)
					BIF_icon_draw(x+17, y-8, ICON_NLA);
				else
					BIF_icon_draw(x+17, y-8, ICON_ACTION);
			}	
			
			/* icon to indicate if ipo-channel muted */
			if (ob->ipo) {
				if (ob->ipo->muteipo) 
					BIF_icon_draw(NLAWIDTH-16, y-NLACHANNELHEIGHT/2, ICON_MUTE_IPO_ON);
				else 
					BIF_icon_draw(NLAWIDTH-16, y-NLACHANNELHEIGHT/2, ICON_MUTE_IPO_OFF);
			}
			
			glDisable(GL_BLEND);
			y-=NLACHANNELHEIGHT+NLACHANNELSKIP;
			
			/* check if object's nla strips are collapsed or not */
			if ((ob->nlaflag & OB_NLA_COLLAPSED)==0) {
				/* Draw the action timeline */
				if (ob->action){
					BIF_ThemeColorShade(TH_HEADER, -20);
					glRectf(x+19,  y-NLACHANNELHEIGHT/2,  (float)NLAWIDTH,  y+NLACHANNELHEIGHT/2);

					if (TESTBASE_SAFE(base))
						BIF_ThemeColor(TH_TEXT_HI);
					else
						BIF_ThemeColor(TH_TEXT);
					glRasterPos2f(x+38,  y-4);
					BMF_DrawString(G.font, ob->action->id.name+2);
					
					/* icon for active action (no strip mapping) */
					for (strip = ob->nlastrips.first; strip; strip=strip->next)
						if(strip->flag & ACTSTRIP_ACTIVE) break;
					if(strip==NULL) {
						glEnable(GL_BLEND);
						BIF_icon_draw(x+5, y-8, ICON_DOT);
						glDisable(GL_BLEND);
					}
					
					y-=NLACHANNELHEIGHT+NLACHANNELSKIP;
				}

				/* Draw the nla strips */
				for (strip = ob->nlastrips.first; strip; strip=strip->next){
					BIF_ThemeColorShade(TH_HEADER, -40);
					glRectf(x+32,  y-NLACHANNELHEIGHT/2,  (float)NLAWIDTH,  y+NLACHANNELHEIGHT/2);

					if (TESTBASE_SAFE(base))
						BIF_ThemeColor(TH_TEXT_HI);
					else
						BIF_ThemeColor(TH_TEXT);

					// why this test? check freeing mem when deleting strips? (ton)
					if(strip->act) {
						glRasterPos2f(x+48,  y-4);
						BMF_DrawString(G.font, strip->act->id.name+2);
						
						glEnable(GL_BLEND);
						
						if(strip->flag & ACTSTRIP_ACTIVE)
							BIF_icon_draw(x+16, y-8, ICON_DOT);
							
						if(strip->modifiers.first)
							BIF_icon_draw(x+34, y-8, ICON_MODIFIER);
						
						if(strip->flag & ACTSTRIP_MUTE)
							BIF_icon_draw(NLAWIDTH-16, y-NLACHANNELHEIGHT/2, ICON_MUTE_IPO_ON);
						else
							BIF_icon_draw(NLAWIDTH-16, y-NLACHANNELHEIGHT/2, ICON_MUTE_IPO_OFF);
						
						glDisable(GL_BLEND);
					}
					
					y-=(NLACHANNELHEIGHT+NLACHANNELSKIP);
				}
			}
		}
	}
	
	myortho2(0,	NLAWIDTH, 0, ( ofsy+G.v2d->mask.ymax)-( ofsy+G.v2d->mask.ymin));	//	Scaling
}

void map_active_strip(gla2DDrawInfo *di, Object *ob, int restore)
{
	static rctf stored;
	
	if(restore)
		gla2DSetMap(di, &stored);
	else {
		rctf map;
		
		gla2DGetMap(di, &stored);
		map= stored;
		map.xmin= get_action_frame(ob, map.xmin);
		map.xmax= get_action_frame(ob, map.xmax);
		if(map.xmin==map.xmax) map.xmax+= 1.0;
		gla2DSetMap(di, &map);
	}
}

/* the right hand side, with strips and keys */
static void draw_nla_strips_keys(SpaceNla *snla)
{
	Base *base;
	rcti scr_rct;
	gla2DDrawInfo *di;
	float	y;
	char col1[3], col2[3];
	
	BIF_GetThemeColor3ubv(TH_SHADE2, col2);
	BIF_GetThemeColor3ubv(TH_HILITE, col1);
	
	/* Draw strips */

	scr_rct.xmin= snla->area->winrct.xmin + snla->v2d.mask.xmin;
	scr_rct.ymin= snla->area->winrct.ymin + snla->v2d.mask.ymin;
	scr_rct.xmax= snla->area->winrct.xmin + snla->v2d.hor.xmax;
	scr_rct.ymax= snla->area->winrct.ymin + snla->v2d.mask.ymax; 
	di= glaBegin2DDraw(&scr_rct, &G.v2d->cur);
	
	y=count_nla_levels();
	y*= (NLACHANNELHEIGHT+NLACHANNELSKIP);
	
	for (base=G.scene->base.first; base; base=base->next){
		Object *ob= base->object;
		bActionStrip *strip;
		int frame1_x, channel_y;
		
		if (nla_filter(base)==0)
			continue;
			
		/* Draw the field */
		glEnable (GL_BLEND);
		if (TESTBASE_SAFE(base))
			glColor4ub (col1[0], col1[1], col1[2], 0x22);
		else
			glColor4ub (col2[0], col2[1], col2[2], 0x22);
		
		gla2DDrawTranslatePt(di, 1, y, &frame1_x, &channel_y);
		glRectf(0,  channel_y-NLACHANNELHEIGHT/2,  frame1_x,  channel_y+NLACHANNELHEIGHT/2);
		
		
		if (TESTBASE_SAFE(base))
			glColor4ub (col1[0], col1[1], col1[2], 0x44);
		else
			glColor4ub (col2[0], col2[1], col2[2], 0x44);
		glRectf(frame1_x,  channel_y-NLACHANNELHEIGHT/2,   G.v2d->hor.xmax,  channel_y+NLACHANNELHEIGHT/2);
		
		glDisable (GL_BLEND);
		
		/* Draw the ipo keys */
		draw_object_channel(di, ob, y);
		
		y-=NLACHANNELHEIGHT+NLACHANNELSKIP;
		
		/* check if object nla-strips expanded or not */
		if (ob->nlaflag & OB_NLA_COLLAPSED)
			continue;
		
		
		/* Draw the action strip */
		if (ob->action) {
			
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
			
			/* Draw the action keys, optionally corrected for active strip */
			map_active_strip(di, ob, 0);
			draw_action_channel(di, ob->action, y);
			map_active_strip(di, ob, 1);
			
			y-=NLACHANNELHEIGHT+NLACHANNELSKIP;
			
		}

		/* Draw the nla strips */
		for (strip=ob->nlastrips.first; strip; strip=strip->next){
			int stripstart, stripend;
			int blendstart, blendend;
			
			/* Draw rect */
			if (strip->flag & ACTSTRIP_SELECT)
				BIF_ThemeColor(TH_STRIP_SELECT);
			else
				BIF_ThemeColor(TH_STRIP);
			
			gla2DDrawTranslatePt(di, strip->start+strip->blendin, y, &stripstart, &channel_y);
			gla2DDrawTranslatePt(di, strip->end-strip->blendout, y, &stripend, &channel_y);
			glRectf(stripstart,  channel_y-NLACHANNELHEIGHT/2+3,  stripend,  channel_y+NLACHANNELHEIGHT/2-3);
			
			if (strip->flag & ACTSTRIP_SELECT)
				BIF_ThemeColorShade(TH_STRIP_SELECT, -60);
			else
				BIF_ThemeColorShade(TH_STRIP, -60);
			
			/* Draw blendin */
			if (strip->blendin>0){
				glBegin(GL_TRIANGLES);
				
				gla2DDrawTranslatePt(di, strip->start, y, &blendstart, &channel_y);
				
				glVertex2f(blendstart, channel_y-NLACHANNELHEIGHT/2+3);
				glVertex2f(stripstart, channel_y+NLACHANNELHEIGHT/2-3);
				glVertex2f(stripstart, channel_y-NLACHANNELHEIGHT/2+3);
				
				
				glEnd();
			}
			if (strip->blendout>0){
				glBegin(GL_TRIANGLES);
				
				gla2DDrawTranslatePt(di, strip->end, y, &blendend, &channel_y);

				glVertex2f(blendend, channel_y-NLACHANNELHEIGHT/2+3);
				glVertex2f(stripend, channel_y+NLACHANNELHEIGHT/2-3);
				glVertex2f(stripend, channel_y-NLACHANNELHEIGHT/2+3);
				glEnd();
			}
			
			gla2DDrawTranslatePt(di, strip->start, y, &stripstart, &channel_y);
			gla2DDrawTranslatePt(di, strip->end, y, &stripend, &channel_y);
			
			/* muted strip */
			if(strip->flag & ACTSTRIP_MUTE) {
				glColor3f(1, 0, 0); 
				glBegin(GL_LINES);
				glVertex2f(stripstart, channel_y-NLACHANNELHEIGHT/2+3);
				glVertex2f(stripend, channel_y+NLACHANNELHEIGHT/2-3);
				glEnd();
			}
			
			/* Draw border */
			glEnable (GL_BLEND);
			glBegin(GL_LINE_STRIP);
			glColor4f(1, 1, 1, 0.7); 
			
			glVertex2f(stripstart, channel_y-NLACHANNELHEIGHT/2+3);
			glVertex2f(stripstart, channel_y+NLACHANNELHEIGHT/2-3);
			glVertex2f(stripend, channel_y+NLACHANNELHEIGHT/2-3);
			glColor4f(0, 0, 0, 0.7); 
			glVertex2f(stripend, channel_y-NLACHANNELHEIGHT/2+3);
			glVertex2f(stripstart, channel_y-NLACHANNELHEIGHT/2+3);
			glEnd();
			
			/* Show strip extension */
			if (strip->flag & ACTSTRIP_HOLDLASTFRAME){
				if (strip->flag & ACTSTRIP_SELECT)
					BIF_ThemeColorShadeAlpha(TH_STRIP_SELECT, 0, -180);
				else
					BIF_ThemeColorShadeAlpha(TH_STRIP, 0, -180);
				
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
	glaEnd2DDraw(di);
	
}

/* ******* panel *********** */

#define B_NLA_PANEL		121
#define B_NLA_LOCK		122
#define B_NLA_SCALE		123
#define B_NLA_SCALE2	124
#define B_NLA_MOD_ADD	125
#define B_NLA_MOD_NEXT	126
#define B_NLA_MOD_PREV	127
#define B_NLA_MOD_DEL	128
#define B_NLA_MOD_DEPS	129

/* For now just returns the first selected strip */
bActionStrip *get_active_nlastrip(Object **obpp)
{
	Base *base;
	bActionStrip *strip;
	
	for (base=G.scene->base.first; base; base=base->next){
		if ((base->object->nlaflag & OB_NLA_COLLAPSED)==0) {
			for (strip=base->object->nlastrips.first; strip; strip=strip->next){
				if (strip->flag & ACTSTRIP_SELECT) {
					*obpp= base->object;
					return strip;
				}
			}
		}
	}
	
	return NULL;
}

void do_nlabuts(unsigned short event)
{
	Object *ob;
	bActionStrip *strip;
		
	/* Determine if an nla strip has been selected */
	strip = get_active_nlastrip(&ob);
	if (!strip) return;
	
	switch(event) {
	case B_REDR:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWNLA, 0);
		break;
	case B_NLA_PANEL:
		DAG_object_flush_update(G.scene, ob, OB_RECALC_OB|OB_RECALC_DATA);
		allqueue (REDRAWNLA, 0);
		allqueue (REDRAWVIEW3D, 0);
		break;
	case B_NLA_SCALE: /* adjust end-frame when scale is changed */
		{
			float actlen= strip->actend - strip->actstart;
			float mapping= strip->scale * strip->repeat;
			
			if (mapping != 0.0f)
				strip->end = (actlen * mapping) + strip->start; 
			else
				printf("NLA Scale Error: Scale = %0.4f, Repeat = %0.4f \n", strip->scale, strip->repeat);
			
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_NLA_SCALE2: /* adjust scale when end-frame is changed */
		{
			float actlen= strip->actend - strip->actstart;
			float len= strip->end - strip->start;
			
			strip->scale= len / (actlen * strip->repeat);
			
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_NLA_LOCK:
		synchronize_action_strips();
		
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
		
	case B_NLA_MOD_ADD:
		{
			bActionModifier *amod= MEM_callocN(sizeof(bActionModifier), "bActionModifier");
			
			BLI_addtail(&strip->modifiers, amod);
			strip->curmod= BLI_countlist(&strip->modifiers)-1;
			allqueue (REDRAWNLA, 0);
		}
		break;
	case B_NLA_MOD_DEL:
		if(strip->modifiers.first) {
			bActionModifier *amod= BLI_findlink(&strip->modifiers, strip->curmod);
			BLI_remlink(&strip->modifiers, amod);
			MEM_freeN(amod);
			if(strip->curmod) strip->curmod--;
			allqueue (REDRAWNLA, 0);
		}
		break;
	case B_NLA_MOD_NEXT:
		if(strip->curmod < BLI_countlist(&strip->modifiers)-1)
			strip->curmod++;
		allqueue (REDRAWNLA, 0);
		break;
	case B_NLA_MOD_PREV:
		if(strip->curmod > 0)
			strip->curmod--;
		allqueue (REDRAWNLA, 0);
		break;
	case B_NLA_MOD_DEPS:
		DAG_scene_sort(G.scene);
		DAG_object_flush_update(G.scene, ob, OB_RECALC_OB|OB_RECALC_DATA);
		break;
	}
}

static char *make_modifier_menu(ListBase *lb)
{
	bActionModifier *amod;
	int index= 1;
	char *str, item[64], *types[3]={"Deform", "Noise", "Oomph"};
	
	for (amod = lb->first; amod; amod=amod->next, index++);
	str= MEM_mallocN(index*64, "key string");
	str[0]= 0;
	
	index= 0;
	for (amod = lb->first; amod; amod=amod->next, index++) {
		sprintf (item,  "|%s %s%%x%d", types[amod->type], amod->channel, index);
		strcat(str, item);
	}
	
	return str;
}


static void nla_panel_properties(short cntrl)	// NLA_HANDLER_PROPERTIES
{
	Object *ob;
	bActionStrip *strip;
	uiBlock *block;
	uiBut *but;

	block= uiNewBlock(&curarea->uiblocks, "nla_panel_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(NLA_HANDLER_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "Transform Properties", "NLA", 10, 230, 318, 224)==0) return;

	/* Determine if an nla strip has been selected */
	strip = get_active_nlastrip(&ob);
	if (!strip) return;
	
	/* first labels, for simpler align code :) */
	uiDefBut(block, LABEL, 0, "Timeline Range:",	10,180,300,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, LABEL, 0, "Blending:",			10,120,150,19, 0, 0, 0, 0, 0, "");
	uiDefBut(block, LABEL, 0, "Options:",			160,120,150,19, 0, 0, 0, 0, 0, "");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_NLA_PANEL, "Strip Start:",	10,160,150,19, &strip->start, -1000.0, strip->end-1, 100, 0, "First frame in the timeline");
	uiDefButF(block, NUM, B_NLA_SCALE2, "Strip End:", 	160,160,150,19, &strip->end, strip->start+1, MAXFRAMEF, 100, 0, "Last frame in the timeline");

	uiDefIconButBitS(block, ICONTOG, ACTSTRIP_LOCK_ACTION, B_NLA_LOCK, ICON_UNLOCKED,	10,140,20,19, &(strip->flag), 0, 0, 0, 0, "Toggles Action end/start to be automatic mapped to strip duration");
	if(strip->flag & ACTSTRIP_LOCK_ACTION) {
		char str[40];
		sprintf(str, "Action Start: %.2f", strip->actstart);
		uiDefBut(block, LABEL, B_NOP, str,			30,140,140,19, NULL, 0.0, 0.0, 0, 0, "First frame of the action to map to the playrange");
		sprintf(str, "Action End: %.2f", strip->actend);
		uiDefBut(block, LABEL, B_NOP, str,			170,140,140,19, NULL, 0.0, 0.0, 0, 0, "Last frame of the action to map to the playrange");
	}
	else {
		uiDefButF(block, NUM, B_NLA_PANEL, "Action Start:",	30,140,140,19, &strip->actstart, -1000.0, strip->actend-1, 100, 0, "First frame of the action to map to the playrange");
		uiDefButF(block, NUM, B_NLA_PANEL, "Action End:",	170,140,140,19, &strip->actend, strip->actstart+1, MAXFRAMEF, 100, 0, "Last frame of the action to map to the playrange");
	}
	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, ACTSTRIP_AUTO_BLENDS, B_NLA_LOCK, "Auto-Blending", 10,100,145,19, &(strip->flag), 0, 0, 0, 0, "Toggles automatic calculation of blendin/out values");
	if (strip->flag & ACTSTRIP_AUTO_BLENDS) {
		char str[32];
		sprintf(str, "In: %.2f", strip->blendin);
		uiDefBut(block, LABEL, B_NOP, str,			10,80,77,19, NULL, 0.0, 0.0, 0, 0, "Number of frames of ease-in");
		sprintf(str, "Out: %.2f", strip->blendout);
		uiDefBut(block, LABEL, B_NOP, str,			77,80,78,19, NULL, 0.0, 0.0, 0, 0, "Number of frames of ease-out");
	}
	else {
		uiDefButF(block, NUM, B_NLA_PANEL, "In:",	10,80,77,19, &strip->blendin, 0.0, strip->end-strip->start, 100, 0, "Number of frames of ease-in");
		uiDefButF(block, NUM, B_NLA_PANEL, "Out:",	77,80,78,19, &strip->blendout, 0.0, strip->end-strip->start, 100, 0, "Number of frames of ease-out");
	}
	uiDefButBitS(block, TOG, ACTSTRIP_MUTE, B_NLA_PANEL, "Mute", 10,60,145,19, &strip->flag, 0, 0, 0, 0, "Toggles whether the strip contributes to the NLA solution");
	
	uiBlockBeginAlign(block);
		// FIXME: repeat and scale are too cramped!
	uiDefButF(block, NUMABS, B_NLA_SCALE, "Repeat:", 	160,100,75,19, &strip->repeat, 0.001, 1000.0f, 100, 0, "Number of times the action should repeat");
	if ((strip->actend - strip->actstart) < 1.0f) {
		uiBlockSetCol(block, TH_REDALERT);
		uiDefButF(block, NUMABS, B_NLA_SCALE, "Scale:", 	235,100,75,19, &strip->scale, 0.001, 1000.0f, 100, 0, "Please run Alt-S to fix up this error");
		uiBlockSetCol(block, TH_AUTO);
	}
	else
		uiDefButF(block, NUMABS, B_NLA_SCALE, "Scale:", 	235,100,75,19, &strip->scale, 0.001, 1000.0f, 100, 0, "Amount the action should be scaled by");
	but= uiDefButC(block, TEX, B_NLA_PANEL, "OffsBone:", 160,80,150,19, strip->offs_bone, 0, 31.0f, 0, 0, "Name of Bone that defines offset for repeat");
	uiButSetCompleteFunc(but, autocomplete_bone, (void *)ob);
	uiDefButBitS(block, TOG, ACTSTRIP_HOLDLASTFRAME, B_NLA_PANEL, "Hold",	160,60,75,19, &strip->flag, 0, 0, 0, 0, "Toggles whether to continue displaying the last frame past the end of the strip");
	uiDefButS(block, TOG, B_NLA_PANEL, "Add",								235,60,75,19, &strip->mode, 0, 0, 0, 0, "Toggles additive blending mode");

	uiBlockEndAlign(block);

	uiDefButBitS(block, TOG, ACTSTRIP_USESTRIDE, B_NLA_PANEL, "Stride Path",	10, 30,140,19, &strip->flag, 0, 0, 0, 0, "Plays action based on path position & stride");
	
	if (strip->offs_bone[0]) {
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, ACTSTRIP_CYCLIC_USEX, B_NLA_PANEL, "Use X",	160,30,50,19, &strip->flag, 0, 0, 0, 0, "Turn off automatic single-axis cycling and use X as an offset axis.  Note that you can use multiple axes at once.");
		uiDefButBitS(block, TOG, ACTSTRIP_CYCLIC_USEY, B_NLA_PANEL, "Use Y",	210,30,50,19, &strip->flag, 0, 0, 0, 0, "Turn off automatic single-axis cycling and use Y as an offset axis.  Note that you can use multiple axes at once.");
		uiDefButBitS(block, TOG, ACTSTRIP_CYCLIC_USEZ, B_NLA_PANEL, "Use Z",	260,30,50,19, &strip->flag, 0, 0, 0, 0, "Turn off automatic single-axis cycling and use Z as an offset axis.  Note that you can use multiple axes at once.");
		uiBlockEndAlign(block);
	}
	
	if(ob->dup_group)
		uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_NLA_PANEL, "Target:",	160,30, 150, 19, &strip->object, "Target Object in this group"); 
	
	if(strip->flag & ACTSTRIP_USESTRIDE) {
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, OB_DISABLE_PATH, B_NLA_PANEL, "Disable",	10,0,60,19, &ob->ipoflag, 0, 0, 0, 0, "Disable path temporally, for editing cycles");
		
		uiDefButF(block, NUM, B_NLA_PANEL, "Offs:",			70,0,120,19, &strip->actoffs, -500, 500.0, 100, 0, "Action offset in frames to tweak cycle of the action within the stride");
		uiDefButF(block, NUM, B_NLA_PANEL, "Stri:",			190,0,120,19, &strip->stridelen, 0.0001, 1000.0, 100, 0, "Distance covered by one complete cycle of the action specified in the Action Range");
		
		uiDefButS(block, ROW, B_NLA_PANEL, "X",				10, -20, 33, 19, &strip->stride_axis, 1, 0, 0, 0, "Dominant axis for Stride Bone");
		uiDefButS(block, ROW, B_NLA_PANEL, "Y",				43, -20, 33, 19, &strip->stride_axis, 1, 1, 0, 0, "Dominant axis for Stride Bone");
		uiDefButS(block, ROW, B_NLA_PANEL, "Z",				76, -20, 34, 19, &strip->stride_axis, 1, 2, 0, 0, "Dominant axis for Stride Bone");
		
		but= uiDefBut(block, TEX, B_NLA_PANEL, "Stride Bone:",	110, -20, 200, 19, strip->stridechannel, 1, 31, 0, 0, "Name of Bone used for stride");
		uiButSetCompleteFunc(but, autocomplete_bone, (void *)ob);
	}
	else {	/* modifiers */
		bActionModifier *amod= BLI_findlink(&strip->modifiers, strip->curmod);
		
		uiBlockBeginAlign(block);
		uiDefBut(block, BUT, B_NLA_MOD_ADD, "Add Modifier",				10,0,140,19, NULL, 0, 0, 0, 0, "");
		if(amod) {
			char *strp= make_modifier_menu(&strip->modifiers);
			
			uiDefIconBut(block, BUT, B_NLA_MOD_NEXT, ICON_TRIA_LEFT,	150,0,20,19, NULL, 0, 0, 0, 0, "Previous Modifier");
			uiDefButS(block, MENU, B_NLA_PANEL, strp,					170,0,20,19, &strip->curmod, 0, 0, 0, 0, "Browse modifier");
			MEM_freeN(strp);
			uiDefIconBut(block, BUT, B_NLA_MOD_PREV, ICON_TRIA_RIGHT,	190,0,20,19, NULL, 0, 0, 0, 0, "Next Modifier");
			uiDefButS(block, MENU, B_REDR, "Deform %x0|Noise %x1|Oomph %x2",	210,0,80,19, &amod->type, 0, 0, 0, 0, "Modifier type");
			uiDefIconBut(block, BUT, B_NLA_MOD_DEL, ICON_X,				290,0,20,19, NULL, 0, 0, 0, 0, "Delete Modifier");
			
			if(amod->type==ACTSTRIP_MOD_DEFORM) {
				but= uiDefBut(block, TEX, B_NLA_PANEL, "Chan:",				10, -20, 130, 19, amod->channel, 1, 31, 0, 0, "Name of channel used for modifier");
				uiButSetCompleteFunc(but, autocomplete_bone, (void *)ob);
				uiDefButS(block, MENU, B_REDR, "All%x0|XY%x3|XZ%x2|YZ%x1",	140,-20,40,19, &amod->no_rot_axis, 0, 0, 0, 0, "Enable rotation axes (local for curve)");
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_NLA_MOD_DEPS, "Ob:",	180,-20, 130, 19, &amod->ob, "Curve Object"); 
			}
#if 0	/* this is not really ready for the primetime yet, but is here for testing */
			else if(amod->type==ACTSTRIP_MOD_NOISE) {
				but= uiDefBut(block, TEX, B_NLA_PANEL, "Chan:",				10, -20, 130, 19, amod->channel, 1, 31, 0, 0, "Name of channel used for modifier");
				uiButSetCompleteFunc(but, autocomplete_bone, (void *)ob);
				uiDefButBitS(block, TOG, 1, B_NLA_PANEL, "L", 140, -20, 20, 19, &amod->channels, 0, 24, 0, 0, "Apply noise to Location of channel");
				uiDefButBitS(block, TOG, 2, B_NLA_PANEL, "R", 160, -20, 20, 19, &amod->channels, 0, 24, 0, 0, "Apply noise to Rotation of channel");
				uiDefButBitS(block, TOG, 4, B_NLA_PANEL, "S", 180, -20, 20, 19, &amod->channels, 0, 24, 0, 0, "Apply noise to Scaling of channel");
				uiDefButF(block, NUM, B_NLA_PANEL, "NSize:",			200,-20,55,19, &amod->noisesize, 0.0001, 2.0, 10, 0, "Sets scaling for noise input");
				uiDefButF(block, NUM, B_NLA_PANEL, "Turb:",			255,-20,55,19, &amod->turbul, 0.0, 200.0, 10, 0, "Sets the depth of the noise");
			}
#endif
			else
				uiDefBut(block, LABEL, B_NOP, "Ack! Not implemented.",	10, -20, 150, 19, NULL, 0, 0, 0, 0, "");
				
		}
		else { /* for panel aligning */
			uiBlockEndAlign(block);
			uiDefBut(block, LABEL, B_NOP, " ",				10, -20, 150, 19, NULL, 0, 0, 0, 0, "");
		}
	}
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
	
	calc_scrollrcts(sa, G.v2d, curarea->winx, curarea->winy);
	
	/* clear all, becomes the color for left part */
	BIF_GetThemeColor3fv(TH_HEADER, col);
	glClearColor(col[0], col[1], col[2], 0.0); 
	glClear(GL_COLOR_BUFFER_BIT);
	
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

	/* the right hand side, with strips and keys */
	draw_nla_strips_keys(G.snla);

	
	glViewport(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1); 
	glScissor(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
	myortho2 (G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
	
	/* Draw current frame */
	draw_cfra_action();
	
	/* draw markers */
	draw_markers_timespace(SCE_MARKERS, 0);
	
	/* Draw preview 'curtains' */
	draw_anim_preview_timespace();

	/* Draw scroll */
	mywinset(curarea->win);	// reset scissor too
	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);
		if(G.v2d->scroll) drawscroll(0);
	}
	if(G.v2d->mask.xmin!=0) {
		/* Draw channel names */
		draw_nla_channels();
	}
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
	int y= 0;

	for (base=G.scene->base.first; base; base=base->next) {
		if (nla_filter(base)) {
			/* object level */
			y++;
			
			/* nla strips for object collapsed? */
			if ((base->object->nlaflag & OB_NLA_COLLAPSED)==0) {
				if(base->object->action)
					y++;
				
				/* Nla strips */
				y+= BLI_countlist(&base->object->nlastrips);
			}
		}
	}

	return y;
}

int nla_filter (Base *base)
{
	Object *ob = base->object;
	
	if ((G.snla->flag & SNLA_ALLKEYED) || (base->lay & G.scene->lay)) {
		if(ob->action || ob->nlastrips.first) 
			return 1;

		/* should become option */
		if (ob->ipo)
			return 1;

		if (ob->constraintChannels.first)
			return 1;
	}
	return 0;
}

