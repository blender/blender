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

// TODO...

/* Adding ------------------------------------------- */

/* Add a NLA Strip referencing the given Action, to the given NLA Track */
// TODO: any extra parameters to control how this is done?
NlaStrip *add_nlastrip (NlaTrack *nlt, bAction *act)
{
	NlaStrip *strip;
	
	/* sanity checks */
	if ELEM(NULL, nlt, act)
		return NULL;
		
	/* allocate new strip */
	strip= MEM_callocN(sizeof(NlaStrip), "NlaStrip");
	BLI_addtail(&nlt->strips, strip);
	
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

/* Add a NLA Track to the given AnimData */
NlaTrack *add_nlatrack (AnimData *adt)
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
	BLI_addtail(&adt->nla_tracks, nlt);
	BKE_nlatrack_set_active(&adt->nla_tracks, nlt);
	
	/* must have unique name, but we need to seed this */
	sprintf(nlt->name, "NlaTrack");
	BLI_uniquename(&adt->nla_tracks, nlt, "NlaTrack", '.', offsetof(NlaTrack, name), 64);
	
	/* return the new track */
	return nlt;
}

/* *************************************************** */
/* Basic Utilities */

/* States ------------------------------------------- */

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

/* Tools ------------------------------------------- */

/* For the given AnimData block, add the active action to the NLA
 * stack (i.e. 'push-down' action). The UI should only allow this 
 * for normal editing only (i.e. not in editmode for some strip's action),
 * so no checks for this are performed.
 */
// TODO: maybe we should have checks for this too...
void BKE_nla_action_pushdown (AnimData *adt)
{
	NlaTrack *nlt;
	NlaStrip *strip;
	
	/* sanity checks */
	// TODO: need to report the error for this
	if ELEM(NULL, adt, adt->action) 
		return;
		
	/* if the action is empty, we also shouldn't try to add to stack, 
	 * as that will cause us grief down the track
	 */
	// TODO: code a method for this, and report errors after...
		
	/* add a new NLA track to house this action 
	 *	- we could investigate trying to fit the action into an appropriately
	 *	  sized gap in the existing tracks, however, this may result in unexpected 
	 *	  changes in blending behaviour...
	 */
	nlt= add_nlatrack(adt);
	if (nlt == NULL)
		return;
	
	/* add a new NLA strip to the track, which references the active action */
	strip= add_nlastrip(nlt, adt->action);
	
	/* clear reference to action now that we've pushed it onto the stack */
	if (strip) {
		adt->action->id.us--;
		adt->action= NULL;
	}
	
	// TODO: set any other flags necessary here...
}

/* *************************************************** */
