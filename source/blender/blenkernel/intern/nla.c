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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "nla_private.h"



/* *************************************************** */
/* Data Management */

/* Freeing ------------------------------------------- */

/* Remove the given NLA strip from the NLA track it occupies, free the strip's data,
 * and the strip itself. 
 */
void free_nlastrip (ListBase *strips, NlaStrip *strip)
{
	NlaStrip *cs, *csn;
	
	/* sanity checks */
	if (strip == NULL)
		return;
		
	/* free child-strips */
	for (cs= strip->strips.first; cs; cs= csn) {
		csn= cs->next;
		free_nlastrip(&strip->strips, cs);
	}
		
	/* remove reference to action */
	if (strip->act)
		strip->act->id.us--;
		
	/* free remapping info */
	//if (strip->remap)
	//	BKE_animremap_free();
	
	/* free own F-Curves */
	free_fcurves(&strip->fcurves);
	
	/* free own F-Modifiers */
	free_fmodifiers(&strip->modifiers);
	
	/* free the strip itself */
	if (strips)
		BLI_freelinkN(strips, strip);
	else
		MEM_freeN(strip);
}

/* Remove the given NLA track from the set of NLA tracks, free the track's data,
 * and the track itself.
 */
void free_nlatrack (ListBase *tracks, NlaTrack *nlt)
{
	NlaStrip *strip, *stripn;
	
	/* sanity checks */
	if (nlt == NULL)
		return;
		
	/* free strips */
	for (strip= nlt->strips.first; strip; strip= stripn) {
		stripn= strip->next;
		free_nlastrip(&nlt->strips, strip);
	}
	
	/* free NLA track itself now */
	if (tracks)
		BLI_freelinkN(tracks, nlt);
	else
		MEM_freeN(nlt);
}

/* Free the elements of type NLA Tracks provided in the given list, but do not free
 * the list itself since that is not free-standing
 */
void free_nladata (ListBase *tracks)
{
	NlaTrack *nlt, *nltn;
	
	/* sanity checks */
	if ELEM(NULL, tracks, tracks->first)
		return;
		
	/* free tracks one by one */
	for (nlt= tracks->first; nlt; nlt= nltn) {
		nltn= nlt->next;
		free_nlatrack(tracks, nlt);
	}
	
	/* clear the list's pointers to be safe */
	tracks->first= tracks->last= NULL;
}

/* Copying ------------------------------------------- */

/* Copy NLA strip */
NlaStrip *copy_nlastrip (NlaStrip *strip)
{
	NlaStrip *strip_d;
	NlaStrip *cs, *cs_d;
	
	/* sanity check */
	if (strip == NULL)
		return NULL;
		
	/* make a copy */
	strip_d= MEM_dupallocN(strip);
	strip_d->next= strip_d->prev= NULL;
	
	/* increase user-count of action */
	if (strip_d->act)
		strip_d->act->id.us++;
		
	/* copy F-Curves and modifiers */
	copy_fcurves(&strip_d->fcurves, &strip->fcurves);
	copy_fmodifiers(&strip_d->modifiers, &strip->modifiers);
	
	/* make a copy of all the child-strips, one at a time */
	strip_d->strips.first= strip_d->strips.last= NULL;
	
	for (cs= strip->strips.first; cs; cs= cs->next) {
		cs_d= copy_nlastrip(cs);
		BLI_addtail(&strip_d->strips, cs_d);
	}
	
	/* return the strip */
	return strip_d;
}

/* Copy NLA Track */
NlaTrack *copy_nlatrack (NlaTrack *nlt)
{
	NlaStrip *strip, *strip_d;
	NlaTrack *nlt_d;
	
	/* sanity check */
	if (nlt == NULL)
		return NULL;
		
	/* make a copy */
	nlt_d= MEM_dupallocN(nlt);
	nlt_d->next= nlt_d->prev= NULL;
	
	/* make a copy of all the strips, one at a time */
	nlt_d->strips.first= nlt_d->strips.last= NULL;
	
	for (strip= nlt->strips.first; strip; strip= strip->next) {
		strip_d= copy_nlastrip(strip);
		BLI_addtail(&nlt_d->strips, strip_d);
	}
	
	/* return the copy */
	return nlt_d;
}

/* Copy all NLA data */
void copy_nladata (ListBase *dst, ListBase *src)
{
	NlaTrack *nlt, *nlt_d;
	
	/* sanity checks */
	if ELEM(NULL, dst, src)
		return;
		
	/* clear out the destination list first for precautions... */
	dst->first= dst->last= NULL;
		
	/* copy each NLA-track, one at a time */
	for (nlt= src->first; nlt; nlt= nlt->next) {
		/* make a copy, and add the copy to the destination list */
		nlt_d= copy_nlatrack(nlt);
		BLI_addtail(dst, nlt_d);
	}
}

/* Adding ------------------------------------------- */

/* Add a NLA Track to the given AnimData 
 *	- prev: NLA-Track to add the new one after
 */
NlaTrack *add_nlatrack (AnimData *adt, NlaTrack *prev)
{
	NlaTrack *nlt;
	
	/* sanity checks */
	if (adt == NULL)
		return NULL;
		
	/* allocate new track */
	nlt= MEM_callocN(sizeof(NlaTrack), "NlaTrack");
	
	/* set settings requiring the track to not be part of the stack yet */
	nlt->flag = NLATRACK_SELECTED;
	nlt->index= BLI_countlist(&adt->nla_tracks);
	
	/* add track to stack, and make it the active one */
	if (prev)
		BLI_insertlinkafter(&adt->nla_tracks, prev, nlt);
	else
		BLI_addtail(&adt->nla_tracks, nlt);
	BKE_nlatrack_set_active(&adt->nla_tracks, nlt);
	
	/* must have unique name, but we need to seed this */
	strcpy(nlt->name, "NlaTrack");
	BLI_uniquename(&adt->nla_tracks, nlt, "NlaTrack", '.', offsetof(NlaTrack, name), sizeof(nlt->name));
	
	/* return the new track */
	return nlt;
}

/* Add a NLA Strip referencing the given Action */
NlaStrip *add_nlastrip (bAction *act)
{
	NlaStrip *strip;
	
	/* sanity checks */
	if (act == NULL)
		return NULL;
		
	/* allocate new strip */
	strip= MEM_callocN(sizeof(NlaStrip), "NlaStrip");
	
	/* generic settings 
	 *	- selected flag to highlight this to the user
	 *	- auto-blends to ensure that blend in/out values are automatically 
	 *	  determined by overlaps of strips
	 *	- (XXX) synchronisation of strip-length in accordance with changes to action-length
	 *	  is not done though, since this should only really happens in editmode for strips now
	 *	  though this decision is still subject to further review...
	 */
	strip->flag = NLASTRIP_FLAG_SELECT|NLASTRIP_FLAG_AUTO_BLENDS;
	
	/* assign the action reference */
	strip->act= act;
	id_us_plus(&act->id);
	
	/* determine initial range 
	 *	- strip length cannot be 0... ever...
	 */
	calc_action_range(strip->act, &strip->actstart, &strip->actend, 0);
	
	strip->start = strip->actstart;
	strip->end = (IS_EQ(strip->actstart, strip->actend)) ?  (strip->actstart + 1.0f): (strip->actend);
	
	/* strip should be referenced as-is */
	strip->scale= 1.0f;
	strip->repeat = 1.0f;
	
	/* return the new strip */
	return strip;
}

/* Add new NLA-strip to the top of the NLA stack - i.e. into the last track if space, or a new one otherwise */
NlaStrip *add_nlastrip_to_stack (AnimData *adt, bAction *act)
{
	NlaStrip *strip;
	NlaTrack *nlt;
	
	/* sanity checks */
	if ELEM(NULL, adt, act)
		return NULL;
	
	/* create a new NLA strip */
	strip= add_nlastrip(act);
	if (strip == NULL)
		return NULL;
	
	/* firstly try adding strip to last track, but if that fails, add to a new track */
	if (BKE_nlatrack_add_strip(adt->nla_tracks.last, strip) == 0) {
		/* trying to add to the last track failed (no track or no space), 
		 * so add a new track to the stack, and add to that...
		 */
		nlt= add_nlatrack(adt, NULL);
		BKE_nlatrack_add_strip(nlt, strip);
	}
	
	/* automatically name it too */
	BKE_nlastrip_validate_name(adt, strip);
	
	/* returns the strip added */
	return strip;
}

/* *************************************************** */
/* NLA Evaluation <-> Editing Stuff */

/* Strip Mapping ------------------------------------- */

/* non clipped mapping for strip-time <-> global time (for Action-Clips)
 *	invert = convert action-strip time to global time 
 */
static float nlastrip_get_frame_actionclip (NlaStrip *strip, float cframe, short mode)
{
	float actlength, repeat, scale;
	
	/* get number of repeats */
	if (IS_EQ(strip->repeat, 0.0f)) strip->repeat = 1.0f;
	repeat = strip->repeat;
	
	/* scaling */
	if (IS_EQ(strip->scale, 0.0f)) strip->scale= 1.0f;
	scale = (float)fabs(strip->scale); /* scale must be positive - we've got a special flag for reversing */
	
	/* length of referenced action */
	actlength = strip->actend - strip->actstart;
	if (IS_EQ(actlength, 0.0f)) actlength = 1.0f;
	
	/* reversed = play strip backwards */
	if (strip->flag & NLASTRIP_FLAG_REVERSE) {
		// FIXME: this won't work right with Graph Editor?
		if (mode == NLATIME_CONVERT_MAP) {
			return strip->end - scale*(cframe - strip->actstart);
		}
		else if (mode == NLATIME_CONVERT_UNMAP) {
			return strip->actend - (strip->end - cframe) / scale;	
		}
		else /* if (mode == NLATIME_CONVERT_EVAL) */{
			if (IS_EQ(cframe, strip->end) && IS_EQ(strip->repeat, ((int)strip->repeat))) {
				/* this case prevents the motion snapping back to the first frame at the end of the strip 
				 * by catching the case where repeats is a whole number, which means that the end of the strip
				 * could also be interpreted as the end of the start of a repeat
				 */
				return strip->actstart;
			}
			else {
				/* - the 'fmod(..., actlength*scale)' is needed to get the repeats working
				 * - the '/ scale' is needed to ensure that scaling influences the timing within the repeat
				 */
				return strip->actend - fmod(cframe - strip->start, actlength*scale) / scale; 
			}
		}
	}
	else {
		if (mode == NLATIME_CONVERT_MAP) {
			return strip->start + scale*(cframe - strip->actstart);
		}
		else if (mode == NLATIME_CONVERT_UNMAP) {
			return strip->actstart + (cframe - strip->start) / scale;
		}
		else /* if (mode == NLATIME_CONVERT_EVAL) */{
			if (IS_EQ(cframe, strip->end) && IS_EQ(strip->repeat, ((int)strip->repeat))) {
				/* this case prevents the motion snapping back to the first frame at the end of the strip 
				 * by catching the case where repeats is a whole number, which means that the end of the strip
				 * could also be interpreted as the end of the start of a repeat
				 */
				return strip->actend;
			}
			else {
				/* - the 'fmod(..., actlength*scale)' is needed to get the repeats working
				 * - the '/ scale' is needed to ensure that scaling influences the timing within the repeat
				 */
				return strip->actstart + fmod(cframe - strip->start, actlength*scale) / scale; 
			}
		}
	}
}

/* non clipped mapping for strip-time <-> global time (for Transitions)
 *	invert = convert action-strip time to global time 
 */
static float nlastrip_get_frame_transition (NlaStrip *strip, float cframe, short mode)
{
	float length;
	
	/* length of strip */
	length= strip->end - strip->start;
	
	/* reversed = play strip backwards */
	if (strip->flag & NLASTRIP_FLAG_REVERSE) {
		if (mode == NLATIME_CONVERT_MAP)
			return strip->end - (length * cframe);
		else
			return (strip->end - cframe) / length;
	}
	else {
		if (mode == NLATIME_CONVERT_MAP)
			return (length * cframe) + strip->start;
		else
			return (cframe - strip->start) / length;
	}
}

/* non clipped mapping for strip-time <-> global time
 * 	mode = eNlaTime_ConvertModes[] -> NLATIME_CONVERT_*
 *
 * only secure for 'internal' (i.e. within AnimSys evaluation) operations,
 * but should not be directly relied on for stuff which interacts with editors
 */
float nlastrip_get_frame (NlaStrip *strip, float cframe, short mode)
{
	switch (strip->type) {
		case NLASTRIP_TYPE_META: /* meta - for now, does the same as transition (is really just an empty container) */
		case NLASTRIP_TYPE_TRANSITION: /* transition */
			return nlastrip_get_frame_transition(strip, cframe, mode);
		
		case NLASTRIP_TYPE_CLIP: /* action-clip (default) */
		default:
			return nlastrip_get_frame_actionclip(strip, cframe, mode);
	}	
}


/* Non clipped mapping for strip-time <-> global time
 *	mode = eNlaTime_ConvertModesp[] -> NLATIME_CONVERT_*
 *
 * Public API method - perform this mapping using the given AnimData block
 * and perform any necessary sanity checks on the value
 */
float BKE_nla_tweakedit_remap (AnimData *adt, float cframe, short mode)
{
	NlaStrip *strip;
	
	/* sanity checks 
	 *	- obviously we've got to have some starting data
	 *	- when not in tweakmode, the active Action does not have any scaling applied :)
	 *	- when in tweakmode, if the no-mapping flag is set, do not map
	 */
	if ((adt == NULL) || (adt->flag & ADT_NLA_EDIT_ON)==0 || (adt->flag & ADT_NLA_EDIT_NOMAP))
		return cframe;
		
	/* if the active-strip info has been stored already, access this, otherwise look this up
	 * and store for (very probable) future usage
	 */
	if (adt->actstrip == NULL) {
		NlaTrack *nlt= BKE_nlatrack_find_active(&adt->nla_tracks);
		adt->actstrip= BKE_nlastrip_find_active(nlt);
	}
	strip= adt->actstrip;
	
	/* sanity checks 
	 *	- in rare cases, we may not be able to find this strip for some reason (internal error)
	 *	- for now, if the user has defined a curve to control the time, this correction cannot be performed
	 *	  reliably...
	 */
	if ((strip == NULL) || (strip->flag & NLASTRIP_FLAG_USR_TIME))
		return cframe;
		
	/* perform the correction now... */
	return nlastrip_get_frame(strip, cframe, mode);
}

/* *************************************************** */
/* NLA API */

/* List of Strips ------------------------------------ */
/* (these functions are used for NLA-Tracks and also for nested/meta-strips) */

/* Check if there is any space in the given list to add the given strip */
short BKE_nlastrips_has_space (ListBase *strips, float start, float end)
{
	NlaStrip *strip;
	
	/* sanity checks */
	if ((strips == NULL) || IS_EQ(start, end))
		return 0;
	if (start > end) {
		puts("BKE_nlastrips_has_space() error... start and end arguments swapped");
		SWAP(float, start, end);
	}
	
	/* loop over NLA strips checking for any overlaps with this area... */
	for (strip= strips->first; strip; strip= strip->next) {
		/* if start frame of strip is past the target end-frame, that means that
		 * we've gone past the window we need to check for, so things are fine
		 */
		if (strip->start > end)
			return 1;
		
		/* if the end of the strip is greater than either of the boundaries, the range
		 * must fall within the extents of the strip
		 */
		if ((strip->end > start) || (strip->end > end))
			return 0;
	}
	
	/* if we are still here, we haven't encountered any overlapping strips */
	return 1;
}

/* Rearrange the strips in the track so that they are always in order 
 * (usually only needed after a strip has been moved) 
 */
void BKE_nlastrips_sort_strips (ListBase *strips)
{
	ListBase tmp = {NULL, NULL};
	NlaStrip *strip, *sstrip, *stripn;
	
	/* sanity checks */
	if ELEM(NULL, strips, strips->first)
		return;
	
	/* we simply perform insertion sort on this list, since it is assumed that per track,
	 * there are only likely to be at most 5-10 strips
	 */
	for (strip= strips->first; strip; strip= stripn) {
		short not_added = 1;
		
		stripn= strip->next;
		
		/* remove this strip from the list, and add it to the new list, searching from the end of 
		 * the list, assuming that the lists are in order 
		 */
		BLI_remlink(strips, strip);
		
		for (sstrip= tmp.last; sstrip; sstrip= sstrip->prev) {
			/* check if add after */
			if (sstrip->end <= strip->start) {
				BLI_insertlinkafter(&tmp, sstrip, strip);
				not_added= 0;
				break;
			}
		}
		
		/* add before first? */
		if (not_added)
			BLI_addhead(&tmp, strip);
	}
	
	/* reassign the start and end points of the strips */
	strips->first= tmp.first;
	strips->last= tmp.last;
}

/* Add the given NLA-Strip to the given list of strips, assuming that it 
 * isn't currently a member of another list
 */
short BKE_nlastrips_add_strip (ListBase *strips, NlaStrip *strip)
{
	NlaStrip *ns;
	short not_added = 1;
	
	/* sanity checks */
	if ELEM(NULL, strips, strip)
		return 0;
		
	/* check if any space to add */
	if (BKE_nlastrips_has_space(strips, strip->start, strip->end)==0)
		return 0;
	
	/* find the right place to add the strip to the nominated track */
	for (ns= strips->first; ns; ns= ns->next) {
		/* if current strip occurs after the new strip, add it before */
		if (ns->start > strip->end) {
			BLI_insertlinkbefore(strips, ns, strip);
			not_added= 0;
			break;
		}
	}
	if (not_added) {
		/* just add to the end of the list of the strips then... */
		BLI_addtail(strips, strip);
	}
	
	/* added... */
	return 1;
}


/* Meta-Strips ------------------------------------ */

/* Convert 'islands' (i.e. continuous string of) selected strips to be
 * contained within 'Meta-Strips' which act as strips which contain strips.
 *	temp: are the meta-strips to be created 'temporary' ones used for transforms?
 */
void BKE_nlastrips_make_metas (ListBase *strips, short temp)
{
	NlaStrip *mstrip = NULL;
	NlaStrip *strip, *stripn;
	
	/* sanity checks */
	if ELEM(NULL, strips, strips->first)
		return;
	
	/* group all continuous chains of selected strips into meta-strips */
	for (strip= strips->first; strip; strip= stripn) {
		stripn= strip->next;
		
		if (strip->flag & NLASTRIP_FLAG_SELECT) {
			/* if there is an existing meta-strip, add this strip to it, otherwise, create a new one */
			if (mstrip == NULL) {
				/* add a new meta-strip, and add it before the current strip that it will replace... */
				mstrip= MEM_callocN(sizeof(NlaStrip), "Meta-NlaStrip");
				mstrip->type = NLASTRIP_TYPE_META;
				BLI_insertlinkbefore(strips, strip, mstrip);
				
				/* set flags */
				mstrip->flag = NLASTRIP_FLAG_SELECT;
				
				/* set temp flag if appropriate (i.e. for transform-type editing) */
				if (temp)
					mstrip->flag |= NLASTRIP_FLAG_TEMP_META;
					
				/* set default repeat/scale values to prevent warnings */
				mstrip->repeat= mstrip->scale= 1.0f;
				
				/* make its start frame be set to the start frame of the current strip */
				mstrip->start= strip->start;
			}
			
			/* remove the selected strips from the track, and add to the meta */
			BLI_remlink(strips, strip);
			BLI_addtail(&mstrip->strips, strip);
			
			/* expand the meta's dimensions to include the newly added strip- i.e. its last frame */
			mstrip->end= strip->end;
		}
		else {
			/* current strip wasn't selected, so the end of 'island' of selected strips has been reached,
			 * so stop adding strips to the current meta
			 */
			mstrip= NULL;
		}
	}
}

/* Split a meta-strip into a set of normal strips */
void BKE_nlastrips_clear_metastrip (ListBase *strips, NlaStrip *strip)
{
	NlaStrip *cs, *csn;
	
	/* sanity check */
	if ELEM(NULL, strips, strip)
		return;
	
	/* move each one of the meta-strip's children before the meta-strip
	 * in the list of strips after unlinking them from the meta-strip
	 */
	for (cs= strip->strips.first; cs; cs= csn) {
		csn= cs->next;
		BLI_remlink(&strip->strips, cs);
		BLI_insertlinkbefore(strips, strip, cs);
	}
	
	/* free the meta-strip now */
	BLI_freelinkN(strips, strip);
}

/* Remove meta-strips (i.e. flatten the list of strips) from the top-level of the list of strips
 *	sel: only consider selected meta-strips, otherwise all meta-strips are removed
 *	onlyTemp: only remove the 'temporary' meta-strips used for transforms
 */
void BKE_nlastrips_clear_metas (ListBase *strips, short onlySel, short onlyTemp)
{
	NlaStrip *strip, *stripn;
	
	/* sanity checks */
	if ELEM(NULL, strips, strips->first)
		return;
	
	/* remove meta-strips fitting the criteria of the arguments */
	for (strip= strips->first; strip; strip= stripn) {
		stripn= strip->next;
		
		/* check if strip is a meta-strip */
		if (strip->type == NLASTRIP_TYPE_META) {
			/* if check if selection and 'temporary-only' considerations are met */
			if ((onlySel==0) || (strip->flag & NLASTRIP_FLAG_SELECT)) {
				if ((!onlyTemp) || (strip->flag & NLASTRIP_FLAG_TEMP_META)) {
					BKE_nlastrips_clear_metastrip(strips, strip);
				}
			}
		}
	}
}

/* Add the given NLA-Strip to the given Meta-Strip, assuming that the
 * strip isn't attached to anyy list of strips 
 */
short BKE_nlameta_add_strip (NlaStrip *mstrip, NlaStrip *strip)
{
	/* sanity checks */
	if ELEM(NULL, mstrip, strip)
		return 0;
		
	/* firstly, check if the meta-strip has space for this */
	if (BKE_nlastrips_has_space(&mstrip->strips, strip->start, strip->end) == 0)
		return 0;
		
	/* check if this would need to be added to the ends of the meta,
	 * and subsequently, if the neighbouring strips allow us enough room
	 */
	if (strip->start < mstrip->start) {
		/* check if strip to the left (if it exists) ends before the 
		 * start of the strip we're trying to add 
		 */
		if ((mstrip->prev == NULL) || (mstrip->prev->end <= strip->start)) {
			/* add strip to start of meta's list, and expand dimensions */
			BLI_addhead(&mstrip->strips, strip);
			mstrip->start= strip->start;
			
			return 1;
		}
		else /* failed... no room before */
			return 0;
	}
	else if (strip->end > mstrip->end) {
		/* check if strip to the right (if it exists) starts before the 
		 * end of the strip we're trying to add 
		 */
		if ((mstrip->next == NULL) || (mstrip->next->start >= strip->end)) {
			/* add strip to end of meta's list, and expand dimensions */
			BLI_addtail(&mstrip->strips, strip);
			mstrip->end= strip->end;
			
			return 1;
		}
		else /* failed... no room after */
			return 0;
	}
	else {
		/* just try to add to the meta-strip (no dimension changes needed) */
		return BKE_nlastrips_add_strip(&mstrip->strips, strip);
	}
}

/* Adjust the settings of NLA-Strips contained within a Meta-Strip (recursively), 
 * until the Meta-Strips children all fit within the Meta-Strip's new dimensions
 */
void BKE_nlameta_flush_transforms (NlaStrip *mstrip) 
{
	NlaStrip *strip;
	float oStart, oEnd, offset;
	float oLen, nLen;
	short scaleChanged= 0;
	
	/* sanity checks 
	 *	- strip must exist
	 *	- strip must be a meta-strip with some contents
	 */
	if ELEM(NULL, mstrip, mstrip->strips.first)
		return;
	if (mstrip->type != NLASTRIP_TYPE_META)
		return;
		
	/* get the original start/end points, and calculate the start-frame offset
	 *	- these are simply the start/end frames of the child strips, 
	 *	  since we assume they weren't transformed yet
	 */
	oStart= ((NlaStrip *)mstrip->strips.first)->start;
	oEnd= ((NlaStrip *)mstrip->strips.last)->end;
	offset= mstrip->start - oStart;
	
	/* optimisation:
	 * don't flush if nothing changed yet
	 *	TODO: maybe we need a flag to say always flush?
	 */
	if (IS_EQ(oStart, mstrip->start) && IS_EQ(oEnd, mstrip->end))
		return;
	
	/* check if scale changed */
	oLen = oEnd - oStart;
	nLen = mstrip->end - mstrip->start;
	if (IS_EQ(nLen, oLen) == 0)
		scaleChanged= 1;
	
	/* for each child-strip, calculate new start/end points based on this new info */
	for (strip= mstrip->strips.first; strip; strip= strip->next) {
		if (scaleChanged) {
			PointerRNA ptr;
			float p1, p2, nStart, nEnd;
			
			/* compute positions of endpoints relative to old extents of strip */
			p1= (strip->start - oStart) / oLen;
			p2= (strip->end - oStart) / oLen;
			
			/* compute the new strip endpoints using the proportions */
			nStart= (p1 * nLen) + mstrip->start;
			nEnd= (p2 * nLen) + mstrip->start;
			
			/* firstly, apply the new positions manually, then apply using RNA 
			 *	- first time is to make sure no truncation errors from one endpoint not being 
			 *	  set yet occur
			 *	- second time is to make sure scale is computed properly...
			 */
			strip->start= nStart;
			strip->end= nEnd;
			
			RNA_pointer_create(NULL, &RNA_NlaStrip, strip, &ptr);
			RNA_float_set(&ptr, "frame_start", nStart);
			RNA_float_set(&ptr, "frame_end", nEnd);
		}
		else {
			/* just apply the changes in offset to both ends of the strip */
			strip->start += offset;
			strip->end += offset;
		}
		
		/* finally, make sure the strip's children (if it is a meta-itself), get updated */
		BKE_nlameta_flush_transforms(strip);
	}
}

/* NLA-Tracks ---------------------------------------- */

/* Find the active NLA-track for the given stack */
NlaTrack *BKE_nlatrack_find_active (ListBase *tracks)
{
	NlaTrack *nlt;
	
	/* sanity check */
	if ELEM(NULL, tracks, tracks->first)
		return NULL;
		
	/* try to find the first active track */
	for (nlt= tracks->first; nlt; nlt= nlt->next) {
		if (nlt->flag & NLATRACK_ACTIVE)
			return nlt;
	}
	
	/* none found */
	return NULL;
}

/* Toggle the 'solo' setting for the given NLA-track, making sure that it is the only one
 * that has this status in its AnimData block.
 */
void BKE_nlatrack_solo_toggle (AnimData *adt, NlaTrack *nlt)
{
	NlaTrack *nt;
	
	/* sanity check */
	if ELEM(NULL, adt, adt->nla_tracks.first)
		return;
		
	/* firstly, make sure 'solo' flag for all tracks is disabled */
	for (nt= adt->nla_tracks.first; nt; nt= nt->next) {
		if (nt != nlt)
			nt->flag &= ~NLATRACK_SOLO;
	}
		
	/* now, enable 'solo' for the given track if appropriate */
	if (nlt) {
		/* toggle solo status */
		nlt->flag ^= NLATRACK_SOLO;
		
		/* set or clear solo-status on AnimData */
		if (nlt->flag & NLATRACK_SOLO)
			adt->flag |= ADT_NLA_SOLO_TRACK;
		else
			adt->flag &= ~ADT_NLA_SOLO_TRACK;
	}
	else
		adt->flag &= ~ADT_NLA_SOLO_TRACK;
}

/* Make the given NLA-track the active one for the given stack. If no track is provided, 
 * this function can be used to simply deactivate all the NLA tracks in the given stack too.
 */
void BKE_nlatrack_set_active (ListBase *tracks, NlaTrack *nlt_a)
{
	NlaTrack *nlt;
	
	/* sanity check */
	if ELEM(NULL, tracks, tracks->first)
		return;
	
	/* deactive all the rest */
	for (nlt= tracks->first; nlt; nlt= nlt->next) 
		nlt->flag &= ~NLATRACK_ACTIVE;
		
	/* set the given one as the active one */
	if (nlt_a)
		nlt_a->flag |= NLATRACK_ACTIVE;
}

/* Check if there is any space in the given track to add a strip of the given length */
short BKE_nlatrack_has_space (NlaTrack *nlt, float start, float end)
{
	/* sanity checks */
	if ((nlt == NULL) || IS_EQ(start, end))
		return 0;
	if (start > end) {
		puts("BKE_nlatrack_has_space() error... start and end arguments swapped");
		SWAP(float, start, end);
	}
	
	/* check if there's any space left in the track for a strip of the given length */
	return BKE_nlastrips_has_space(&nlt->strips, start, end);
}

/* Rearrange the strips in the track so that they are always in order 
 * (usually only needed after a strip has been moved) 
 */
void BKE_nlatrack_sort_strips (NlaTrack *nlt)
{
	/* sanity checks */
	if ELEM(NULL, nlt, nlt->strips.first)
		return;
	
	/* sort the strips with a more generic function */
	BKE_nlastrips_sort_strips(&nlt->strips);
}

/* Add the given NLA-Strip to the given NLA-Track, assuming that it 
 * isn't currently attached to another one 
 */
short BKE_nlatrack_add_strip (NlaTrack *nlt, NlaStrip *strip)
{
	/* sanity checks */
	if ELEM(NULL, nlt, strip)
		return 0;
		
	/* try to add the strip to the track using a more generic function */
	return BKE_nlastrips_add_strip(&nlt->strips, strip);
}

/* Get the extents of the given NLA-Track including gaps between strips,
 * returning whether this succeeded or not
 */
short BKE_nlatrack_get_bounds (NlaTrack *nlt, float bounds[2])
{
	NlaStrip *strip;
	
	/* initialise bounds */
	if (bounds)
		bounds[0] = bounds[1] = 0.0f;
	else
		return 0;
	
	/* sanity checks */
	if ELEM(NULL, nlt, nlt->strips.first)
		return 0;
		
	/* lower bound is first strip's start frame */
	strip = nlt->strips.first;
	bounds[0] = strip->start;
	
	/* upper bound is last strip's end frame */
	strip = nlt->strips.last;
	bounds[1] = strip->end;
	
	/* done */
	return 1;
}

/* NLA Strips -------------------------------------- */

/* Find the active NLA-strip within the given track */
NlaStrip *BKE_nlastrip_find_active (NlaTrack *nlt)
{
	NlaStrip *strip;
	
	/* sanity check */
	if ELEM(NULL, nlt, nlt->strips.first)
		return NULL;
		
	/* try to find the first active strip */
	for (strip= nlt->strips.first; strip; strip= strip->next) {
		if (strip->flag & NLASTRIP_FLAG_ACTIVE)
			return strip;
	}
	
	/* none found */
	return NULL;
}

/* Make the given NLA-Strip the active one within the given block */
void BKE_nlastrip_set_active (AnimData *adt, NlaStrip *strip)
{
	NlaTrack *nlt;
	NlaStrip *nls;
	
	/* sanity checks */
	if (adt == NULL)
		return;
	
	/* loop over tracks, deactivating*/
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) {
		for (nls= nlt->strips.first; nls; nls= nls->next)  {
			if (nls != strip)
				nls->flag &= ~NLASTRIP_FLAG_ACTIVE;
			else
				nls->flag |= NLASTRIP_FLAG_ACTIVE;
		}
	}
}


/* Does the given NLA-strip fall within the given bounds (times)? */
short BKE_nlastrip_within_bounds (NlaStrip *strip, float min, float max)
{
	const float stripLen= (strip) ? strip->end - strip->start : 0.0f;
	const float boundsLen= (float)fabs(max - min);
	
	/* sanity checks */
	if ((strip == NULL) || IS_EQ(stripLen, 0.0f) || IS_EQ(boundsLen, 0.0f))
		return 0;
	
	/* only ok if at least part of the strip is within the bounding window
	 *	- first 2 cases cover when the strip length is less than the bounding area
	 *	- second 2 cases cover when the strip length is greater than the bounding area
	 */
	if ( (stripLen < boundsLen) && 
		 !(IN_RANGE(strip->start, min, max) ||
		   IN_RANGE(strip->end, min, max)) )
	{
		return 0;
	}
	if ( (stripLen > boundsLen) && 
		 !(IN_RANGE(min, strip->start, strip->end) ||
		   IN_RANGE(max, strip->start, strip->end)) )
	{
		return 0;
	}
	
	/* should be ok! */
	return 1;
}

/* Recalculate the start and end frames for the current strip, after changing
 * the extents of the action or the mapping (repeats or scale factor) info
 */
void BKE_nlastrip_recalculate_bounds (NlaStrip *strip)
{
	float actlen, mapping;
	
	/* sanity checks
	 *	- must have a strip
	 *	- can only be done for action clips
	 */
	if ((strip == NULL) || (strip->type != NLASTRIP_TYPE_CLIP))
		return;
		
	/* calculate new length factors */
	actlen= strip->actend - strip->actstart;
	if (IS_EQ(actlen, 0.0f)) actlen= 1.0f;
	
	mapping= strip->scale * strip->repeat;
	
	/* adjust endpoint of strip in response to this */
	if (IS_EQ(mapping, 0.0f) == 0)
		strip->end = (actlen * mapping) + strip->start;
}

/* Is the given NLA-strip the first one to occur for the given AnimData block */
// TODO: make this an api method if necesary, but need to add prefix first
static short nlastrip_is_first (AnimData *adt, NlaStrip *strip)
{
	NlaTrack *nlt;
	NlaStrip *ns;
	
	/* sanity checks */
	if ELEM(NULL, adt, strip)
		return 0;
		
	/* check if strip has any strips before it */
	if (strip->prev)
		return 0;
		
	/* check other tracks to see if they have a strip that's earlier */
	// TODO: or should we check that the strip's track is also the first?
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) {
		/* only check the first strip, assuming that they're all in order */
		ns= nlt->strips.first;
		if (ns) {
			if (ns->start < strip->start)
				return 0;
		}
	}	
	
	/* should be first now */
	return 1;
}

/* Animated Strips ------------------------------------------- */

/* Check if the given NLA-Track has any strips with own F-Curves */
short BKE_nlatrack_has_animated_strips (NlaTrack *nlt)
{
	NlaStrip *strip;
	
	/* sanity checks */
	if ELEM(NULL, nlt, nlt->strips.first)
		return 0;
		
	/* check each strip for F-Curves only (don't care about whether the flags are set) */
	for (strip= nlt->strips.first; strip; strip= strip->next) {
		if (strip->fcurves.first)
			return 1;
	}
	
	/* none found */
	return 0;
}

/* Check if given NLA-Tracks have any strips with own F-Curves */
short BKE_nlatracks_have_animated_strips (ListBase *tracks)
{
	NlaTrack *nlt;
	
	/* sanity checks */
	if ELEM(NULL, tracks, tracks->first)
		return 0;
		
	/* check each track, stopping on the first hit */
	for (nlt= tracks->first; nlt; nlt= nlt->next) {
		if (BKE_nlatrack_has_animated_strips(nlt))
			return 1;
	}
	
	/* none found */
	return 0;
}

/* Validate the NLA-Strips 'control' F-Curves based on the flags set*/
void BKE_nlastrip_validate_fcurves (NlaStrip *strip) 
{
	FCurve *fcu;
	
	/* sanity checks */
	if (strip == NULL)
		return;
	
	/* if controlling influence... */
	if (strip->flag & NLASTRIP_FLAG_USR_INFLUENCE) {
		/* try to get F-Curve */
		fcu= list_find_fcurve(&strip->fcurves, "influence", 0);
		
		/* add one if not found */
		if (fcu == NULL) {
			/* make new F-Curve */
			fcu= MEM_callocN(sizeof(FCurve), "NlaStrip FCurve");
			BLI_addtail(&strip->fcurves, fcu);
			
			/* set default flags */
			fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
			
			/* store path - make copy, and store that */
			fcu->rna_path= BLI_strdupn("influence", 9);
			
			// TODO: insert a few keyframes to ensure default behaviour?
		}
	}
	
	/* if controlling time... */
	if (strip->flag & NLASTRIP_FLAG_USR_TIME) {
		/* try to get F-Curve */
		fcu= list_find_fcurve(&strip->fcurves, "strip_time", 0);
		
		/* add one if not found */
		if (fcu == NULL) {
			/* make new F-Curve */
			fcu= MEM_callocN(sizeof(FCurve), "NlaStrip FCurve");
			BLI_addtail(&strip->fcurves, fcu);
			
			/* set default flags */
			fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
			
			/* store path - make copy, and store that */
			fcu->rna_path= BLI_strdupn("strip_time", 10);
			
			// TODO: insert a few keyframes to ensure default behaviour?
		}
	}
}

/* Sanity Validation ------------------------------------ */

/* Find (and set) a unique name for a strip from the whole AnimData block 
 * Uses a similar method to the BLI method, but is implemented differently
 * as we need to ensure that the name is unique over several lists of tracks,
 * not just a single track.
 */
void BKE_nlastrip_validate_name (AnimData *adt, NlaStrip *strip)
{
	GHash *gh;
	NlaStrip *tstrip;
	NlaTrack *nlt;
	
	/* sanity checks */
	if ELEM(NULL, adt, strip)
		return;
		
	/* give strip a default name if none already */
	if (strip->name[0]==0) {
		switch (strip->type) {
			case NLASTRIP_TYPE_CLIP: /* act-clip */
				sprintf(strip->name, "Act: %s", (strip->act)?(strip->act->id.name+2):("<None>"));
				break;
			case NLASTRIP_TYPE_TRANSITION: /* transition */
				sprintf(strip->name, "Transition");
				break;
			case NLASTRIP_TYPE_META: /* meta */
				sprintf(strip->name, "Meta");
				break;
			default:
				sprintf(strip->name, "NLA Strip");
				break;
		}
	}
	
	/* build a hash-table of all the strips in the tracks 
	 *	- this is easier than iterating over all the tracks+strips hierarchy everytime
	 *	  (and probably faster)
	 */
	gh= BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp, "nlastrip_validate_name gh");
	
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) {
		for (tstrip= nlt->strips.first; tstrip; tstrip= tstrip->next) {
			/* don't add the strip of interest */
			if (tstrip == strip) 
				continue;
			
			/* use the name of the strip as the key, and the strip as the value, since we're mostly interested in the keys */
			BLI_ghash_insert(gh, tstrip->name, tstrip);
		}
	}
	
	/* if the hash-table has a match for this name, try other names... 
	 *	- in an extreme case, it might not be able to find a name, but then everything else in Blender would fail too :)
	 */
	if (BLI_ghash_haskey(gh, strip->name)) {
		char tempname[128];
		int	number = 1;
		char *dot;
		
		/* Strip off the suffix */
		dot = strchr(strip->name, '.');
		if (dot) *dot=0;
		
		/* Try different possibilities */
		for (number = 1; number <= 999; number++) {
			/* assemble alternative name */
			BLI_snprintf(tempname, 128, "%s%c%03d", strip->name, ".", number);
			
			/* if hash doesn't have this, set it */
			if (BLI_ghash_haskey(gh, tempname) == 0) {
				BLI_strncpy(strip->name, tempname, sizeof(strip->name));
				break;
			}
		}
	}
	
	/* free the hash... */
	BLI_ghash_free(gh, NULL, NULL);
}

/* ---- */

/* Get strips which overlap the given one at the start/end of its range 
 *	- strip: strip that we're finding overlaps for
 *	- track: nla-track that the overlapping strips should be found from
 *	- start, end: frames for the offending endpoints
 */
static void nlastrip_get_endpoint_overlaps (NlaStrip *strip, NlaTrack *track, float **start, float **end)
{
	NlaStrip *nls;
	
	/* find strips that overlap over the start/end of the given strip,
	 * but which don't cover the entire length 
	 */
	// TODO: this scheme could get quite slow for doing this on many strips...
	for (nls= track->strips.first; nls; nls= nls->next) {
		/* check if strip overlaps (extends over or exactly on) the entire range of the strip we're validating */
		if ((nls->start <= strip->start) && (nls->end >= strip->end)) {
			*start= NULL;
			*end= NULL;
			return;
		}
		
		/* check if strip doesn't even occur anywhere near... */
		if (nls->end < strip->start)
			continue; /* skip checking this strip... not worthy of mention */
		if (nls->start > strip->end)
			return; /* the range we're after has already passed */
			
		/* if this strip is not part of an island of continuous strips, it can be used
		 *	- this check needs to be done for each end of the strip we try and use...
		 */
		if ((nls->next == NULL) || IS_EQ(nls->next->start, nls->end)==0) {
			if ((nls->end > strip->start) && (nls->end < strip->end))
				*start= &nls->end;
		}
		if ((nls->prev == NULL) || IS_EQ(nls->prev->end, nls->start)==0) {
			if ((nls->start < strip->end) && (nls->start > strip->start))
				*end= &nls->start;
		}
	}
}

/* Determine auto-blending for the given strip */
void BKE_nlastrip_validate_autoblends (NlaTrack *nlt, NlaStrip *nls)
{
	float *ps=NULL, *pe=NULL;
	float *ns=NULL, *ne=NULL;
	
	/* sanity checks */
	if ELEM(NULL, nls, nlt)
		return;
	if ((nlt->prev == NULL) && (nlt->next == NULL))
		return;
	if ((nls->flag & NLASTRIP_FLAG_AUTO_BLENDS)==0)
		return;
	
	/* get test ranges */
	if (nlt->prev)
		nlastrip_get_endpoint_overlaps(nls, nlt->prev, &ps, &pe);
	if (nlt->next)
		nlastrip_get_endpoint_overlaps(nls, nlt->next, &ns, &ne);
		
	/* set overlaps for this strip 
	 *	- don't use the values obtained though if the end in question 
	 *	  is directly followed/preceeded by another strip, forming an 
	 *	  'island' of continuous strips
	 */
	if ( (ps || ns) && ((nls->prev == NULL) || IS_EQ(nls->prev->end, nls->start)==0) ) 
	{
		/* start overlaps - pick the largest overlap */
		if ( ((ps && ns) && (*ps > *ns)) || (ps) )
			nls->blendin= *ps - nls->start;
		else
			nls->blendin= *ns - nls->start;
	}
	else /* no overlap allowed/needed */
		nls->blendin= 0.0f;
		
	if ( (pe || ne) && ((nls->next == NULL) || IS_EQ(nls->next->start, nls->end)==0) ) 
	{
		/* end overlaps - pick the largest overlap */
		if ( ((pe && ne) && (*pe > *ne)) || (pe) )
			nls->blendout= nls->end - *pe;
		else
			nls->blendout= nls->end - *ne;
	}
	else /* no overlap allowed/needed */
		nls->blendout= 0.0f;
}

/* Ensure that auto-blending and other settings are set correctly */
void BKE_nla_validate_state (AnimData *adt)
{
	NlaStrip *strip, *fstrip=NULL;
	NlaTrack *nlt;
	
	/* sanity checks */
	if ELEM(NULL, adt, adt->nla_tracks.first)
		return;
		
	/* adjust blending values for auto-blending, and also do an initial pass to find the earliest strip */
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) {
		for (strip= nlt->strips.first; strip; strip= strip->next) {
			/* auto-blending first */
			BKE_nlastrip_validate_autoblends(nlt, strip);
			
			/* extend mode - find first strip */
			if ((fstrip == NULL) || (strip->start < fstrip->start))
				fstrip= strip;
		}
	}
	
	/* second pass over the strips to adjust the extend-mode to fix any problems */
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) {
		for (strip= nlt->strips.first; strip; strip= strip->next) {
			/* apart from 'nothing' option which user has to explicitly choose, we don't really know if 
			 * we should be overwriting the extend setting (but assume that's what the user wanted)
			 */
			// TODO: 1 solution is to tie this in with auto-blending...
			if (strip->extendmode != NLASTRIP_EXTEND_NOTHING) {
				if (strip == fstrip)
					strip->extendmode= NLASTRIP_EXTEND_HOLD;
				else
					strip->extendmode= NLASTRIP_EXTEND_HOLD_FORWARD;
			}
		}
	}
}

/* Core Tools ------------------------------------------- */

/* For the given AnimData block, add the active action to the NLA
 * stack (i.e. 'push-down' action). The UI should only allow this 
 * for normal editing only (i.e. not in editmode for some strip's action),
 * so no checks for this are performed.
 */
// TODO: maybe we should have checks for this too...
void BKE_nla_action_pushdown (AnimData *adt)
{
	NlaStrip *strip;
	
	/* sanity checks */
	// TODO: need to report the error for this
	if ELEM(NULL, adt, adt->action) 
		return;
		
	/* if the action is empty, we also shouldn't try to add to stack, 
	 * as that will cause us grief down the track
	 */
	// TODO: what about modifiers?
	if (action_has_motion(adt->action) == 0) {
		printf("BKE_nla_action_pushdown(): action has no data \n");
		return;
	}
	
	/* add a new NLA strip to the track, which references the active action */
	strip= add_nlastrip_to_stack(adt, adt->action);
	
	/* do other necessary work on strip */	
	if (strip) {
		/* clear reference to action now that we've pushed it onto the stack */
		adt->action->id.us--;
		adt->action= NULL;
		
		/* if the strip is the first one in the track it lives in, check if there
		 * are strips in any other tracks that may be before this, and set the extend
		 * mode accordingly
		 */
		if (nlastrip_is_first(adt, strip) == 0) {
			/* not first, so extend mode can only be NLASTRIP_EXTEND_HOLD_FORWARD not NLASTRIP_EXTEND_HOLD,
			 * so that it doesn't override strips in previous tracks
			 */
			// FIXME: this needs to be more automated, since user can rearrange strips
			strip->extendmode= NLASTRIP_EXTEND_HOLD_FORWARD;
		}
		
		/* make strip the active one... */
		BKE_nlastrip_set_active(adt, strip);
	}
}


/* Find the active strip + track combo, and set them up as the tweaking track,
 * and return if successful or not.
 */
short BKE_nla_tweakmode_enter (AnimData *adt)
{
	NlaTrack *nlt, *activeTrack=NULL;
	NlaStrip *strip, *activeStrip=NULL;
	
	/* verify that data is valid */
	if ELEM(NULL, adt, adt->nla_tracks.first)
		return 0;
		
	/* if block is already in tweakmode, just leave, but we should report 
	 * that this block is in tweakmode (as our returncode)
	 */
	if (adt->flag & ADT_NLA_EDIT_ON)
		return 1;
		
	/* go over the tracks, finding the active one, and its active strip
	 * 	- if we cannot find both, then there's nothing to do
	 */
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) {
		/* check if active */
		if (nlt->flag & NLATRACK_ACTIVE) {
			/* store reference to this active track */
			activeTrack= nlt;
			
			/* now try to find active strip */
			activeStrip= BKE_nlastrip_find_active(nlt);
			break;
		}	
	}
	if ELEM3(NULL, activeTrack, activeStrip, activeStrip->act) {
		if (G.f & G_DEBUG) {
			printf("NLA tweakmode enter - neither active requirement found \n");
			printf("\tactiveTrack = %p, activeStrip = %p \n", activeTrack, activeStrip);
		}
		return 0;
	}
		
	/* go over all the tracks up to the active one, tagging each strip that uses the same 
	 * action as the active strip, but leaving everything else alone
	 */
	for (nlt= activeTrack->prev; nlt; nlt= nlt->prev) {
		for (strip= nlt->strips.first; strip; strip= strip->next) {
			if (strip->act == activeStrip->act)
				strip->flag |= NLASTRIP_FLAG_TWEAKUSER;
			else
				strip->flag &= ~NLASTRIP_FLAG_TWEAKUSER;
		}
	}
	
	
	/* go over all the tracks after AND INCLUDING the active one, tagging them as being disabled 
	 *	- the active track needs to also be tagged, otherwise, it'll overlap with the tweaks going on
	 */
	for (nlt= activeTrack; nlt; nlt= nlt->next)
		nlt->flag |= NLATRACK_DISABLED;
	
	/* handle AnimData level changes:
	 *	- 'real' active action to temp storage (no need to change user-counts)
	 *	- action of active strip set to be the 'active action', and have its usercount incremented
	 *	- editing-flag for this AnimData block should also get turned on (for more efficient restoring)
	 *	- take note of the active strip for mapping-correction of keyframes in the action being edited
	 */
	adt->tmpact= adt->action;
	adt->action= activeStrip->act;
	adt->actstrip= activeStrip;
	id_us_plus(&activeStrip->act->id);
	adt->flag |= ADT_NLA_EDIT_ON;
	
	/* done! */
	return 1;
}

/* Exit tweakmode for this AnimData block */
void BKE_nla_tweakmode_exit (AnimData *adt)
{
	NlaStrip *strip;
	NlaTrack *nlt;
	
	/* verify that data is valid */
	if ELEM(NULL, adt, adt->nla_tracks.first)
		return;
		
	/* hopefully the flag is correct - skip if not on */
	if ((adt->flag & ADT_NLA_EDIT_ON) == 0)
		return;
		
	// TODO: need to sync the user-strip with the new state of the action!
		
	/* for all Tracks, clear the 'disabled' flag
	 * for all Strips, clear the 'tweak-user' flag
	 */
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) {
		nlt->flag &= ~NLATRACK_DISABLED;
		
		for (strip= nlt->strips.first; strip; strip= strip->next) 
			strip->flag &= ~NLASTRIP_FLAG_TWEAKUSER;
	}
	
	/* handle AnimData level changes:
	 *	- 'temporary' active action needs its usercount decreased, since we're removing this reference
	 *	- 'real' active action is restored from storage
	 *	- storage pointer gets cleared (to avoid having bad notes hanging around)
	 *	- editing-flag for this AnimData block should also get turned off
	 *	- clear pointer to active strip
	 */
	if (adt->action) adt->action->id.us--;
	adt->action= adt->tmpact;
	adt->tmpact= NULL;
	adt->actstrip= NULL;
	adt->flag &= ~ADT_NLA_EDIT_ON;
}

/* Baking Tools ------------------------------------------- */

void BKE_nla_bake (Scene *scene, ID *id, AnimData *adt, int flag)
{

	/* verify that data is valid 
	 *	1) Scene and AnimData must be provided 
	 *	2) there must be tracks to merge...
	 */
	if ELEM3(NULL, scene, adt, adt->nla_tracks.first)
		return;
	
	/* if animdata currently has an action, 'push down' this onto the stack first */
	if (adt->action)
		BKE_nla_action_pushdown(adt);
	
	/* get range of motion to bake, and the channels involved... */
	
	/* temporarily mute the action, and start keying to it */
	
	/* start keying... */
	
	/* unmute the action */
}

/* *************************************************** */
