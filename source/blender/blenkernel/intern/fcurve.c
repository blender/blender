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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/fcurve.c
 *  \ingroup bke
 */

 

#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_fcurve.h"
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h" 
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h" 
#endif

#define SMALL -1.0e-10
#define SELECT 1

/* ************************** Data-Level Functions ************************* */

/* ---------------------- Freeing --------------------------- */

/* Frees the F-Curve itself too, so make sure BLI_remlink is called before calling this... */
void free_fcurve (FCurve *fcu) 
{
	if (fcu == NULL) 
		return;
	
	/* free curve data */
	if (fcu) {
		if (fcu->bezt) MEM_freeN(fcu->bezt);
		if (fcu->fpt) MEM_freeN(fcu->fpt);
	}
	
	/* free RNA-path, as this were allocated when getting the path string */
	if (fcu->rna_path)
		MEM_freeN(fcu->rna_path);
	
	/* free extra data - i.e. modifiers, and driver */
	fcurve_free_driver(fcu);
	free_fmodifiers(&fcu->modifiers);
	
	/* free f-curve itself */
	MEM_freeN(fcu);
}

/* Frees a list of F-Curves */
void free_fcurves (ListBase *list)
{
	FCurve *fcu, *fcn;
	
	/* sanity check */
	if (list == NULL)
		return;
		
	/* free data - no need to call remlink before freeing each curve, 
	 * as we store reference to next, and freeing only touches the curve
	 * it's given
	 */
	for (fcu= list->first; fcu; fcu= fcn) {
		fcn= fcu->next;
		free_fcurve(fcu);
	}
	
	/* clear pointers just in case */
	list->first= list->last= NULL;
}	

/* ---------------------- Copy --------------------------- */

/* duplicate an F-Curve */
FCurve *copy_fcurve (FCurve *fcu)
{
	FCurve *fcu_d;
	
	/* sanity check */
	if (fcu == NULL)
		return NULL;
		
	/* make a copy */
	fcu_d= MEM_dupallocN(fcu);
	
	fcu_d->next= fcu_d->prev= NULL;
	fcu_d->grp= NULL;
	
	/* copy curve data */
	fcu_d->bezt= MEM_dupallocN(fcu_d->bezt);
	fcu_d->fpt= MEM_dupallocN(fcu_d->fpt);
	
	/* copy rna-path */
	fcu_d->rna_path= MEM_dupallocN(fcu_d->rna_path);
	
	/* copy driver */
	fcu_d->driver= fcurve_copy_driver(fcu_d->driver);
	
	/* copy modifiers */
	copy_fmodifiers(&fcu_d->modifiers, &fcu->modifiers);
	
	/* return new data */
	return fcu_d;
}

/* duplicate a list of F-Curves */
void copy_fcurves (ListBase *dst, ListBase *src)
{
	FCurve *dfcu, *sfcu;
	
	/* sanity checks */
	if ELEM(NULL, dst, src)
		return;
	
	/* clear destination list first */
	dst->first= dst->last= NULL;
	
	/* copy one-by-one */
	for (sfcu= src->first; sfcu; sfcu= sfcu->next) {
		dfcu= copy_fcurve(sfcu);
		BLI_addtail(dst, dfcu);
	}
}

/* ----------------- Finding F-Curves -------------------------- */

/* high level function to get an fcurve from C without having the rna */
FCurve *id_data_find_fcurve(ID *id, void *data, StructRNA *type, const char *prop_name, int index, char *driven)
{
	/* anim vars */
	AnimData *adt= BKE_animdata_from_id(id);
	FCurve *fcu= NULL;

	/* rna vars */
	PointerRNA ptr;
	PropertyRNA *prop;
	char *path;

	if(driven)
		*driven = FALSE;
	
	/* only use the current action ??? */
	if (ELEM(NULL, adt, adt->action))
		return NULL;
	
	RNA_pointer_create(id, type, data, &ptr);
	prop = RNA_struct_find_property(&ptr, prop_name);
	
	if (prop) {
		path= RNA_path_from_ID_to_property(&ptr, prop);
			
		if (path) {
			/* animation takes priority over drivers */
			if ((adt->action) && (adt->action->curves.first))
				fcu= list_find_fcurve(&adt->action->curves, path, index);
			
			/* if not animated, check if driven */
			if ((fcu == NULL) && (adt->drivers.first)) {
				fcu= list_find_fcurve(&adt->drivers, path, index);
				if(fcu && driven)
					*driven = TRUE;
				fcu = NULL;
			}
			
			MEM_freeN(path);
		}
	}

	return fcu;
}


/* Find the F-Curve affecting the given RNA-access path + index, in the list of F-Curves provided */
FCurve *list_find_fcurve (ListBase *list, const char rna_path[], const int array_index)
{
	FCurve *fcu;
	
	/* sanity checks */
	if ( ELEM(NULL, list, rna_path) || (array_index < 0) )
		return NULL;
	
	/* check paths of curves, then array indices... */
	for (fcu= list->first; fcu; fcu= fcu->next) {
		/* simple string-compare (this assumes that they have the same root...) */
		if (fcu->rna_path && !strcmp(fcu->rna_path, rna_path)) {
			/* now check indices */
			if (fcu->array_index == array_index)
				return fcu;
		}
	}
	
	/* return */
	return NULL;
}

/* quick way to loop over all fcurves of a given 'path' */
FCurve *iter_step_fcurve (FCurve *fcu_iter, const char rna_path[])
{
	FCurve *fcu;
	
	/* sanity checks */
	if (ELEM(NULL, fcu_iter, rna_path))
		return NULL;

	/* check paths of curves, then array indices... */
	for (fcu= fcu_iter; fcu; fcu= fcu->next) {
		/* simple string-compare (this assumes that they have the same root...) */
		if (fcu->rna_path && !strcmp(fcu->rna_path, rna_path)) {
			return fcu;
		}
	}

	/* return */
	return NULL;
}

/* Get list of LinkData's containing pointers to the F-Curves which control the types of data indicated 
 * Lists...
 *	- dst: list of LinkData's matching the criteria returned. 
 *	  List must be freed after use, and is assumed to be empty when passed.
 *	- src: list of F-Curves to search through
 * Filters...
 * 	- dataPrefix: i.e. 'pose.bones[' or 'nodes['
 *	- dataName: name of entity within "" immediately following the prefix
 */
int list_find_data_fcurves (ListBase *dst, ListBase *src, const char *dataPrefix, const char *dataName)
{
	FCurve *fcu;
	int matches = 0;
	
	/* sanity checks */
	if (ELEM4(NULL, dst, src, dataPrefix, dataName))
		return 0;
	else if ((dataPrefix[0] == 0) || (dataName[0] == 0))
		return 0;
	
	/* search each F-Curve one by one */
	for (fcu= src->first; fcu; fcu= fcu->next) {
		/* check if quoted string matches the path */
		if ((fcu->rna_path) && strstr(fcu->rna_path, dataPrefix)) {
			char *quotedName= BLI_getQuotedStr(fcu->rna_path, dataPrefix);
			
			if (quotedName) {
				/* check if the quoted name matches the required name */
				if (strcmp(quotedName, dataName) == 0) {
					LinkData *ld= MEM_callocN(sizeof(LinkData), "list_find_data_fcurves");
					
					ld->data= fcu;
					BLI_addtail(dst, ld);
					
					matches++;
				}
				
				/* always free the quoted string, since it needs freeing */
				MEM_freeN(quotedName);
			}
		}
	}
	
	/* return the number of matches */
	return matches;
}

FCurve *rna_get_fcurve (PointerRNA *ptr, PropertyRNA *prop, int rnaindex, bAction **action, int *driven)
{
	FCurve *fcu= NULL;
	
	*driven= 0;
	
	/* there must be some RNA-pointer + property combon */
	if (prop && ptr->id.data && RNA_property_animateable(ptr, prop)) {
		AnimData *adt= BKE_animdata_from_id(ptr->id.data);
		char *path;
		
		if (adt) {
			if ((adt->action && adt->action->curves.first) || (adt->drivers.first)) {
				/* XXX this function call can become a performance bottleneck */
				path= RNA_path_from_ID_to_property(ptr, prop);
				
				if (path) {
					/* animation takes priority over drivers */
					if (adt->action && adt->action->curves.first)
						fcu= list_find_fcurve(&adt->action->curves, path, rnaindex);
					
					/* if not animated, check if driven */
					if (!fcu && (adt->drivers.first)) {
						fcu= list_find_fcurve(&adt->drivers, path, rnaindex);
						
						if (fcu)
							*driven= 1;
					}
					
					if (fcu && action)
						*action= adt->action;
					
					MEM_freeN(path);
				}
			}
		}
	}
	
	return fcu;
}

/* ----------------- Finding Keyframes/Extents -------------------------- */

/* threshold for binary-searching keyframes - threshold here should be good enough for now, but should become userpref */
#define BEZT_BINARYSEARCH_THRESH 	0.01f /* was 0.00001, but giving errors */

/* Binary search algorithm for finding where to insert BezTriple. (for use by insert_bezt_fcurve)
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
int binarysearch_bezt_index (BezTriple array[], float frame, int arraylen, short *replace)
{
	int start=0, end=arraylen;
	int loopbreaker= 0, maxloop= arraylen * 2;
	
	/* initialize replace-flag first */
	*replace= 0;
	
	/* sneaky optimisations (don't go through searching process if...):
	 *	- keyframe to be added is to be added out of current bounds
	 *	- keyframe to be added would replace one of the existing ones on bounds
	 */
	if ((arraylen <= 0) || (array == NULL)) {
		printf("Warning: binarysearch_bezt_index() encountered invalid array \n");
		return 0;
	}
	else {
		/* check whether to add before/after/on */
		float framenum;
		
		/* 'First' Keyframe (when only one keyframe, this case is used) */
		framenum= array[0].vec[1][0];
		if (IS_EQT(frame, framenum, BEZT_BINARYSEARCH_THRESH)) {
			*replace = 1;
			return 0;
		}
		else if (frame < framenum)
			return 0;
			
		/* 'Last' Keyframe */
		framenum= array[(arraylen-1)].vec[1][0];
		if (IS_EQT(frame, framenum, BEZT_BINARYSEARCH_THRESH)) {
			*replace= 1;
			return (arraylen - 1);
		}
		else if (frame > framenum)
			return arraylen;
	}
	
	
	/* most of the time, this loop is just to find where to put it
	 * 'loopbreaker' is just here to prevent infinite loops 
	 */
	for (loopbreaker=0; (start <= end) && (loopbreaker < maxloop); loopbreaker++) {
		/* compute and get midpoint */
		int mid = start + ((end - start) / 2);	/* we calculate the midpoint this way to avoid int overflows... */
		float midfra= array[mid].vec[1][0];
		
		/* check if exactly equal to midpoint */
		if (IS_EQT(frame, midfra, BEZT_BINARYSEARCH_THRESH)) {
			*replace = 1;
			return mid;
		}
		
		/* repeat in upper/lower half */
		if (frame > midfra)
			start= mid + 1;
		else if (frame < midfra)
			end= mid - 1;
	}
	
	/* print error if loop-limit exceeded */
	if (loopbreaker == (maxloop-1)) {
		printf("Error: binarysearch_bezt_index() was taking too long \n");
		
		// include debug info 
		printf("\tround = %d: start = %d, end = %d, arraylen = %d \n", loopbreaker, start, end, arraylen);
	}
	
	/* not found, so return where to place it */
	return start;
}

/* ...................................... */

/* helper for calc_fcurve_* functions -> find first and last BezTriple to be used */
static void get_fcurve_end_keyframes (FCurve *fcu, BezTriple **first, BezTriple **last,
                                      const short do_sel_only)
{
	/* init outputs */
	*first = NULL;
	*last = NULL;
	
	/* sanity checks */
	if (fcu->bezt == NULL)
		return;
	
	/* only include selected items? */
	if (do_sel_only) {
		BezTriple *bezt;
		unsigned int i;
		
		/* find first selected */
		bezt = fcu->bezt;
		for (i=0; i < fcu->totvert; bezt++, i++) {
			if (BEZSELECTED(bezt)) {
				*first= bezt;
				break;
			}
		}
		
		/* find last selected */
		bezt = ARRAY_LAST_ITEM(fcu->bezt, BezTriple, sizeof(BezTriple), fcu->totvert);
		for (i=0; i < fcu->totvert; bezt--, i++) {
			if (BEZSELECTED(bezt)) {
				*last= bezt;
				break;
			}
		}
	}
	else {
		/* just full array */
		*first = fcu->bezt;
		*last = ARRAY_LAST_ITEM(fcu->bezt, BezTriple, sizeof(BezTriple), fcu->totvert);
	}
}


/* Calculate the extents of F-Curve's data */
void calc_fcurve_bounds (FCurve *fcu, float *xmin, float *xmax, float *ymin, float *ymax,
                         const short do_sel_only)
{
	float xminv=999999999.0f, xmaxv=-999999999.0f;
	float yminv=999999999.0f, ymaxv=-999999999.0f;
	short foundvert= FALSE;
	unsigned int i;
	
	if (fcu->totvert) {
		if (fcu->bezt) {
			BezTriple *bezt_first= NULL, *bezt_last= NULL;
			
			if (xmin || xmax) {
				/* get endpoint keyframes */
				get_fcurve_end_keyframes(fcu, &bezt_first, &bezt_last, do_sel_only);
				
				if (bezt_first) {
					BLI_assert(bezt_last != NULL);
					
					xminv= MIN2(xminv, bezt_first->vec[1][0]);
					xmaxv= MAX2(xmaxv, bezt_last->vec[1][0]);
				}
			}
			
			/* only loop over keyframes to find extents for values if needed */
			if (ymin || ymax) {	
				BezTriple *bezt;
				
				for (bezt=fcu->bezt, i=0; i < fcu->totvert; bezt++, i++) {
					if ((do_sel_only == 0) || BEZSELECTED(bezt)) {
						if (bezt->vec[1][1] < yminv)
							yminv= bezt->vec[1][1];
						if (bezt->vec[1][1] > ymaxv)
							ymaxv= bezt->vec[1][1];
						foundvert= TRUE;
					}
				}
			}
		}
		else if (fcu->fpt) {
			/* frame range can be directly calculated from end verts */
			if (xmin || xmax) {
				xminv= MIN2(xminv, fcu->fpt[0].vec[0]);
				xmaxv= MAX2(xmaxv, fcu->fpt[fcu->totvert-1].vec[0]);
			}
			
			/* only loop over keyframes to find extents for values if needed */
			if (ymin || ymax) {
				FPoint *fpt;
				
				for (fpt=fcu->fpt, i=0; i < fcu->totvert; fpt++, i++) {
					if (fpt->vec[1] < yminv)
						yminv= fpt->vec[1];
					if (fpt->vec[1] > ymaxv)
						ymaxv= fpt->vec[1];

					foundvert= TRUE;
				}
			}
		}
	}
	
	if (foundvert) {
		if (xmin) *xmin= xminv;
		if (xmax) *xmax= xmaxv;
		
		if (ymin) *ymin= yminv;
		if (ymax) *ymax= ymaxv;
	}
	else {
		if (G.f & G_DEBUG)
			printf("F-Curve calc bounds didn't find anything, so assuming minimum bounds of 1.0\n");
			
		if (xmin) *xmin= 0.0f;
		if (xmax) *xmax= 1.0f;
		
		if (ymin) *ymin= 0.0f;
		if (ymax) *ymax= 1.0f;
	}
}

/* Calculate the extents of F-Curve's keyframes */
void calc_fcurve_range (FCurve *fcu, float *start, float *end,
                        const short do_sel_only, const short do_min_length)
{
	float min=999999999.0f, max=-999999999.0f;
	short foundvert= FALSE;

	if (fcu->totvert) {
		if (fcu->bezt) {
			BezTriple *bezt_first= NULL, *bezt_last= NULL;
			
			/* get endpoint keyframes */
			get_fcurve_end_keyframes(fcu, &bezt_first, &bezt_last, do_sel_only);

			if (bezt_first) {
				BLI_assert(bezt_last != NULL);

				min= MIN2(min, bezt_first->vec[1][0]);
				max= MAX2(max, bezt_last->vec[1][0]);

				foundvert= TRUE;
			}
		}
		else if (fcu->fpt) {
			min= MIN2(min, fcu->fpt[0].vec[0]);
			max= MAX2(max, fcu->fpt[fcu->totvert-1].vec[0]);

			foundvert= TRUE;
		}
		
	}
	
	if (foundvert == FALSE) {
		min= max= 0.0f;
	}

	if (do_min_length) {
		/* minimum length is 1 frame */
		if (min == max) {
			max += 1.0f;
		}
	}

	*start= min;
	*end= max;
}

/* ----------------- Status Checks -------------------------- */

/* Are keyframes on F-Curve of any use? 
 * Usability of keyframes refers to whether they should be displayed,
 * and also whether they will have any influence on the final result.
 */
short fcurve_are_keyframes_usable (FCurve *fcu)
{
	/* F-Curve must exist */
	if (fcu == NULL)
		return 0;
		
	/* F-Curve must not have samples - samples are mutually exclusive of keyframes */
	if (fcu->fpt)
		return 0;
	
	/* if it has modifiers, none of these should "drastically" alter the curve */
	if (fcu->modifiers.first) {
		FModifier *fcm;
		
		/* check modifiers from last to first, as last will be more influential */
		// TODO: optionally, only check modifier if it is the active one...
		for (fcm = fcu->modifiers.last; fcm; fcm = fcm->prev) {
			/* ignore if muted/disabled */
			if (fcm->flag & (FMODIFIER_FLAG_DISABLED|FMODIFIER_FLAG_MUTED))
				continue;
				
			/* type checks */
			switch (fcm->type) {
				/* clearly harmless - do nothing */
				case FMODIFIER_TYPE_CYCLES:
				case FMODIFIER_TYPE_STEPPED:
				case FMODIFIER_TYPE_NOISE:
					break;
					
				/* sometimes harmful - depending on whether they're "additive" or not */
				case FMODIFIER_TYPE_GENERATOR:
				{
					FMod_Generator *data = (FMod_Generator *)fcm->data;
					
					if ((data->flag & FCM_GENERATOR_ADDITIVE) == 0)
						return 0;
				}
					break;
				case FMODIFIER_TYPE_FN_GENERATOR:
				{
					FMod_FunctionGenerator *data = (FMod_FunctionGenerator *)fcm->data;
					
					if ((data->flag & FCM_GENERATOR_ADDITIVE) == 0)
						return 0;
				}
					break;
					
				/* always harmful - cannot allow */
				default:
					return 0;
			}
		}
	}
	
	/* keyframes are usable */
	return 1;
}

/* Can keyframes be added to F-Curve? 
 * Keyframes can only be added if they are already visible
 */
short fcurve_is_keyframable (FCurve *fcu)
{
	/* F-Curve's keyframes must be "usable" (i.e. visible + have an effect on final result) */
	if (fcurve_are_keyframes_usable(fcu) == 0)
		return 0;
		
	/* F-Curve must currently be editable too */
	if ( (fcu->flag & FCURVE_PROTECTED) || ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) )
		return 0;
	
	/* F-Curve is keyframable */
	return 1;
}

/* ***************************** Keyframe Column Tools ********************************* */

/* add a BezTriple to a column */
void bezt_add_to_cfra_elem (ListBase *lb, BezTriple *bezt)
{
	CfraElem *ce, *cen;
	
	for (ce= lb->first; ce; ce= ce->next) {
		/* double key? */
		if (ce->cfra == bezt->vec[1][0]) {
			if (bezt->f2 & SELECT) ce->sel= bezt->f2;
			return;
		}
		/* should key be inserted before this column? */
		else if (ce->cfra > bezt->vec[1][0]) break;
	}
	
	/* create a new column */
	cen= MEM_callocN(sizeof(CfraElem), "add_to_cfra_elem");	
	if (ce) BLI_insertlinkbefore(lb, ce, cen);
	else BLI_addtail(lb, cen);

	cen->cfra= bezt->vec[1][0];
	cen->sel= bezt->f2;
}

/* ***************************** Samples Utilities ******************************* */
/* Some utilities for working with FPoints (i.e. 'sampled' animation curve data, such as
 * data imported from BVH/Mocap files), which are specialized for use with high density datasets,
 * which BezTriples/Keyframe data are ill equipped to do.
 */
 
 
/* Basic sampling callback which acts as a wrapper for evaluate_fcurve() 
 *	'data' arg here is unneeded here...
 */
float fcurve_samplingcb_evalcurve (FCurve *fcu, void *UNUSED(data), float evaltime)
{
	/* assume any interference from drivers on the curve is intended... */
	return evaluate_fcurve(fcu, evaltime);
} 

 
/* Main API function for creating a set of sampled curve data, given some callback function 
 * used to retrieve the values to store.
 */
void fcurve_store_samples (FCurve *fcu, void *data, int start, int end, FcuSampleFunc sample_cb)
{
	FPoint *fpt, *new_fpt;
	int cfra;
	
	/* sanity checks */
	// TODO: make these tests report errors using reports not printf's
	if ELEM(NULL, fcu, sample_cb) {
		printf("Error: No F-Curve with F-Curve Modifiers to Bake\n");
		return;
	}
	if (start >= end) {
		printf("Error: Frame range for Sampled F-Curve creation is inappropriate \n");
		return;
	}
	
	/* set up sample data */
	fpt= new_fpt= MEM_callocN(sizeof(FPoint)*(end-start+1), "FPoint Samples");
	
	/* use the sampling callback at 1-frame intervals from start to end frames */
	for (cfra= start; cfra <= end; cfra++, fpt++) {
		fpt->vec[0]= (float)cfra;
		fpt->vec[1]= sample_cb(fcu, data, (float)cfra);
	}
	
	/* free any existing sample/keyframe data on curve  */
	if (fcu->bezt) MEM_freeN(fcu->bezt);
	if (fcu->fpt) MEM_freeN(fcu->fpt);
	
	/* store the samples */
	fcu->bezt= NULL;
	fcu->fpt= new_fpt;
	fcu->totvert= end - start + 1;
}

/* ***************************** F-Curve Sanity ********************************* */
/* The functions here are used in various parts of Blender, usually after some editing
 * of keyframe data has occurred. They ensure that keyframe data is properly ordered and
 * that the handles are correctly 
 */

/* This function recalculates the handles of an F-Curve 
 * If the BezTriples have been rearranged, sort them first before using this.
 */
void calchandles_fcurve (FCurve *fcu)
{
	BezTriple *bezt, *prev, *next;
	int a= fcu->totvert;

	/* Error checking:
	 *	- need at least two points
	 *	- need bezier keys
	 *	- only bezier-interpolation has handles (for now)
	 */
	if (ELEM(NULL, fcu, fcu->bezt) || (a < 2) /*|| ELEM(fcu->ipo, BEZT_IPO_CONST, BEZT_IPO_LIN)*/) 
		return;
	
	/* get initial pointers */
	bezt= fcu->bezt;
	prev= NULL;
	next= (bezt + 1);
	
	/* loop over all beztriples, adjusting handles */
	while (a--) {
		/* clamp timing of handles to be on either side of beztriple */
		if (bezt->vec[0][0] > bezt->vec[1][0]) bezt->vec[0][0]= bezt->vec[1][0];
		if (bezt->vec[2][0] < bezt->vec[1][0]) bezt->vec[2][0]= bezt->vec[1][0];
		
		/* calculate auto-handles */
		calchandleNurb(bezt, prev, next, 1);	/* 1==special autohandle */
		
		/* for automatic ease in and out */
		if (ELEM(bezt->h1,HD_AUTO,HD_AUTO_ANIM) && ELEM(bezt->h2,HD_AUTO,HD_AUTO_ANIM)) {
			/* only do this on first or last beztriple */
			if ((a == 0) || (a == fcu->totvert-1)) {
				/* set both handles to have same horizontal value as keyframe */
				if (fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) {
					bezt->vec[0][1]= bezt->vec[2][1]= bezt->vec[1][1];
				}
			}
		}
		
		/* advance pointers for next iteration */
		prev= bezt;
		if (a == 1) next= NULL;
		else next++;
		bezt++;
	}
}

/* Use when F-Curve with handles has changed
 * It treats all BezTriples with the following rules:
 *  - PHASE 1: do types have to be altered?
 * 		-> Auto handles: become aligned when selection status is NOT(000 || 111)
 * 		-> Vector handles: become 'nothing' when (one half selected AND other not)
 *  - PHASE 2: recalculate handles
 */
void testhandles_fcurve (FCurve *fcu, const short use_handle)
{
	BezTriple *bezt;
	unsigned int a;

	/* only beztriples have handles (bpoints don't though) */
	if ELEM(NULL, fcu, fcu->bezt)
		return;
	
	/* loop over beztriples */
	for (a=0, bezt=fcu->bezt; a < fcu->totvert; a++, bezt++) {
		short flag= 0;
		
		/* flag is initialised as selection status
		 * of beztriple control-points (labelled 0,1,2)
		 */
		if (bezt->f2 & SELECT) flag |= (1<<1); // == 2
		if(use_handle == FALSE) {
			if(flag & 2) {
				flag |= (1<<0) | (1<<2);
			}
		}
		else {
			if (bezt->f1 & SELECT) flag |= (1<<0); // == 1
			if (bezt->f3 & SELECT) flag |= (1<<2); // == 4
		}
		
		/* one or two handles selected only */
		if (ELEM(flag, 0, 7)==0) {
			/* auto handles become aligned */
			if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM))
				bezt->h1= HD_ALIGN;
			if (ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM))
				bezt->h2= HD_ALIGN;
			
			/* vector handles become 'free' when only one half selected */
			if (bezt->h1==HD_VECT) {
				/* only left half (1 or 2 or 1+2) */
				if (flag < 4) 
					bezt->h1= 0;
			}
			if (bezt->h2==HD_VECT) {
				/* only right half (4 or 2+4) */
				if (flag > 3) 
					bezt->h2= 0;
			}
		}
	}

	/* recalculate handles */
	calchandles_fcurve(fcu);
}

/* This function sorts BezTriples so that they are arranged in chronological order,
 * as tools working on F-Curves expect that the BezTriples are in order.
 */
void sort_time_fcurve (FCurve *fcu)
{
	short ok= 1;
	
	/* keep adjusting order of beztriples until nothing moves (bubble-sort) */
	while (ok) {
		ok= 0;
		
		/* currently, will only be needed when there are beztriples */
		if (fcu->bezt) {
			BezTriple *bezt;
			unsigned int a;
			
			/* loop over ALL points to adjust position in array and recalculate handles */
			for (a=0, bezt=fcu->bezt; a < fcu->totvert; a++, bezt++) {
				/* check if thee's a next beztriple which we could try to swap with current */
				if (a < (fcu->totvert-1)) {
					/* swap if one is after the other (and indicate that order has changed) */
					if (bezt->vec[1][0] > (bezt+1)->vec[1][0]) {
						SWAP(BezTriple, *bezt, *(bezt+1));
						ok= 1;
					}
					
					/* if either one of both of the points exceeds crosses over the keyframe time... */
					if ( (bezt->vec[0][0] > bezt->vec[1][0]) && (bezt->vec[2][0] < bezt->vec[1][0]) ) {
						/* swap handles if they have switched sides for some reason */
						SWAP(float, bezt->vec[0][0], bezt->vec[2][0]);
						SWAP(float, bezt->vec[0][1], bezt->vec[2][1]);
					}
					else {
						/* clamp handles */
						if (bezt->vec[0][0] > bezt->vec[1][0]) 
							bezt->vec[0][0]= bezt->vec[1][0];
						if (bezt->vec[2][0] < bezt->vec[1][0]) 
							bezt->vec[2][0]= bezt->vec[1][0];
					}
				}
			}
		}
	}
}

/* This function tests if any BezTriples are out of order, thus requiring a sort */
short test_time_fcurve (FCurve *fcu)
{
	unsigned int a;
	
	/* sanity checks */
	if (fcu == NULL)
		return 0;
	
	/* currently, only need to test beztriples */
	if (fcu->bezt) {
		BezTriple *bezt;
		
		/* loop through all BezTriples, stopping when one exceeds the one after it */
		for (a=0, bezt= fcu->bezt; a < (fcu->totvert - 1); a++, bezt++) {
			if (bezt->vec[1][0] > (bezt+1)->vec[1][0])
				return 1;
		}
	}
	else if (fcu->fpt) {
		FPoint *fpt;
		
		/* loop through all FPoints, stopping when one exceeds the one after it */
		for (a=0, fpt= fcu->fpt; a < (fcu->totvert - 1); a++, fpt++) {
			if (fpt->vec[0] > (fpt+1)->vec[0])
				return 1;
		}
	}
	
	/* none need any swapping */
	return 0;
}

/* ***************************** Drivers ********************************* */

/* Driver Variables --------------------------- */

/* TypeInfo for Driver Variables (dvti) */
typedef struct DriverVarTypeInfo {
	/* evaluation callback */
	float (*get_value)(ChannelDriver *driver, DriverVar *dvar);
	
	/* allocation of target slots */
	int num_targets; 						/* number of target slots required */
	const char *target_names[MAX_DRIVER_TARGETS];	/* UI names that should be given to the slots */
	int target_flags[MAX_DRIVER_TARGETS];	/* flags defining the requirements for each slot */
} DriverVarTypeInfo;

/* Macro to begin definitions */
#define BEGIN_DVAR_TYPEDEF(type) \
	{
	
/* Macro to end definitions */
#define END_DVAR_TYPEDEF \
	}

/* ......... */

static ID *dtar_id_ensure_proxy_from(ID *id)
{
	if (id && GS(id->name)==ID_OB && ((Object *)id)->proxy_from)
		return (ID *)(((Object *)id)->proxy_from);
	return id;
}

/* Helper function to obtain a value using RNA from the specified source (for evaluating drivers) */
static float dtar_get_prop_val (ChannelDriver *driver, DriverTarget *dtar)
{
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	ID *id;
	int index;
	float value= 0.0f;
	
	/* sanity check */
	if ELEM(NULL, driver, dtar)
		return 0.0f;
	
	id= dtar_id_ensure_proxy_from(dtar->id);
	
	/* error check for missing pointer... */
	// TODO: tag the specific target too as having issues
	if (id == NULL) {
		printf("Error: driver has an invalid target to use \n");
		if (G.f & G_DEBUG) printf("\tpath = %s\n", dtar->rna_path);
		driver->flag |= DRIVER_FLAG_INVALID;
		return 0.0f;
	}
	
	/* get RNA-pointer for the ID-block given in target */
	RNA_id_pointer_create(id, &id_ptr);
	
	/* get property to read from, and get value as appropriate */
	if (RNA_path_resolve_full(&id_ptr, dtar->rna_path, &ptr, &prop, &index)) {
		if(RNA_property_array_check(prop)) {
			/* array */
			if (index < RNA_property_array_length(&ptr, prop)) {	
				switch (RNA_property_type(prop)) {
				case PROP_BOOLEAN:
					value= (float)RNA_property_boolean_get_index(&ptr, prop, index);
					break;
				case PROP_INT:
					value= (float)RNA_property_int_get_index(&ptr, prop, index);
					break;
				case PROP_FLOAT:
					value= RNA_property_float_get_index(&ptr, prop, index);
					break;
				default:
					break;
				}
			}
		}
		else {
			/* not an array */
			switch (RNA_property_type(prop)) {
			case PROP_BOOLEAN:
				value= (float)RNA_property_boolean_get(&ptr, prop);
				break;
			case PROP_INT:
				value= (float)RNA_property_int_get(&ptr, prop);
				break;
			case PROP_FLOAT:
				value= RNA_property_float_get(&ptr, prop);
				break;
			case PROP_ENUM:
				value= (float)RNA_property_enum_get(&ptr, prop);
				break;
			default:
				break;
			}
		}

	}
	else {
		if (G.f & G_DEBUG)
			printf("Driver Evaluation Error: cannot resolve target for %s -> %s \n", id->name, dtar->rna_path);
		
		driver->flag |= DRIVER_FLAG_INVALID;
		return 0.0f;
	}
	
	return value;
}

/* Helper function to obtain a pointer to a Pose Channel (for evaluating drivers) */
static bPoseChannel *dtar_get_pchan_ptr (ChannelDriver *driver, DriverTarget *dtar)
{
	ID *id;
	/* sanity check */
	if ELEM(NULL, driver, dtar)
		return NULL;

	id= dtar_id_ensure_proxy_from(dtar->id);

	/* check if the ID here is a valid object */
	if (id && GS(id->name)) {
		Object *ob= (Object *)id;
		
		/* get pose, and subsequently, posechannel */
		return get_pose_channel(ob->pose, dtar->pchan_name);
	}
	else {
		/* cannot find a posechannel this way */
		return NULL;
	}
}

/* ......... */

/* evaluate 'single prop' driver variable */
static float dvar_eval_singleProp (ChannelDriver *driver, DriverVar *dvar)
{
	/* just evaluate the first target slot */
	return dtar_get_prop_val(driver, &dvar->targets[0]);
}

/* evaluate 'rotation difference' driver variable */
static float dvar_eval_rotDiff (ChannelDriver *driver, DriverVar *dvar)
{
	bPoseChannel *pchan, *pchan2;
	float q1[4], q2[4], quat[4], angle;
	
	/* get pose channels, and check if we've got two */
	pchan= dtar_get_pchan_ptr(driver, &dvar->targets[0]);
	pchan2= dtar_get_pchan_ptr(driver, &dvar->targets[1]);
	
	if (ELEM(NULL, pchan, pchan2)) {
		/* disable this driver, since it doesn't work correctly... */
		driver->flag |= DRIVER_FLAG_INVALID;
		
		/* check what the error was */
		if ((pchan == NULL) && (pchan2 == NULL))
			printf("Driver Evaluation Error: Rotational difference failed - first 2 targets invalid \n");
		else if (pchan == NULL)
			printf("Driver Evaluation Error: Rotational difference failed - first target not valid PoseChannel \n");
		else if (pchan2 == NULL)
			printf("Driver Evaluation Error: Rotational difference failed - second target not valid PoseChannel \n");
			
		/* stop here... */
		return 0.0f;
	}			
	
	/* use the final posed locations */
	mat4_to_quat(q1, pchan->pose_mat);
	mat4_to_quat(q2, pchan2->pose_mat);
	
	invert_qt(q1);
	mul_qt_qtqt(quat, q1, q2);
	angle = 2.0f * (saacos(quat[0]));
	angle= ABS(angle);
	
	return (angle > (float)M_PI) ? (float)((2.0f * (float)M_PI) - angle) : (float)(angle);
}

/* evaluate 'location difference' driver variable */
// TODO: this needs to take into account space conversions...
static float dvar_eval_locDiff (ChannelDriver *driver, DriverVar *dvar)
{
	float loc1[3] = {0.0f,0.0f,0.0f};
	float loc2[3] = {0.0f,0.0f,0.0f};
	
	/* get two location values */
	// NOTE: for now, these are all just worldspace
	DRIVER_TARGETS_USED_LOOPER(dvar)
	{
		/* get pointer to loc values to store in */
		Object *ob= (Object *)dtar_id_ensure_proxy_from(dtar->id);
		bPoseChannel *pchan;
		float tmp_loc[3];
		
		/* check if this target has valid data */
		if ((ob == NULL) || (GS(ob->id.name) != ID_OB)) {
			/* invalid target, so will not have enough targets */
			driver->flag |= DRIVER_FLAG_INVALID;
			return 0.0f;
		}
		
		/* try to get posechannel */
		pchan= get_pose_channel(ob->pose, dtar->pchan_name);
		
		/* check if object or bone */
		if (pchan) {
			/* bone */
			if (dtar->flag & DTAR_FLAG_LOCALSPACE) {
				if (dtar->flag & DTAR_FLAG_LOCAL_CONSTS) {
					float mat[4][4];
					
					/* extract transform just like how the constraints do it! */
					copy_m4_m4(mat, pchan->pose_mat);
					constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_LOCAL);
					
					/* ... and from that, we get our transform */
					copy_v3_v3(tmp_loc, mat[3]);
				}
				else {
					/* transform space (use transform values directly) */
					copy_v3_v3(tmp_loc, pchan->loc);
				}
			}
			else {
				/* convert to worldspace */
				copy_v3_v3(tmp_loc, pchan->pose_head);
				mul_m4_v3(ob->obmat, tmp_loc);
			}
		}
		else {
			/* object */
			if (dtar->flag & DTAR_FLAG_LOCALSPACE) {
				if (dtar->flag & DTAR_FLAG_LOCAL_CONSTS) {
					// XXX: this should practically be the same as transform space...
					float mat[4][4];
					
					/* extract transform just like how the constraints do it! */
					copy_m4_m4(mat, ob->obmat);
					constraint_mat_convertspace(ob, NULL, mat, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL);
					
					/* ... and from that, we get our transform */
					copy_v3_v3(tmp_loc, mat[3]);
				}
				else {
					/* transform space (use transform values directly) */
					copy_v3_v3(tmp_loc, ob->loc);
				}
			}
			else {
				/* worldspace */
				copy_v3_v3(tmp_loc, ob->obmat[3]);
			}
		}
		
		/* copy the location to the right place */
		if (tarIndex) {
			copy_v3_v3(loc2, tmp_loc);
		}
		else {
			copy_v3_v3(loc1, tmp_loc);
		}
	}
	DRIVER_TARGETS_LOOPER_END
	
	
	/* if we're still here, there should now be two targets to use,
	 * so just take the length of the vector between these points 
	 */
	return len_v3v3(loc1, loc2);
}

/* evaluate 'transform channel' driver variable */
static float dvar_eval_transChan (ChannelDriver *driver, DriverVar *dvar)
{
	DriverTarget *dtar= &dvar->targets[0];
	Object *ob= (Object *)dtar_id_ensure_proxy_from(dtar->id);
	bPoseChannel *pchan;
	float mat[4][4];
	float oldEul[3] = {0.0f,0.0f,0.0f};
	short useEulers=0, rotOrder=ROT_MODE_EUL;
	
	/* check if this target has valid data */
	if ((ob == NULL) || (GS(ob->id.name) != ID_OB)) {
		/* invalid target, so will not have enough targets */
		driver->flag |= DRIVER_FLAG_INVALID;
		return 0.0f;
	}
	
	/* try to get posechannel */
	pchan= get_pose_channel(ob->pose, dtar->pchan_name);
	
	/* check if object or bone, and get transform matrix accordingly 
	 *	- "useEulers" code is used to prevent the problems associated with non-uniqueness
	 *	  of euler decomposition from matrices [#20870]
	 *	- localspace is for [#21384], where parent results are not wanted
	 *	  but local-consts is for all the common "corrective-shapes-for-limbs" situations
	 */
	if (pchan) {
		/* bone */
		if (pchan->rotmode > 0) {
			copy_v3_v3(oldEul, pchan->eul);
			rotOrder= pchan->rotmode;
			useEulers = 1;
		}
		
		if (dtar->flag & DTAR_FLAG_LOCALSPACE) {
			if (dtar->flag & DTAR_FLAG_LOCAL_CONSTS) {
				/* just like how the constraints do it! */
				copy_m4_m4(mat, pchan->pose_mat);
				constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_LOCAL);
			}
			else {
				/* specially calculate local matrix, since chan_mat is not valid 
				 * since it stores delta transform of pose_mat so that deforms work
				 * so it cannot be used here for "transform" space
				 */
				pchan_to_mat4(pchan, mat);
			}
		}
		else {
			/* worldspace matrix */
			mult_m4_m4m4(mat, ob->obmat, pchan->pose_mat);
		}
	}
	else {
		/* object */
		if (ob->rotmode > 0) {
			copy_v3_v3(oldEul, ob->rot);
			rotOrder= ob->rotmode;
			useEulers = 1;
		}
		
		if (dtar->flag & DTAR_FLAG_LOCALSPACE) {
			if (dtar->flag & DTAR_FLAG_LOCAL_CONSTS) {
				/* just like how the constraints do it! */
				copy_m4_m4(mat, ob->obmat);
				constraint_mat_convertspace(ob, NULL, mat, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL);
			}
			else {
				/* transforms to matrix */
				object_to_mat4(ob, mat);
			}
		}
		else {
			/* worldspace matrix - just the good-old one */
			copy_m4_m4(mat, ob->obmat);
		}
	}
	
	/* check which transform */
	if (dtar->transChan >= MAX_DTAR_TRANSCHAN_TYPES) {
		/* not valid channel */
		return 0.0f;
	}
	else if (dtar->transChan >= DTAR_TRANSCHAN_SCALEX) {
		/* extract scale, and choose the right axis */
		float scale[3];
		
		mat4_to_size(scale, mat);
		return scale[dtar->transChan - DTAR_TRANSCHAN_SCALEX];
	}
	else if (dtar->transChan >= DTAR_TRANSCHAN_ROTX) {
		/* extract rotation as eulers (if needed) 
		 *	- definitely if rotation order isn't eulers already
		 *	- if eulers, then we have 2 options:
		 *		a) decompose transform matrix as required, then try to make eulers from
		 *		   there compatible with original values
		 *		b) [NOT USED] directly use the original values (no decomposition) 
		 *			- only an option for "transform space", if quality is really bad with a)
		 */
		float eul[3];
		
		mat4_to_eulO(eul, rotOrder, mat);
		
		if (useEulers) {
			compatible_eul(eul, oldEul);
		}
		
		return eul[dtar->transChan - DTAR_TRANSCHAN_ROTX];
	}
	else {
		/* extract location and choose right axis */
		return mat[3][dtar->transChan];
	}
}

/* ......... */

/* Table of Driver Varaiable Type Info Data */
static DriverVarTypeInfo dvar_types[MAX_DVAR_TYPES] = {
	BEGIN_DVAR_TYPEDEF(DVAR_TYPE_SINGLE_PROP)
		dvar_eval_singleProp, /* eval callback */
		1, /* number of targets used */
		{"Property"}, /* UI names for targets */
		{0} /* flags */
	END_DVAR_TYPEDEF,
	
	BEGIN_DVAR_TYPEDEF(DVAR_TYPE_ROT_DIFF)
		dvar_eval_rotDiff, /* eval callback */
		2, /* number of targets used */
		{"Bone 1", "Bone 2"}, /* UI names for targets */
		{DTAR_FLAG_STRUCT_REF|DTAR_FLAG_ID_OB_ONLY, DTAR_FLAG_STRUCT_REF|DTAR_FLAG_ID_OB_ONLY} /* flags */
	END_DVAR_TYPEDEF,
	
	BEGIN_DVAR_TYPEDEF(DVAR_TYPE_LOC_DIFF)
		dvar_eval_locDiff, /* eval callback */
		2, /* number of targets used */
		{"Object/Bone 1", "Object/Bone 2"}, /* UI names for targets */
		{DTAR_FLAG_STRUCT_REF|DTAR_FLAG_ID_OB_ONLY, DTAR_FLAG_STRUCT_REF|DTAR_FLAG_ID_OB_ONLY} /* flags */
	END_DVAR_TYPEDEF,
	
	BEGIN_DVAR_TYPEDEF(DVAR_TYPE_TRANSFORM_CHAN)
		dvar_eval_transChan, /* eval callback */
		1, /* number of targets used */
		{"Object/Bone"}, /* UI names for targets */
		{DTAR_FLAG_STRUCT_REF|DTAR_FLAG_ID_OB_ONLY} /* flags */
	END_DVAR_TYPEDEF,
};

/* Get driver variable typeinfo */
static DriverVarTypeInfo *get_dvar_typeinfo (int type)
{
	/* check if valid type */
	if ((type >= 0) && (type < MAX_DVAR_TYPES))
		return &dvar_types[type];
	else
		return NULL;
}

/* Driver API --------------------------------- */

/* This frees the driver variable itself */
void driver_free_variable (ChannelDriver *driver, DriverVar *dvar)
{
	/* sanity checks */
	if (dvar == NULL)
		return;
		
	/* free target vars 
	 *	- need to go over all of them, not just up to the ones that are used
	 *	  currently, since there may be some lingering RNA paths from 
	 * 	  previous users needing freeing
	 */
	DRIVER_TARGETS_LOOPER(dvar) 
	{
		/* free RNA path if applicable */
		if (dtar->rna_path)
			MEM_freeN(dtar->rna_path);
	}
	DRIVER_TARGETS_LOOPER_END
	
	/* remove the variable from the driver */
	BLI_freelinkN(&driver->variables, dvar);

#ifdef WITH_PYTHON
	/* since driver variables are cached, the expression needs re-compiling too */
	if(driver->type==DRIVER_TYPE_PYTHON)
		driver->flag |= DRIVER_FLAG_RENAMEVAR;
#endif
}

/* Change the type of driver variable */
void driver_change_variable_type (DriverVar *dvar, int type)
{
	DriverVarTypeInfo *dvti= get_dvar_typeinfo(type);
	
	/* sanity check */
	if (ELEM(NULL, dvar, dvti))
		return;
		
	/* set the new settings */
	dvar->type= type;
	dvar->num_targets= dvti->num_targets;
	
	/* make changes to the targets based on the defines for these types 
	 * NOTE: only need to make sure the ones we're using here are valid...
	 */
	DRIVER_TARGETS_USED_LOOPER(dvar)
	{
		int flags = dvti->target_flags[tarIndex];
		
		/* store the flags */
		dtar->flag = flags;
		
		/* object ID types only, or idtype not yet initialised*/
		if ((flags & DTAR_FLAG_ID_OB_ONLY) || (dtar->idtype == 0))
			dtar->idtype= ID_OB;
	}
	DRIVER_TARGETS_LOOPER_END
}

/* Add a new driver variable */
DriverVar *driver_add_new_variable (ChannelDriver *driver)
{
	DriverVar *dvar;
	
	/* sanity checks */
	if (driver == NULL)
		return NULL;
		
	/* make a new variable */
	dvar= MEM_callocN(sizeof(DriverVar), "DriverVar");
	BLI_addtail(&driver->variables, dvar);
	
	/* give the variable a 'unique' name */
	strcpy(dvar->name, "var");
	BLI_uniquename(&driver->variables, dvar, "var", '_', offsetof(DriverVar, name), sizeof(dvar->name));
	
	/* set the default type to 'single prop' */
	driver_change_variable_type(dvar, DVAR_TYPE_SINGLE_PROP);
	
#ifdef WITH_PYTHON
	/* since driver variables are cached, the expression needs re-compiling too */
	if (driver->type==DRIVER_TYPE_PYTHON)
		driver->flag |= DRIVER_FLAG_RENAMEVAR;
#endif

	/* return the target */
	return dvar;
}

/* This frees the driver itself */
void fcurve_free_driver(FCurve *fcu)
{
	ChannelDriver *driver;
	DriverVar *dvar, *dvarn;
	
	/* sanity checks */
	if ELEM(NULL, fcu, fcu->driver)
		return;
	driver= fcu->driver;
	
	/* free driver targets */
	for (dvar= driver->variables.first; dvar; dvar= dvarn) {
		dvarn= dvar->next;
		driver_free_variable(driver, dvar);
	}

#ifdef WITH_PYTHON
	/* free compiled driver expression */
	if (driver->expr_comp)
		BPY_DECREF(driver->expr_comp);
#endif

	/* free driver itself, then set F-Curve's point to this to NULL (as the curve may still be used) */
	MEM_freeN(driver);
	fcu->driver= NULL;
}

/* This makes a copy of the given driver */
ChannelDriver *fcurve_copy_driver (ChannelDriver *driver)
{
	ChannelDriver *ndriver;
	DriverVar *dvar;
	
	/* sanity checks */
	if (driver == NULL)
		return NULL;
		
	/* copy all data */
	ndriver= MEM_dupallocN(driver);
	ndriver->expr_comp= NULL;
	
	/* copy variables */
	ndriver->variables.first= ndriver->variables.last= NULL;
	BLI_duplicatelist(&ndriver->variables, &driver->variables);
	
	for (dvar= ndriver->variables.first; dvar; dvar= dvar->next) {	
		/* need to go over all targets so that we don't leave any dangling paths */
		DRIVER_TARGETS_LOOPER(dvar) 
		{	
			/* make a copy of target's rna path if available */
			if (dtar->rna_path)
				dtar->rna_path = MEM_dupallocN(dtar->rna_path);
		}
		DRIVER_TARGETS_LOOPER_END
	}
	
	/* return the new driver */
	return ndriver;
}

/* Driver Evaluation -------------------------- */

/* Evaluate a Driver Variable to get a value that contributes to the final */
float driver_get_variable_value (ChannelDriver *driver, DriverVar *dvar)
{
	DriverVarTypeInfo *dvti;

	/* sanity check */
	if (ELEM(NULL, driver, dvar))
		return 0.0f;
	
	/* call the relevant callbacks to get the variable value 
	 * using the variable type info, storing the obtained value
	 * in dvar->curval so that drivers can be debugged
	 */
	dvti= get_dvar_typeinfo(dvar->type);
	
	if (dvti && dvti->get_value)
		dvar->curval= dvti->get_value(driver, dvar);
	else
		dvar->curval= 0.0f;
	
	return dvar->curval;
}

/* Evaluate an Channel-Driver to get a 'time' value to use instead of "evaltime"
 *	- "evaltime" is the frame at which F-Curve is being evaluated
 * 	- has to return a float value 
 */
static float evaluate_driver (ChannelDriver *driver, const float evaltime)
{
	DriverVar *dvar;
	
	/* check if driver can be evaluated */
	if (driver->flag & DRIVER_FLAG_INVALID)
		return 0.0f;
	
	switch (driver->type) {
		case DRIVER_TYPE_AVERAGE: /* average values of driver targets */
		case DRIVER_TYPE_SUM: /* sum values of driver targets */
		{
			/* check how many variables there are first (i.e. just one?) */
			if (driver->variables.first == driver->variables.last) {
				/* just one target, so just use that */
				dvar= driver->variables.first;
				driver->curval= driver_get_variable_value(driver, dvar);
			}
			else {
				/* more than one target, so average the values of the targets */
				float value = 0.0f;
				int tot = 0;
				
				/* loop through targets, adding (hopefully we don't get any overflow!) */
				for (dvar= driver->variables.first; dvar; dvar=dvar->next) {
					value += driver_get_variable_value(driver, dvar);
					tot++;
				}
				
				/* perform operations on the total if appropriate */
				if (driver->type == DRIVER_TYPE_AVERAGE)
					driver->curval= (value / (float)tot);
				else
					driver->curval= value;
			}
		}
			break;
			
		case DRIVER_TYPE_MIN: /* smallest value */
		case DRIVER_TYPE_MAX: /* largest value */
		{
			float value = 0.0f;
			
			/* loop through the variables, getting the values and comparing them to existing ones */
			for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
				/* get value */
				float tmp_val= driver_get_variable_value(driver, dvar);
				
				/* store this value if appropriate */
				if (dvar->prev) {
					/* check if greater/smaller than the baseline */
					if (driver->type == DRIVER_TYPE_MAX) {
						/* max? */
						if (tmp_val > value) 
							value= tmp_val;
					}
					else {
						/* min? */
						if (tmp_val < value) 
							value= tmp_val;
					}
				}
				else {
					/* first item - make this the baseline for comparisons */
					value= tmp_val;
				}
			}
			
			/* store value in driver */
			driver->curval= value;
		}
			break;
			
		case DRIVER_TYPE_PYTHON: /* expression */
		{
#ifdef WITH_PYTHON
			/* check for empty or invalid expression */
			if ( (driver->expression[0] == '\0') ||
				 (driver->flag & DRIVER_FLAG_INVALID) )
			{
				driver->curval= 0.0f;
			}
			else
			{
				/* this evaluates the expression using Python,and returns its result:
				 * 	- on errors it reports, then returns 0.0f
				 */
				driver->curval= BPY_driver_exec(driver, evaltime);
			}
#else /* WITH_PYTHON*/
		(void)evaltime;
#endif /* WITH_PYTHON*/
		}
			break;
		
		default:
		{
			/* special 'hack' - just use stored value 
			 *	This is currently used as the mechanism which allows animated settings to be able
			 * 	to be changed via the UI.
			 */
		}
	}
	
	/* return value for driver */
	return driver->curval;
}

/* ***************************** Curve Calculations ********************************* */

/* The total length of the handles is not allowed to be more
 * than the horizontal distance between (v1-v4).
 * This is to prevent curve loops.
 */
void correct_bezpart(float v1[2], float v2[2], float v3[2], float v4[2])
{
	float h1[2], h2[2], len1, len2, len, fac;
	
	/* calculate handle deltas */
	h1[0]= v1[0] - v2[0];
	h1[1]= v1[1] - v2[1];
	
	h2[0]= v4[0] - v3[0];
	h2[1]= v4[1] - v3[1];
	
	/* calculate distances: 
	 * 	- len	= span of time between keyframes 
	 *	- len1	= length of handle of start key
	 *	- len2 	= length of handle of end key
	 */
	len= v4[0]- v1[0];
	len1= fabsf(h1[0]);
	len2= fabsf(h2[0]);
	
	/* if the handles have no length, no need to do any corrections */
	if ((len1+len2) == 0.0f) 
		return;
		
	/* the two handles cross over each other, so force them
	 * apart using the proportion they overlap 
	 */
	if ((len1+len2) > len) {
		fac= len / (len1+len2);
		
		v2[0]= (v1[0] - fac*h1[0]);
		v2[1]= (v1[1] - fac*h1[1]);
		
		v3[0]= (v4[0] - fac*h2[0]);
		v3[1]= (v4[1] - fac*h2[1]);
	}
}

/* find root ('zero') */
static int findzero (float x, float q0, float q1, float q2, float q3, float *o)
{
	double c0, c1, c2, c3, a, b, c, p, q, d, t, phi;
	int nr= 0;

	c0= q0 - x;
	c1= 3.0f * (q1 - q0);
	c2= 3.0f * (q0 - 2.0f*q1 + q2);
	c3= q3 - q0 + 3.0f * (q1 - q2);
	
	if (c3 != 0.0) {
		a= c2/c3;
		b= c1/c3;
		c= c0/c3;
		a= a/3;
		
		p= b/3 - a*a;
		q= (2*a*a*a - a*b + c) / 2;
		d= q*q + p*p*p;
		
		if (d > 0.0) {
			t= sqrt(d);
			o[0]= (float)(sqrt3d(-q+t) + sqrt3d(-q-t) - a);
			
			if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) return 1;
			else return 0;
		}
		else if (d == 0.0) {
			t= sqrt3d(-q);
			o[0]= (float)(2*t - a);
			
			if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) nr++;
			o[nr]= (float)(-t-a);
			
			if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) return nr+1;
			else return nr;
		}
		else {
			phi= acos(-q / sqrt(-(p*p*p)));
			t= sqrt(-p);
			p= cos(phi/3);
			q= sqrt(3 - 3*p*p);
			o[0]= (float)(2*t*p - a);
			
			if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) nr++;
			o[nr]= (float)(-t * (p + q) - a);
			
			if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) nr++;
			o[nr]= (float)(-t * (p - q) - a);
			
			if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) return nr+1;
			else return nr;
		}
	}
	else {
		a=c2;
		b=c1;
		c=c0;
		
		if (a != 0.0) {
			// discriminant
			p= b*b - 4*a*c;
			
			if (p > 0) {
				p= sqrt(p);
				o[0]= (float)((-b-p) / (2 * a));
				
				if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) nr++;
				o[nr]= (float)((-b+p)/(2*a));
				
				if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) return nr+1;
				else return nr;
			}
			else if (p == 0) {
				o[0]= (float)(-b / (2 * a));
				if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) return 1;
				else return 0;
			}
		}
		else if (b != 0.0) {
			o[0]= (float)(-c/b);
			
			if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) return 1;
			else return 0;
		}
		else if (c == 0.0) {
			o[0]= 0.0;
			return 1;
		}
		
		return 0;	
	}
}

static void berekeny (float f1, float f2, float f3, float f4, float *o, int b)
{
	float t, c0, c1, c2, c3;
	int a;

	c0= f1;
	c1= 3.0f * (f2 - f1);
	c2= 3.0f * (f1 - 2.0f*f2 + f3);
	c3= f4 - f1 + 3.0f * (f2 - f3);
	
	for (a=0; a < b; a++) {
		t= o[a];
		o[a]= c0 + t*c1 + t*t*c2 + t*t*t*c3;
	}
}

#if 0
static void berekenx (float *f, float *o, int b)
{
	float t, c0, c1, c2, c3;
	int a;

	c0= f[0];
	c1= 3.0f * (f[3] - f[0]);
	c2= 3.0f * (f[0] - 2.0f*f[3] + f[6]);
	c3= f[9] - f[0] + 3.0f * (f[3] - f[6]);
	
	for (a=0; a < b; a++) {
		t= o[a];
		o[a]= c0 + t*c1 + t*t*c2 + t*t*t*c3;
	}
}
#endif


/* -------------------------- */

/* Calculate F-Curve value for 'evaltime' using BezTriple keyframes */
static float fcurve_eval_keyframes (FCurve *fcu, BezTriple *bezts, float evaltime)
{
	BezTriple *bezt, *prevbezt, *lastbezt;
	float v1[2], v2[2], v3[2], v4[2], opl[32], dx, fac;
	unsigned int a;
	int b;
	float cvalue = 0.0f;
	
	/* get pointers */
	a= fcu->totvert-1;
	prevbezt= bezts;
	bezt= prevbezt+1;
	lastbezt= prevbezt + a;
	
	/* evaluation time at or past endpoints? */
	if (prevbezt->vec[1][0] >= evaltime) 
	{
		/* before or on first keyframe */
		if ( (fcu->extend == FCURVE_EXTRAPOLATE_LINEAR) && (prevbezt->ipo != BEZT_IPO_CONST) &&
			!(fcu->flag & FCURVE_DISCRETE_VALUES) ) 
		{
			/* linear or bezier interpolation */
			if (prevbezt->ipo==BEZT_IPO_LIN) 
			{
				/* Use the next center point instead of our own handle for
				 * linear interpolated extrapolate 
				 */
				if (fcu->totvert == 1) 
					cvalue= prevbezt->vec[1][1];
				else 
				{
					bezt = prevbezt+1;
					dx= prevbezt->vec[1][0] - evaltime;
					fac= bezt->vec[1][0] - prevbezt->vec[1][0];
					
					/* prevent division by zero */
					if (fac) {
						fac= (bezt->vec[1][1] - prevbezt->vec[1][1]) / fac;
						cvalue= prevbezt->vec[1][1] - (fac * dx);
					}
					else 
						cvalue= prevbezt->vec[1][1];
				}
			} 
			else 
			{
				/* Use the first handle (earlier) of first BezTriple to calculate the
				 * gradient and thus the value of the curve at evaltime
				 */
				dx= prevbezt->vec[1][0] - evaltime;
				fac= prevbezt->vec[1][0] - prevbezt->vec[0][0];
				
				/* prevent division by zero */
				if (fac) {
					fac= (prevbezt->vec[1][1] - prevbezt->vec[0][1]) / fac;
					cvalue= prevbezt->vec[1][1] - (fac * dx);
				}
				else 
					cvalue= prevbezt->vec[1][1];
			}
		}
		else 
		{
			/* constant (BEZT_IPO_HORIZ) extrapolation or constant interpolation, 
			 * so just extend first keyframe's value 
			 */
			cvalue= prevbezt->vec[1][1];
		}
	}
	else if (lastbezt->vec[1][0] <= evaltime) 
	{
		/* after or on last keyframe */
		if ( (fcu->extend == FCURVE_EXTRAPOLATE_LINEAR) && (lastbezt->ipo != BEZT_IPO_CONST) &&
			!(fcu->flag & FCURVE_DISCRETE_VALUES) ) 
		{
			/* linear or bezier interpolation */
			if (lastbezt->ipo==BEZT_IPO_LIN) 
			{
				/* Use the next center point instead of our own handle for
				 * linear interpolated extrapolate 
				 */
				if (fcu->totvert == 1) 
					cvalue= lastbezt->vec[1][1];
				else 
				{
					prevbezt = lastbezt - 1;
					dx= evaltime - lastbezt->vec[1][0];
					fac= lastbezt->vec[1][0] - prevbezt->vec[1][0];
					
					/* prevent division by zero */
					if (fac) {
						fac= (lastbezt->vec[1][1] - prevbezt->vec[1][1]) / fac;
						cvalue= lastbezt->vec[1][1] + (fac * dx);
					}
					else 
						cvalue= lastbezt->vec[1][1];
				}
			} 
			else 
			{
				/* Use the gradient of the second handle (later) of last BezTriple to calculate the
				 * gradient and thus the value of the curve at evaltime
				 */
				dx= evaltime - lastbezt->vec[1][0];
				fac= lastbezt->vec[2][0] - lastbezt->vec[1][0];
				
				/* prevent division by zero */
				if (fac) {
					fac= (lastbezt->vec[2][1] - lastbezt->vec[1][1]) / fac;
					cvalue= lastbezt->vec[1][1] + (fac * dx);
				}
				else 
					cvalue= lastbezt->vec[1][1];
			}
		}
		else 
		{
			/* constant (BEZT_IPO_HORIZ) extrapolation or constant interpolation, 
			 * so just extend last keyframe's value 
			 */
			cvalue= lastbezt->vec[1][1];
		}
	}
	else 
	{
		/* evaltime occurs somewhere in the middle of the curve */
		for (a=0; prevbezt && bezt && (a < fcu->totvert-1); a++, prevbezt=bezt, bezt++) 
		{
			/* use if the key is directly on the frame, rare cases this is needed else we get 0.0 instead. */
			if(fabsf(bezt->vec[1][0] - evaltime) < SMALL_NUMBER) {
				cvalue= bezt->vec[1][1];
			}
			/* evaltime occurs within the interval defined by these two keyframes */
			else if ((prevbezt->vec[1][0] <= evaltime) && (bezt->vec[1][0] >= evaltime))
			{
				/* value depends on interpolation mode */
				if ((prevbezt->ipo == BEZT_IPO_CONST) || (fcu->flag & FCURVE_DISCRETE_VALUES))
				{
					/* constant (evaltime not relevant, so no interpolation needed) */
					cvalue= prevbezt->vec[1][1];
				}
				else if (prevbezt->ipo == BEZT_IPO_LIN) 
				{
					/* linear - interpolate between values of the two keyframes */
					fac= bezt->vec[1][0] - prevbezt->vec[1][0];
					
					/* prevent division by zero */
					if (fac) {
						fac= (evaltime - prevbezt->vec[1][0]) / fac;
						cvalue= prevbezt->vec[1][1] + (fac * (bezt->vec[1][1] - prevbezt->vec[1][1]));
					}
					else
						cvalue= prevbezt->vec[1][1];
				}
				else 
				{
					/* bezier interpolation */
						/* v1,v2 are the first keyframe and its 2nd handle */
					v1[0]= prevbezt->vec[1][0];
					v1[1]= prevbezt->vec[1][1];
					v2[0]= prevbezt->vec[2][0];
					v2[1]= prevbezt->vec[2][1];
						/* v3,v4 are the last keyframe's 1st handle + the last keyframe */
					v3[0]= bezt->vec[0][0];
					v3[1]= bezt->vec[0][1];
					v4[0]= bezt->vec[1][0];
					v4[1]= bezt->vec[1][1];
					
					/* adjust handles so that they don't overlap (forming a loop) */
					correct_bezpart(v1, v2, v3, v4);
					
					/* try to get a value for this position - if failure, try another set of points */
					b= findzero(evaltime, v1[0], v2[0], v3[0], v4[0], opl);
					if (b) {
						berekeny(v1[1], v2[1], v3[1], v4[1], opl, 1);
						cvalue= opl[0];
						break;
					}
				}
			}
		}
	}
	
	/* return value */
	return cvalue;
}

/* Calculate F-Curve value for 'evaltime' using FPoint samples */
static float fcurve_eval_samples (FCurve *fcu, FPoint *fpts, float evaltime)
{
	FPoint *prevfpt, *lastfpt, *fpt;
	float cvalue= 0.0f;
	
	/* get pointers */
	prevfpt= fpts;
	lastfpt= prevfpt + fcu->totvert-1;
	
	/* evaluation time at or past endpoints? */
	if (prevfpt->vec[0] >= evaltime) {
		/* before or on first sample, so just extend value */
		cvalue= prevfpt->vec[1];
	}
	else if (lastfpt->vec[0] <= evaltime) {
		/* after or on last sample, so just extend value */
		cvalue= lastfpt->vec[1];
	}
	else {
		float t= (float)abs(evaltime - (int)evaltime);
		
		/* find the one on the right frame (assume that these are spaced on 1-frame intervals) */
		fpt= prevfpt + (int)(evaltime - prevfpt->vec[0]);
		
		/* if not exactly on the frame, perform linear interpolation with the next one */
		if (t != 0.0f) 
			cvalue= interpf(fpt->vec[1], (fpt+1)->vec[1], t);
		else
			cvalue= fpt->vec[1];
	}
	
	/* return value */
	return cvalue;
}

/* ***************************** F-Curve - Evaluation ********************************* */

/* Evaluate and return the value of the given F-Curve at the specified frame ("evaltime") 
 * Note: this is also used for drivers
 */
float evaluate_fcurve (FCurve *fcu, float evaltime)
{
	float cvalue= 0.0f;
	float devaltime;
	
	/* if there is a driver (only if this F-Curve is acting as 'driver'), evaluate it to find value to use as "evaltime" 
	 * since drivers essentially act as alternative input (i.e. in place of 'time') for F-Curves
	 *	- this value will also be returned as the value of the 'curve', if there are no keyframes
	 */
	if (fcu->driver) {
		/* evaltime now serves as input for the curve */
		evaltime= cvalue= evaluate_driver(fcu->driver, evaltime);
	}
	
	/* evaluate modifiers which modify time to evaluate the base curve at */
	devaltime= evaluate_time_fmodifiers(&fcu->modifiers, fcu, cvalue, evaltime);
	
	/* evaluate curve-data 
	 *	- 'devaltime' instead of 'evaltime', as this is the time that the last time-modifying 
	 *	  F-Curve modifier on the stack requested the curve to be evaluated at
	 */
	if (fcu->bezt)
		cvalue= fcurve_eval_keyframes(fcu, fcu->bezt, devaltime);
	else if (fcu->fpt)
		cvalue= fcurve_eval_samples(fcu, fcu->fpt, devaltime);
	
	/* evaluate modifiers */
	evaluate_value_fmodifiers(&fcu->modifiers, fcu, &cvalue, evaltime);
	
	/* if curve can only have integral values, perform truncation (i.e. drop the decimal part)
	 * here so that the curve can be sampled correctly
	 */
	if (fcu->flag & FCURVE_INT_VALUES)
		cvalue= floorf(cvalue + 0.5f);
	
	/* return evaluated value */
	return cvalue;
}

/* Calculate the value of the given F-Curve at the given frame, and set its curval */
void calculate_fcurve (FCurve *fcu, float ctime)
{
	/* only calculate + set curval (overriding the existing value) if curve has 
	 * any data which warrants this...
	 */
	if ( (fcu->totvert) || (fcu->driver && !(fcu->driver->flag & DRIVER_FLAG_INVALID)) ||
		 list_has_suitable_fmodifier(&fcu->modifiers, 0, FMI_TYPE_GENERATE_CURVE) )
	{
		/* calculate and set curval (evaluates driver too if necessary) */
		fcu->curval= evaluate_fcurve(fcu, ctime);
	}
}

