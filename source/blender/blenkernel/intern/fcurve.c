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
 

#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_noise.h"

#include "BKE_fcurve.h"
#include "BKE_animsys.h"

#include "BKE_curve.h" 
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"

#ifndef DISABLE_PYTHON
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

/* ---------------------- Relink --------------------------- */

#if 0
/* uses id->newid to match pointers with other copied data 
 * 	- called after single-user or other such
 */
			if (icu->driver)
				ID_NEW(icu->driver->ob);
#endif

/* --------------------- Finding -------------------------- */

FCurve *id_data_find_fcurve(ID *id, void *data, StructRNA *type, char *prop_name, int index)
{
	/* anim vars */
	AnimData *adt;
	FCurve *fcu= NULL;

	/* rna vars */
	PointerRNA ptr;
	PropertyRNA *prop;
	char *path;

	adt= BKE_animdata_from_id(id);

	/* only use the current action ??? */
	if(adt==NULL || adt->action==NULL)
		return NULL;

	RNA_pointer_create(id, type, data, &ptr);
	prop = RNA_struct_find_property(&ptr, prop_name);

	if(prop) {
		path= RNA_path_from_ID_to_property(&ptr, prop);

		if(path) {
			/* animation takes priority over drivers */
			if(adt->action && adt->action->curves.first)
				fcu= list_find_fcurve(&adt->action->curves, path, index);

			/* if not animated, check if driven */
#if 0
			if(!fcu && (adt->drivers.first)) {
				fcu= list_find_fcurve(&adt->drivers, path, but->rnaindex);
			}
#endif

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
			/* now check indicies */
			if (fcu->array_index == array_index)
				return fcu;
		}
	}
	
	/* return */
	return NULL;
}

/* threshold for binary-searching keyframes - threshold here should be good enough for now, but should become userpref */
#define BEZT_BINARYSEARCH_THRESH 	0.00001f

/* Binary search algorithm for finding where to insert BezTriple. (for use by insert_bezt_fcurve)
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
int binarysearch_bezt_index (BezTriple array[], float frame, int arraylen, short *replace)
{
	int start=0, end=arraylen;
	int loopbreaker= 0, maxloop= arraylen * 2;
	
	/* initialise replace-flag first */
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

/* Calculate the extents of F-Curve's data */
void calc_fcurve_bounds (FCurve *fcu, float *xmin, float *xmax, float *ymin, float *ymax)
{
	float xminv=999999999.0f, xmaxv=-999999999.0f;
	float yminv=999999999.0f, ymaxv=-999999999.0f;
	short foundvert=0;
	unsigned int i;
	
	if (fcu->totvert) {
		if (fcu->bezt) {
			/* frame range can be directly calculated from end verts */
			if (xmin || xmax) {
				xminv= MIN2(xminv, fcu->bezt[0].vec[1][0]);
				xmaxv= MAX2(xmaxv, fcu->bezt[fcu->totvert-1].vec[1][0]);
			}
			
			/* only loop over keyframes to find extents for values if needed */
			if (ymin || ymax) {
				BezTriple *bezt;
				
				for (bezt=fcu->bezt, i=0; i < fcu->totvert; bezt++, i++) {
					if (bezt->vec[1][1] < yminv)
						yminv= bezt->vec[1][1];
					if (bezt->vec[1][1] > ymaxv)
						ymaxv= bezt->vec[1][1];
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
				}
			}
		}
		
		foundvert=1;
	}
	
	/* minimum sizes are 1.0f */
	if (foundvert) {
		if (xminv == xmaxv) xmaxv += 1.0f;
		if (yminv == ymaxv) ymaxv += 1.0f;
		
		if (xmin) *xmin= xminv;
		if (xmax) *xmax= xmaxv;
		
		if (ymin) *ymin= yminv;
		if (ymax) *ymax= ymaxv;
	}
	else {
		if (xmin) *xmin= 0.0f;
		if (xmax) *xmax= 0.0f;
		
		if (ymin) *ymin= 1.0f;
		if (ymax) *ymax= 1.0f;
	}
}

/* Calculate the extents of F-Curve's keyframes */
void calc_fcurve_range (FCurve *fcu, float *start, float *end)
{
	float min=999999999.0f, max=-999999999.0f;
	short foundvert=0;

	if (fcu->totvert) {
		if (fcu->bezt) {
			min= MIN2(min, fcu->bezt[0].vec[1][0]);
			max= MAX2(max, fcu->bezt[fcu->totvert-1].vec[1][0]);
		}
		else if (fcu->fpt) {
			min= MIN2(min, fcu->fpt[0].vec[0]);
			max= MAX2(max, fcu->fpt[fcu->totvert-1].vec[0]);
		}
		
		foundvert=1;
	}
	
	/* minimum length is 1 frame */
	if (foundvert) {
		if (min == max) max += 1.0f;
		*start= min;
		*end= max;
	}
	else {
		*start= 0.0f;
		*end= 1.0f;
	}
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
 * data imported from BVH/Mocap files), which are specialised for use with high density datasets,
 * which BezTriples/Keyframe data are ill equipped to do.
 */
 
 
/* Basic sampling callback which acts as a wrapper for evaluate_fcurve() 
 *	'data' arg here is unneeded here...
 */
float fcurve_samplingcb_evalcurve (FCurve *fcu, void *data, float evaltime)
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
		if (fcu->flag & FCURVE_AUTO_HANDLES) 
			calchandleNurb(bezt, prev, next, 2);	/* 2==special autohandle && keep extrema horizontal */
		else
			calchandleNurb(bezt, prev, next, 1);	/* 1==special autohandle */
		
		/* for automatic ease in and out */
		if ((bezt->h1==HD_AUTO) && (bezt->h2==HD_AUTO)) {
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
void testhandles_fcurve (FCurve *fcu)
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
		if (bezt->f1 & SELECT) flag |= (1<<0); // == 1
		if (bezt->f2 & SELECT) flag |= (1<<1); // == 2
		if (bezt->f3 & SELECT) flag |= (1<<2); // == 4
		
		/* one or two handles selected only */
		if (ELEM(flag, 0, 7)==0) {
			/* auto handles become aligned */
			if (bezt->h1==HD_AUTO)
				bezt->h1= HD_ALIGN;
			if (bezt->h2==HD_AUTO)
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

/* Driver API --------------------------------- */

/* This frees the driver target itself */
void driver_free_target (ChannelDriver *driver, DriverTarget *dtar)
{
	/* sanity checks */
	if (dtar == NULL)
		return;
		
	/* free target vars */
	if (dtar->rna_path)
		MEM_freeN(dtar->rna_path);
	
	/* remove the target from the driver */
	if (driver)
		BLI_freelinkN(&driver->targets, dtar);
	else
		MEM_freeN(dtar);
}

/* Add a new driver target variable */
DriverTarget *driver_add_new_target (ChannelDriver *driver)
{
	DriverTarget *dtar;
	
	/* sanity checks */
	if (driver == NULL)
		return NULL;
		
	/* make a new target */
	dtar= MEM_callocN(sizeof(DriverTarget), "DriverTarget");
	BLI_addtail(&driver->targets, dtar);
	
	/* make the default ID-type ID_OB, since most driver targets refer to objects */
	dtar->idtype= ID_OB;
	
	/* give the target a 'unique' name */
	strcpy(dtar->name, "var");
	BLI_uniquename(&driver->targets, dtar, "var", '_', offsetof(DriverTarget, name), 64);
	
	/* return the target */
	return dtar;
}

/* This frees the driver itself */
void fcurve_free_driver(FCurve *fcu)
{
	ChannelDriver *driver;
	DriverTarget *dtar, *dtarn;
	
	/* sanity checks */
	if ELEM(NULL, fcu, fcu->driver)
		return;
	driver= fcu->driver;
	
	/* free driver targets */
	for (dtar= driver->targets.first; dtar; dtar= dtarn) {
		dtarn= dtar->next;
		driver_free_target(driver, dtar);
	}

#ifndef DISABLE_PYTHON
	if(driver->expr_comp)
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
	DriverTarget *dtar;
	
	/* sanity checks */
	if (driver == NULL)
		return NULL;
		
	/* copy all data */
	ndriver= MEM_dupallocN(driver);
	
	/* copy targets */
	ndriver->targets.first= ndriver->targets.last= NULL;
	BLI_duplicatelist(&ndriver->targets, &driver->targets);
	
	for (dtar= ndriver->targets.first; dtar; dtar= dtar->next) {
		/* make a copy of target's rna path if available */
		if (dtar->rna_path)
			dtar->rna_path = MEM_dupallocN(dtar->rna_path);
	}
	
	/* return the new driver */
	return ndriver;
}

/* Driver Evaluation -------------------------- */

/* Helper function to obtain a value using RNA from the specified source (for evaluating drivers) */
float driver_get_target_value (ChannelDriver *driver, DriverTarget *dtar)
{
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	ID *id;
	char *path;
	int index;
	float value= 0.0f;
	
	/* sanity check */
	if ELEM(NULL, driver, dtar)
		return 0.0f;
	
	/* get RNA-pointer for the ID-block given in target */
	RNA_id_pointer_create(dtar->id, &id_ptr);
	id= dtar->id;
	path= dtar->rna_path;
	index= dtar->array_index;
	
	/* error check for missing pointer... */
	if (id == NULL) {
		printf("Error: driver doesn't have any valid target to use \n");
		if (G.f & G_DEBUG) printf("\tpath = %s [%d] \n", path, index);
		driver->flag |= DRIVER_FLAG_INVALID;
		return 0.0f;
	}
	
	/* get property to read from, and get value as appropriate */
	if (RNA_path_resolve_full(&id_ptr, path, &ptr, &prop, &index)) {
		/* for now, if there is no valid index, fall back to the array-index specified separately */
		if (index == -1)
			index= dtar->array_index;
		
		switch (RNA_property_type(prop)) {
			case PROP_BOOLEAN:
				if (RNA_property_array_length(&ptr, prop))
					value= (float)RNA_property_boolean_get_index(&ptr, prop, index);
				else
					value= (float)RNA_property_boolean_get(&ptr, prop);
				break;
			case PROP_INT:
				if (RNA_property_array_length(&ptr, prop))
					value= (float)RNA_property_int_get_index(&ptr, prop, index);
				else
					value= (float)RNA_property_int_get(&ptr, prop);
				break;
			case PROP_FLOAT:
				if (RNA_property_array_length(&ptr, prop))
					value= RNA_property_float_get_index(&ptr, prop, index);
				else
					value= RNA_property_float_get(&ptr, prop);
				break;
			case PROP_ENUM:
				value= (float)RNA_property_enum_get(&ptr, prop);
				break;
			default:
				break;
		}
	}
	else {
		if (G.f & G_DEBUG)
			printf("Driver Evaluation Error: cannot resolve target for %s -> %s \n", id->name, path);
		
		driver->flag |= DRIVER_FLAG_INVALID;
		return 0.0f;
	}
	
	return value;
}

/* Get two PoseChannels from the targets of the given Driver */
static void driver_get_target_pchans2 (ChannelDriver *driver, bPoseChannel **pchan1, bPoseChannel **pchan2)
{
	DriverTarget *dtar;
	short i = 0;
	
	/* before doing anything */
	*pchan1= NULL;
	*pchan2= NULL;
	
	/* only take the first two targets */
	for (dtar= driver->targets.first; (dtar) && (i < 2); dtar=dtar->next, i++) {
		PointerRNA id_ptr, ptr;
		PropertyRNA *prop;
		
		/* get RNA-pointer for the ID-block given in target */
		if (dtar->id)
			RNA_id_pointer_create(dtar->id, &id_ptr);
		else
			continue;
		
		/* resolve path so that we have pointer to the right posechannel */
		if (RNA_path_resolve(&id_ptr, dtar->rna_path, &ptr, &prop)) {
			/* is pointer valid (i.e. pointing to an actual posechannel */
			if ((ptr.type == &RNA_PoseBone) && (ptr.data)) {
				/* first or second target? */
				if (i)
					*pchan1= ptr.data;
				else
					*pchan2= ptr.data;
			}
		}
	}
}

/* Evaluate an Channel-Driver to get a 'time' value to use instead of "evaltime"
 *	- "evaltime" is the frame at which F-Curve is being evaluated
 * 	- has to return a float value 
 */
static float evaluate_driver (ChannelDriver *driver, float evaltime)
{
	DriverTarget *dtar;
	
	/* check if driver can be evaluated */
	if (driver->flag & DRIVER_FLAG_INVALID)
		return 0.0f;
	
	// TODO: the flags for individual targets need to be used too for more fine-grained support...
	switch (driver->type) {
		case DRIVER_TYPE_AVERAGE: /* average values of driver targets */
		case DRIVER_TYPE_SUM: /* sum values of driver targets */
		{
			/* check how many targets there are first (i.e. just one?) */
			if (driver->targets.first == driver->targets.last) {
				/* just one target, so just use that */
				dtar= driver->targets.first;
				return driver_get_target_value(driver, dtar);
			}
			else {
				/* more than one target, so average the values of the targets */
				int tot = 0;
				float value = 0.0f;
				
				/* loop through targets, adding (hopefully we don't get any overflow!) */
				for (dtar= driver->targets.first; dtar; dtar=dtar->next) {
					value += driver_get_target_value(driver, dtar);
					tot++;
				}
				
				/* return the average of these */
				if (driver->type == DRIVER_TYPE_AVERAGE)
					return (value / (float)tot);
				else
					return value;
				
			}
		}
			break;
		case DRIVER_TYPE_PYTHON: /* expression */
		{
#ifndef DISABLE_PYTHON
			/* check for empty or invalid expression */
			if ( (driver->expression[0] == '\0') ||
				 (driver->flag & DRIVER_FLAG_INVALID) )
			{
				return 0.0f;
			}
			
			/* this evaluates the expression using Python,and returns its result:
			 * 	- on errors it reports, then returns 0.0f
			 */
			return BPY_pydriver_eval(driver);
#endif /* DISABLE_PYTHON*/
		}
			break;

		
		case DRIVER_TYPE_ROTDIFF: /* difference of rotations of 2 bones (should ideally be in same armature) */
		{
			bPoseChannel *pchan, *pchan2;
			float q1[4], q2[4], quat[4], angle;
			
			/* get pose channels, and check if we've got two */
			driver_get_target_pchans2(driver, &pchan, &pchan2);
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
			
			return (angle > M_PI) ? (float)((2.0f * M_PI) - angle) : (float)(angle);
		}
			break;
		
		default:
		{
			/* special 'hack' - just use stored value 
			 *	This is currently used as the mechanism which allows animated settings to be able
			 * 	to be changed via the UI.
			 */
			return driver->curval;
		}
	}
	
	/* return 0.0f, as couldn't find relevant data to use */
	return 0.0f;
}

/* ***************************** Curve Calculations ********************************* */

/* The total length of the handles is not allowed to be more
 * than the horizontal distance between (v1-v4).
 * This is to prevent curve loops.
*/
void correct_bezpart (float *v1, float *v2, float *v3, float *v4)
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
	len1= (float)fabs(h1[0]);
	len2= (float)fabs(h2[0]);
	
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
	c1= 3.0 * (q1 - q0);
	c2= 3.0 * (q0 - 2.0*q1 + q2);
	c3= q3 - q0 + 3.0 * (q1 - q2);
	
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
			
			if ((o[0] >= SMALL) && (o[0] <= 1.000001)) return 1;
			else return 0;
		}
		else if (d == 0.0) {
			t= sqrt3d(-q);
			o[0]= (float)(2*t - a);
			
			if ((o[0] >= SMALL) && (o[0] <= 1.000001)) nr++;
			o[nr]= (float)(-t-a);
			
			if ((o[nr] >= SMALL) && (o[nr] <= 1.000001)) return nr+1;
			else return nr;
		}
		else {
			phi= acos(-q / sqrt(-(p*p*p)));
			t= sqrt(-p);
			p= cos(phi/3);
			q= sqrt(3 - 3*p*p);
			o[0]= (float)(2*t*p - a);
			
			if ((o[0] >= SMALL) && (o[0] <= 1.000001)) nr++;
			o[nr]= (float)(-t * (p + q) - a);
			
			if ((o[nr] >= SMALL) && (o[nr] <= 1.000001)) nr++;
			o[nr]= (float)(-t * (p - q) - a);
			
			if ((o[nr] >= SMALL) && (o[nr] <= 1.000001)) return nr+1;
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
				
				if ((o[0] >= SMALL) && (o[0] <= 1.000001)) nr++;
				o[nr]= (float)((-b+p)/(2*a));
				
				if ((o[nr] >= SMALL) && (o[nr] <= 1.000001)) return nr+1;
				else return nr;
			}
			else if (p == 0) {
				o[0]= (float)(-b / (2 * a));
				if ((o[0] >= SMALL) && (o[0] <= 1.000001)) return 1;
				else return 0;
			}
		}
		else if (b != 0.0) {
			o[0]= (float)(-c/b);
			
			if ((o[0] >= SMALL) && (o[0] <= 1.000001)) return 1;
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
			/* evaltime occurs within the interval defined by these two keyframes */
			if ((prevbezt->vec[1][0] <= evaltime) && (bezt->vec[1][0] >= evaltime)) 
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
		/* find the one on the right frame (assume that these are spaced on 1-frame intervals) */
		fpt= prevfpt + (int)(evaltime - prevfpt->vec[0]);
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
		cvalue= (float)((int)cvalue);
	
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

