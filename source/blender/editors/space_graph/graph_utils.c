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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_graph/graph_utils.c
 *  \ingroup spgraph
 */


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_anim_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"


#include "WM_api.h"


#include "ED_anim_api.h"


#include "graph_intern.h"   // own include

/* ************************************************************** */
/* Active F-Curve */

/* Find 'active' F-Curve. It must be editable, since that's the purpose of these buttons (subject to change).  
 * We return the 'wrapper' since it contains valuable context info (about hierarchy), which will need to be freed 
 * when the caller is done with it.
 *
 * NOTE: curve-visible flag isn't included, otherwise selecting a curve via list to edit is too cumbersome
 */
bAnimListElem *get_active_fcurve_channel(bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_ACTIVE);
	size_t items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* We take the first F-Curve only, since some other ones may have had 'active' flag set
	 * if they were from linked data.
	 */
	if (items) {
		bAnimListElem *ale = (bAnimListElem *)anim_data.first;
		
		/* remove first item from list, then free the rest of the list and return the stored one */
		BLI_remlink(&anim_data, ale);
		BLI_freelistN(&anim_data);
		
		return ale;
	}
	
	/* no active F-Curve */
	return NULL;
}

/* ************************************************************** */
/* Operator Polling Callbacks */

/* Check if there are any visible keyframes (for selection tools) */
int graphop_visible_keyframes_poll(bContext *C)
{
	bAnimContext ac;
	bAnimListElem *ale;
	ListBase anim_data = {NULL, NULL};
	ScrArea *sa = CTX_wm_area(C);
	size_t items;
	int filter;
	short found = 0;
	
	/* firstly, check if in Graph Editor */
	// TODO: also check for region?
	if ((sa == NULL) || (sa->spacetype != SPACE_IPO))
		return 0;
		
	/* try to init Anim-Context stuff ourselves and check */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return 0;
	
	/* loop over the visible (selection doesn't matter) F-Curves, and see if they're suitable
	 * stopping on the first successful match
	 */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE);
	items = ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	if (items == 0) 
		return 0;
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->data;
		
		/* visible curves for selection must fulfill the following criteria:
		 *	- it has bezier keyframes
		 *	- F-Curve modifiers do not interfere with the result too much 
		 *	  (i.e. the modifier-control drawing check returns false)
		 */
		if (fcu->bezt == NULL)
			continue;
		if (fcurve_are_keyframes_usable(fcu)) {
			found = 1;
			break;
		}
	}
	
	/* cleanup and return findings */
	BLI_freelistN(&anim_data);
	return found;
}

/* Check if there are any visible + editable keyframes (for editing tools) */
int graphop_editable_keyframes_poll(bContext *C)
{
	bAnimContext ac;
	bAnimListElem *ale;
	ListBase anim_data = {NULL, NULL};
	ScrArea *sa = CTX_wm_area(C);
	size_t items;
	int filter;
	short found = 0;
	
	/* firstly, check if in Graph Editor */
	// TODO: also check for region?
	if ((sa == NULL) || (sa->spacetype != SPACE_IPO))
		return 0;
		
	/* try to init Anim-Context stuff ourselves and check */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return 0;
	
	/* loop over the editable F-Curves, and see if they're suitable
	 * stopping on the first successful match
	 */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVE_VISIBLE);
	items = ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	if (items == 0) 
		return 0;
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->data;
		
		/* editable curves must fulfill the following criteria:
		 *	- it has bezier keyframes
		 *	- it must not be protected from editing (this is already checked for with the foredit flag
		 *	- F-Curve modifiers do not interfere with the result too much 
		 *	  (i.e. the modifier-control drawing check returns false)
		 */
		if (fcu->bezt == NULL)
			continue;
		if (fcurve_is_keyframable(fcu)) {
			found = 1;
			break;
		}
	}
	
	/* cleanup and return findings */
	BLI_freelistN(&anim_data);
	return found;
}

/* has active F-Curve that's editable */
int graphop_active_fcurve_poll(bContext *C)
{
	bAnimContext ac;
	bAnimListElem *ale;
	ScrArea *sa = CTX_wm_area(C);
	short has_fcurve = 0;
	
	/* firstly, check if in Graph Editor */
	// TODO: also check for region?
	if ((sa == NULL) || (sa->spacetype != SPACE_IPO))
		return 0;
		
	/* try to init Anim-Context stuff ourselves and check */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return 0;
		
	/* try to get the Active F-Curve */
	ale = get_active_fcurve_channel(&ac);
	if (ale == NULL)
		return 0;
		
	/* free temp data... */
	has_fcurve = ((ale->data) && (ale->type == ANIMTYPE_FCURVE));
	if (has_fcurve) {
		FCurve *fcu = (FCurve *)ale->data;
		has_fcurve = (fcu->flag & FCURVE_VISIBLE) != 0;
	}
	
	MEM_freeN(ale);
	
	/* return success */
	return has_fcurve;
}

/* has selected F-Curve that's editable */
int graphop_selected_fcurve_poll(bContext *C)
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	ScrArea *sa = CTX_wm_area(C);
	size_t items;
	int filter;
	
	/* firstly, check if in Graph Editor */
	// TODO: also check for region?
	if ((sa == NULL) || (sa->spacetype != SPACE_IPO))
		return 0;
		
	/* try to init Anim-Context stuff ourselves and check */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return 0;
	
	/* get the editable + selected F-Curves, and as long as we got some, we can return 
	 * NOTE: curve-visible flag isn't included, otherwise selecting a curve via list to edit is too cumbersome
	 */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT);
	items = ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	if (items == 0) 
		return 0;
	
	/* cleanup and return findings */
	BLI_freelistN(&anim_data);
	return 1;
}

/* ************************************************************** */
