/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file blender/editors/space_action/action_draw.c
 *  \ingroup spaction
 */


/* System includes ----------------------------------------------------- */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

/* Types --------------------------------------------------------------- */

#include "DNA_anim_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.h"
#include "BKE_context.h"


/* Everything from source (BIF, BDR, BSE) ------------------------------ */ 

#include "BIF_gl.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"

#include "action_intern.h"

/* ************************************************************************* */
/* Channel List */

/* left hand part */
void draw_channel_names(bContext *C, bAnimContext *ac, ARegion *ar) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d = &ar->v2d;
	float y = 0.0f;
	size_t items;
	int height;
	
	/* build list of channels to draw */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* Update max-extent of channels here (taking into account scrollers):
	 *  - this is done to allow the channel list to be scrollable, but must be done here
	 *    to avoid regenerating the list again and/or also because channels list is drawn first
	 *	- offset of ACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for 
	 *	  start of list offset, and the second is as a correction for the scrollers.
	 */
	height = ((items * ACHANNEL_STEP) + (ACHANNEL_HEIGHT * 2));
	if (height > (v2d->mask.ymax - v2d->mask.ymin)) {
		/* don't use totrect set, as the width stays the same 
		 * (NOTE: this is ok here, the configuration is pretty straightforward) 
		 */
		v2d->tot.ymin = (float)(-height);
	}
	/* need to do a view-sync here, so that the keys area doesn't jump around (it must copy this) */
	UI_view2d_sync(NULL, ac->sa, v2d, V2D_LOCK_COPY);
	
	/* loop through channels, and set up drawing depending on their type  */	
	{   /* first pass: just the standard GL-drawing for backdrop + text */
		y = (float)ACHANNEL_FIRST;
		
		for (ale = anim_data.first; ale; ale = ale->next) {
			float yminc = (float)(y - ACHANNEL_HEIGHT_HALF);
			float ymaxc = (float)(y + ACHANNEL_HEIGHT_HALF);
			
			/* check if visible */
			if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) )
			{
				/* draw all channels using standard channel-drawing API */
				ANIM_channel_draw(ac, ale, yminc, ymaxc);
			}
			
			/* adjust y-position for next one */
			y -= ACHANNEL_STEP;
		}
	}
	{   /* second pass: widgets */
		uiBlock *block = uiBeginBlock(C, ar, __func__, UI_EMBOSS);
		size_t channel_index = 0;
		
		y = (float)ACHANNEL_FIRST;
		
		for (ale = anim_data.first; ale; ale = ale->next) {
			float yminc = (float)(y - ACHANNEL_HEIGHT_HALF);
			float ymaxc = (float)(y + ACHANNEL_HEIGHT_HALF);
			
			/* check if visible */
			if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) )
			{
				/* draw all channels using standard channel-drawing API */
				ANIM_channel_draw_widgets(C, ac, ale, block, yminc, ymaxc, channel_index);
			}
			
			/* adjust y-position for next one */
			y -= ACHANNEL_STEP;
			channel_index++;
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
#define EXTRA_SCROLL_PAD    100.0f

/* draw keyframes in each channel */
void draw_channel_strips(bAnimContext *ac, SpaceAction *saction, ARegion *ar)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d = &ar->v2d;
	bDopeSheet *ads = &saction->ads;
	AnimData *adt = NULL;
	
	float act_start, act_end, y;
	size_t items;
	int height;
	
	unsigned char col1[3], col2[3];
	unsigned char col1a[3], col2a[3];
	unsigned char col1b[3], col2b[3];
	
	
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
		/* adt= ANIM_nla_mapping_get(ac, NULL); */ /* UNUSED */
		
		/* start and end of action itself */
		calc_action_range(ac->data, &act_start, &act_end, 0);
	}
	
	/* build list of channels to draw */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* Update max-extent of channels here (taking into account scrollers):
	 *  - this is done to allow the channel list to be scrollable, but must be done here
	 *    to avoid regenerating the list again and/or also because channels list is drawn first
	 *	- offset of ACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for 
	 *	  start of list offset, and the second is as a correction for the scrollers.
	 */
	height = ((items * ACHANNEL_STEP) + (ACHANNEL_HEIGHT * 2));
	/* don't use totrect set, as the width stays the same 
	 * (NOTE: this is ok here, the configuration is pretty straightforward) 
	 */
	v2d->tot.ymin = (float)(-height);
	
	/* first backdrop strips */
	y = (float)(-ACHANNEL_HEIGHT);
	glEnable(GL_BLEND);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		const float yminc = (float)(y - ACHANNEL_HEIGHT_HALF);
		const float ymaxc = (float)(y + ACHANNEL_HEIGHT_HALF);
		
		/* check if visible */
		if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
		    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) )
		{
			bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
			int sel = 0;
			
			/* determine if any need to draw channel */
			if (ale->datatype != ALE_NONE) {
				/* determine if channel is selected */
				if (acf->has_setting(ac, ale, ACHANNEL_SETTING_SELECT))
					sel = ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_SELECT);
				
				if (ELEM3(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET, ANIMCONT_SHAPEKEY)) {
					switch (ale->type) {
						case ANIMTYPE_SUMMARY:
						{
							/* reddish color from NLA */
							UI_ThemeColor4(TH_ANIM_ACTIVE);
						}
						break;
						
						case ANIMTYPE_SCENE:
						case ANIMTYPE_OBJECT:
						{
							if (sel) glColor4ub(col1b[0], col1b[1], col1b[2], 0x45); 
							else glColor4ub(col1b[0], col1b[1], col1b[2], 0x22); 
						}
						break;
						
						case ANIMTYPE_FILLACTD:
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
					glRectf(v2d->cur.xmin,  (float)y - ACHANNEL_HEIGHT_HALF,  v2d->cur.xmax + EXTRA_SCROLL_PAD,  (float)y + ACHANNEL_HEIGHT_HALF);
					
					if (ac->datatype == ANIMCONT_ACTION)
						glRectf(act_start,  (float)y - ACHANNEL_HEIGHT_HALF,  act_end,  (float)y + ACHANNEL_HEIGHT_HALF);
				}
				else if (ac->datatype == ANIMCONT_GPENCIL) {
					/* frames less than one get less saturated background */
					if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x22);
					else glColor4ub(col2[0], col2[1], col2[2], 0x22);
					glRectf(0.0f, (float)y - ACHANNEL_HEIGHT_HALF, v2d->cur.xmin, (float)y + ACHANNEL_HEIGHT_HALF);
					
					/* frames one and higher get a saturated background */
					if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x44);
					else glColor4ub(col2[0], col2[1], col2[2], 0x44);
					glRectf(v2d->cur.xmin, (float)y - ACHANNEL_HEIGHT_HALF, v2d->cur.xmax + EXTRA_SCROLL_PAD,  (float)y + ACHANNEL_HEIGHT_HALF);
				}
			}
		}
		
		/*	Increment the step */
		y -= ACHANNEL_STEP;
	}		
	glDisable(GL_BLEND);
	
	/* Draw keyframes 
	 *	1) Only channels that are visible in the Action Editor get drawn/evaluated.
	 *	   This is to try to optimize this for heavier data sets
	 *	2) Keyframes which are out of view horizontally are disregarded 
	 */
	y = (float)(-ACHANNEL_HEIGHT);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		const float yminc = (float)(y - ACHANNEL_HEIGHT_HALF);
		const float ymaxc = (float)(y + ACHANNEL_HEIGHT_HALF);
		
		/* check if visible */
		if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
		    IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) )
		{
			/* check if anything to show for this channel */
			if (ale->datatype != ALE_NONE) {
				adt = ANIM_nla_mapping_get(ac, ale);
				
				/* draw 'keyframes' for each specific datatype */
				switch (ale->datatype) {
					case ALE_ALL:
						draw_summary_channel(v2d, ale->data, y);
						break;
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
		
		y -= ACHANNEL_STEP;
	}
	
	/* free tempolary channels used for drawing */
	BLI_freelistN(&anim_data);

	/* black line marking 'current frame' for Time-Slide transform mode */
	if (saction->flag & SACTION_MOVING) {
		glColor3f(0.0f, 0.0f, 0.0f);
		
		glBegin(GL_LINES);
		glVertex2f(saction->timeslide, v2d->cur.ymin - EXTRA_SCROLL_PAD);
		glVertex2f(saction->timeslide, v2d->cur.ymax);
		glEnd();
	}
}
