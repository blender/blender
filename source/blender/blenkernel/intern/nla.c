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
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_blender.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "nla_private.h"


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/* *************************************************** */
/* Data Management */

/* Freeing ------------------------------------------- */

/* Remove the given NLA strip from the NLA track it occupies, free the strip's data,
 * and the strip itself. 
 */
// TODO: with things like transitions, should these get freed too? Maybe better as a UI tool
void free_nlastrip (ListBase *strips, NlaStrip *strip)
{
	FModifier *fcm, *fmn;
	
	/* sanity checks */
	if (strip == NULL)
		return;
		
	/* remove reference to action */
	if (strip->act)
		strip->act->id.us--;
		
	/* free remapping info */
	//if (strip->remap)
	//	BKE_animremap_free();
	
	/* free own F-Curves */
	free_fcurves(&strip->fcurves);
	
	/* free F-Modifiers */
	for (fcm= strip->modifiers.first; fcm; fcm= fmn) {
		fmn= fcm->next;
		
		BLI_remlink(&strip->modifiers, fcm);
		fcurve_remove_modifier(NULL, fcm);
	}
	
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
	fcurve_copy_modifiers(&strip_d->modifiers, &strip->modifiers);
	
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
	sprintf(nlt->name, "NlaTrack");
	BLI_uniquename(&adt->nla_tracks, nlt, "NlaTrack", '.', offsetof(NlaTrack, name), 64);
	
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
	calc_action_range(strip->act, &strip->actstart, &strip->actend, 1);
	
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
	
	/* returns the strip added */
	return strip;
}

/* *************************************************** */
/* NLA Evaluation <-> Editing Stuff */

/* Strip Mapping ------------------------------------- */

/* non clipped mapping for strip-time <-> global time (for Action-Clips)
 *	invert = convert action-strip time to global time 
 */
static float nlastrip_get_frame_actionclip (NlaStrip *strip, float cframe, short invert)
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
		/* invert = convert action-strip time to global time */
		if (invert)
			return scale*(strip->actend - cframe) + strip->start; // FIXME: this doesn't work for multiple repeats yet
		else {
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
		/* invert = convert action-strip time to global time */
		if (invert)
			return scale*(cframe - strip->actstart) + strip->start; // FIXME: this doesn't work for mutiple repeats yet
		else {
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
static float nlastrip_get_frame_transition (NlaStrip *strip, float cframe, short invert)
{
	float length;
	
	/* length of strip */
	length= strip->end - strip->start;
	
	/* reversed = play strip backwards */
	if (strip->flag & NLASTRIP_FLAG_REVERSE) {
		/* invert = convert within-strip-time to global time */
		if (invert)
			return strip->end - (length * cframe);
		else
			return (strip->end - cframe) / length;
	}
	else {
		/* invert = convert within-strip-time to global time */
		if (invert)
			return (length * cframe) + strip->start;
		else
			return (cframe - strip->start) / length;
	}
}

/* non clipped mapping for strip-time <-> global time
 *	invert = convert action-strip time to global time 
 *
 * only secure for 'internal' (i.e. within AnimSys evaluation) operations,
 * but should not be directly relied on for stuff which interacts with editors
 */
float nlastrip_get_frame (NlaStrip *strip, float cframe, short invert)
{
	switch (strip->type) {
		case NLASTRIP_TYPE_TRANSITION: /* transition */
			return nlastrip_get_frame_transition(strip, cframe, invert);
		
		case NLASTRIP_TYPE_CLIP: /* action-clip (default) */
		default:
			return nlastrip_get_frame_actionclip(strip, cframe, invert);
	}	
}


/* Non clipped mapping for strip-time <-> global time
 *	invert = convert strip-time to global time 
 *
 * Public API method - perform this mapping using the given AnimData block
 * and perform any necessary sanity checks on the value
 */
float BKE_nla_tweakedit_remap (AnimData *adt, float cframe, short invert)
{
	NlaStrip *strip;
	
	/* sanity checks 
	 *	- obviously we've got to have some starting data
	 *	- when not in tweakmode, the active Action does not have any scaling applied :)
	 */
	if ((adt == NULL) || (adt->flag & ADT_NLA_EDIT_ON)==0)
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
	return nlastrip_get_frame(strip, cframe, invert);
}

/* *************************************************** */
/* Basic Utilities */

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

/* Check if there is any space in the last track to add the given strip */
short BKE_nlatrack_has_space (NlaTrack *nlt, float start, float end)
{
	NlaStrip *strip;
	
	/* sanity checks */
	if ((nlt == NULL) || IS_EQ(start, end))
		return 0;
	if (start > end) {
		puts("BKE_nlatrack_has_space error... start and end arguments swapped");
		SWAP(float, start, end);
	}
	
	/* loop over NLA strips checking for any overlaps with this area... */
	for (strip= nlt->strips.first; strip; strip= strip->next) {
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
void BKE_nlatrack_sort_strips (NlaTrack *nlt)
{
	ListBase tmp = {NULL, NULL};
	NlaStrip *strip, *sstrip;
	
	/* sanity checks */
	if ELEM(NULL, nlt, nlt->strips.first)
		return;
		
	/* we simply perform insertion sort on this list, since it is assumed that per track,
	 * there are only likely to be at most 5-10 strips
	 */
	for (strip= nlt->strips.first; strip; strip= strip->next) {
		short not_added = 1;
		
		/* remove this strip from the list, and add it to the new list, searching from the end of 
		 * the list, assuming that the lists are in order 
		 */
		BLI_remlink(&nlt->strips, strip);
		
		for (sstrip= tmp.last; not_added && sstrip; sstrip= sstrip->prev) {
			/* check if add after */
			if (sstrip->end < strip->start) {
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
	nlt->strips.first= tmp.first;
	nlt->strips.last= tmp.last;
}

/* Add the given NLA-Strip to the given NLA-Track, assuming that it 
 * isn't currently attached to another one 
 */
short BKE_nlatrack_add_strip (NlaTrack *nlt, NlaStrip *strip)
{
	NlaStrip *ns;
	short not_added = 1;
	
	/* sanity checks */
	if ELEM(NULL, nlt, strip)
		return 0;
		
	/* check if any space to add */
	if (BKE_nlatrack_has_space(nlt, strip->start, strip->end)==0)
		return 0;
	
	/* find the right place to add the strip to the nominated track */
	for (ns= nlt->strips.first; ns; ns= ns->next) {
		/* if current strip occurs after the new strip, add it before */
		if (ns->start > strip->end) {
			BLI_insertlinkbefore(&nlt->strips, ns, strip);
			not_added= 0;
			break;
		}
	}
	if (not_added) {
		/* just add to the end of the list of the strips then... */
		BLI_addtail(&nlt->strips, strip);
	}
	
	/* added... */
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

/* Is the given NLA-strip the first one to occur for the given AnimData block */
// TODO: make this an api method if necesary, but need to add prefix first
short nlastrip_is_first (AnimData *adt, NlaStrip *strip)
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
 
/* Tools ------------------------------------------- */

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
			strip->extendmode= NLASTRIP_EXTEND_HOLD_FORWARD;
		}
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
		printf("NLA tweakmode enter - neither active requirement found \n");
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
				strip->flag &= ~NLASTRIP_FLAG_TWEAKUSER; // XXX probably don't need to clear this...
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
		
	/* for all NLA-tracks, clear the 'disabled' flag
	 * for all NLA-strips, clear the 'tweak-user' flag
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

/* *************************************************** */
