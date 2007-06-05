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
 * Drawing routines for the Action window type
 */

/* System includes ----------------------------------------------------- */

#include <math.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

/* Types --------------------------------------------------------------- */
#include "DNA_listBase.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"

#include "BKE_action.h"
#include "BKE_ipo.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

/* Everything from source (BIF, BDR, BSE) ------------------------------ */ 

#include "BIF_editaction.h"
#include "BIF_editkey.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"

#include "BDR_drawaction.h"
#include "BDR_editcurve.h"

#include "BSE_drawnla.h"
#include "BSE_drawipo.h"
#include "BSE_editipo.h"
#include "BSE_time.h"
#include "BSE_view.h"

/* 'old' stuff": defines and types, and own include -------------------- */

#include "blendef.h"
#include "mydevice.h"

/* sliders for shapekeys */
static void meshactionbuts(SpaceAction *saction, Object *ob, Key *key)
{
	int           i;
	char          str[64];
	float	        x, y;
	uiBlock       *block;
	uiBut 		  *but;

#define XIC 20
#define YIC 20

	/* lets make the rvk sliders */

	/* reset the damn myortho2 or the sliders won't draw/redraw
	 * correctly *grumble*
	 */
	mywinset(curarea->win);
	myortho2(-0.375, curarea->winx-0.375, G.v2d->cur.ymin, G.v2d->cur.ymax);

    sprintf(str, "actionbuttonswin %d", curarea->win);
    block= uiNewBlock (&curarea->uiblocks, str, 
                       UI_EMBOSS, UI_HELV, curarea->win);

	x = NAMEWIDTH + 1;
    y = CHANNELHEIGHT/2;

	/* make the little 'open the sliders' widget */
    BIF_ThemeColor(TH_FACE); // this slot was open...
	glRects(2,            y + 2*CHANNELHEIGHT - 2,  
			ACTWIDTH - 2, y + CHANNELHEIGHT + 2);
	glColor3ub(0, 0, 0);
	glRasterPos2f(4, y + CHANNELHEIGHT + 6);
	BMF_DrawString(G.font, "Sliders");

	uiBlockSetEmboss(block, UI_EMBOSSN);

	if (!(G.saction->flag & SACTION_SLIDERS)) {
		ACTWIDTH = NAMEWIDTH;
		but=uiDefIconButBitS(block, TOG, SACTION_SLIDERS, B_REDR, 
					  ICON_DISCLOSURE_TRI_RIGHT,
					  NAMEWIDTH - XIC - 5, y + CHANNELHEIGHT,
					  XIC,YIC-2,
					  &(G.saction->flag), 0, 0, 0, 0, 
					  "Show action window sliders");
		/* no hilite, the winmatrix is not correct later on... */
		uiButSetFlag(but, UI_NO_HILITE);

	}
	else {
		but= uiDefIconButBitS(block, TOG, SACTION_SLIDERS, B_REDR, 
					  ICON_DISCLOSURE_TRI_DOWN,
					  NAMEWIDTH - XIC - 5, y + CHANNELHEIGHT,
					  XIC,YIC-2,
					  &(G.saction->flag), 0, 0, 0, 0, 
					  "Hide action window sliders");
		/* no hilite, the winmatrix is not correct later on... */
		uiButSetFlag(but, UI_NO_HILITE);
					  
		ACTWIDTH = NAMEWIDTH + SLIDERWIDTH;

		/* sliders are open so draw them */
		BIF_ThemeColor(TH_FACE); 

		glRects(NAMEWIDTH,  0,  NAMEWIDTH+SLIDERWIDTH,  curarea->winy);
		uiBlockSetEmboss(block, UI_EMBOSS);
		for (i=1 ; i < key->totkey ; ++ i) {
			make_rvk_slider(block, ob, i, 
							x, y, SLIDERWIDTH-2, CHANNELHEIGHT-1, "Slider to control Shape Keys");

			y-=CHANNELHEIGHT+CHANNELSKIP;
			
			/* see sliderval array in editkey.c */
			if(i>=255) break;
		}
	}
	uiDrawBlock(block);

}

static void icu_slider_func(void *voidicu, void *voidignore) 
{
	/* the callback for the icu sliders ... copies the
	 * value from the icu->curval into a bezier at the
	 * right frame on the right ipo curve (creating both the
	 * ipo curve and the bezier if needed).
	 */
	IpoCurve  *icu= voidicu;
	BezTriple *bezt=NULL;
	float cfra, icuval;

	cfra = frame_to_float(CFRA);
	if (G.saction->pin==0 && OBACT)
		cfra= get_action_frame(OBACT, cfra);
	
	/* if the ipocurve exists, try to get a bezier
	 * for this frame
	 */
	bezt = get_bezt_icu_time(icu, &cfra, &icuval);

	/* create the bezier triple if one doesn't exist,
	 * otherwise modify it's value
	 */
	if (!bezt) {
		insert_vert_ipo(icu, cfra, icu->curval);
	}
	else {
		bezt->vec[1][1] = icu->curval;
	}

	/* make sure the Ipo's are properly process and
	 * redraw as necessary
	 */
	sort_time_ipocurve(icu);
	testhandles_ipocurve(icu);
	
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue (REDRAWNLA, 0);
	allqueue (REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWBUTSALL, 0);
}

static void make_icu_slider(uiBlock *block, IpoCurve *icu,
					 int x, int y, int w, int h, char *tip)
{
	/* create a slider for the ipo-curve*/
	uiBut *but;
	
	if(icu==NULL) return;
	
	if (IS_EQ(icu->slide_max, icu->slide_min)) {
		if (IS_EQ(icu->ymax, icu->ymin)) {
			if (icu->blocktype == ID_CO) {
				/* hack for constraints (and maybe a few others) */
				icu->slide_min= 0.0;
				icu->slide_max= 1.0;
			}
			else {
				icu->slide_min= -100;
				icu->slide_max= 100;
			}
		}
		else {
			icu->slide_min= icu->ymin;
			icu->slide_max= icu->ymax;
		}
	}
	if (icu->slide_min >= icu->slide_max) {
		SWAP(float, icu->slide_min, icu->slide_max);
	}

	but=uiDefButF(block, NUMSLI, REDRAWVIEW3D, "",
				  x, y , w, h,
				  &(icu->curval), icu->slide_min, icu->slide_max, 
				  10, 2, tip);
	
	uiButSetFunc(but, icu_slider_func, icu, NULL);
	
	// no hilite, the winmatrix is not correct later on...
	uiButSetFlag(but, UI_NO_HILITE);
}

/* sliders for ipo-curves of active action-channel */
static void action_icu_buts(SpaceAction *saction)
{
	bAction *act= saction->action;
	bActionChannel *achan;
	bConstraintChannel *conchan;
	IpoCurve *icu;
	char          str[64];
	float	        x, y;
	uiBlock       *block;

	/* lets make the action sliders */

	/* reset the damn myortho2 or the sliders won't draw/redraw
	 * correctly *grumble*
	 */
	mywinset(curarea->win);
	myortho2(-0.375, curarea->winx-0.375, G.v2d->cur.ymin, G.v2d->cur.ymax);
	
    sprintf(str, "actionbuttonswin %d", curarea->win);
    block= uiNewBlock (&curarea->uiblocks, str, 
                       UI_EMBOSS, UI_HELV, curarea->win);

	x = NAMEWIDTH + 1;
    y = 0.0;
	
	uiBlockSetEmboss(block, UI_EMBOSSN);

	if (G.saction->flag & SACTION_SLIDERS) {
		/* sliders are open so draw them */
		
		/* draw backdrop first */
		BIF_ThemeColor(TH_FACE); // change this color... it's ugly
		glRects(NAMEWIDTH,  G.v2d->cur.ymin,  NAMEWIDTH+SLIDERWIDTH,  G.v2d->cur.ymax);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
		for (achan=act->chanbase.first; achan; achan= achan->next) {
			if(VISIBLE_ACHAN(achan)) {
				y-=CHANNELHEIGHT+CHANNELSKIP;
				
				if (EXPANDED_ACHAN(achan)) {					
					if (achan->ipo) {
						y-=CHANNELHEIGHT+CHANNELSKIP;
						
						if (FILTER_IPO_ACHAN(achan)) {
							for (icu= achan->ipo->curve.first; icu; icu=icu->next) {
								if (achan->flag & ACHAN_HILIGHTED) {
									make_icu_slider(block, icu,
													x, y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
													"Slider to control current value of IPO-Curve");
								}
								
								y-=CHANNELHEIGHT+CHANNELSKIP;
							}
						}
					}
					
					if (achan->constraintChannels.first) {
						y-=CHANNELHEIGHT+CHANNELSKIP;
						
						if (FILTER_CON_ACHAN(achan)) {
							for (conchan= achan->constraintChannels.first; conchan; conchan=conchan->next) {
								if ((achan->flag & ACHAN_HILIGHTED) && EDITABLE_CONCHAN(conchan)) {
									icu= (IpoCurve *)conchan->ipo->curve.first;
									make_icu_slider(block, icu,
													x, y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
													"Slider to control current value of Constraint Channel");
								}
								
								y-=CHANNELHEIGHT+CHANNELSKIP;
							}
						}
					}
				}
			}
		}
	}
	uiDrawBlock(block);
}

void draw_cfra_action(void)
{
	Object *ob;
	float vec[2];
	
	vec[0]= (G.scene->r.cfra);
	vec[0]*= G.scene->r.framelen;
	
	vec[1]= G.v2d->cur.ymin;
	glColor3ub(0x60, 0xc0, 0x40);
	glLineWidth(2.0);
	
	glBegin(GL_LINE_STRIP);
	glVertex2fv(vec);
	vec[1]= G.v2d->cur.ymax;
	glVertex2fv(vec);
	glEnd();
	
	ob= (G.scene->basact) ? (G.scene->basact->object) : 0;
	if(ob && ob->sf!=0.0 && (ob->ipoflag & OB_OFFS_OB) ) {
		vec[0]-= ob->sf;
		
		glColor3ub(0x10, 0x60, 0);
		
		glBegin(GL_LINE_STRIP);
		glVertex2fv(vec);
		vec[1]= G.v2d->cur.ymin;
		glVertex2fv(vec);
		glEnd();
	}
	
	glLineWidth(1.0);
}

/* left hand */
static void draw_action_channel_names(bAction *act) 
{
    bActionChannel *achan;
    bConstraintChannel *conchan;
	IpoCurve *icu;
    float	x, y;

    x = 0.0;
	y = 0.0f;

	for (achan=act->chanbase.first; achan; achan= achan->next) {
		if(VISIBLE_ACHAN(achan)) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) ;
			
			/* draw backing strip behind action channel name */
			BIF_ThemeColorShade(TH_HEADER, 20);
			glRectf(x,  y-CHANNELHEIGHT/2,  (float)NAMEWIDTH,  y+CHANNELHEIGHT/2);
			
			/* draw expand/collapse triangle for action-channel */
			if (EXPANDED_ACHAN(achan))
				BIF_icon_draw(x+1, y-CHANNELHEIGHT/2, ICON_TRIA_DOWN);
			else
				BIF_icon_draw(x+1, y-CHANNELHEIGHT/2, ICON_TRIA_RIGHT);
			
			/* draw name of action channel */
			if (SEL_ACHAN(achan))
				BIF_ThemeColor(TH_TEXT_HI);
			else
				BIF_ThemeColor(TH_TEXT);
			glRasterPos2f(x+18,  y-4);
			BMF_DrawString(G.font, achan->name);
			
			/* draw 'lock' indicating whether channel is protected */
			if (EDITABLE_ACHAN(achan)==0) 
				BIF_icon_draw(NAMEWIDTH-16, y-CHANNELHEIGHT/2, ICON_LOCKED);
			else 
				BIF_icon_draw(NAMEWIDTH-16, y-CHANNELHEIGHT/2, ICON_UNLOCKED);
			y-=CHANNELHEIGHT+CHANNELSKIP;
			
			if (EXPANDED_ACHAN(achan)) {
				/* Draw IPO-curves show/hide widget */
				if (achan->ipo) {					
					/* draw backing strip behind */
					BIF_ThemeColorShade(TH_HEADER, -20);
					glRectf(x+7,  y-CHANNELHEIGHT/2,  (float)NAMEWIDTH,  y+CHANNELHEIGHT/2);
					
					/* draw expand/collapse triangle for showing sub-channels  */
					if (FILTER_IPO_ACHAN(achan))
						BIF_icon_draw(x+8, y-CHANNELHEIGHT/2, ICON_TRIA_DOWN);
					else
						BIF_icon_draw(x+8, y-CHANNELHEIGHT/2, ICON_TRIA_RIGHT);
					
					/* draw icon showing type of ipo-block */
					BIF_icon_draw(x+24, y-CHANNELHEIGHT/2, geticon_ipo_blocktype(achan->ipo->blocktype));
					
					/* draw name of ipo-block */
					if (SEL_ACHAN(achan))
						BIF_ThemeColor(TH_TEXT_HI);
					else
						BIF_ThemeColor(TH_TEXT);
					glRasterPos2f(x+40,  y-4);
					BMF_DrawString(G.font, "IPO Curves"); // TODO: make proper naming scheme
				
					y-=CHANNELHEIGHT+CHANNELSKIP;
				
					/* Draw IPO-curve-channels? */
					if (FILTER_IPO_ACHAN(achan)) {
						for (icu=achan->ipo->curve.first; icu; icu=icu->next) {
							char *icu_name= getname_ipocurve(icu);
							
							/* draw backing strip behind ipo-curve channel*/
							BIF_ThemeColorShade(TH_HEADER, -40);
							glRectf(x+14,  y-CHANNELHEIGHT/2,  (float)NAMEWIDTH,  y+CHANNELHEIGHT/2);
							
							/* draw name of ipo-curve channel */
							if (SEL_ICU(icu))
								BIF_ThemeColor(TH_TEXT_HI);
							else
								BIF_ThemeColor(TH_TEXT);
							glRasterPos2f(x+24,  y-4);
							BMF_DrawString(G.font, icu_name);
							
#if 0 /* tempolarily disabled until all ipo-code can support this option */
							/* draw 'lock' to indicate if ipo-curve channel is protected */
							if (EDITABLE_ICU(icu)==0) 
								BIF_icon_draw(NAMEWIDTH-16, y-CHANNELHEIGHT/2, ICON_LOCKED);
							else 
								BIF_icon_draw(NAMEWIDTH-16, y-CHANNELHEIGHT/2, ICON_UNLOCKED);	
#endif
							y-=CHANNELHEIGHT+CHANNELSKIP;
						}
					}
				}

				/* Draw constraints show/hide widget */
				if (achan->constraintChannels.first) {
					/* draw backing strip behind */
					BIF_ThemeColorShade(TH_HEADER, -20);
					glRectf(x+7,  y-CHANNELHEIGHT/2,  (float)NAMEWIDTH,  y+CHANNELHEIGHT/2);
					
					/* draw expand/collapse triangle for showing sub-channels  */
					if (FILTER_CON_ACHAN(achan))
						BIF_icon_draw(x+8, y-CHANNELHEIGHT/2, ICON_TRIA_DOWN);
					else
						BIF_icon_draw(x+8, y-CHANNELHEIGHT/2, ICON_TRIA_RIGHT);
					
					/* draw constraint icon */
					BIF_icon_draw(x+24, y-CHANNELHEIGHT/2, ICON_CONSTRAINT);
					
					/* draw name of widget */
					if (SEL_ACHAN(achan))
						BIF_ThemeColor(TH_TEXT_HI);
					else
						BIF_ThemeColor(TH_TEXT);
					glRasterPos2f(x+40,  y-4);
					BMF_DrawString(G.font, "Constraints");
				
					y-=CHANNELHEIGHT+CHANNELSKIP;
				
					/* Draw constraint channels?  */
					if (FILTER_CON_ACHAN(achan)) {
						for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
							/* draw backing strip behind constraint channel*/
							BIF_ThemeColorShade(TH_HEADER, -40);
							glRectf(x+14,  y-CHANNELHEIGHT/2,  (float)NAMEWIDTH,  y+CHANNELHEIGHT/2);
							
							/* draw name of constraint channel */
							if (SEL_CONCHAN(conchan))
								BIF_ThemeColor(TH_TEXT_HI);
							else
								BIF_ThemeColor(TH_TEXT);
							glRasterPos2f(x+25,  y-4);
							BMF_DrawString(G.font, conchan->name);
							
							/* draw 'lock' to indicate if constraint channel is protected */
							if (EDITABLE_CONCHAN(conchan)==0) 
								BIF_icon_draw(NAMEWIDTH-16, y-CHANNELHEIGHT/2, ICON_LOCKED);
							else 
								BIF_icon_draw(NAMEWIDTH-16, y-CHANNELHEIGHT/2, ICON_UNLOCKED);	
							y-=CHANNELHEIGHT+CHANNELSKIP;
						}
					}
				}
			}
			
			glDisable(GL_BLEND);
		}
	}
}


static void draw_action_mesh_names(Key *key) 
{
	/* draws the names of the rvk keys in the
	 * left side of the action window
	 */
	int	     i;
	char     keyname[32];
	float    x, y;
	KeyBlock *kb;

	x = 0.0;
	y= 0.0;

	kb= key->block.first;

	for (i=1 ; i < key->totkey ; ++ i) {
		glColor3ub(0xAA, 0xAA, 0xAA);
		glRectf(x,	y-CHANNELHEIGHT/2,	(float)NAMEWIDTH,  y+CHANNELHEIGHT/2);

		glColor3ub(0, 0, 0);

		glRasterPos2f(x+8,	y-4);
		kb = kb->next;
		/* Blender now has support for named
		 * key blocks. If a name hasn't
		 * been set for an key block then
		 * just display the key number -- 
		 * otherwise display the name stored
		 * in the keyblock.
		 */
		if (kb->name[0] == '\0') {
		  sprintf(keyname, "Key %d", i);
		  BMF_DrawString(G.font, keyname);
		}
		else {
		  BMF_DrawString(G.font, kb->name);
		}

		y-=CHANNELHEIGHT+CHANNELSKIP;

	}
}

/* left hand part */
static void draw_channel_names(void) 
{
	short ofsx, ofsy = 0; 
	bAction	*act;
	Key *key;

	/* Clip to the scrollable area */
	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {	
			ofsx= curarea->winrct.xmin;	
			ofsy= curarea->winrct.ymin;
			glViewport(ofsx,  ofsy+G.v2d->mask.ymin, NAMEWIDTH, 
					   (ofsy+G.v2d->mask.ymax) -
					   (ofsy+G.v2d->mask.ymin)); 
			glScissor(ofsx,	 ofsy+G.v2d->mask.ymin, NAMEWIDTH, 
					  (ofsy+G.v2d->mask.ymax) -
					  (ofsy+G.v2d->mask.ymin));
		}
	}
	
	myortho2(0,	NAMEWIDTH, G.v2d->cur.ymin, G.v2d->cur.ymax);	//	Scaling
	
	glColor3ub(0x00, 0x00, 0x00);

	act=G.saction->action;

	if (act) {
		/* if there is a selected action then
		 * draw the channel names
		 */
		draw_action_channel_names(act);
	}
	else if ( (key = get_action_mesh_key()) ) {
		/* if there is a mesh selected with rvk's,
		 * then draw the RVK names
		 */
		draw_action_mesh_names(key);
    }

    myortho2(0,	NAMEWIDTH, 0, (ofsy+G.v2d->mask.ymax) -
              (ofsy+G.v2d->mask.ymin));	//	Scaling

}

int count_action_levels(bAction *act)
{
	bActionChannel *achan;
	int y=0;

	if (!act) 
		return 0;

	for (achan=act->chanbase.first; achan; achan=achan->next) {
		if(VISIBLE_ACHAN(achan)) {
			y++;
			
			if (EXPANDED_ACHAN(achan)) {
				if (achan->constraintChannels.first) {
					y++;
					if (FILTER_CON_ACHAN(achan))
						y += BLI_countlist(&achan->constraintChannels);
				}
				else if (achan->ipo) {
					y++;
					if (FILTER_IPO_ACHAN(achan))
						y += BLI_countlist(&achan->ipo->curve);
				}
			}
		}
	}

	return y;
}

/* sets or clears hidden flags */
void check_action_context(SpaceAction *saction)
{
	bActionChannel *achan;
	
	if(saction->action==NULL) return;
	
	for (achan=saction->action->chanbase.first; achan; achan=achan->next)
		achan->flag &= ~ACHAN_HIDDEN;
	
	if (G.saction->pin==0 && OBACT) {
		Object *ob= OBACT;
		bPoseChannel *pchan;
		bArmature *arm= ob->data;
		
		for (achan=saction->action->chanbase.first; achan; achan=achan->next) {
			pchan= get_pose_channel(ob->pose, achan->name);
			if (pchan) {
				if ((pchan->bone->layer & arm->layer)==0)
					achan->flag |= ACHAN_HIDDEN;
				else if (pchan->bone->flag & BONE_HIDDEN_P)
					achan->flag |= ACHAN_HIDDEN;
			}
		}
	}
}

static void draw_channel_strips(SpaceAction *saction)
{
	rcti scr_rct;
	gla2DDrawInfo *di;
	bAction	*act;
	bActionChannel *achan;
	bConstraintChannel *conchan;
	IpoCurve *icu;
	float y, sta, end;
	int act_start, act_end, dummy;
	char col1[3], col2[3];
	
	BIF_GetThemeColor3ubv(TH_SHADE2, col2);
	BIF_GetThemeColor3ubv(TH_HILITE, col1);

	act= saction->action;
	if (!act)
		return;

	scr_rct.xmin= saction->area->winrct.xmin + saction->v2d.mask.xmin;
	scr_rct.ymin= saction->area->winrct.ymin + saction->v2d.mask.ymin;
	scr_rct.xmax= saction->area->winrct.xmin + saction->v2d.hor.xmax;
	scr_rct.ymax= saction->area->winrct.ymin + saction->v2d.mask.ymax; 
	di= glaBegin2DDraw(&scr_rct, &G.v2d->cur);

	/* if in NLA there's a strip active, map the view */
	if (G.saction->pin==0 && OBACT)
		map_active_strip(di, OBACT, 0);
	
	/* start and end of action itself */
	calc_action_range(act, &sta, &end, 0);
	gla2DDrawTranslatePt(di, sta, 0.0f, &act_start, &dummy);
	gla2DDrawTranslatePt(di, end, 0.0f, &act_end, &dummy);
	
	if (G.saction->pin==0 && OBACT)
		map_active_strip(di, OBACT, 1);
	
	/* first backdrop strips */
	y = 0.0;
	glEnable(GL_BLEND);
	for (achan=act->chanbase.first; achan; achan= achan->next) {
		if(VISIBLE_ACHAN(achan)) {
			int frame1_x, channel_y;
			
			gla2DDrawTranslatePt(di, G.v2d->cur.xmin, y, &frame1_x, &channel_y);
			
			if (SEL_ACHAN(achan)) glColor4ub(col1[0], col1[1], col1[2], 0x22);
			else glColor4ub(col2[0], col2[1], col2[2], 0x22);
			glRectf(frame1_x,  channel_y-CHANNELHEIGHT/2,  G.v2d->hor.xmax,  channel_y+CHANNELHEIGHT/2);
			
			if (SEL_ACHAN(achan)) glColor4ub(col1[0], col1[1], col1[2], 0x22);
			else glColor4ub(col2[0], col2[1], col2[2], 0x22);
			glRectf(act_start,  channel_y-CHANNELHEIGHT/2,  act_end,  channel_y+CHANNELHEIGHT/2);
			
			/*	Increment the step */
			y-=CHANNELHEIGHT+CHANNELSKIP;
			
			/* Draw sub channels */
			if (EXPANDED_ACHAN(achan)) {
				/* Draw ipo channels */
				if (achan->ipo) {
					y-=CHANNELHEIGHT+CHANNELSKIP;
					
					if (FILTER_IPO_ACHAN(achan)) {
						for (icu=achan->ipo->curve.first; icu; icu=icu->next) {
							gla2DDrawTranslatePt(di, 1, y, &frame1_x, &channel_y);
							
							if (SEL_ICU(icu)) glColor4ub(col1[0], col1[1], col1[2], 0x22);
							else glColor4ub(col2[0], col2[1], col2[2], 0x22);
							glRectf(frame1_x,  channel_y-CHANNELHEIGHT/2+4,  G.v2d->hor.xmax,  channel_y+CHANNELHEIGHT/2-4);
							
							if (SEL_ICU(icu)) glColor4ub(col1[0], col1[1], col1[2], 0x22);
							else glColor4ub(col2[0], col2[1], col2[2], 0x22);
							glRectf(act_start,  channel_y-CHANNELHEIGHT/2+4,  act_end,  channel_y+CHANNELHEIGHT/2-4);
							
							y-=CHANNELHEIGHT+CHANNELSKIP;
						}
					}
				}
				
				/* Draw constraint channels */
				if (achan->constraintChannels.first) {
					y-=CHANNELHEIGHT+CHANNELSKIP;
					
					if (FILTER_CON_ACHAN(achan)) {
						for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
							gla2DDrawTranslatePt(di, 1, y, &frame1_x, &channel_y);
							
							if (SEL_CONCHAN(conchan)) glColor4ub(col1[0], col1[1], col1[2], 0x22);
							else glColor4ub(col2[0], col2[1], col2[2], 0x22);
							glRectf(frame1_x,  channel_y-CHANNELHEIGHT/2+4,  G.v2d->hor.xmax,  channel_y+CHANNELHEIGHT/2-4);
							
							if (SEL_CONCHAN(conchan)) glColor4ub(col1[0], col1[1], col1[2], 0x22);
							else glColor4ub(col2[0], col2[1], col2[2], 0x22);
							glRectf(act_start,  channel_y-CHANNELHEIGHT/2+4,  act_end,  channel_y+CHANNELHEIGHT/2-4);
							
							y-=CHANNELHEIGHT+CHANNELSKIP;
						}
					}
				}
			}
		}
	}		
	glDisable(GL_BLEND);
	
	if (G.saction->pin==0 && OBACT)
		map_active_strip(di, OBACT, 0);
	
	/* keyframes  */
	y = 0.0;
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		if(VISIBLE_ACHAN(achan)) {
			
			draw_ipo_channel(di, achan->ipo, y);
			y-=CHANNELHEIGHT+CHANNELSKIP;

			/* Draw sub channels */
			if (EXPANDED_ACHAN(achan)) {
				/* Draw ipo curves */
				if (achan->ipo) {
					y-=CHANNELHEIGHT+CHANNELSKIP;
					
					if (FILTER_IPO_ACHAN(achan)) {
						for (icu=achan->ipo->curve.first; icu; icu=icu->next) {
							draw_icu_channel(di, icu, y);
							y-=CHANNELHEIGHT+CHANNELSKIP;
						}
					}
				}
				
				/* Draw constraint channels */
				if (achan->constraintChannels.first) {
					y-=CHANNELHEIGHT+CHANNELSKIP;
					
					if (FILTER_CON_ACHAN(achan)) {
						for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
							draw_ipo_channel(di, conchan->ipo, y);
							y-=CHANNELHEIGHT+CHANNELSKIP;
						}
					}
				}
			}
		}
	}

	if(saction->flag & SACTION_MOVING) {
		int frame1_x, channel_y;
		gla2DDrawTranslatePt(di, saction->timeslide, 0, &frame1_x, &channel_y);
		cpack(0x0);
		glBegin(GL_LINES);
		glVertex2f(frame1_x, G.v2d->mask.ymin - 100);
		glVertex2f(frame1_x, G.v2d->mask.ymax);
		glEnd();
	}
	
	glaEnd2DDraw(di);
}

static void draw_mesh_strips(SpaceAction *saction, Key *key)
{
	/* draw the RVK keyframes */
	rcti scr_rct;
	gla2DDrawInfo *di;
	float	y, ybase;
	IpoCurve *icu;
	char col1[3], col2[3];
	
	BIF_GetThemeColor3ubv(TH_SHADE2, col2);
	BIF_GetThemeColor3ubv(TH_HILITE, col1);

	if (!key->ipo) return;

	scr_rct.xmin= saction->area->winrct.xmin + ACTWIDTH;
	scr_rct.ymin= saction->area->winrct.ymin + saction->v2d.mask.ymin;
	scr_rct.xmax= saction->area->winrct.xmin + saction->v2d.hor.xmax;
	scr_rct.ymax= saction->area->winrct.ymin + saction->v2d.mask.ymax; 
	di= glaBegin2DDraw(&scr_rct, &G.v2d->cur);

	ybase = 0;

	for (icu = key->ipo->curve.first; icu ; icu = icu->next) {
		int frame1_x, channel_y;
		
		/* lets not deal with the "speed" Ipo */
		if (icu->adrcode==0) continue;
		
		y = ybase	- (CHANNELHEIGHT+CHANNELSKIP)*(icu->adrcode-1);
		gla2DDrawTranslatePt(di, 1, y, &frame1_x, &channel_y);
			
		/* all frames that have a frame number less than one
		 * get a desaturated orange background
		 */
		glEnable(GL_BLEND);
		glColor4ub(col2[0], col2[1], col2[2], 0x22);
		glRectf(0,        channel_y-CHANNELHEIGHT/2,  
				frame1_x, channel_y+CHANNELHEIGHT/2);

		/* frames one and higher get a saturated orange background */
		glColor4ub(col2[0], col2[1], col2[2], 0x44);
		glRectf(frame1_x,         channel_y-CHANNELHEIGHT/2,  
				G.v2d->hor.xmax,  channel_y+CHANNELHEIGHT/2);
		glDisable(GL_BLEND);

		/* draw the keyframes */
		draw_icu_channel(di, icu, y); 
	}

	glaEnd2DDraw(di);
}

/* ********* action panel *********** */


void do_actionbuts(unsigned short event)
{
	switch(event) {
	case REDRAWVIEW3D:
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_REDR:
		allqueue(REDRAWACTION, 0);
		break;
	}
}


static void action_panel_properties(short cntrl)	// ACTION_HANDLER_PROPERTIES
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "action_panel_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(ACTION_HANDLER_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "Transform Properties", "Action", 10, 230, 318, 204)==0) return;

	uiDefBut(block, LABEL, 0, "test text",		10,180,300,19, 0, 0, 0, 0, 0, "");

}

static void action_blockhandlers(ScrArea *sa)
{
	SpaceAction *sact= sa->spacedata.first;
	short a;
	
	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		switch(sact->blockhandler[a]) {

		case ACTION_HANDLER_PROPERTIES:
			action_panel_properties(sact->blockhandler[a+1]);
			break;
		
		}
		/* clear action value for event */
		sact->blockhandler[a+1]= 0;
	}
	uiDrawBlocksPanels(sa, 0);
}

void drawactionspace(ScrArea *sa, void *spacedata)
{
	short ofsx = 0, ofsy = 0;
	bAction *act;
	Key *key;
	float col[3];
	short maxymin;

	if (!G.saction)
		return;

	/* warning; blocks need to be freed each time, handlers dont remove  */
	uiFreeBlocksWin(&sa->uiblocks, sa->win);

	if (!G.saction->pin) {
		/* allow more than one active action sometime? */
		if (OBACT)
			G.saction->action = OBACT->action;
		else
			G.saction->action=NULL;
	}
	key = get_action_mesh_key();
	act= G.saction->action;

	/* Damn I hate hunting to find my rvk's because
	 * they have scrolled off of the screen ... this
	 * oughta fix it
	 */
	
	if (!act && key) {
		if (G.v2d->cur.ymin < -CHANNELHEIGHT) 
			G.v2d->cur.ymin = -CHANNELHEIGHT;
		
		maxymin = -(key->totkey*(CHANNELHEIGHT+CHANNELSKIP));
		if (G.v2d->cur.ymin > maxymin) G.v2d->cur.ymin = maxymin;
	}

	/* Lets make sure the width of the left hand of the screen
	 * is set to an appropriate value based on whether sliders
	 * are showing of not
	 */
	if (((key)||(act)) && (G.saction->flag & SACTION_SLIDERS)) 
		ACTWIDTH = NAMEWIDTH + SLIDERWIDTH;
	else 
		ACTWIDTH = NAMEWIDTH;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) ;

	calc_scrollrcts(sa, G.v2d, curarea->winx, curarea->winy);

	/* background color for entire window (used in lefthand part tho) */
	BIF_GetThemeColor3fv(TH_HEADER, col);
	glClearColor(col[0], col[1], col[2], 0.0); 
	glClear(GL_COLOR_BUFFER_BIT);
	
	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {	
			ofsx= curarea->winrct.xmin;	
			ofsy= curarea->winrct.ymin;
			glViewport(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, 
					   ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, 
					   ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1); 
			glScissor(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, 
					  ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, 
					  ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
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

	check_action_context(G.saction);
	
	/* Draw channel strips */
	if (act) {
		draw_channel_strips(G.saction);
	}
	else if (key) {
		/* if there is a mesh with rvk's selected,
		 * then draw the key frames in the action window
		 */
		draw_mesh_strips(G.saction, key);
	}
	
	/* reset matrices for stuff to be drawn on top of keys*/
	glViewport(ofsx+G.v2d->mask.xmin,  
             ofsy+G.v2d->mask.ymin, 
             ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, 
             ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1); 
	glScissor(ofsx+G.v2d->mask.xmin,  
            ofsy+G.v2d->mask.ymin, 
            ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, 
            ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax,  G.v2d->cur.ymin, G.v2d->cur.ymax);
	
	/* Draw current frame */
	draw_cfra_action();
	
	/* Draw markers */
	draw_markers_timespace();
	
	/* Draw 'curtains' for preview */
	draw_anim_preview_timespace();

	/* Draw scroll */
	mywinset(curarea->win);	// reset scissor too
	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
      myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);
      if(G.v2d->scroll) drawscroll(0);
	}

	if(G.v2d->mask.xmin!=0) {
		/* Draw channel names */
		draw_channel_names();

		if(sa->winx > 50 + NAMEWIDTH + SLIDERWIDTH) {
			if (act) {
				/* if there is an action, draw sliders for its
				 * ipo-curve channels in the action window
				 */
				action_icu_buts(G.saction);
			}
			else if (key) {
				/* if there is a mesh with rvk's selected,
				 * then draw the key frames in the action window
				 */
				meshactionbuts(G.saction, OBACT, key);
			}
		}
	}
	
	mywinset(curarea->win);	// reset scissor too
	myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);
	draw_area_emboss(sa);

	/* it is important to end a view in a transform compatible with buttons */
	bwin_scalematrix(sa->win, G.saction->blockscale, G.saction->blockscale, G.saction->blockscale);
	action_blockhandlers(sa);

	curarea->win_swap= WIN_BACK_OK;
}

static void draw_keylist(gla2DDrawInfo *di, ListBase *keys, ListBase *blocks, float ypos, int totcurve)
{
	CfraElem *ce;
	ActKeyBlock *ab;
	
	glEnable(GL_BLEND);
	
	/* draw keyblocks */
	if (blocks) {
		for (ab= blocks->first; ab; ab= ab->next) {
			/* only draw keyblock if it appears in all curves sampled */
			if (ab->totcurve == totcurve) {
				int sc_xa, sc_ya;
				int sc_xb, sc_yb;
				
				/* get co-ordinates of block */
				gla2DDrawTranslatePt(di, ab->start, ypos, &sc_xa, &sc_ya);
				gla2DDrawTranslatePt(di, ab->end, ypos, &sc_xb, &sc_yb);
				
				/* draw block */
				if (ab->sel & 1)
					BIF_ThemeColor4(TH_STRIP_SELECT);
				else
					BIF_ThemeColor4(TH_STRIP);
				glRectf(sc_xa,  sc_ya-3,  sc_xb,  sc_yb+5);
			}
		}
	}
	
	/* draw keys */
	if (keys) {
		for (ce= keys->first; ce; ce= ce->next) {
			int sc_x, sc_y;
			
			/* get co-ordinate to draw at */
			gla2DDrawTranslatePt(di, ce->cfra, ypos, &sc_x, &sc_y);
			
			if(ce->sel & 1) BIF_icon_draw_aspect(sc_x-7, sc_y-6, ICON_SPACE2, 1.0f);
			else BIF_icon_draw_aspect(sc_x-7, sc_y-6, ICON_SPACE3, 1.0f);
		}	
	}
	
	glDisable(GL_BLEND);
}

void draw_object_channel(gla2DDrawInfo *di, Object *ob, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};
	int totcurve;

	totcurve= ob_to_keylist(ob, &keys, &blocks);
	draw_keylist(di, &keys, &blocks, ypos, totcurve);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_ipo_channel(gla2DDrawInfo *di, Ipo *ipo, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};
	int totcurve;

	totcurve= ipo_to_keylist(ipo, &keys, &blocks);
	draw_keylist(di, &keys, &blocks, ypos, totcurve);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_icu_channel(gla2DDrawInfo *di, IpoCurve *icu, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	icu_to_keylist(icu, &keys, &blocks);
	draw_keylist(di, &keys, &blocks, ypos, 1);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_action_channel(gla2DDrawInfo *di, bAction *act, float ypos)
{
	ListBase keys = {0, 0};

	action_to_keylist(act, &keys, NULL);
	draw_keylist(di, &keys, NULL, ypos, 0);
	BLI_freelistN(&keys);
}

static void add_bezt_to_keyblockslist(ListBase *blocks, IpoCurve *icu, int index)
{
	/* The equivilant of add_to_cfra_elem except this version 
	 * makes ActKeyBlocks - one of the two datatypes required
	 * for action editor drawing.
	 */
	ActKeyBlock *ab, *abn;
	BezTriple *beztn=NULL, *prev=NULL;
	BezTriple *bezt;
	int v;
	
	/* get beztriples */
	beztn= (icu->bezt + index);
	/* The following search for previous beztriple doesn't work
	 * that great on actions with a large amount of keys. There
	 * are a few commented out shortcuts for these cases, which will
	 * remain so until the definitive point where slowdown starts to
	 * bite is determined.
	 */
	//if (icu->totvert > 3500) {
	//	if (index >= 1) 
	//		prev= (icu->bezt + (index - 1));
	//}
	//else {
		for (v=0, bezt=icu->bezt; v<icu->totvert; v++, bezt++) {
			/* skip if beztriple is current */
			if (v != index) {
				/* check if beztriple is immediately before */
				if (beztn->vec[1][0] > bezt->vec[1][0]) {
					/* check if closer than previous was */
					if (prev) {
						if (prev->vec[1][0] < bezt->vec[1][0])
							prev= bezt;
					}
					else {
						prev= bezt;
					}
				}
			}
		}
	//}
	
	/* check if block needed - same value? */
	if ((!prev) || (!beztn))
		return;
	if (beztn->vec[1][1] != prev->vec[1][1])
		return;
	
	/* try to find a keyblock that starts on the previous beztriple */
	for (ab= blocks->first; ab; ab= ab->next) {
		/* check if alter existing block or add new block */
		if (ab->start == prev->vec[1][0]) {			
			/* set selection status and 'touched' status */
			if (BEZSELECTED(beztn)) ab->sel = SELECT;
			ab->modified += 1;
			
			return;
		}
		else if (ab->start > prev->vec[1][0]) break;
	}
	
	/* add new block */
	abn= MEM_callocN(sizeof(ActKeyBlock), "add_bezt_to_keyblockslist");
	if (ab) BLI_insertlinkbefore(blocks, ab, abn);
	else BLI_addtail(blocks, abn);
	
	abn->start= prev->vec[1][0];
	abn->end= beztn->vec[1][0];
	abn->val= beztn->vec[1][1];
	
	if (BEZSELECTED(prev) || BEZSELECTED(beztn))
		abn->sel = SELECT;
	else
		abn->sel = 0;
	abn->modified += 1;
}

int ob_to_keylist(Object *ob, ListBase *keys, ListBase *blocks)
{
	bConstraintChannel *conchan;
	int totcurve = 0;

	if (ob) {
		/* Add object keyframes */
		if (ob->ipo) {
			totcurve += ipo_to_keylist(ob->ipo, keys, blocks);
		}
		
		/* Add constraint keyframes */
		for (conchan=ob->constraintChannels.first; conchan; conchan=conchan->next){
			if(conchan->ipo) {
				totcurve += ipo_to_keylist(conchan->ipo, keys, blocks);
			}				
		}
			
		/* Add object data keyframes */
		// 		TODO??
	}
	
	return totcurve;
}

void icu_to_keylist(IpoCurve *icu, ListBase *keys, ListBase *blocks)
{
	BezTriple *bezt;
	ActKeyBlock *ab, *abn;
	int v;
	
	if (icu && icu->totvert) {
		/* loop through beztriples, making ActKeys and ActKeyBlocks */
		bezt= icu->bezt;
		
		for (v=0; v<icu->totvert; v++, bezt++) {
			add_to_cfra_elem(keys, bezt);
			if (blocks) add_bezt_to_keyblockslist(blocks, icu, v);
		}
		
		/* update the number of curves the blocks have appeared in */
		if (blocks) {
			for (ab= blocks->first; ab; ab= abn) {
				abn= ab->next;
				
				if (ab->modified) {
					ab->modified = 0;
					ab->totcurve += 1;
				}
			}
		}
	}
}

int ipo_to_keylist(Ipo *ipo, ListBase *keys, ListBase *blocks)
{
	IpoCurve *icu;
	int totcurve = 0;
	
	if (ipo) {
		for (icu= ipo->curve.first; icu; icu= icu->next) {
			icu_to_keylist(icu, keys, blocks);
			totcurve++;
		}
	}
	
	return totcurve;
}

int action_to_keylist(bAction *act, ListBase *keys, ListBase *blocks)
{
	bActionChannel *achan;
	bConstraintChannel *conchan;
	int totcurve = 0;

	if (act) {
		/* loop through action channels */
		for (achan= act->chanbase.first; achan; achan= achan->next) {
			/* firstly, add keys from action channel's ipo block */
			if (achan->ipo) {
				totcurve+= ipo_to_keylist(achan->ipo, keys, blocks);
			}
			
			/* then, add keys from constraint channels */
			for (conchan= achan->constraintChannels.first; conchan; conchan= conchan->next) {
				if (conchan->ipo) {
					totcurve+= ipo_to_keylist(achan->ipo, keys, blocks);
				}
			}
		}
	}
	
	return totcurve;
}

