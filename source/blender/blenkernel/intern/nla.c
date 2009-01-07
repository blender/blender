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

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "DNA_space_types.h"
#include "DNA_nla_types.h"
#include "DNA_action_types.h"
#include "DNA_ID.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"

#include "BKE_nla.h"
#include "BKE_action.h"
#include "BKE_blender.h"
#include "BKE_library.h"
#include "BKE_object.h" /* for convert_action_to_strip(ob) */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* NOTE: in group.c the strips get copied for group-nla override, this assumes
   that strips are one single block, without additional data to be copied */

void copy_actionstrip (bActionStrip **dst, bActionStrip **src){
	bActionStrip *dstrip;
	bActionStrip *sstrip = *src;

	if (!*src){
		*dst=NULL;
		return;
	}

	*dst = MEM_dupallocN(sstrip);

	dstrip = *dst;
	if (dstrip->act)
		dstrip->act->id.us++;

	if (dstrip->ipo)
		dstrip->ipo->id.us++;
	
	if (dstrip->modifiers.first) {
		BLI_duplicatelist (&dstrip->modifiers, &sstrip->modifiers);
	}
	
}

void copy_nlastrips (ListBase *dst, ListBase *src)
{
	bActionStrip *strip;

	dst->first=dst->last=NULL;

	BLI_duplicatelist (dst, src);

	/* Update specific data */
	if (!dst->first)
		return;

	for (strip = dst->first; strip; strip=strip->next){
		if (strip->act)
			strip->act->id.us++;
		if (strip->ipo)
			strip->ipo->id.us++;
		if (strip->modifiers.first) {
			ListBase listb;
			BLI_duplicatelist (&listb, &strip->modifiers);
			strip->modifiers= listb;
		}
	}
}

/* from editnla, for convert_action_to_strip -- no UI code so should be ok here.. */
void find_stridechannel(Object *ob, bActionStrip *strip)
{
	if(ob && ob->pose) {
		bPoseChannel *pchan;
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next)
			if(pchan->flag & POSE_STRIDE)
				break;
		if(pchan)
			BLI_strncpy(strip->stridechannel, pchan->name, 32);
		else
			strip->stridechannel[0]= 0;
	}
}

//called by convert_nla / bpy api with an object with the action to be converted to a new strip
bActionStrip *convert_action_to_strip (Object *ob)
{
	bActionStrip *nstrip;

	/* Make new actionstrip */
	nstrip = MEM_callocN(sizeof(bActionStrip), "bActionStrip");
			
	/* Link the action to the nstrip */
	nstrip->act = ob->action;
	id_us_plus(&nstrip->act->id);
	calc_action_range(nstrip->act, &nstrip->actstart, &nstrip->actend, 1);
	nstrip->start = nstrip->actstart;
	nstrip->end = nstrip->actend;
	nstrip->flag = ACTSTRIP_SELECT|ACTSTRIP_LOCK_ACTION;
			
	find_stridechannel(ob, nstrip);
	//set_active_strip(ob, nstrip); /* is in editnla as does UI calls */
			
	nstrip->repeat = 1.0;

	if(ob->nlastrips.first == NULL)
		ob->nlaflag |= OB_NLA_OVERRIDE;
	
	BLI_addtail(&ob->nlastrips, nstrip);
	return nstrip; /* is created, malloced etc. here so is safe to just return the pointer?
			  this is needed for setting this active in UI, and probably useful for API too */
	
}


/* not strip itself! */
void free_actionstrip(bActionStrip* strip)
{
	if (!strip)
		return;

	if (strip->act){
		strip->act->id.us--;
		strip->act = NULL;
	}
	if (strip->ipo){
		strip->ipo->id.us--;
		strip->ipo = NULL;
	}
	if (strip->modifiers.first) {
		BLI_freelistN(&strip->modifiers);
	}
	
}

void free_nlastrips (ListBase *nlalist)
{
	bActionStrip *strip;

	if (!nlalist->first)
		return;

	/* Do any specific freeing */
	for (strip=nlalist->first; strip; strip=strip->next)
	{
		free_actionstrip (strip);
	};

	/* Free the whole list */
	BLI_freelistN(nlalist);
}
