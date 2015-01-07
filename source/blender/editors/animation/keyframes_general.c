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
 * The Original Code is Copyright (C) 2008 Blender Foundation
 * All rights reserved.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/keyframes_general.c
 *  \ingroup edanimation
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"


#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_report.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_deform.h"

#include "RNA_access.h"

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

/* Only delete the nominated keyframe from provided F-Curve. 
 * Not recommended to be used many times successively. For that
 * there is delete_fcurve_keys(). 
 */
void delete_fcurve_key(FCurve *fcu, int index, bool do_recalc)
{
	/* sanity check */
	if (fcu == NULL) 
		return;
		
	/* verify the index:
	 *	1) cannot be greater than the number of available keyframes
	 *	2) negative indices are for specifying a value from the end of the array
	 */
	if (abs(index) >= fcu->totvert)
		return;
	else if (index < 0)
		index += fcu->totvert;
	
	/* Delete this keyframe */
	memmove(&fcu->bezt[index], &fcu->bezt[index + 1], sizeof(BezTriple) * (fcu->totvert - index - 1));
	fcu->totvert--;

	if (fcu->totvert == 0) {
		if (fcu->bezt)
			MEM_freeN(fcu->bezt);
		fcu->bezt = NULL;
	}
	
	/* recalc handles - only if it won't cause problems */
	if (do_recalc)
		calchandles_fcurve(fcu);
}

/* Delete selected keyframes in given F-Curve */
bool delete_fcurve_keys(FCurve *fcu)
{
	int i;
	bool changed = false;
	
	if (fcu->bezt == NULL) /* ignore baked curves */
		return false;

	/* Delete selected BezTriples */
	for (i = 0; i < fcu->totvert; i++) {
		if (fcu->bezt[i].f2 & SELECT) {
			memmove(&fcu->bezt[i], &fcu->bezt[i + 1], sizeof(BezTriple) * (fcu->totvert - i - 1));
			fcu->totvert--;
			i--;
			changed = true;
		}
	}
	
	/* Free the array of BezTriples if there are not keyframes */
	if (fcu->totvert == 0)
		clear_fcurve_keys(fcu);

	return changed;
}


void clear_fcurve_keys(FCurve *fcu)
{
	if (fcu->bezt)
		MEM_freeN(fcu->bezt);
	fcu->bezt = NULL;

	fcu->totvert = 0;
}

/* ---------------- */

/* duplicate selected keyframes for the given F-Curve */
void duplicate_fcurve_keys(FCurve *fcu)
{
	BezTriple *newbezt;
	int i;
	
	/* this can only work when there is an F-Curve, and also when there are some BezTriples */
	if (ELEM(NULL, fcu, fcu->bezt))
		return;
	
	for (i = 0; i < fcu->totvert; i++) {
		/* If a key is selected */
		if (fcu->bezt[i].f2 & SELECT) {
			/* Expand the list */
			newbezt = MEM_callocN(sizeof(BezTriple) * (fcu->totvert + 1), "beztriple");
			
			memcpy(newbezt, fcu->bezt, sizeof(BezTriple) * (i + 1));
			memcpy(newbezt + i + 1, fcu->bezt + i, sizeof(BezTriple));
			memcpy(newbezt + i + 2, fcu->bezt + i + 1, sizeof(BezTriple) * (fcu->totvert - (i + 1)));
			fcu->totvert++;
			
			/* reassign pointers... (free old, and add new) */
			MEM_freeN(fcu->bezt);
			fcu->bezt = newbezt;
			
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
	if ((fcu == NULL) || (fcu->bezt == NULL) || (fcu->totvert <= 1))
		return;
	
	/* make a copy of the old BezTriples, and clear F-Curve */
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
	for (i = 1; i < totCount; i++) {
		float prev[2], cur[2], next[2];
		
		/* get BezTriples and their values */
		if (i < (totCount - 1)) {
			beztn = (old_bezts + (i + 1));
			next[0] = beztn->vec[1][0]; next[1] = beztn->vec[1][1];
		}
		else {
			beztn = NULL;
			next[0] = next[1] = 0.0f;
		}
		lastb = (fcu->bezt + (fcu->totvert - 1));
		bezt = (old_bezts + i);
		
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
			    (IS_EQT(next[1], prev[1], thresh) == 0))
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

/* temp struct used for smooth_fcurve */
typedef struct tSmooth_Bezt {
	float *h1, *h2, *h3;    /* bezt->vec[0,1,2][1] */
	float y1, y2, y3;       /* averaged before/new/after y-values */
} tSmooth_Bezt;

/* Use a weighted moving-means method to reduce intensity of fluctuations */
// TODO: introduce scaling factor for weighting falloff
void smooth_fcurve(FCurve *fcu)
{
	BezTriple *bezt;
	int i, x, totSel = 0;

	if (fcu->bezt == NULL) {
		return;
	}

	/* first loop through - count how many verts are selected */
	bezt = fcu->bezt;
	for (i = 0; i < fcu->totvert; i++, bezt++) {
		if (BEZSELECTED(bezt))
			totSel++;
	}
	
	/* if any points were selected, allocate tSmooth_Bezt points to work on */
	if (totSel >= 3) {
		tSmooth_Bezt *tarray, *tsb;
		
		/* allocate memory in one go */
		tsb = tarray = MEM_callocN(totSel * sizeof(tSmooth_Bezt), "tSmooth_Bezt Array");
		
		/* populate tarray with data of selected points */
		bezt = fcu->bezt;
		for (i = 0, x = 0; (i < fcu->totvert) && (x < totSel); i++, bezt++) {
			if (BEZSELECTED(bezt)) {
				/* tsb simply needs pointer to vec, and index */
				tsb->h1 = &bezt->vec[0][1];
				tsb->h2 = &bezt->vec[1][1];
				tsb->h3 = &bezt->vec[2][1];
				
				/* advance to the next tsb to populate */
				if (x < totSel - 1)
					tsb++;
				else
					break;
			}
		}
			
		/* calculate the new smoothed F-Curve's with weighted averages:
		 *	- this is done with two passes to avoid progressive corruption errors
		 *	- uses 5 points for each operation (which stores in the relevant handles)
		 *	-   previous: w/a ratio = 3:5:2:1:1
		 *	-   next: w/a ratio = 1:1:2:5:3
		 */
		
		/* round 1: calculate smoothing deltas and new values */ 
		tsb = tarray;
		for (i = 0; i < totSel; i++, tsb++) {
			/* don't touch end points (otherwise, curves slowly explode, as we don't have enough data there) */
			if (ELEM(i, 0, (totSel - 1)) == 0) {
				const tSmooth_Bezt *tP1 = tsb - 1;
				const tSmooth_Bezt *tP2 = (i - 2 > 0) ? (tsb - 2) : (NULL);
				const tSmooth_Bezt *tN1 = tsb + 1;
				const tSmooth_Bezt *tN2 = (i + 2 < totSel) ? (tsb + 2) : (NULL);
				
				const float p1 = *tP1->h2;
				const float p2 = (tP2) ? (*tP2->h2) : (*tP1->h2);
				const float c1 = *tsb->h2;
				const float n1 = *tN1->h2;
				const float n2 = (tN2) ? (*tN2->h2) : (*tN1->h2);
				
				/* calculate previous and next, then new position by averaging these */
				tsb->y1 = (3 * p2 + 5 * p1 + 2 * c1 + n1 + n2) / 12;
				tsb->y3 = (p2 + p1 + 2 * c1 + 5 * n1 + 3 * n2) / 12;
				
				tsb->y2 = (tsb->y1 + tsb->y3) / 2;
			}
		}
		
		/* round 2: apply new values */
		tsb = tarray;
		for (i = 0; i < totSel; i++, tsb++) {
			/* don't touch end points, as their values weren't touched above */
			if (ELEM(i, 0, (totSel - 1)) == 0) {
				/* y2 takes the average of the 2 points */
				*tsb->h2 = tsb->y2;
				
				/* handles are weighted between their original values and the averaged values */
				*tsb->h1 = ((*tsb->h1) * 0.7f) + (tsb->y1 * 0.3f); 
				*tsb->h3 = ((*tsb->h3) * 0.7f) + (tsb->y3 * 0.3f);
			}
		}
		
		/* free memory required for tarray */
		MEM_freeN(tarray);
	}
	
	/* recalculate handles */
	calchandles_fcurve(fcu);
}

/* ---------------- */

/* little cache for values... */
typedef struct TempFrameValCache {
	float frame, val;
} TempFrameValCache;


/* Evaluates the curves between each selected keyframe on each frame, and keys the value  */
void sample_fcurve(FCurve *fcu)
{
	BezTriple *bezt, *start = NULL, *end = NULL;
	TempFrameValCache *value_cache, *fp;
	int sfra, range;
	int i, n, nIndex;

	if (fcu->bezt == NULL) /* ignore baked */
		return;
	
	/* find selected keyframes... once pair has been found, add keyframes  */
	for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
		/* check if selected, and which end this is */
		if (BEZSELECTED(bezt)) {
			if (start) {
				/* set end */
				end = bezt;
				
				/* cache values then add keyframes using these values, as adding
				 * keyframes while sampling will affect the outcome...
				 *	- only start sampling+adding from index=1, so that we don't overwrite original keyframe
				 */
				range = (int)(ceil(end->vec[1][0] - start->vec[1][0]));
				sfra = (int)(floor(start->vec[1][0]));
				
				if (range) {
					value_cache = MEM_callocN(sizeof(TempFrameValCache) * range, "IcuFrameValCache");
					
					/* sample values */
					for (n = 1, fp = value_cache; n < range && fp; n++, fp++) {
						fp->frame = (float)(sfra + n);
						fp->val = evaluate_fcurve(fcu, fp->frame);
					}
					
					/* add keyframes with these, tagging as 'breakdowns' */
					for (n = 1, fp = value_cache; n < range && fp; n++, fp++) {
						nIndex = insert_vert_fcurve(fcu, fp->frame, fp->val, 1);
						BEZKEYTYPE(fcu->bezt + nIndex) = BEZT_KEYTYPE_BREAKDOWN;
					}
					
					/* free temp cache */
					MEM_freeN(value_cache);
					
					/* as we added keyframes, we need to compensate so that bezt is at the right place */
					bezt = fcu->bezt + i + range - 1;
					i += (range - 1);
				}
				
				/* bezt was selected, so it now marks the start of a whole new chain to search */
				start = bezt;
				end = NULL;
			}
			else {
				/* just set start keyframe */
				start = bezt;
				end = NULL;
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
static ListBase animcopybuf = {NULL, NULL};
static float animcopy_firstframe = 999999999.0f;
static float animcopy_lastframe = -999999999.0f;
static float animcopy_cfra = 0.0;

/* datatype for use in copy/paste buffer */
typedef struct tAnimCopybufItem {
	struct tAnimCopybufItem *next, *prev;
	
	ID *id;             /* ID which owns the curve */
	bActionGroup *grp;  /* Action Group */
	char *rna_path;     /* RNA-Path */
	int array_index;    /* array index */
	
	int totvert;        /* number of keyframes stored for this channel */
	BezTriple *bezt;    /* keyframes in buffer */

	short id_type;      /* Result of GS(id->name)*/
	bool  is_bone;      /* special flag for armature bones */
} tAnimCopybufItem;


/* This function frees any MEM_calloc'ed copy/paste buffer data */
// XXX find some header to put this in!
void free_anim_copybuf(void)
{
	tAnimCopybufItem *aci, *acn;
	
	/* free each buffer element */
	for (aci = animcopybuf.first; aci; aci = acn) {
		acn = aci->next;
		
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
	BLI_listbase_clear(&animcopybuf);
	animcopy_firstframe = 999999999.0f;
	animcopy_lastframe = -999999999.0f;
}

/* ------------------- */

/* This function adds data to the keyframes copy/paste buffer, freeing existing data first */
short copy_animedit_keys(bAnimContext *ac, ListBase *anim_data)
{	
	bAnimListElem *ale;
	Scene *scene = ac->scene;
	
	/* clear buffer first */
	free_anim_copybuf();
	
	/* assume that each of these is an F-Curve */
	for (ale = anim_data->first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->key_data;
		tAnimCopybufItem *aci;
		BezTriple *bezt, *nbezt, *newbuf;
		int i;
		
		/* firstly, check if F-Curve has any selected keyframes
		 *	- skip if no selected keyframes found (so no need to create unnecessary copy-buffer data)
		 *	- this check should also eliminate any problems associated with using sample-data
		 */
		if (ANIM_fcurve_keyframes_loop(NULL, fcu, NULL, ANIM_editkeyframes_ok(BEZT_OK_SELECTED), NULL) == 0)
			continue;
		
		/* init copybuf item info */
		aci = MEM_callocN(sizeof(tAnimCopybufItem), "AnimCopybufItem");
		aci->id = ale->id;
		aci->id_type = GS(ale->id->name);
		aci->grp = fcu->grp;
		aci->rna_path = MEM_dupallocN(fcu->rna_path);
		aci->array_index = fcu->array_index;
		
		/* detect if this is a bone. We do that here rather than during pasting because ID pointers will get invalidated if we undo.
		 * storing the relavant information here helps avoiding crashes if we undo-repaste */
		if ((aci->id_type == ID_OB) && (((Object *)aci->id)->type == OB_ARMATURE) && aci->rna_path) {
			Object *ob = (Object *)aci->id;
			char *str_start;
			
			if ((str_start = strstr(aci->rna_path, "pose.bones["))) {
				bPoseChannel *pchan;
				int length = 0;
				char *str_end;
				
				str_start += 12;
				str_end = strchr(str_start, '\"');
				length = str_end - str_start;
				str_start[length] = 0;
				pchan = BKE_pose_channel_find_name(ob->pose, str_start);
				str_start[length] = '\"';
		
				if (pchan) {
					aci->is_bone = true;
				}
			}
		}
		
		BLI_addtail(&animcopybuf, aci);
		
		/* add selected keyframes to buffer */
		/* TODO: currently, we resize array every time we add a new vert -
		 * this works ok as long as it is assumed only a few keys are copied */
		for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
			if (BEZSELECTED(bezt)) {
				/* add to buffer */
				newbuf = MEM_callocN(sizeof(BezTriple) * (aci->totvert + 1), "copybuf beztriple");
				
				/* assume that since we are just re-sizing the array, just copy all existing data across */
				if (aci->bezt)
					memcpy(newbuf, aci->bezt, sizeof(BezTriple) * (aci->totvert));
				
				/* copy current beztriple across too */
				nbezt = &newbuf[aci->totvert];
				*nbezt = *bezt;
				
				/* ensure copy buffer is selected so pasted keys are selected */
				BEZ_SEL(nbezt);
				
				/* free old array and set the new */
				if (aci->bezt) MEM_freeN(aci->bezt);
				aci->bezt = newbuf;
				aci->totvert++;
				
				/* check if this is the earliest frame encountered so far */
				if (bezt->vec[1][0] < animcopy_firstframe)
					animcopy_firstframe = bezt->vec[1][0];
				if (bezt->vec[1][0] > animcopy_lastframe)
					animcopy_lastframe = bezt->vec[1][0];
			}
		}
		
	}
	
	/* check if anything ended up in the buffer */
	if (ELEM(NULL, animcopybuf.first, animcopybuf.last))
		return -1;

	/* in case 'relative' paste method is used */
	animcopy_cfra = CFRA;

	/* everything went fine */
	return 0;
}

static void flip_names(tAnimCopybufItem *aci, char **name) {
	if (aci->is_bone) {
		char *str_start;
		if ((str_start = strstr(aci->rna_path, "pose.bones["))) {
			/* ninja coding, try to change the name */
			char bname_new[MAX_VGROUP_NAME];
			char *str_iter, *str_end;
			int length, prefix_l, postfix_l;
			
			str_start += 12;
			prefix_l = str_start - aci->rna_path;
			
			str_end = strchr(str_start, '\"');
			
			length = str_end - str_start;
			postfix_l = strlen(str_end);
			
			/* more ninja stuff, temporary substitute with NULL terminator */
			str_start[length] = 0;
			BKE_deform_flip_side_name(bname_new, str_start, false);
			str_start[length] = '\"';
			
			str_iter = *name = MEM_mallocN(sizeof(char) * (prefix_l + postfix_l + length + 1), "flipped_path");
			
			BLI_strncpy(str_iter, aci->rna_path, prefix_l + 1);
			str_iter += prefix_l ;
			BLI_strncpy(str_iter, bname_new, length + 1);
			str_iter += length;
			BLI_strncpy(str_iter, str_end, postfix_l + 1);
			str_iter[postfix_l] = 0;
		}
	}
}

/* ------------------- */

/* most strict method: exact matches only */
static tAnimCopybufItem *pastebuf_match_path_full(FCurve *fcu, const short from_single, const short to_simple, bool flip)
{
	tAnimCopybufItem *aci;

	for (aci = animcopybuf.first; aci; aci = aci->next) {
		if (to_simple || (aci->rna_path && fcu->rna_path)) {
			if (!to_simple && flip && aci->is_bone && fcu->rna_path) {
				if ((from_single) || (aci->array_index == fcu->array_index)) {
					char *name = NULL;
					flip_names(aci, &name);
					if (strcmp(name, fcu->rna_path) == 0) {
						MEM_freeN(name);
						break;
					}
					MEM_freeN(name);
				}
			}
			else if (to_simple || (strcmp(aci->rna_path, fcu->rna_path) == 0)) {
				if ((from_single) || (aci->array_index == fcu->array_index)) {
					break;
				}
			}
		}
	}

	return aci;
}

/* medium match strictness: path match only (i.e. ignore ID) */
static tAnimCopybufItem *pastebuf_match_path_property(FCurve *fcu, const short from_single, const short UNUSED(to_simple))
{
	tAnimCopybufItem *aci;

	for (aci = animcopybuf.first; aci; aci = aci->next) {
		/* check that paths exist */
		if (aci->rna_path && fcu->rna_path) {
			/* find the property of the fcurve and compare against the end of the tAnimCopybufItem
			 * more involved since it needs to to path lookups.
			 * This is not 100% reliable since the user could be editing the curves on a path that wont
			 * resolve, or a bone could be renamed after copying for eg. but in normal copy & paste
			 * this should work out ok. 
			 */
			if (BLI_findindex(which_libbase(G.main, aci->id_type), aci->id) == -1) {
				/* pedantic but the ID could have been removed, and beats crashing! */
				printf("paste_animedit_keys: error ID has been removed!\n");
			}
			else {
				PointerRNA id_ptr, rptr;
				PropertyRNA *prop;
				
				RNA_id_pointer_create(aci->id, &id_ptr);
				
				if (RNA_path_resolve_property(&id_ptr, aci->rna_path, &rptr, &prop)) {
					const char *identifier = RNA_property_identifier(prop);
					int len_id = strlen(identifier);
					int len_path = strlen(fcu->rna_path);
					if (len_id <= len_path) {
						/* note, paths which end with "] will fail with this test - Animated ID Props */
						if (strcmp(identifier, fcu->rna_path + (len_path - len_id)) == 0) {
							if ((from_single) || (aci->array_index == fcu->array_index))
								break;
						}
					}
				}
				else {
					printf("paste_animedit_keys: failed to resolve path id:%s, '%s'!\n", aci->id->name, aci->rna_path);
				}
			}
		}
	}

	return aci;
}

/* least strict matching heuristic: indices only */
static tAnimCopybufItem *pastebuf_match_index_only(FCurve *fcu, const short from_single, const short UNUSED(to_simple))
{
	tAnimCopybufItem *aci;

	for (aci = animcopybuf.first; aci; aci = aci->next) {
		/* check that paths exist */
		if ((from_single) || (aci->array_index == fcu->array_index)) {
			break;
		}
	}

	return aci;
}

/* ................ */

static void do_curve_mirror_flippping(tAnimCopybufItem *aci, BezTriple *bezt) {
	if (aci->is_bone) {
		int slength = strlen(aci->rna_path);
		bool flip = false;
		if (BLI_strn_ends_with(aci->rna_path, "location", slength) && aci->array_index == 0) 
			flip = true;
		else if (BLI_strn_ends_with(aci->rna_path, "rotation_quaternion", slength) && ELEM(aci->array_index, 2, 3))
			flip = true;
		else if (BLI_strn_ends_with(aci->rna_path, "rotation_euler", slength) && ELEM(aci->array_index, 1, 2))
			flip = true;
		else if (BLI_strn_ends_with(aci->rna_path, "rotation_axis_angle", slength) && ELEM(aci->array_index, 2, 3))
			flip = true;
		
		if (flip) {
			bezt->vec[0][1] = -bezt->vec[0][1];
			bezt->vec[1][1] = -bezt->vec[1][1];
			bezt->vec[2][1] = -bezt->vec[2][1];
		}
	}
}

/* helper for paste_animedit_keys() - performs the actual pasting */
static void paste_animedit_keys_fcurve(FCurve *fcu, tAnimCopybufItem *aci, float offset, const eKeyMergeMode merge_mode, bool flip)
{
	BezTriple *bezt;
	int i;

	/* First de-select existing FCurve's keyframes */
	for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
		bezt->f2 &= ~SELECT;
	}

	/* mix mode with existing data */
	switch (merge_mode) {
		case KEYFRAME_PASTE_MERGE_MIX:
			/* do-nothing */
			break;
			
		case KEYFRAME_PASTE_MERGE_OVER:
			/* remove all keys */
			clear_fcurve_keys(fcu);
			break;
			
		case KEYFRAME_PASTE_MERGE_OVER_RANGE:
		case KEYFRAME_PASTE_MERGE_OVER_RANGE_ALL:
		{
			float f_min;
			float f_max;
			
			if (merge_mode == KEYFRAME_PASTE_MERGE_OVER_RANGE) {
				f_min = aci->bezt[0].vec[1][0] + offset;
				f_max = aci->bezt[aci->totvert - 1].vec[1][0] + offset;
			}
			else { /* Entire Range */
				f_min = animcopy_firstframe + offset;
				f_max = animcopy_lastframe + offset;
			}
			
			/* remove keys in range */
			if (f_min < f_max) {
				/* select verts in range for removal */
				for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
					if ((f_min < bezt[0].vec[1][0]) && (bezt[0].vec[1][0] < f_max)) {
						bezt->f2 |= SELECT;
					}
				}
				
				/* remove frames in the range */
				delete_fcurve_keys(fcu);
			}
			break;
		}
	}
	
	/* just start pasting, with the first keyframe on the current frame, and so on */
	for (i = 0, bezt = aci->bezt; i < aci->totvert; i++, bezt++) {
		/* temporarily apply offset to src beztriple while copying */
		if (flip)
			do_curve_mirror_flippping(aci, bezt);
		
		bezt->vec[0][0] += offset;
		bezt->vec[1][0] += offset;
		bezt->vec[2][0] += offset;
		
		/* insert the keyframe
		 * NOTE: we do not want to inherit handles from existing keyframes in this case!
		 */
		
		insert_bezt_fcurve(fcu, bezt, INSERTKEY_OVERWRITE_FULL);

		/* un-apply offset from src beztriple after copying */
		bezt->vec[0][0] -= offset;
		bezt->vec[1][0] -= offset;
		bezt->vec[2][0] -= offset;
		
		if (flip)
			do_curve_mirror_flippping(aci, bezt);
	}
	
	/* recalculate F-Curve's handles? */
	calchandles_fcurve(fcu);
}

/* ------------------- */

EnumPropertyItem keyframe_paste_offset_items[] = {
	{KEYFRAME_PASTE_OFFSET_CFRA_START, "START", 0, "Frame Start", "Paste keys starting at current frame"},
	{KEYFRAME_PASTE_OFFSET_CFRA_END, "END", 0, "Frame End", "Paste keys ending at current frame"},
	{KEYFRAME_PASTE_OFFSET_CFRA_RELATIVE, "RELATIVE", 0, "Frame Relative", "Paste keys relative to the current frame when copying"},
	{KEYFRAME_PASTE_OFFSET_NONE, "NONE", 0, "No Offset", "Paste keys from original time"},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem keyframe_paste_merge_items[] = {
	{KEYFRAME_PASTE_MERGE_MIX, "MIX", 0, "Mix", "Overlay existing with new keys"},
	{KEYFRAME_PASTE_MERGE_OVER, "OVER_ALL", 0, "Overwrite All", "Replace all keys"},
	{KEYFRAME_PASTE_MERGE_OVER_RANGE, "OVER_RANGE", 0, "Overwrite Range", "Overwrite keys in pasted range"},
	{KEYFRAME_PASTE_MERGE_OVER_RANGE_ALL, "OVER_RANGE_ALL", 0, "Overwrite Entire Range", "Overwrite keys in pasted range, using the range of all copied keys"},
	{0, NULL, 0, NULL, NULL}};


/**
 * This function pastes data from the keyframes copy/paste buffer
 *
 * \return Status code is whether the method FAILED to do anything
 */
short paste_animedit_keys(bAnimContext *ac, ListBase *anim_data,
                          const eKeyPasteOffset offset_mode, const eKeyMergeMode merge_mode, bool flip)
{
	bAnimListElem *ale;
	
	const Scene *scene = (ac->scene);
	
	const bool from_single = BLI_listbase_is_single(&animcopybuf);
	const bool to_simple = BLI_listbase_is_single(anim_data);
	
	float offset = 0.0f;
	int pass;

	/* check if buffer is empty */
	if (BLI_listbase_is_empty(&animcopybuf)) {
		BKE_report(ac->reports, RPT_ERROR, "No animation data in buffer to paste");
		return -1;
	}

	if (BLI_listbase_is_empty(anim_data)) {
		BKE_report(ac->reports, RPT_ERROR, "No selected F-Curves to paste into");
		return -1;
	}
	
	/* mathods of offset */
	switch (offset_mode) {
		case KEYFRAME_PASTE_OFFSET_CFRA_START:
			offset = (float)(CFRA - animcopy_firstframe);
			break;
		case KEYFRAME_PASTE_OFFSET_CFRA_END:
			offset = (float)(CFRA - animcopy_lastframe);
			break;
		case KEYFRAME_PASTE_OFFSET_CFRA_RELATIVE:
			offset = (float)(CFRA - animcopy_cfra);
			break;
		case KEYFRAME_PASTE_OFFSET_NONE:
			offset = 0.0f;
			break;
	}

	if (from_single && to_simple) {
		/* 1:1 match, no tricky checking, just paste */
		FCurve *fcu;
		tAnimCopybufItem *aci;
		
		ale = anim_data->first;
		fcu = (FCurve *)ale->data;  /* destination F-Curve */
		aci = animcopybuf.first;
		
		paste_animedit_keys_fcurve(fcu, aci, offset, merge_mode, false);
	}
	else {
		/* from selected channels 
		 *  This "passes" system aims to try to find "matching" channels to paste keyframes
		 *  into with increasingly loose matching heuristics. The process finishes when at least
		 *  one F-Curve has been pasted into.
		 */
		for (pass = 0; pass < 3; pass++) {
			unsigned int totmatch = 0;
			
			for (ale = anim_data->first; ale; ale = ale->next) {
				/* find buffer item to paste from 
				 *	- if names don't matter (i.e. only 1 channel in buffer), don't check id/group
				 *	- if names do matter, only check if id-type is ok for now (group check is not that important)
				 *	- most importantly, rna-paths should match (array indices are unimportant for now)
				 */
				FCurve *fcu = (FCurve *)ale->data;  /* destination F-Curve */
				tAnimCopybufItem *aci = NULL;
				
				switch (pass) {
					case 0:
						/* most strict, must be exact path match data_path & index */
						aci = pastebuf_match_path_full(fcu, from_single, to_simple, flip);
						break;
					
					case 1:
						/* less strict, just compare property names */
						aci = pastebuf_match_path_property(fcu, from_single, to_simple);
						break;
					
					case 2:
						/* Comparing properties gave no results, so just do index comparisons */
						aci = pastebuf_match_index_only(fcu, from_single, to_simple);
						break;
				}
				
				/* copy the relevant data from the matching buffer curve */
				if (aci) {
					totmatch++;
					paste_animedit_keys_fcurve(fcu, aci, offset, merge_mode, flip);
				}

				ale->update |= ANIM_UPDATE_DEFAULT;
			}
			
			/* don't continue if some fcurves were pasted */
			if (totmatch)
				break;
		}
	}
	
	ANIM_animdata_update(ac, anim_data);

	return 0;
}

/* **************************************************** */
