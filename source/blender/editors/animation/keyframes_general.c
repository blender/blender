/**
 * $Id: 
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
 * The Original Code is Copyright (C) 2008 Blender Foundation
 * All rights reserved.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_utildefines.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"

/* This file contains code for various keyframe-editing tools which are 'destructive'
 * (i.e. they will modify the order of the keyframes, and change the size of the array).
 * While some of these tools may eventually be moved out into blenkernel, for now, it is
 * fine to have these calls here.
 * 
 * There are also a few tools here which cannot be easily coded for in the other system (yet).
 * These may also be moved around at some point, but for now, they are best added here.
 *
 * - Joshua Leung, Dec 2008
 */
 
/* **************************************************** */

/* Only delete the nominated keyframe from provided ipo-curve. 
 * Not recommended to be used many times successively. For that
 * there is delete_ipo_keys(). 
 */
void delete_fcurve_key(FCurve *fcu, int index, short do_recalc)
{
	/* firstly check that index is valid */
	if (index < 0) 
		index *= -1;
	if (fcu == NULL) 
		return;
	if (index >= fcu->totvert)
		return;
	
	/*	Delete this key */
	memmove(&fcu->bezt[index], &fcu->bezt[index+1], sizeof(BezTriple)*(fcu->totvert-index-1));
	fcu->totvert--;
	
	/* recalc handles - only if it won't cause problems */
	if (do_recalc)
		calchandles_fcurve(fcu);
}

/* Delete selected keyframes in given F-Curve */
void delete_fcurve_keys(FCurve *fcu)
{
	int i;
	
	/* Delete selected BezTriples */
	for (i=0; i < fcu->totvert; i++) {
		if (fcu->bezt[i].f2 & SELECT) {
			memmove(&fcu->bezt[i], &fcu->bezt[i+1], sizeof(BezTriple)*(fcu->totvert-i-1));
			fcu->totvert--;
			i--;
		}
	}
	
	/* Free the array of BezTriples if there are not keyframes */
	if (fcu->totvert == 0) {
		if (fcu->bezt) 
			MEM_freeN(fcu->bezt);
		fcu->bezt= NULL;
	}
}

/* ---------------- */

/* duplicate selected keyframes for the given F-Curve */
void duplicate_fcurve_keys(FCurve *fcu)
{
	BezTriple *newbezt;
	int i;
	
	/* this can only work when there is an F-Curve, and also when there are some BezTriples */
	if ELEM(NULL, fcu, fcu->bezt)
		return;
	
	for (i=0; i < fcu->totvert; i++) {
		/* If a key is selected */
		if (fcu->bezt[i].f2 & SELECT) {
			/* Expand the list */
			newbezt = MEM_callocN(sizeof(BezTriple) * (fcu->totvert+1), "beztriple");
			
			memcpy(newbezt, fcu->bezt, sizeof(BezTriple) * (i+1));
			memcpy(newbezt+i+1, fcu->bezt+i, sizeof(BezTriple));
			memcpy(newbezt+i+2, fcu->bezt+i+1, sizeof (BezTriple) *(fcu->totvert-(i+1)));
			fcu->totvert++;
			
			/* reassign pointers... (free old, and add new) */
			MEM_freeN(fcu->bezt);
			fcu->bezt=newbezt;
			
			/* Unselect the current key */
			BEZ_DESEL(&fcu->bezt[i]);
			i++;
			
			/* Select the copied key */
			BEZ_SEL(&fcu->bezt[i]);
		}
	}
}

/* **************************************************** */
/* Various Tools */

/* Basic F-Curve 'cleanup' function that removes 'double points' and unnecessary keyframes on linear-segments only */
void clean_fcurve(FCurve *fcu, float thresh)
{
	BezTriple *old_bezts, *bezt, *beztn;
	BezTriple *lastb;
	int totCount, i;
	
	/* check if any points  */
	if ((fcu == NULL) || (fcu->totvert <= 1)) 
		return;
	
	/* make a copy of the old BezTriples, and clear IPO curve */
	old_bezts = fcu->bezt;
	totCount = fcu->totvert;	
	fcu->bezt = NULL;
	fcu->totvert = 0;
	
	/* now insert first keyframe, as it should be ok */
	bezt = old_bezts;
	insert_vert_fcurve(fcu, bezt->vec[1][0], bezt->vec[1][1], 0);
	
	/* Loop through BezTriples, comparing them. Skip any that do 
	 * not fit the criteria for "ok" points.
	 */
	for (i=1; i<totCount; i++) {	
		float prev[2], cur[2], next[2];
		
		/* get BezTriples and their values */
		if (i < (totCount - 1)) {
			beztn = (old_bezts + (i+1));
			next[0]= beztn->vec[1][0]; next[1]= beztn->vec[1][1];
		}
		else {
			beztn = NULL;
			next[0] = next[1] = 0.0f;
		}
		lastb= (fcu->bezt + (fcu->totvert - 1));
		bezt= (old_bezts + i);
		
		/* get references for quicker access */
		prev[0] = lastb->vec[1][0]; prev[1] = lastb->vec[1][1];
		cur[0] = bezt->vec[1][0]; cur[1] = bezt->vec[1][1];
		
		/* check if current bezt occurs at same time as last ok */
		if (IS_EQT(cur[0], prev[0], thresh)) {
			/* If there is a next beztriple, and if occurs at the same time, only insert 
			 * if there is a considerable distance between the points, and also if the 
			 * current is further away than the next one is to the previous.
			 */
			if (beztn && (IS_EQT(cur[0], next[0], thresh)) && 
				(IS_EQT(next[1], prev[1], thresh)==0)) 
			{
				/* only add if current is further away from previous */
				if (cur[1] > next[1]) {
					if (IS_EQT(cur[1], prev[1], thresh) == 0) {
						/* add new keyframe */
						insert_vert_fcurve(fcu, cur[0], cur[1], 0);
					}
				}
			}
			else {
				/* only add if values are a considerable distance apart */
				if (IS_EQT(cur[1], prev[1], thresh) == 0) {
					/* add new keyframe */
					insert_vert_fcurve(fcu, cur[0], cur[1], 0);
				}
			}
		}
		else {
			/* checks required are dependent on whether this is last keyframe or not */
			if (beztn) {
				/* does current have same value as previous and next? */
				if (IS_EQT(cur[1], prev[1], thresh) == 0) {
					/* add new keyframe*/
					insert_vert_fcurve(fcu, cur[0], cur[1], 0);
				}
				else if (IS_EQT(cur[1], next[1], thresh) == 0) {
					/* add new keyframe */
					insert_vert_fcurve(fcu, cur[0], cur[1], 0);
				}
			}
			else {	
				/* add if value doesn't equal that of previous */
				if (IS_EQT(cur[1], prev[1], thresh) == 0) {
					/* add new keyframe */
					insert_vert_fcurve(fcu, cur[0], cur[1], 0);
				}
			}
		}
	}
	
	/* now free the memory used by the old BezTriples */
	if (old_bezts)
		MEM_freeN(old_bezts);
}

/* ---------------- */

/* temp struct used for smooth_ipo */
typedef struct tSmooth_Bezt {
	float *h1, *h2, *h3;	/* bezt->vec[0,1,2][1] */
} tSmooth_Bezt;

/* Use a weighted moving-means method to reduce intensity of fluctuations */
void smooth_fcurve (FCurve *fcu)
{
	BezTriple *bezt;
	int i, x, totSel = 0;
	
	/* first loop through - count how many verts are selected, and fix up handles 
	 *	this is done for both modes
	 */
	bezt= fcu->bezt;
	for (i=0; i < fcu->totvert; i++, bezt++) {						
		if (BEZSELECTED(bezt)) {							
			/* line point's handles up with point's vertical position */
			bezt->vec[0][1]= bezt->vec[2][1]= bezt->vec[1][1];
			if ((bezt->h1==HD_AUTO) || (bezt->h1==HD_VECT)) bezt->h1= HD_ALIGN;
			if ((bezt->h2==HD_AUTO) || (bezt->h2==HD_VECT)) bezt->h2= HD_ALIGN;
			
			/* add value to total */
			totSel++;
		}
	}
	
	/* if any points were selected, allocate tSmooth_Bezt points to work on */
	if (totSel >= 3) {
		tSmooth_Bezt *tarray, *tsb;
		
		/* allocate memory in one go */
		tsb= tarray= MEM_callocN(totSel*sizeof(tSmooth_Bezt), "tSmooth_Bezt Array");
		
		/* populate tarray with data of selected points */
		bezt= fcu->bezt;
		for (i=0, x=0; (i < fcu->totvert) && (x < totSel); i++, bezt++) {
			if (BEZSELECTED(bezt)) {
				/* tsb simply needs pointer to vec, and index */
				tsb->h1 = &bezt->vec[0][1];
				tsb->h2 = &bezt->vec[1][1];
				tsb->h3 = &bezt->vec[2][1];
				
				/* advance to the next tsb to populate */
				if (x < totSel- 1) 
					tsb++;
				else
					break;
			}
		}
			
		/* calculate the new smoothed F-Curve's with weighted averages:
		 *	- this is done with two passes
		 *	- uses 5 points for each operation (which stores in the relevant handles)
		 *	-	previous: w/a ratio = 3:5:2:1:1
		 *	- 	next: w/a ratio = 1:1:2:5:3
		 */
		
		/* round 1: calculate previous and next */ 
		tsb= tarray;
		for (i=0; i < totSel; i++, tsb++) {
			/* don't touch end points (otherwise, curves slowly explode) */
			if (ELEM(i, 0, (totSel-1)) == 0) {
				const tSmooth_Bezt *tP1 = tsb - 1;
				const tSmooth_Bezt *tP2 = (i-2 > 0) ? (tsb - 2) : (NULL);
				const tSmooth_Bezt *tN1 = tsb + 1;
				const tSmooth_Bezt *tN2 = (i+2 < totSel) ? (tsb + 2) : (NULL);
				
				const float p1 = *tP1->h2;
				const float p2 = (tP2) ? (*tP2->h2) : (*tP1->h2);
				const float c1 = *tsb->h2;
				const float n1 = *tN1->h2;
				const float n2 = (tN2) ? (*tN2->h2) : (*tN1->h2);
				
				/* calculate previous and next */
				*tsb->h1= (3*p2 + 5*p1 + 2*c1 + n1 + n2) / 12;
				*tsb->h3= (p2 + p1 + 2*c1 + 5*n1 + 3*n2) / 12;
			}
		}
		
		/* round 2: calculate new values and reset handles */
		tsb= tarray;
		for (i=0; i < totSel; i++, tsb++) {
			/* calculate new position by averaging handles */
			*tsb->h2 = (*tsb->h1 + *tsb->h3) / 2;
			
			/* reset handles now */
			*tsb->h1 = *tsb->h2;
			*tsb->h3 = *tsb->h2;
		}
		
		/* free memory required for tarray */
		MEM_freeN(tarray);
	}
	
	/* recalculate handles */
	calchandles_fcurve(fcu);
}

/* ---------------- */

/* little cache for values... */
typedef struct tempFrameValCache {
	float frame, val;
} tempFrameValCache;


/* Evaluates the curves between each selected keyframe on each frame, and keys the value  */
void sample_fcurve (FCurve *fcu)
{
	BezTriple *bezt, *start=NULL, *end=NULL;
	tempFrameValCache *value_cache, *fp;
	int sfra, range;
	int i, n, nIndex;
	
	/* find selected keyframes... once pair has been found, add keyframes  */
	for (i=0, bezt=fcu->bezt; i < fcu->totvert; i++, bezt++) {
		/* check if selected, and which end this is */
		if (BEZSELECTED(bezt)) {
			if (start) {
				/* set end */
				end= bezt;
				
				/* cache values then add keyframes using these values, as adding
				 * keyframes while sampling will affect the outcome...
				 *	- only start sampling+adding from index=1, so that we don't overwrite original keyframe
				 */
				range= (int)( ceil(end->vec[1][0] - start->vec[1][0]) );
				sfra= (int)( floor(start->vec[1][0]) );
				
				if (range) {
					value_cache= MEM_callocN(sizeof(tempFrameValCache)*range, "IcuFrameValCache");
					
					/* 	sample values 	*/
					for (n=1, fp=value_cache; n<range && fp; n++, fp++) {
						fp->frame= (float)(sfra + n);
						fp->val= evaluate_fcurve(fcu, fp->frame);
					}
					
					/* 	add keyframes with these, tagging as 'breakdowns' 	*/
					for (n=1, fp=value_cache; n<range && fp; n++, fp++) {
						nIndex= insert_vert_fcurve(fcu, fp->frame, fp->val, 1);
						BEZKEYTYPE(fcu->bezt + nIndex)= BEZT_KEYTYPE_BREAKDOWN;
					}
					
					/* free temp cache */
					MEM_freeN(value_cache);
					
					/* as we added keyframes, we need to compensate so that bezt is at the right place */
					bezt = fcu->bezt + i + range - 1;
					i += (range - 1);
				}
				
				/* bezt was selected, so it now marks the start of a whole new chain to search */
				start= bezt;
				end= NULL;
			}
			else {
				/* just set start keyframe */
				start= bezt;
				end= NULL;
			}
		}
	}
	
	/* recalculate channel's handles? */
	calchandles_fcurve(fcu);
}

/* **************************************************** */
/* Copy/Paste Tools */
/* - The copy/paste buffer currently stores a set of temporary F-Curves containing only the keyframes 
 *   that were selected in each of the original F-Curves
 * - All pasted frames are offset by the same amount. This is calculated as the difference in the times of
 *	the current frame and the 'first keyframe' (i.e. the earliest one in all channels).
 * - The earliest frame is calculated per copy operation.
 */

/* globals for copy/paste data (like for other copy/paste buffers) */
ListBase animcopybuf = {NULL, NULL};
static float animcopy_firstframe= 999999999.0f;

/* datatype for use in copy/paste buffer */
typedef struct tAnimCopybufItem {
	struct tAnimCopybufItem *next, *prev;
	
	ID *id;				/* ID which owns the curve */
	bActionGroup *grp;	/* Action Group */
	char *rna_path;		/* RNA-Path */
	int array_index;	/* array index */
	
	int totvert;		/* number of keyframes stored for this channel */
	BezTriple *bezt;	/* keyframes in buffer */
} tAnimCopybufItem;


/* This function frees any MEM_calloc'ed copy/paste buffer data */
// XXX find some header to put this in!
void free_anim_copybuf (void)
{
	tAnimCopybufItem *aci, *acn;
	
	/* free each buffer element */
	for (aci= animcopybuf.first; aci; aci= acn) {
		acn= aci->next;
		
		/* free keyframes */
		if (aci->bezt) 
			MEM_freeN(aci->bezt);
			
		/* free RNA-path */
		if (aci->rna_path)
			MEM_freeN(aci->rna_path);
			
		/* free ourself */
		BLI_freelinkN(&animcopybuf, aci);
	}
	
	/* restore initial state */
	animcopybuf.first= animcopybuf.last= NULL;
	animcopy_firstframe= 999999999.0f;
}

/* ------------------- */

/* This function adds data to the keyframes copy/paste buffer, freeing existing data first */
short copy_animedit_keys (bAnimContext *ac, ListBase *anim_data)
{	
	bAnimListElem *ale;
	
	/* clear buffer first */
	free_anim_copybuf();
	
	/* assume that each of these is an F-Curve */
	for (ale= anim_data->first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		tAnimCopybufItem *aci;
		BezTriple *bezt, *newbuf;
		int i;
		
		/* firstly, check if F-Curve has any selected keyframes
		 *	- skip if no selected keyframes found (so no need to create unnecessary copy-buffer data)
		 *	- this check should also eliminate any problems associated with using sample-data
		 */
		if (ANIM_fcurve_keys_bezier_loop(NULL, fcu, NULL, ANIM_editkeyframes_ok(BEZT_OK_SELECTED), NULL) == 0)
			continue;
		
		/* init copybuf item info */
		aci= MEM_callocN(sizeof(tAnimCopybufItem), "AnimCopybufItem");
		aci->id= ale->id;
		aci->grp= fcu->grp;
		aci->rna_path= MEM_dupallocN(fcu->rna_path);
		aci->array_index= fcu->array_index;
		BLI_addtail(&animcopybuf, aci);
		
		/* add selected keyframes to buffer */
		// TODO: currently, we resize array everytime we add a new vert - this works ok as long as it is assumed only a few keys are copied
		for (i=0, bezt=fcu->bezt; i < fcu->totvert; i++, bezt++) {
			if (BEZSELECTED(bezt)) {
				/* add to buffer */
				newbuf= MEM_callocN(sizeof(BezTriple)*(aci->totvert+1), "copybuf beztriple");
				
				/* assume that since we are just resizing the array, just copy all existing data across */
				if (aci->bezt)
					memcpy(newbuf, aci->bezt, sizeof(BezTriple)*(aci->totvert));
				
				/* copy current beztriple across too */
				*(newbuf + aci->totvert)= *bezt; 
				
				/* free old array and set the new */
				if (aci->bezt) MEM_freeN(aci->bezt);
				aci->bezt= newbuf;
				aci->totvert++;
				
				/* check if this is the earliest frame encountered so far */
				if (bezt->vec[1][0] < animcopy_firstframe)
					animcopy_firstframe= bezt->vec[1][0];
			}
		}
		
	}
	
	/* check if anything ended up in the buffer */
	if (ELEM(NULL, animcopybuf.first, animcopybuf.last))
		return -1;
	
	/* everything went fine */
	return 0;
}

/* This function pastes data from the keyframes copy/paste buffer */
short paste_animedit_keys (bAnimContext *ac, ListBase *anim_data)
{
	bAnimListElem *ale;
	const Scene *scene= (ac->scene);
	const float offset = (float)(CFRA - animcopy_firstframe);
	short no_name= 0;
	
	/* check if buffer is empty */
	if (ELEM(NULL, animcopybuf.first, animcopybuf.last)) {
		//error("No data in buffer to paste");
		return -1;
	}
	/* check if single channel in buffer (disregard names if so)  */
	if (animcopybuf.first == animcopybuf.last)
		no_name= 1;
	
	/* from selected channels */
	for (ale= anim_data->first; ale; ale= ale->next) {
		FCurve *fcu = (FCurve *)ale->data;		/* destination F-Curve */
		tAnimCopybufItem *aci= NULL;
		BezTriple *bezt;
		int i;
		
		/* find buffer item to paste from 
		 *	- if names don't matter (i.e. only 1 channel in buffer), don't check id/group
		 *	- if names do matter, only check if id-type is ok for now (group check is not that important)
		 *	- most importantly, rna-paths should match (array indices are unimportant for now)
		 */
		// TODO: the matching algorithm here is pathetic!
		for (aci= animcopybuf.first; aci; aci= aci->next) {
			/* check that paths exist */
			if (aci->rna_path && fcu->rna_path) {
				// FIXME: this breaks for bone names!
				if (strcmp(aci->rna_path, fcu->rna_path) == 0) {
					/* should be a match unless there's more than one of these */
					if ((no_name) || (aci->array_index == fcu->array_index)) 
						break;
				}
			}
		}
		
		
		/* copy the relevant data from the matching buffer curve */
		if (aci) {
			/* just start pasting, with the the first keyframe on the current frame, and so on */
			for (i=0, bezt=aci->bezt; i < aci->totvert; i++, bezt++) {						
				/* temporarily apply offset to src beztriple while copying */
				bezt->vec[0][0] += offset;
				bezt->vec[1][0] += offset;
				bezt->vec[2][0] += offset;
				
				/* insert the keyframe
				 * NOTE: no special flags here for now
				 */
				insert_bezt_fcurve(fcu, bezt, 0); 
				
				/* un-apply offset from src beztriple after copying */
				bezt->vec[0][0] -= offset;
				bezt->vec[1][0] -= offset;
				bezt->vec[2][0] -= offset;
			}
			
			/* recalculate F-Curve's handles? */
			calchandles_fcurve(fcu);
		}
	}
	
	return 0;
}

/* **************************************************** */
