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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung (major recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_nla/nla_draw.c
 *  \ingroup spnla
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "DNA_anim_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_dlrbTree.h"
#include "BLI_utildefines.h"

#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"


#include "nla_intern.h"	// own include


/* *********************************************** */
/* Strips */

/* Action-Line ---------------------- */

/* get colors for drawing Action-Line 
 * NOTE: color returned includes fine-tuned alpha!
 */
static void nla_action_get_color (AnimData *adt, bAction *act, float color[4])
{
	if (adt && (adt->flag & ADT_NLA_EDIT_ON)) {
		// greenish color (same as tweaking strip) - hardcoded for now
		color[0]= 0.30f;
		color[1]= 0.95f;
		color[2]= 0.10f;
		color[3]= 0.30f;
	}
	else {
		if (act) {
			// reddish color - hardcoded for now 	
			color[0]= 0.8f;
			color[1]= 0.2f;
			color[2]= 0.0f;
			color[3]= 0.4f;
		}
		else {
			// greyish-red color - hardcoded for now
			color[0]= 0.6f;
			color[1]= 0.5f;
			color[2]= 0.5f;
			color[3]= 0.3f;
		}
	}
	
	/* when an NLA track is tagged "solo", action doesn't contribute, so shouldn't be as prominent */
	if (adt && (adt->flag & ADT_NLA_SOLO_TRACK))
		color[3] *= 0.15f;
}

/* draw the keyframes in the specified Action */
static void nla_action_draw_keyframes (AnimData *adt, bAction *act, View2D *v2d, float y, float ymin, float ymax)
{
	DLRBT_Tree keys;
	ActKeyColumn *ak;
	float xscale, f1, f2;
	float color[4];
	
	/* get a list of the keyframes with NLA-scaling applied */
	BLI_dlrbTree_init(&keys);
	action_to_keylist(adt, act, &keys, NULL);
	BLI_dlrbTree_linkedlist_sync(&keys);
	
	if ELEM(NULL, act, keys.first)
		return;
	
	/* draw a darkened region behind the strips 
	 *	- get and reset the background color, this time without the alpha to stand out better 
	 *	  (amplified alpha is used instead)
	 */
	nla_action_get_color(adt, act, color);
	color[3] *= 2.5f;
	
	glColor4fv(color);
	/* 	- draw a rect from the first to the last frame (no extra overlaps for now) 
	 *	  that is slightly stumpier than the track background (hardcoded 2-units here)
	 */
	f1= ((ActKeyColumn *)keys.first)->cfra;
	f2= ((ActKeyColumn *)keys.last)->cfra;
	
	glRectf(f1, ymin+2, f2, ymax-2);
	
	
	/* get View2D scaling factor */
	UI_view2d_getscale(v2d, &xscale, NULL);
	
	/* for now, color is hardcoded to be black */
	glColor3f(0.0f, 0.0f, 0.0f);
	
	/* just draw each keyframe as a simple dot (regardless of the selection status) 
	 *	- size is 3.0f which is smaller than the editable keyframes, so that there is a distinction
	 */
	for (ak= keys.first; ak; ak= ak->next)
		draw_keyframe_shape(ak->cfra, y, xscale, 3.0f, 0, ak->key_type, KEYFRAME_SHAPE_FRAME, 1.0f);
	
	/* free icons */
	BLI_dlrbTree_free(&keys);
}

/* Strips (Proper) ---------------------- */

/* get colors for drawing NLA-Strips */
static void nla_strip_get_color_inside (AnimData *adt, NlaStrip *strip, float color[3])
{
	if (strip->type == NLASTRIP_TYPE_TRANSITION) {
		/* Transition Clip */
		if (strip->flag & NLASTRIP_FLAG_SELECT) {
			/* selected - use a bright blue color */
			// FIXME: hardcoded temp-hack colors
			color[0]= 0.18f;
			color[1]= 0.46f;
			color[2]= 0.86f;
		}
		else {
			/* normal, unselected strip - use (hardly noticable) blue tinge */
			// FIXME: hardcoded temp-hack colors
			color[0]= 0.11f;
			color[1]= 0.15f;
			color[2]= 0.19f;
		}
	}	
	else if (strip->type == NLASTRIP_TYPE_META) {
		/* Meta Clip */
		// TODO: should temporary metas get different colors too?
		if (strip->flag & NLASTRIP_FLAG_SELECT) {
			/* selected - use a bold purple color */
			// FIXME: hardcoded temp-hack colors
			color[0]= 0.41f;
			color[1]= 0.13f;
			color[2]= 0.59f;
		}
		else {
			/* normal, unselected strip - use (hardly noticable) dark purple tinge */
			// FIXME: hardcoded temp-hack colors
			color[0]= 0.20f;
			color[1]= 0.15f;
			color[2]= 0.26f;
		}
	}
	else if (strip->type == NLASTRIP_TYPE_SOUND) {
		/* Sound Clip */
		if (strip->flag & NLASTRIP_FLAG_SELECT) {
			/* selected - use a bright teal color */
			// FIXME: hardcoded temp-hack colors
			color[0]= 0.12f;
			color[1]= 0.48f;
			color[2]= 0.48f;
		}
		else {
			/* normal, unselected strip - use (hardly noticable) teal tinge */
			// FIXME: hardcoded temp-hack colors
			color[0]= 0.17f;
			color[1]= 0.24f;
			color[2]= 0.24f;
		}
	}
	else {
		/* Action Clip (default/normal type of strip) */
		if ((strip->flag & NLASTRIP_FLAG_ACTIVE) && (adt && (adt->flag & ADT_NLA_EDIT_ON))) {
			/* active strip should be drawn green when it is acting as the tweaking strip.
			 * however, this case should be skipped for when not in EditMode...
			 */
			// FIXME: hardcoded temp-hack colors
			color[0]= 0.3f;
			color[1]= 0.95f;
			color[2]= 0.1f;
		}
		else if (strip->flag & NLASTRIP_FLAG_TWEAKUSER) {
			/* alert user that this strip is also used by the tweaking track (this is set when going into
			 * 'editmode' for that strip), since the edits made here may not be what the user anticipated
			 */
			// FIXME: hardcoded temp-hack colors
			color[0]= 0.85f;
			color[1]= 0.0f;
			color[2]= 0.0f;
		}
		else if (strip->flag & NLASTRIP_FLAG_SELECT) {
			/* selected strip - use theme color for selected */
			UI_GetThemeColor3fv(TH_STRIP_SELECT, color);
		}
		else {
			/* normal, unselected strip - use standard strip theme color */
			UI_GetThemeColor3fv(TH_STRIP, color);
		}
	}
}

/* helper call for drawing influence/time control curves for a given NLA-strip */
static void nla_draw_strip_curves (NlaStrip *strip, float yminc, float ymaxc)
{
	const float yheight = ymaxc - yminc;
	
	/* drawing color is simply a light-grey */
	// TODO: is this color suitable?
	// XXX nasty hacked color for now... which looks quite bad too...
	glColor3f(0.7f, 0.7f, 0.7f);
	
	/* draw with AA'd line */
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	
	/* influence -------------------------- */
	if (strip->flag & NLASTRIP_FLAG_USR_INFLUENCE) {
		FCurve *fcu= list_find_fcurve(&strip->fcurves, "influence", 0);
		float cfra;
		
		/* plot the curve (over the strip's main region) */
		glBegin(GL_LINE_STRIP);
			/* sample at 1 frame intervals, and draw 
			 *	- min y-val is yminc, max is y-maxc, so clamp in those regions
			 */
			for (cfra= strip->start; cfra <= strip->end; cfra += 1.0f) {
				float y= evaluate_fcurve(fcu, cfra); // assume this to be in 0-1 range
				glVertex2f(cfra, ((y*yheight)+yminc));
			}
		glEnd(); // GL_LINE_STRIP
	}
	else {
		/* use blend in/out values only if both aren't zero */
		if ((IS_EQF(strip->blendin, 0.0f) && IS_EQF(strip->blendout, 0.0f))==0) {
			glBegin(GL_LINE_STRIP);
				/* start of strip - if no blendin, start straight at 1, otherwise from 0 to 1 over blendin frames */
				if (IS_EQF(strip->blendin, 0.0f) == 0) {
					glVertex2f(strip->start, 					yminc);
					glVertex2f(strip->start + strip->blendin, 	ymaxc);
				}
				else
					glVertex2f(strip->start, ymaxc);
					
				/* end of strip */
				if (IS_EQF(strip->blendout, 0.0f) == 0) {
					glVertex2f(strip->end - strip->blendout,	ymaxc);
					glVertex2f(strip->end, 						yminc);
				}
				else
					glVertex2f(strip->end, ymaxc);
			glEnd(); // GL_LINE_STRIP
		}
	}
	
	/* time -------------------------- */
	// XXX do we want to draw this curve? in a different color too?
	
	/* turn off AA'd lines */
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
}

/* main call for drawing a single NLA-strip */
static void nla_draw_strip (SpaceNla *snla, AnimData *adt, NlaTrack *nlt, NlaStrip *strip, View2D *v2d, float yminc, float ymaxc)
{
	short nonSolo = ((adt && (adt->flag & ADT_NLA_SOLO_TRACK)) && (nlt->flag & NLATRACK_SOLO)==0);
	float color[3];
	
	/* get color of strip */
	nla_strip_get_color_inside(adt, strip, color);
	
	/* draw extrapolation info first (as backdrop)
	 *	- but this should only be drawn if track has some contribution
	 */
	if ((strip->extendmode != NLASTRIP_EXTEND_NOTHING) && (nonSolo == 0)) {
		/* enable transparency... */
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		
		switch (strip->extendmode) {
			/* since this does both sides, only do the 'before' side, and leave the rest to the next case */
			case NLASTRIP_EXTEND_HOLD: 
				/* only need to draw here if there's no strip before since 
				 * it only applies in such a situation 
				 */
				if (strip->prev == NULL) {
					/* set the drawing color to the color of the strip, but with very faint alpha */
					glColor4f(color[0], color[1], color[2], 0.15f);
					
					/* draw the rect to the edge of the screen */
					glBegin(GL_QUADS);
						glVertex2f(v2d->cur.xmin, yminc);
						glVertex2f(v2d->cur.xmin, ymaxc);
						glVertex2f(strip->start, ymaxc);
						glVertex2f(strip->start, yminc);
					glEnd();
				}
				/* no break needed... */
				
			/* this only draws after the strip */
			case NLASTRIP_EXTEND_HOLD_FORWARD: 
				/* only need to try and draw if the next strip doesn't occur immediately after */
				if ((strip->next == NULL) || (IS_EQF(strip->next->start, strip->end)==0)) {
					/* set the drawing color to the color of the strip, but this time less faint */
					glColor4f(color[0], color[1], color[2], 0.3f);
					
					/* draw the rect to the next strip or the edge of the screen */
					glBegin(GL_QUADS);
						glVertex2f(strip->end, yminc);
						glVertex2f(strip->end, ymaxc);
						
						if (strip->next) {
							glVertex2f(strip->next->start, ymaxc);
							glVertex2f(strip->next->start, yminc);
						}
						else {
							glVertex2f(v2d->cur.xmax, ymaxc);
							glVertex2f(v2d->cur.xmax, yminc);
						}
					glEnd();
				}
				break;
		}
		
		glDisable(GL_BLEND);
	}
	
	
	/* draw 'inside' of strip itself */
	if (nonSolo == 0) {
		/* strip is in normal track */
		glColor3fv(color);
		uiSetRoundBox(UI_CNR_ALL); /* all corners rounded */
		
		uiDrawBoxShade(GL_POLYGON, strip->start, yminc, strip->end, ymaxc, 0.0, 0.5, 0.1);
	}
	else {
		/* strip is in disabled track - make less visible */
		glColor4f(color[0], color[1], color[2], 0.1f);
		
		glEnable(GL_BLEND);
			glRectf(strip->start, yminc, strip->end, ymaxc);
		glDisable(GL_BLEND);
	}
	
	
	/* draw strip's control 'curves'
	 *	- only if user hasn't hidden them...
	 */
	if ((snla->flag & SNLA_NOSTRIPCURVES) == 0)
		nla_draw_strip_curves(strip, yminc, ymaxc);
	
	
	/* draw strip outline 
	 *	- color used here is to indicate active vs non-active
	 */
	if (strip->flag & NLASTRIP_FLAG_ACTIVE) {
		/* strip should appear 'sunken', so draw a light border around it */
		glColor3f(0.9f, 1.0f, 0.9f); // FIXME: hardcoded temp-hack colors
	}
	else {
		/* strip should appear to stand out, so draw a dark border around it */
		glColor3f(0.0f, 0.0f, 0.0f);
	}
	
	/* - line style: dotted for muted */
	if (strip->flag & NLASTRIP_FLAG_MUTED)
		setlinestyle(4);
		
	/* draw outline */
	uiDrawBoxShade(GL_LINE_LOOP, strip->start, yminc, strip->end, ymaxc, 0.0, 0.0, 0.1);
	
	/* if action-clip strip, draw lines delimiting repeats too (in the same color as outline) */
	if ((strip->type == NLASTRIP_TYPE_CLIP) && IS_EQF(strip->repeat, 1.0f)==0) {
		float repeatLen = (strip->actend - strip->actstart) * strip->scale;
		int i;
		
		/* only draw lines for whole-numbered repeats, starting from the first full-repeat
		 * up to the last full repeat (but not if it lies on the end of the strip)
		 */
		for (i = 1; i < strip->repeat; i++) {
			float repeatPos = strip->start + (repeatLen * i);
			
			/* don't draw if line would end up on or after the end of the strip */
			if (repeatPos < strip->end)
				fdrawline(repeatPos, yminc+4, repeatPos, ymaxc-4);
		}
	}
	/* or if meta-strip, draw lines delimiting extents of sub-strips (in same color as outline, if more than 1 exists) */
	else if ((strip->type == NLASTRIP_TYPE_META) && (strip->strips.first != strip->strips.last)) {
		NlaStrip *cs;
		float y= (ymaxc-yminc)/2.0f + yminc;
		
		/* only draw first-level of child-strips, but don't draw any lines on the endpoints */
		for (cs= strip->strips.first; cs; cs= cs->next) {
			/* draw start-line if not same as end of previous (and only if not the first strip) 
			 *	- on upper half of strip
			 */
			if ((cs->prev) && IS_EQF(cs->prev->end, cs->start)==0)
				fdrawline(cs->start, y, cs->start, ymaxc);
				
			/* draw end-line if not the last strip
			 *	- on lower half of strip
			 */
			if (cs->next) 
				fdrawline(cs->end, yminc, cs->end, y);
		}
	}
	
	/* reset linestyle */
	setlinestyle(0);
} 

/* add the relevant text to the cache of text-strings to draw in pixelspace */
static void nla_draw_strip_text (AnimData *adt, NlaTrack *nlt, NlaStrip *strip, int index, View2D *v2d, float yminc, float ymaxc)
{
	short notSolo = ((adt && (adt->flag & ADT_NLA_SOLO_TRACK)) && (nlt->flag & NLATRACK_SOLO)==0);
	char str[256];
	char col[4];
	float xofs;
	rctf rect;
	
	/* just print the name and the range */
	if (strip->flag & NLASTRIP_FLAG_TEMP_META) {
		BLI_snprintf(str, sizeof(str), "%d) Temp-Meta", index);
	}
	else {
		BLI_strncpy(str, strip->name, sizeof(str));
	}
	
	/* set text color - if colors (see above) are light, draw black text, otherwise draw white */
	if (strip->flag & (NLASTRIP_FLAG_ACTIVE|NLASTRIP_FLAG_SELECT|NLASTRIP_FLAG_TWEAKUSER)) {
		col[0]= col[1]= col[2]= 0;
	}
	else {
		col[0]= col[1]= col[2]= 255;
	}
	
	/* text opacity depends on whether if there's a solo'd track, this isn't it */
	if (notSolo == 0)
		col[3]= 255;
	else
		col[3]= 128;
	
	/* determine the amount of padding required - cannot be constant otherwise looks weird in some cases */
	if ((strip->end - strip->start) <= 5.0f)
		xofs = 0.5f;
	else
		xofs = 1.0f;
	
	/* set bounding-box for text 
	 *	- padding of 2 'units' on either side
	 */
	// TODO: make this centered?
	rect.xmin= strip->start + xofs;
	rect.ymin= yminc;
	rect.xmax= strip->end - xofs;
	rect.ymax= ymaxc;
	
	/* add this string to the cache of texts to draw */
	UI_view2d_text_cache_rectf(v2d, &rect, str, col);
}

/* add frame extents to cache of text-strings to draw in pixelspace
 * for now, only used when transforming strips
 */
static void nla_draw_strip_frames_text(NlaTrack *UNUSED(nlt), NlaStrip *strip, View2D *v2d, float UNUSED(yminc), float ymaxc)
{
	const float ytol = 1.0f; /* small offset to vertical positioning of text, for legibility */
	const char col[4] = {220, 220, 220, 255}; /* light grey */
	char numstr[32] = "";
	
	
	/* Always draw times above the strip, whereas sequencer drew below + above.
	 * However, we should be fine having everything on top, since these tend to be 
	 * quite spaced out. 
	 *	- 1 dp is compromise between lack of precision (ints only, as per sequencer)
	 *	  while also preserving some accuracy, since we do use floats
	 */
		/* start frame */
	BLI_snprintf(numstr, sizeof(numstr), "%.1f", strip->start);
	UI_view2d_text_cache_add(v2d, strip->start-1.0f, ymaxc+ytol, numstr, col);
	
		/* end frame */
	BLI_snprintf(numstr, sizeof(numstr), "%.1f", strip->end);
	UI_view2d_text_cache_add(v2d, strip->end, ymaxc+ytol, numstr, col);
}

/* ---------------------- */

void draw_nla_main_data (bAnimContext *ac, SpaceNla *snla, ARegion *ar)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d= &ar->v2d;
	float y= 0.0f;
	size_t items;
	int height;
	
	/* build list of channels to draw */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	items= ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* Update max-extent of channels here (taking into account scrollers):
	 * 	- this is done to allow the channel list to be scrollable, but must be done here
	 * 	  to avoid regenerating the list again and/or also because channels list is drawn first
	 *	- offset of NLACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for 
	 *	  start of list offset, and the second is as a correction for the scrollers.
	 */
	height= ((items*NLACHANNEL_STEP(snla)) + (NLACHANNEL_HEIGHT(snla)*2));
	/* don't use totrect set, as the width stays the same 
	 * (NOTE: this is ok here, the configuration is pretty straightforward) 
	 */
	v2d->tot.ymin= (float)(-height);
	
	/* loop through channels, and set up drawing depending on their type  */	
	y= (float)(-NLACHANNEL_HEIGHT(snla));
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		const float yminc= (float)(y - NLACHANNEL_HEIGHT_HALF(snla));
		const float ymaxc= (float)(y + NLACHANNEL_HEIGHT_HALF(snla));
		
		/* check if visible */
		if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
		{
			/* data to draw depends on the type of channel */
			switch (ale->type) {
				case ANIMTYPE_NLATRACK:
				{
					AnimData *adt= ale->adt;
					NlaTrack *nlt= (NlaTrack *)ale->data;
					NlaStrip *strip;
					int index;
					
					/* draw each strip in the track (if visible) */
					for (strip=nlt->strips.first, index=1; strip; strip=strip->next, index++) {
						if (BKE_nlastrip_within_bounds(strip, v2d->cur.xmin, v2d->cur.xmax)) {
							/* draw the visualisation of the strip */
							nla_draw_strip(snla, adt, nlt, strip, v2d, yminc, ymaxc);
							
							/* add the text for this strip to the cache */
							nla_draw_strip_text(adt, nlt, strip, index, v2d, yminc, ymaxc);
							
							/* if transforming strips (only real reason for temp-metas currently), 
							 * add to the cache the frame numbers of the strip's extents
							 */
							if (strip->flag & NLASTRIP_FLAG_TEMP_META)
								nla_draw_strip_frames_text(nlt, strip, v2d, yminc, ymaxc);
						}
					}
				}
					break;
					
				case ANIMTYPE_NLAACTION:
				{
					AnimData *adt= ale->adt;
					float color[4];
					
					/* just draw a semi-shaded rect spanning the width of the viewable area if there's data,
					 * and a second darker rect within which we draw keyframe indicator dots if there's data
					 */
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glEnable(GL_BLEND);
						
					/* get colors for drawing */
					nla_action_get_color(adt, ale->data, color);
					glColor4fv(color);
					
					/* draw slightly shifted up for greater separation from standard channels,
					 * but also slightly shorter for some more contrast when viewing the strips
					 */
					glRectf(v2d->cur.xmin, yminc+NLACHANNEL_SKIP, v2d->cur.xmax, ymaxc-NLACHANNEL_SKIP);
					
					/* draw keyframes in the action */
					nla_action_draw_keyframes(adt, ale->data, v2d, y, yminc+NLACHANNEL_SKIP, ymaxc-NLACHANNEL_SKIP);
					
					/* draw 'embossed' lines above and below the strip for effect */
						/* white base-lines */
					glLineWidth(2.0f);
					glColor4f(1.0f, 1.0f, 1.0f, 0.3);
					fdrawline(v2d->cur.xmin, yminc+NLACHANNEL_SKIP, v2d->cur.xmax, yminc+NLACHANNEL_SKIP);
					fdrawline(v2d->cur.xmin, ymaxc-NLACHANNEL_SKIP, v2d->cur.xmax, ymaxc-NLACHANNEL_SKIP);
					
						/* black top-lines */
					glLineWidth(1.0f);
					glColor3f(0.0f, 0.0f, 0.0f);
					fdrawline(v2d->cur.xmin, yminc+NLACHANNEL_SKIP, v2d->cur.xmax, yminc+NLACHANNEL_SKIP);
					fdrawline(v2d->cur.xmin, ymaxc-NLACHANNEL_SKIP, v2d->cur.xmax, ymaxc-NLACHANNEL_SKIP);
					
					glDisable(GL_BLEND);
				}
					break;
			}
		}
		
		/* adjust y-position for next one */
		y -= NLACHANNEL_STEP(snla);
	}
	
	/* free tempolary channels */
	BLI_freelistN(&anim_data);
}

/* *********************************************** */
/* Channel List */

/* old code for drawing NLA channels using GL only */
// TODO: depreceate this code...
static void draw_nla_channel_list_gl (bAnimContext *ac, ListBase *anim_data, View2D *v2d, float y)
{
	SpaceNla *snla = (SpaceNla *)ac->sl;
	bAnimListElem *ale;
	float x = 0.0f;
	
	/* loop through channels, and set up drawing depending on their type  */	
	for (ale= anim_data->first; ale; ale= ale->next) {
		const float yminc= (float)(y - NLACHANNEL_HEIGHT_HALF(snla));
		const float ymaxc= (float)(y + NLACHANNEL_HEIGHT_HALF(snla));
		const float ydatac= (float)(y - 7);
		
		/* check if visible */
		if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
		{
			AnimData *adt = ale->adt;
			
			short indent= 0, offset= 0, sel= 0, group= 0, nonSolo= 0;
			int expand= -1, protect = -1, special= -1, mute = -1;
			char name[128];
			short doDraw=0;
			
			/* determine what needs to be drawn */
			switch (ale->type) {
				case ANIMTYPE_NLATRACK: /* NLA Track */
				{
					NlaTrack *nlt= (NlaTrack *)ale->data;
					
					/* 'solo' as the 'special' button? */
					if (nlt->flag & NLATRACK_SOLO)
						special= ICON_SOLO_ON;
					else
						special= ICON_SOLO_OFF;
						
					/* if this track is active and we're tweaking it, don't draw these toggles */
					// TODO: need a special macro for this...
					if ( ((nlt->flag & NLATRACK_ACTIVE) && (nlt->flag & NLATRACK_DISABLED)) == 0 ) 
					{
						if (nlt->flag & NLATRACK_MUTED)
							mute = ICON_MUTE_IPO_ON;
						else	
							mute = ICON_MUTE_IPO_OFF;
							
						if (EDITABLE_NLT(nlt))
							protect = ICON_UNLOCKED;
						else
							protect = ICON_LOCKED;
					}
					
					/* is track enabled for solo drawing? */
					if ((adt) && (adt->flag & ADT_NLA_SOLO_TRACK)) {
						if ((nlt->flag & NLATRACK_SOLO) == 0) {
							/* tag for special non-solo handling; also hide the mute toggles */
							nonSolo= 1;
							mute = 0;
						}
					}
						
					sel = SEL_NLT(nlt);
					BLI_strncpy(name, nlt->name, sizeof(name));
					
					// draw manually still
					doDraw= 1;
				}
					break;
				case ANIMTYPE_NLAACTION: /* NLA Action-Line */
				{
					bAction *act= (bAction *)ale->data;
					
					group = 5;
					
					special = ICON_ACTION;
					
					if (act)
						BLI_snprintf(name, sizeof(name), "%s", act->id.name+2);
					else
						BLI_strncpy(name, "<No Action>", sizeof(name));
						
					// draw manually still
					doDraw= 1;
				}
					break;
					
				default: /* handled by standard channel-drawing API */
					// draw backdrops only...
					ANIM_channel_draw(ac, ale, yminc, ymaxc);
					break;
			}	
			
			/* if special types, draw manually for now... */
			if (doDraw) {
				if (ale->id) {
					/* special exception for textures */
					if (GS(ale->id->name) == ID_TE) {
						offset= 14;
						indent= 1;
					}
					/* special exception for nodetrees */
					else if (GS(ale->id->name) == ID_NT) {
						bNodeTree *ntree = (bNodeTree *)ale->id;
						
						switch (ntree->type) {
							case NTREE_SHADER:
							{
								/* same as for textures */
								offset= 14;
								indent= 1;
							}
								break;
								
							case NTREE_TEXTURE:
							{
								/* even more */
								offset= 21;
								indent= 1;
							}	
								break;
								
							default:
								/* normal will do */
								offset= 14;
								break;
						}
					}
					else
						offset= 14;
				}
				else
					offset= 0;
				
				/* now, start drawing based on this information */
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glEnable(GL_BLEND);
				
				/* draw backing strip behind channel name */
				if (group == 5) {
					/* Action Line */
					// TODO: if tweaking some action, use the same color as for the tweaked track (quick hack done for now)
					if (adt && (adt->flag & ADT_NLA_EDIT_ON)) {
						// greenish color (same as tweaking strip) - hardcoded for now
						glColor3f(0.3f, 0.95f, 0.1f);
					}
					else {
						/* if a track is being solo'd, action is ignored, so draw less boldly (alpha lower) */
						float alpha = (adt && (adt->flag & ADT_NLA_SOLO_TRACK))? 0.3f : 1.0f;
						
						if (ale->data)
							glColor4f(0.8f, 0.2f, 0.0f, alpha);	// reddish color - hardcoded for now 
						else
							glColor4f(0.6f, 0.5f, 0.5f, alpha); // greyish-red color - hardcoded for now
					}
					
					offset += 7 * indent;
					
					/* only on top two corners, to show that this channel sits on top of the preceding ones */
					uiSetRoundBox(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT);
					
					/* draw slightly shifted up vertically to look like it has more separtion from other channels,
					 * but we then need to slightly shorten it so that it doesn't look like it overlaps
					 */
					uiDrawBox(GL_POLYGON, x+offset,  yminc+NLACHANNEL_SKIP, (float)v2d->cur.xmax, ymaxc+NLACHANNEL_SKIP-1, 8);
					
					/* clear group value, otherwise we cause errors... */
					group = 0;
				}
				else {
					/* NLA tracks - darker color if not solo track when we're showing solo */
					UI_ThemeColorShade(TH_HEADER, ((nonSolo == 0)? 20 : -20));
					
					indent += group;
					offset += 7 * indent;
					glBegin(GL_QUADS);
						glVertex2f(x+offset, yminc);
						glVertex2f(x+offset, ymaxc);
						glVertex2f((float)v2d->cur.xmax, ymaxc);
						glVertex2f((float)v2d->cur.xmax, yminc);
					glEnd();
				}
				
				/* draw expand/collapse triangle */
				if (expand > 0) {
					UI_icon_draw(x+offset, ydatac, expand);
					offset += 17;
				}
				
				/* draw special icon indicating certain data-types */
				if (special > -1) {
					/* for normal channels */
					UI_icon_draw(x+offset, ydatac, special);
					offset += 17;
				}
				glDisable(GL_BLEND);
				
				/* draw name */
				if (sel)
					UI_ThemeColor(TH_TEXT_HI);
				else
					UI_ThemeColor(TH_TEXT);
				offset += 3;
				UI_DrawString(x+offset, y-4, name);
				
				/* reset offset - for RHS of panel */
				offset = 0;
				
				/* set blending again, as text drawing may clear it */
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glEnable(GL_BLEND);
				
				/* draw protect 'lock' */
				if (protect > -1) {
					offset = 16;
					UI_icon_draw((float)(v2d->cur.xmax-offset), ydatac, protect);
				}
				
				/* draw mute 'eye' */
				if (mute > -1) {
					offset += 16;
					UI_icon_draw((float)(v2d->cur.xmax-offset), ydatac, mute);
				}
				
				/* draw NLA-action line 'status-icons' - only when there's an action */
				if ((ale->type == ANIMTYPE_NLAACTION) && (ale->data)) {
					AnimData *adt= ale->adt;
					
					offset += 16;
					
					/* now draw some indicator icons  */
					if ((adt) && (adt->flag & ADT_NLA_EDIT_ON)) {
						/* toggle for tweaking with mapping/no-mapping (i.e. 'in place editing' toggle) */
						// for now, use pin icon to symbolise this
						if (adt->flag & ADT_NLA_EDIT_NOMAP)
							UI_icon_draw((float)(v2d->cur.xmax-offset), ydatac, ICON_PINNED);
						else
							UI_icon_draw((float)(v2d->cur.xmax-offset), ydatac, ICON_UNPINNED);
						
						fdrawline((float)(v2d->cur.xmax-offset), yminc, 
								  (float)(v2d->cur.xmax-offset), ymaxc);
						offset += 16;
						
						/* 'tweaking action' indicator - not a button */
						UI_icon_draw((float)(v2d->cur.xmax-offset), ydatac, ICON_EDIT); 
					}
					else {
						/* XXX firstly draw a little rect to help identify that it's different from the toggles */
						glBegin(GL_LINE_LOOP);
							glVertex2f((float)v2d->cur.xmax-offset-1, y-7);
							glVertex2f((float)v2d->cur.xmax-offset-1, y+9);
							glVertex2f((float)v2d->cur.xmax-1, y+9);
							glVertex2f((float)v2d->cur.xmax-1, y-7);
						glEnd(); // GL_LINES
						
						/* 'push down' icon for normal active-actions */
						UI_icon_draw((float)v2d->cur.xmax-offset, ydatac, ICON_FREEZE);
					}
				}
				
				glDisable(GL_BLEND);
			}
		}
		
		/* adjust y-position for next one */
		y -= NLACHANNEL_STEP(snla);
	}
}

void draw_nla_channel_list (bContext *C, bAnimContext *ac, ARegion *ar)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	SpaceNla *snla = (SpaceNla *)ac->sl;
	View2D *v2d= &ar->v2d;
	float y= 0.0f;
	size_t items;
	int height;
	
	/* build list of channels to draw */
	filter= (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	items= ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* Update max-extent of channels here (taking into account scrollers):
	 * 	- this is done to allow the channel list to be scrollable, but must be done here
	 * 	  to avoid regenerating the list again and/or also because channels list is drawn first
	 *	- offset of NLACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for 
	 *	  start of list offset, and the second is as a correction for the scrollers.
	 */
	height= ((items*NLACHANNEL_STEP(snla)) + (NLACHANNEL_HEIGHT(snla)*2));
	/* don't use totrect set, as the width stays the same 
	 * (NOTE: this is ok here, the configuration is pretty straightforward) 
	 */
	v2d->tot.ymin= (float)(-height);
	/* need to do a view-sync here, so that the keys area doesn't jump around (it must copy this) */
	UI_view2d_sync(NULL, ac->sa, v2d, V2D_LOCK_COPY);
	
	/* draw channels */
	{	/* first pass: backdrops + oldstyle drawing */
		y= (float)(-NLACHANNEL_HEIGHT(snla));
		
		draw_nla_channel_list_gl(ac, &anim_data, v2d, y);
	}
	{	/* second pass: UI widgets */
		uiBlock *block= uiBeginBlock(C, ar, __func__, UI_EMBOSS);
		size_t channel_index = 0;
		
		y= (float)(-NLACHANNEL_HEIGHT(snla));
		
		/* set blending again, as may not be set in previous step */
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		
		/* loop through channels, and set up drawing depending on their type  */	
		for (ale= anim_data.first; ale; ale= ale->next) {
			const float yminc= (float)(y - NLACHANNEL_HEIGHT_HALF(snla));
			const float ymaxc= (float)(y + NLACHANNEL_HEIGHT_HALF(snla));
			
			/* check if visible */
			if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
				 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
			{
				/* draw all channels using standard channel-drawing API */
				ANIM_channel_draw_widgets(C, ac, ale, block, yminc, ymaxc, channel_index);
			}
			
			/* adjust y-position for next one */
			y -= NLACHANNEL_STEP(snla);
			channel_index++;
		}
		
		uiEndBlock(C, block);
		uiDrawBlock(C, block);
		
		glDisable(GL_BLEND);
	}
	
	/* free tempolary channels */
	BLI_freelistN(&anim_data);
}

/* *********************************************** */
