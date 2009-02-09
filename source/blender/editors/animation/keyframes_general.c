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
#include "BLI_arithb.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h" // XXX to be removed
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

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
 * These may also be moved around at some point, but for now, they 
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
	
#if 0 // XXX for now, we don't get rid of empty curves...
	/* Only delete if there isn't an ipo-driver still hanging around on an empty curve */
	if ((icu->totvert==0) && (icu->driver==NULL)) {
		BLI_remlink(&ipo->curve, icu);
		free_ipo_curve(icu);
	}
#endif 
}

/* ---------------- */

/* duplicate selected keyframes for the given F-Curve */
void duplicate_fcurve_keys(FCurve *fcu)
{
	BezTriple *newbezt;
	int i;

	if (fcu == NULL)
		return;
	
	// XXX this does not take into account sample data...
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

/* Basic IPO-Curve 'cleanup' function that removes 'double points' and unnecessary keyframes on linear-segments only */
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

/* **************************************************** */
