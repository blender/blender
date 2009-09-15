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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* System includes ----------------------------------------------------- */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

/* Types --------------------------------------------------------------- */

#include "DNA_listBase.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_userdef_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_global.h" 	// XXX remove me!
#include "BKE_context.h"
#include "BKE_utildefines.h"

/* Everything from source (BIF, BDR, BSE) ------------------------------ */ 

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_draw.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#if 0 // XXX old includes for reference only
	#include "BIF_editaction.h"
	#include "BIF_editkey.h"
	#include "BIF_editnla.h"
	#include "BIF_drawgpencil.h"
	#include "BIF_keyframing.h"
	#include "BIF_language.h"
	#include "BIF_space.h"
	
	#include "BDR_editcurve.h"
	#include "BDR_gpencil.h"

	#include "BSE_drawnla.h"
	#include "BSE_drawipo.h"
	#include "BSE_drawview.h"
	#include "BSE_editaction_types.h"
	#include "BSE_editipo.h"
	#include "BSE_headerbuttons.h"
	#include "BSE_time.h"
	#include "BSE_view.h"
#endif // XXX old defines for reference only

/* XXX */
extern void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad);

/********************************** Slider Stuff **************************** */

#if 0 // XXX all of this slider stuff will need a rethink!
/* sliders for shapekeys */
static void meshactionbuts(SpaceAction *saction, Object *ob, Key *key)
{
	int           i;
	char          str[64];
	float	      x, y;
	uiBlock       *block;
	uiBut 		  *but;
	
	/* lets make the shapekey sliders */
	
	/* reset the damn myortho2 or the sliders won't draw/redraw
	 * correctly *grumble*
	 */
	mywinset(curarea->win);
	myortho2(-0.375f, curarea->winx-0.375f, G.v2d->cur.ymin, G.v2d->cur.ymax);
	
    sprintf(str, "actionbuttonswin %d", curarea->win);
    block= uiNewBlock (&curarea->uiblocks, str, UI_EMBOSS);

	x = ACHANNEL_NAMEWIDTH + 1;
    y = 0.0f;
	
	uiBlockSetEmboss(block, UI_EMBOSSN);

	if (!(G.saction->flag & SACTION_SLIDERS)) {
		ACTWIDTH = ACHANNEL_NAMEWIDTH;
		but=uiDefIconButBitS(block, TOG, SACTION_SLIDERS, B_REDR, 
					  ICON_DISCLOSURE_TRI_RIGHT,
					  ACHANNEL_NAMEWIDTH - XIC - 5, (short)y + CHANNELHEIGHT,
					  XIC,YIC-2,
					  &(G.saction->flag), 0, 0, 0, 0, 
					  "Show action window sliders");
		/* no hilite, the winmatrix is not correct later on... */
		uiButSetFlag(but, UI_NO_HILITE);
	}
	else {
		but= uiDefIconButBitS(block, TOG, SACTION_SLIDERS, B_REDR, 
					  ICON_DISCLOSURE_TRI_DOWN,
					  ACHANNEL_NAMEWIDTH - XIC - 5, (short)y + CHANNELHEIGHT,
					  XIC,YIC-2,
					  &(G.saction->flag), 0, 0, 0, 0, 
					  "Hide action window sliders");
		/* no hilite, the winmatrix is not correct later on... */
		uiButSetFlag(but, UI_NO_HILITE);
		
		ACTWIDTH = ACHANNEL_NAMEWIDTH + SLIDERWIDTH;
		
		/* sliders are open so draw them */
		BIF_ThemeColor(TH_FACE); 
		
		glRects(ACHANNEL_NAMEWIDTH,  0,  ACHANNEL_NAMEWIDTH+SLIDERWIDTH,  curarea->winy);
		uiBlockSetEmboss(block, UI_EMBOSS);
		for (i=1; i < key->totkey; i++) {
			make_rvk_slider(block, ob, i, 
							(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-1, "Slider to control Shape Keys");
			
			y-=CHANNELHEIGHT+CHANNELSKIP;
			
			/* see sliderval array in editkey.c */
			if (i >= 255) break;
		}
	}
	uiDrawBlock(C, block);
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
	if (bezt == NULL) {
		insert_vert_icu(icu, cfra, icu->curval, 0);
	}
	else {
		bezt->vec[1][1] = icu->curval;
	}

	/* make sure the Ipo's are properly processed and
	 * redraw as necessary
	 */
	sort_time_ipocurve(icu);
	testhandles_ipocurve(icu);
	
	/* nla-update (in case this affects anything) */
	synchronize_action_strips();
	
	/* do redraw pushes, and also the depsgraph flushes */
	if (OBACT->pose || ob_get_key(OBACT))
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC);
	else
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_OB);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWBUTSALL, 0);
}

static void make_icu_slider(uiBlock *block, IpoCurve *icu,
					 int x, int y, int w, int h, char *tip)
{
	/* create a slider for the ipo-curve*/
	uiBut *but;
	
	if(icu == NULL) return;
	
	if (IS_EQ(icu->slide_max, icu->slide_min)) {
		if (IS_EQ(icu->ymax, icu->ymin)) {
			if (ELEM(icu->blocktype, ID_CO, ID_KE)) {
				/* hack for constraints and shapekeys (and maybe a few others) */
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
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	char          str[64];
	float	        x, y;
	uiBlock       *block;

	/* lets make the action sliders */

	/* reset the damn myortho2 or the sliders won't draw/redraw
	 * correctly *grumble*
	 */
	mywinset(curarea->win);
	myortho2(-0.375f, curarea->winx-0.375f, G.v2d->cur.ymin, G.v2d->cur.ymax);
	
    sprintf(str, "actionbuttonswin %d", curarea->win);
    block= uiNewBlock (&curarea->uiblocks, str, UI_EMBOSS);

	x = (float)ACHANNEL_NAMEWIDTH + 1;
    y = 0.0f;
	
	uiBlockSetEmboss(block, UI_EMBOSSN);

	if (G.saction->flag & SACTION_SLIDERS) {
		/* sliders are open so draw them */
		
		/* get editor data */
		data= get_action_context(&datatype);
		if (data == NULL) return;
		
		/* build list of channels to draw */
		filter= (ACTFILTER_FORDRAWING|ACTFILTER_VISIBLE|ACTFILTER_CHANNELS);
		actdata_filter(&act_data, filter, data, datatype);
		
		/* draw backdrop first */
		BIF_ThemeColor(TH_FACE); // change this color... it's ugly
		glRects(ACHANNEL_NAMEWIDTH,  (short)G.v2d->cur.ymin,  ACHANNEL_NAMEWIDTH+SLIDERWIDTH,  (short)G.v2d->cur.ymax);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
		for (ale= act_data.first; ale; ale= ale->next) {
			const float yminc= y-CHANNELHEIGHT/2;
			const float ymaxc= y+CHANNELHEIGHT/2;
			
			/* check if visible */
			if ( IN_RANGE(yminc, G.v2d->cur.ymin, G.v2d->cur.ymax) ||
				 IN_RANGE(ymaxc, G.v2d->cur.ymin, G.v2d->cur.ymax) ) 
			{
				/* determine what needs to be drawn */
				switch (ale->type) {
					case ACTTYPE_CONCHAN: /* constraint channel */
					{
						bActionChannel *achan = (bActionChannel *)ale->owner;
						IpoCurve *icu = (IpoCurve *)ale->key_data;
						
						/* only show if owner is selected */
						if ((ale->ownertype == ACTTYPE_OBJECT) || SEL_ACHAN(achan)) {
							make_icu_slider(block, icu,
											(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
											"Slider to control current value of Constraint Influence");
						}
					}
						break;
					case ACTTYPE_ICU: /* ipo-curve channel */
					{
						bActionChannel *achan = (bActionChannel *)ale->owner;
						IpoCurve *icu = (IpoCurve *)ale->key_data;
						
						/* only show if owner is selected */
						if ((ale->ownertype == ACTTYPE_OBJECT) || SEL_ACHAN(achan)) {
							make_icu_slider(block, icu,
											(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
											"Slider to control current value of IPO-Curve");
						}
					}
						break;
					case ACTTYPE_SHAPEKEY: /* shapekey channel */
					{
						Object *ob= (Object *)ale->id;
						IpoCurve *icu= (IpoCurve *)ale->key_data;
						
						// TODO: only show if object is active 
						if (icu) {
							make_icu_slider(block, icu,
										(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
										"Slider to control ShapeKey");
						}
						else if (ob && ale->index) {
							make_rvk_slider(block, ob, ale->index, 
									(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-1, "Slider to control Shape Keys");
						}
					}
						break;
				}
			}
			
			/* adjust y-position for next one */
			y-=CHANNELHEIGHT+CHANNELSKIP;
		}
		
		/* free tempolary channels */
		BLI_freelistN(&act_data);
	}
	uiDrawBlock(C, block);
}

#endif // XXX all of this slider stuff will need a rethink 


/* ************************************************************************* */
/* Channel List */

/* left hand part */
void draw_channel_names(bContext *C, bAnimContext *ac, SpaceAction *saction, ARegion *ar) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d= &ar->v2d;
	float y= 0.0f;
	int items, height;
	
	/* build list of channels to draw */
	filter= (ANIMFILTER_VISIBLE|ANIMFILTER_CHANNELS);
	items= ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* Update max-extent of channels here (taking into account scrollers):
	 * 	- this is done to allow the channel list to be scrollable, but must be done here
	 * 	  to avoid regenerating the list again and/or also because channels list is drawn first
	 *	- offset of ACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for 
	 *	  start of list offset, and the second is as a correction for the scrollers.
	 */
	height= ((items*ACHANNEL_STEP) + (ACHANNEL_HEIGHT*2));
	if (height > (v2d->mask.ymax - v2d->mask.ymin)) {
		/* don't use totrect set, as the width stays the same 
		 * (NOTE: this is ok here, the configuration is pretty straightforward) 
		 */
		v2d->tot.ymin= (float)(-height);
	}
	/* need to do a view-sync here, so that the keys area doesn't jump around */
	UI_view2d_sync(NULL, ac->sa, v2d, V2D_VIEWSYNC_AREA_VERTICAL);
	
	/* loop through channels, and set up drawing depending on their type  */	
	{ 	/* first pass: just the standard GL-drawing for backdrop + text */
		y= (float)ACHANNEL_FIRST;
		
		for (ale= anim_data.first; ale; ale= ale->next) {
			float yminc= (float)(y - ACHANNEL_HEIGHT_HALF);
			float ymaxc= (float)(y + ACHANNEL_HEIGHT_HALF);
			
			/* check if visible */
			if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
				 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
			{
				/* draw all channels using standard channel-drawing API */
				ANIM_channel_draw(ac, ale, yminc, ymaxc);
			}
			
			/* adjust y-position for next one */
			y -= ACHANNEL_STEP;
		}
	}
	{	/* second pass: widgets */
		uiBlock *block= uiBeginBlock(C, ar, "dopesheet channel buttons", UI_EMBOSS);
		
		y= (float)ACHANNEL_FIRST;
		
		for (ale= anim_data.first; ale; ale= ale->next) {
			float yminc= (float)(y - ACHANNEL_HEIGHT_HALF);
			float ymaxc= (float)(y + ACHANNEL_HEIGHT_HALF);
			
			/* check if visible */
			if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
				 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
			{
				/* draw all channels using standard channel-drawing API */
				ANIM_channel_draw_widgets(ac, ale, block, yminc, ymaxc);
			}
			
			/* adjust y-position for next one */
			y -= ACHANNEL_STEP;
		}
		
		uiEndBlock(C, block);
		uiDrawBlock(C, block);
	}
	
	/* free tempolary channels */
	BLI_freelistN(&anim_data);
}

/* ************************************************************************* */
/* Keyframes */

/* extra padding for lengths (to go under scrollers) */
#define EXTRA_SCROLL_PAD	100.0f

/* draw keyframes in each channel */
void draw_channel_strips(bAnimContext *ac, SpaceAction *saction, ARegion *ar)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d= &ar->v2d;
	bDopeSheet *ads= &saction->ads;
	AnimData *adt= NULL;
	
	float act_start, act_end, y;
	int height, items;
	
	char col1[3], col2[3];
	char col1a[3], col2a[3];
	char col1b[3], col2b[3];
	
	
	/* get theme colors */
	UI_GetThemeColor3ubv(TH_BACK, col2);
	UI_GetThemeColor3ubv(TH_HILITE, col1);
	
	UI_GetThemeColor3ubv(TH_GROUP, col2a);
	UI_GetThemeColor3ubv(TH_GROUP_ACTIVE, col1a);
	
	UI_GetThemeColor3ubv(TH_DOPESHEET_CHANNELOB, col1b);
	UI_GetThemeColor3ubv(TH_DOPESHEET_CHANNELSUBOB, col2b);
	
	/* set view-mapping rect (only used for x-axis), for NLA-scaling mapping with less calculation */

	/* if in NLA there's a strip active, map the view */
	if (ac->datatype == ANIMCONT_ACTION) {
		adt= ANIM_nla_mapping_get(ac, NULL);
		
		/* start and end of action itself */
		// TODO: this has not had scaling applied
		calc_action_range(ac->data, &act_start, &act_end, 0);
	}
	
	/* build list of channels to draw */
	filter= (ANIMFILTER_VISIBLE|ANIMFILTER_CHANNELS);
	items= ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* Update max-extent of channels here (taking into account scrollers):
	 * 	- this is done to allow the channel list to be scrollable, but must be done here
	 * 	  to avoid regenerating the list again and/or also because channels list is drawn first
	 *	- offset of ACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for 
	 *	  start of list offset, and the second is as a correction for the scrollers.
	 */
	height= ((items*ACHANNEL_STEP) + (ACHANNEL_HEIGHT*2));
	/* don't use totrect set, as the width stays the same 
	 * (NOTE: this is ok here, the configuration is pretty straightforward) 
	 */
	v2d->tot.ymin= (float)(-height);
	
	/* first backdrop strips */
	y= (float)(-ACHANNEL_HEIGHT);
	glEnable(GL_BLEND);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		const float yminc= (float)(y - ACHANNEL_HEIGHT_HALF);
		const float ymaxc= (float)(y + ACHANNEL_HEIGHT_HALF);
		
		/* check if visible */
		if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
		{
			int sel=0;
			
			/* determine if any need to draw channel */
			if (ale->datatype != ALE_NONE) {
				/* determine if channel is selected */
				switch (ale->type) {
					case ANIMTYPE_SCENE:
					{
						Scene *sce= (Scene *)ale->data;
						sel = SEL_SCEC(sce);
					}
						break;
					case ANIMTYPE_OBJECT:
					{
						Base *base= (Base *)ale->data;
						sel = SEL_OBJC(base);
					}
						break;
					case ANIMTYPE_GROUP:
					{
						bActionGroup *agrp = (bActionGroup *)ale->data;
						sel = SEL_AGRP(agrp);
					}
						break;
					case ANIMTYPE_FCURVE:
					{
						FCurve *fcu = (FCurve *)ale->data;
						sel = SEL_FCU(fcu);
					}
						break;
					case ANIMTYPE_GPLAYER:
					{
						bGPDlayer *gpl = (bGPDlayer *)ale->data;
						sel = SEL_GPL(gpl);
					}
						break;
				}
				
				if (ELEM(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET)) {
					switch (ale->type) {
						case ANIMTYPE_SCENE:
						case ANIMTYPE_OBJECT:
						{
							if (sel) glColor4ub(col1b[0], col1b[1], col1b[2], 0x45); 
							else glColor4ub(col1b[0], col1b[1], col1b[2], 0x22); 
						}
							break;
						
						case ANIMTYPE_FILLACTD:
						case ANIMTYPE_FILLMATD:
						case ANIMTYPE_FILLPARTD:
						case ANIMTYPE_DSSKEY:
						case ANIMTYPE_DSWOR:
						{
							if (sel) glColor4ub(col2b[0], col2b[1], col2b[2], 0x45); 
							else glColor4ub(col2b[0], col2b[1], col2b[2], 0x22); 
						}
							break;
						
						case ANIMTYPE_GROUP:
						{
							if (sel) glColor4ub(col1a[0], col1a[1], col1a[2], 0x22);
							else glColor4ub(col2a[0], col2a[1], col2a[2], 0x22);
						}
							break;
						
						default:
						{
							if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x22);
							else glColor4ub(col2[0], col2[1], col2[2], 0x22);
						}
							break;
					}
					
					/* draw region twice: firstly backdrop, then the current range */
					glRectf(v2d->cur.xmin,  (float)y-ACHANNEL_HEIGHT_HALF,  v2d->cur.xmax+EXTRA_SCROLL_PAD,  (float)y+ACHANNEL_HEIGHT_HALF);
					
					if (ac->datatype == ANIMCONT_ACTION)
						glRectf(act_start,  (float)y-ACHANNEL_HEIGHT_HALF,  act_end,  (float)y+ACHANNEL_HEIGHT_HALF);
				}
				else if (ac->datatype == ANIMCONT_SHAPEKEY) {
					/* all frames that have a frame number less than one
					 * get a desaturated orange background
					 */
					glColor4ub(col2[0], col2[1], col2[2], 0x22);
					glRectf(0.0f, (float)y-ACHANNEL_HEIGHT_HALF, 1.0f, (float)y+ACHANNEL_HEIGHT_HALF);
					
					/* frames one and higher get a saturated orange background */
					glColor4ub(col2[0], col2[1], col2[2], 0x44);
					glRectf(1.0f, (float)y-ACHANNEL_HEIGHT_HALF, v2d->cur.xmax+EXTRA_SCROLL_PAD,  (float)y+ACHANNEL_HEIGHT_HALF);
				}
				else if (ac->datatype == ANIMCONT_GPENCIL) {
					/* frames less than one get less saturated background */
					if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x22);
					else glColor4ub(col2[0], col2[1], col2[2], 0x22);
					glRectf(0.0f, (float)y-ACHANNEL_HEIGHT_HALF, v2d->cur.xmin, (float)y+ACHANNEL_HEIGHT_HALF);
					
					/* frames one and higher get a saturated background */
					if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x44);
					else glColor4ub(col2[0], col2[1], col2[2], 0x44);
					glRectf(v2d->cur.xmin, (float)y-ACHANNEL_HEIGHT_HALF, v2d->cur.xmax+EXTRA_SCROLL_PAD,  (float)y+ACHANNEL_HEIGHT_HALF);
				}
			}
		}
		
		/*	Increment the step */
		y -= ACHANNEL_STEP;
	}		
	glDisable(GL_BLEND);
	
	/* Draw keyframes 
	 *	1) Only channels that are visible in the Action Editor get drawn/evaluated.
	 *	   This is to try to optimise this for heavier data sets
	 *	2) Keyframes which are out of view horizontally are disregarded 
	 */
	y= (float)(-ACHANNEL_HEIGHT);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		const float yminc= (float)(y - ACHANNEL_HEIGHT_HALF);
		const float ymaxc= (float)(y + ACHANNEL_HEIGHT_HALF);
		
		/* check if visible */
		if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
		{
			/* check if anything to show for this channel */
			if (ale->datatype != ALE_NONE) {
				adt= ANIM_nla_mapping_get(ac, ale);
				
				/* draw 'keyframes' for each specific datatype */
				switch (ale->datatype) {
					case ALE_SCE:
						draw_scene_channel(v2d, ads, ale->key_data, y);
						break;
					case ALE_OB:
						draw_object_channel(v2d, ads, ale->key_data, y);
						break;
					case ALE_ACT:
						draw_action_channel(v2d, adt, ale->key_data, y);
						break;
					case ALE_GROUP:
						draw_agroup_channel(v2d, adt, ale->data, y);
						break;
					case ALE_FCURVE:
						draw_fcurve_channel(v2d, adt, ale->key_data, y);
						break;
					case ALE_GPFRAME:
						draw_gpl_channel(v2d, ads, ale->data, y);
						break;
				}
			}
		}
		
		y-= ACHANNEL_STEP;
	}
	
	/* free tempolary channels used for drawing */
	BLI_freelistN(&anim_data);

	/* black line marking 'current frame' for Time-Slide transform mode */
	if (saction->flag & SACTION_MOVING) {
		glColor3f(0.0f, 0.0f, 0.0f);
		
		glBegin(GL_LINES);
			glVertex2f(saction->timeslide, v2d->cur.ymin-EXTRA_SCROLL_PAD);
			glVertex2f(saction->timeslide, v2d->cur.ymax);
		glEnd();
	}
}
