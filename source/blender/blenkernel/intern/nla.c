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

#include <stdlib.h>

#include "BKE_nla.h"
#include "BKE_blender.h"

#include "BLI_blenlib.h"

#include "DNA_space_types.h"
#include "DNA_nla_types.h"
#include "DNA_action_types.h"
#include "DNA_ID.h"
#include "DNA_ipo_types.h"

#include "MEM_guardedalloc.h"

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
}

void copy_nlastrips (ListBase *dst, ListBase *src)
{
	bActionStrip *strip;

	dst->first=dst->last=NULL;

	duplicatelist (dst, src);

	/* Update specific data */
	if (!dst->first)
		return;

	for (strip = dst->first; strip; strip=strip->next){
		if (strip->act)
			strip->act->id.us++;
		if (strip->ipo)
			strip->ipo->id.us++;
	}
}


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
