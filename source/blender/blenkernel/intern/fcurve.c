/* Testing code for new animation system in 2.5 
 * Copyright 2009, Joshua Leung
 */
 

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_fcurve.h"
#include "BKE_curve.h" 
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"

#ifndef DISABLE_PYTHON
#include "BPY_extern.h" /* for BPY_pydriver_eval() */
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
	fcurve_free_modifiers(fcu);
	
	/* free f-cruve itself */
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
	
	/* copy curve data */
	fcu_d->bezt= MEM_dupallocN(fcu_d->bezt);
	fcu_d->fpt= MEM_dupallocN(fcu_d->fpt);
	
	/* copy rna-path */
	fcu_d->rna_path= MEM_dupallocN(fcu_d->rna_path);
	
	/* copy driver */
	fcu_d->driver= fcurve_copy_driver(fcu_d->driver);
	
	/* copy modifiers */
	fcurve_copy_modifiers(&fcu_d->modifiers, &fcu->modifiers);
	
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
		if (strcmp(fcu->rna_path, rna_path) == 0) {
			/* now check indicies */
			if (fcu->array_index == array_index)
				return fcu;
		}
	}
	
	/* return */
	return NULL;
}

/* Calculate the extents of F-Curve's data */
void calc_fcurve_bounds (FCurve *fcu, float *xmin, float *xmax, float *ymin, float *ymax)
{
	float xminv=999999999.0f, xmaxv=-999999999.0f;
	float yminv=999999999.0f, ymaxv=-999999999.0f;
	short foundvert=0;
	int i;
	
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
					yminv= MIN2(yminv, bezt->vec[1][1]);
					ymaxv= MAX2(ymaxv, bezt->vec[1][1]);
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
					yminv= MIN2(yminv, fpt->vec[1]);
					ymaxv= MAX2(ymaxv, fpt->vec[1]);
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
	int a;

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
			int a;
			
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
	int a;
	
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

/* This frees the driver itself */
void fcurve_free_driver(FCurve *fcu)
{
	ChannelDriver *driver;
	
	/* sanity checks */
	if ELEM(NULL, fcu, fcu->driver)
		return;
	driver= fcu->driver;
	
	/* free RNA-paths, as these were allocated when getting the path string */
	if (driver->rna_path) MEM_freeN(driver->rna_path);
	if (driver->rna_path2) MEM_freeN(driver->rna_path2);
	
	/* free driver itself, then set F-Curve's point to this to NULL (as the curve may still be used) */
	MEM_freeN(driver);
	fcu->driver= NULL;
}

/* This makes a copy of the given driver */
ChannelDriver *fcurve_copy_driver (ChannelDriver *driver)
{
	ChannelDriver *ndriver;
	
	/* sanity checks */
	if (driver == NULL)
		return NULL;
		
	/* copy all data */
	ndriver= MEM_dupallocN(driver);
	ndriver->rna_path= MEM_dupallocN(ndriver->rna_path);
	ndriver->rna_path2= MEM_dupallocN(ndriver->rna_path2);
	
	/* return the new driver */
	return ndriver;
}

/* Driver Evaluation -------------------------- */

/* Helper function to obtain a value using RNA from the specified source (for evaluating drivers) 
 * 	- target: used to specify which of the two driver-targets to use
 */
static float driver_get_driver_value (ChannelDriver *driver, short target)
{
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	ID *id;
	char *path;
	int index;
	float value= 0.0f;
	
	/* get RNA-pointer for the ID-block given in driver */
	if (target == 1) {
		/* second target */
		RNA_id_pointer_create(driver->id2, &id_ptr);
		id= driver->id2;
		path= driver->rna_path2;
		index= driver->array_index2;
	}
	else {
		/* first/main target */
		RNA_id_pointer_create(driver->id, &id_ptr);
		id= driver->id;
		path= driver->rna_path;
		index= driver->array_index;
	}
	
	/* error check for missing pointer... */
	if (id == NULL) {
		printf("Error: driver doesn't have any valid target to use \n");
		if (G.f & G_DEBUG) printf("\tpath = %s [%d] \n", path, index);
		driver->flag |= DRIVER_FLAG_INVALID;
		return 0.0f;
	}
	
	/* get property to read from, and get value as appropriate */
	if (RNA_path_resolve(&id_ptr, path, &ptr, &prop)) {
		switch (RNA_property_type(&ptr, prop)) {
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
	
	return value;
}

/* Evaluate an Channel-Driver to get a 'time' value to use instead of "evaltime"
 *	- "evaltime" is the frame at which F-Curve is being evaluated
 * 	- has to return a float value 
 */
static float evaluate_driver (ChannelDriver *driver, float evaltime)
{
	/* check if driver can be evaluated */
	if (driver->flag & DRIVER_FLAG_DISABLED)
		return 0.0f;
	
	switch (driver->type) {
		case DRIVER_TYPE_CHANNEL: /* channel/setting drivers channel/setting */
			return driver_get_driver_value(driver, 0);
			

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

		
		case DRIVER_TYPE_ROTDIFF: /* difference of rotations of 2 bones (should be in same armature) */
		{
			/*
			float q1[4], q2[4], quat[4], angle;
			
			Mat4ToQuat(pchan->pose_mat, q1);
			Mat4ToQuat(pchan2->pose_mat, q2);
			
			QuatInv(q1);
			QuatMul(quat, q1, q2);
			angle = 2.0f * (saacos(quat[0]));
			angle= ABS(angle);
			
			return (angle > M_PI) ? (float)((2.0f * M_PI) - angle) : (float)(angle);
			*/
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
int findzero (float x, float q0, float q1, float q2, float q3, float *o)
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
			o[0]= (float)(Sqrt3d(-q+t) + Sqrt3d(-q-t) - a);
			
			if ((o[0] >= SMALL) && (o[0] <= 1.000001)) return 1;
			else return 0;
		}
		else if (d == 0.0) {
			t= Sqrt3d(-q);
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

void berekeny (float f1, float f2, float f3, float f4, float *o, int b)
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

void berekenx (float *f, float *o, int b)
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


/* -------------------------- */

/* Calculate F-Curve value for 'evaltime' using BezTriple keyframes */
static float fcurve_eval_keyframes (FCurve *fcu, BezTriple *bezts, float evaltime)
{
	BezTriple *bezt, *prevbezt, *lastbezt;
	float v1[2], v2[2], v3[2], v4[2], opl[32], dx, fac;
	int a, b;
	float cvalue = 0.0f;
	
	/* get pointers */
	a= fcu->totvert-1;
	prevbezt= bezts;
	bezt= prevbezt+1;
	lastbezt= prevbezt + a;
	
	/* evaluation time at or past endpoints? */
	if (prevbezt->vec[1][0] >= evaltime) {
		/* before or on first keyframe */
		if ((fcu->extend == FCURVE_EXTRAPOLATE_LINEAR) && (prevbezt->ipo != BEZT_IPO_CONST)) {
			/* linear or bezier interpolation */
			if (prevbezt->ipo==BEZT_IPO_LIN) {
				/* Use the next center point instead of our own handle for
				 * linear interpolated extrapolate 
				 */
				if (fcu->totvert == 1) 
					cvalue= prevbezt->vec[1][1];
				else {
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
			else {
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
		else {
			/* constant (BEZT_IPO_HORIZ) extrapolation or constant interpolation, 
			 * so just extend first keyframe's value 
			 */
			cvalue= prevbezt->vec[1][1];
		}
	}
	else if (lastbezt->vec[1][0] <= evaltime) {
		/* after or on last keyframe */
		if ((fcu->extend == FCURVE_EXTRAPOLATE_LINEAR) && (lastbezt->ipo != BEZT_IPO_CONST)) {
			/* linear or bezier interpolation */
			if (lastbezt->ipo==BEZT_IPO_LIN) {
				/* Use the next center point instead of our own handle for
				 * linear interpolated extrapolate 
				 */
				if (fcu->totvert == 1) 
					cvalue= lastbezt->vec[1][1];
				else {
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
			else {
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
		else {
			/* constant (BEZT_IPO_HORIZ) extrapolation or constant interpolation, 
			 * so just extend last keyframe's value 
			 */
			cvalue= lastbezt->vec[1][1];
		}
	}
	else {
		/* evaltime occurs somewhere in the middle of the curve */
		for (a=0; prevbezt && bezt && (a < fcu->totvert-1); a++, prevbezt=bezt, bezt++) {  
			/* evaltime occurs within the interval defined by these two keyframes */
			if ((prevbezt->vec[1][0] <= evaltime) && (bezt->vec[1][0] >= evaltime)) {
				/* value depends on interpolation mode */
				if (prevbezt->ipo == BEZT_IPO_CONST) {
					/* constant (evaltime not relevant, so no interpolation needed) */
					cvalue= prevbezt->vec[1][1];
				}
				else if (prevbezt->ipo == BEZT_IPO_LIN) {
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
				else {
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

/* ******************************** F-Curve Modifiers ********************************* */

/* Template --------------------------- */

/* Each modifier defines a set of functions, which will be called at the appropriate
 * times. In addition to this, each modifier should have a type-info struct, where
 * its functions are attached for use. 
 */
 
/* Template for type-info data:
 *	- make a copy of this when creating new modifiers, and just change the functions
 *	  pointed to as necessary
 *	- although the naming of functions doesn't matter, it would help for code
 *	  readability, to follow the same naming convention as is presented here
 * 	- any functions that a constraint doesn't need to define, don't define
 *	  for such cases, just use NULL 
 *	- these should be defined after all the functions have been defined, so that
 * 	  forward-definitions/prototypes don't need to be used!
 *	- keep this copy #if-def'd so that future constraints can get based off this
 */
#if 0
static FModifierTypeInfo FMI_MODNAME = {
	FMODIFIER_TYPE_MODNAME, /* type */
	sizeof(FMod_ModName), /* size */
	FMI_TYPE_SOME_ACTION, /* action type */
	FMI_REQUIRES_SOME_REQUIREMENT, /* requirements */
	"Modifier Name", /* name */
	"FMod_ModName", /* struct name */
	fcm_modname_free, /* free data */
	fcm_modname_relink, /* relink data */
	fcm_modname_copy, /* copy data */
	fcm_modname_new_data, /* new data */
	fcm_modname_evaluate /* evaluate */
};
#endif

/* Generator F-Curve Modifier --------------------------- */

/* Generators available:
 * 	1) simple polynomial generator:
 *		- Exanded form - (y = C[0]*(x^(n)) + C[1]*(x^(n-1)) + ... + C[n])  
 *		- Factorised form - (y = (C[0][0]*x + C[0][1]) * (C[1][0]*x + C[1][1]) * ... * (C[n][0]*x + C[n][1]))
 *	2) simple builin 'functions':
 *		of the form (y = C[0] * fn( C[1]*x + C[2] ) + C[3])
 * 	   where fn() can be any one of:
 *		sin, cos, tan, ln, sqrt
 *	3) expression...
 */

static void fcm_generator_free (FModifier *fcm)
{
	FMod_Generator *data= (FMod_Generator *)fcm->data;
	
	/* free polynomial coefficients array */
	if (data->coefficients)
		MEM_freeN(data->coefficients);
}

static void fcm_generator_copy (FModifier *fcm, FModifier *src)
{
	FMod_Generator *gen= (FMod_Generator *)fcm->data;
	FMod_Generator *ogen= (FMod_Generator *)src->data;
	
	/* copy coefficients array? */
	if (ogen->coefficients)
		gen->coefficients= MEM_dupallocN(ogen->coefficients);
}

static void fcm_generator_new_data (void *mdata)
{
	FMod_Generator *data= (FMod_Generator *)mdata;
	float *cp;
	
	/* set default generator to be linear 0-1 (gradient = 1, y-offset = 0) */
	data->poly_order= 1;
	data->arraysize= 2;
	cp= data->coefficients= MEM_callocN(sizeof(float)*2, "FMod_Generator_Coefs");
	cp[0] = 0; // y-offset 
	cp[1] = 1; // gradient
}


static void fcm_generator_evaluate (FCurve *fcu, FModifier *fcm, float *cvalue, float evaltime)
{
	FMod_Generator *data= (FMod_Generator *)fcm->data;
	
	/* behaviour depends on mode 
	 * NOTE: the data in its default state is fine too
	 */
	switch (data->mode) {
		case FCM_GENERATOR_POLYNOMIAL: /* expanded polynomial expression */
		{
			/* we overwrite cvalue with the sum of the polynomial */
			float *powers = MEM_callocN(sizeof(float)*data->arraysize, "Poly Powers");
			float value= 0.0f;
			unsigned int i;
			
			/* for each x^n, precalculate value based on previous one first... this should be 
			 * faster that calling pow() for each entry
			 */
			for (i=0; i < data->arraysize; i++) {
				/* first entry is x^0 = 1, otherwise, calculate based on previous */
				if (i)
					powers[i]= powers[i-1] * evaltime;
				else
					powers[0]= 1;
			}
			
			/* for each coefficient, add to value, which we'll write to *cvalue in one go */
			for (i=0; i < data->arraysize; i++)
				value += data->coefficients[i] * powers[i];
			
			/* only if something changed, write *cvalue in one go */
			if (data->poly_order)
				*cvalue= value;
				
			/* cleanup */
			if (powers) 
				MEM_freeN(powers);
		}
			break;
			
		case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* factorised polynomial */
		{
			float value= 1.0f, *cp=NULL;
			unsigned int i;
			
			/* for each coefficient pair, solve for that bracket before accumulating in value by multiplying */
			for (cp=data->coefficients, i=0; (cp) && (i < data->poly_order); cp+=2, i++) 
				value *= (cp[0]*evaltime + cp[1]);
				
			/* only if something changed, write *cvalue in one go */
			if (data->poly_order)
				*cvalue= value;
		}
			break;
			
		case FCM_GENERATOR_FUNCTION: /* builtin function */
		{
			double arg= data->coefficients[1]*evaltime + data->coefficients[2];
			double (*fn)(double v) = NULL;
			
			/* get function pointer to the func to use:
			 * WARNING: must perform special argument validation hereto guard against crashes  
			 */
			switch (data->func_type)
			{
				/* simple ones */			
				case FCM_GENERATOR_FN_SIN: /* sine wave */
					fn= sin;
					break;
				case FCM_GENERATOR_FN_COS: /* cosine wave */
					fn= cos;
					break;
					
				/* validation required */
				case FCM_GENERATOR_FN_TAN: /* tangent wave */
				{
					/* check that argument is not on one of the discontinuities (i.e. 90deg, 270 deg, etc) */
					if IS_EQ(fmod((arg - M_PI_2), M_PI), 0.0)
						*cvalue= 0.0f; /* no value possible here */
					else
						fn= tan;
				}
					break;
				case FCM_GENERATOR_FN_LN: /* natural log */
				{
					/* check that value is greater than 1? */
					if (arg > 1.0f)
						fn= log;
					else
						*cvalue= 0.0f; /* no value possible here */
				}
					break;
				case FCM_GENERATOR_FN_SQRT: /* square root */
				{
					/* no negative numbers */
					if (arg > 0.0f)
						fn= sqrt;
					else
						*cvalue= 0.0f; /* no vlaue possible here */
				}
					break;
					
				default:
					printf("Invalid Function-Generator for F-Modifier - %d \n", data->func_type);
			}
			
			/* execute function callback to set value if appropriate */
			if (fn)
				*cvalue= data->coefficients[0]*fn(arg) + data->coefficients[3];
		}
			break;

#ifndef DISABLE_PYTHON
		case FCM_GENERATOR_EXPRESSION: /* py-expression */
			// TODO...
			break;
#endif /* DISABLE_PYTHON */
	}
}

static FModifierTypeInfo FMI_GENERATOR = {
	FMODIFIER_TYPE_GENERATOR, /* type */
	sizeof(FMod_Generator), /* size */
	FMI_TYPE_GENERATE_CURVE, /* action type */
	FMI_REQUIRES_NOTHING, /* requirements */
	"Generator", /* name */
	"FMod_Generator", /* struct name */
	fcm_generator_free, /* free data */
	fcm_generator_copy, /* copy data */
	fcm_generator_new_data, /* new data */
	fcm_generator_evaluate /* evaluate */
};

/* Envelope F-Curve Modifier --------------------------- */

static void fcm_envelope_free (FModifier *fcm)
{
	FMod_Envelope *data= (FMod_Envelope *)fcm->data;
	
	/* free envelope data array */
	if (data->data)
		MEM_freeN(data->data);
}

static void fcm_envelope_copy (FModifier *fcm, FModifier *src)
{
	FMod_Envelope *gen= (FMod_Envelope *)fcm->data;
	FMod_Envelope *ogen= (FMod_Envelope *)src->data;
	
	/* copy envelope data array */
	if (ogen->data)
		gen->data= MEM_dupallocN(ogen->data);
}

static void fcm_envelope_evaluate (FCurve *fcu, FModifier *fcm, float *cvalue, float evaltime)
{
	FMod_Envelope *env= (FMod_Envelope *)fcm->data;
	FCM_EnvelopeData *fed, *prevfed, *lastfed;
	float min=0.0f, max=0.0f, fac=0.0f;
	int a;
	
	/* get pointers */
	if (env->data == NULL) return;
	prevfed= env->data;
	fed= prevfed + 1;
	lastfed= prevfed + env->totvert-1;
	
	/* get min/max values for envelope at evaluation time (relative to mid-value) */
	if (prevfed->time >= evaltime) {
		/* before or on first sample, so just extend value */
		min= prevfed->min;
		max= prevfed->max;
	}
	else if (lastfed->time <= evaltime) {
		/* after or on last sample, so just extend value */
		min= lastfed->min;
		max= lastfed->max;
	}
	else {
		/* evaltime occurs somewhere between segments */
		for (a=0; prevfed && fed && (a < env->totvert-1); a++, prevfed=fed, fed++) {  
			/* evaltime occurs within the interval defined by these two envelope points */
			if ((prevfed->time <= evaltime) && (fed->time >= evaltime)) {
				float afac, bfac, diff;
				
				diff= fed->time - prevfed->time;
				afac= (evaltime - prevfed->time) / diff;
				bfac= (fed->time - evaltime)/(diff);
				
				min= afac*prevfed->min + bfac*fed->min;
				max= afac*prevfed->max + bfac*fed->max;
				
				break;
			}
		}
	}
	
	/* adjust *cvalue 
	 * NOTE: env->min/max are relative to env->midval, and can be either +ve OR -ve, so we add...
	 */
	fac= (*cvalue - min) / (max - min);
	*cvalue= (env->midval + env->min) + (fac * (env->max - env->min)); 
}

static FModifierTypeInfo FMI_ENVELOPE = {
	FMODIFIER_TYPE_ENVELOPE, /* type */
	sizeof(FMod_Envelope), /* size */
	FMI_TYPE_REPLACE_VALUES, /* action type */
	0, /* requirements */
	"Envelope", /* name */
	"FMod_Envelope", /* struct name */
	fcm_envelope_free, /* free data */
	fcm_envelope_copy, /* copy data */
	NULL, /* new data */
	fcm_envelope_evaluate /* evaluate */
};

/* Cycles F-Curve Modifier  --------------------------- */

/* This modifier changes evaltime to something that exists within the curve's frame-range, 
 * then re-evaluates modifier stack up to this point using the new time. This re-entrant behaviour
 * is very likely to be more time-consuming than the original approach... (which was tighly integrated into 
 * the calculation code...).
 *
 * NOTE: this needs to be at the start of the stack to be of use, as it needs to know the extents of the keyframes/sample-data
 * Possible TODO - store length of cycle information that can be initialised from the extents of the keyframes/sample-data, and adjusted
 * 				as appropriate
 */

static void fcm_cycles_evaluate (FCurve *fcu, FModifier *fcm, float *cvalue, float evaltime)
{
	FMod_Cycles *data= (FMod_Cycles *)fcm->data;
	ListBase mods = {NULL, NULL};
	float prevkey[2], lastkey[2], cycyofs=0.0f;
	float new_value;
	short side=0, mode=0;
	int cycles=0;
	
	/* check if modifier is first in stack, otherwise disable ourself... */
	// FIXME...
	if (fcm->prev) {
		fcm->flag |= FMODIFIER_FLAG_DISABLED;
		return;
	}
	
	/* calculate new evaltime due to cyclic interpolation */
	if (fcu && fcu->bezt) {
		BezTriple *prevbezt= fcu->bezt;
		BezTriple *lastbezt= prevbezt + fcu->totvert-1;
		
		prevkey[0]= prevbezt->vec[1][0];
		prevkey[1]= prevbezt->vec[1][1];
		
		lastkey[0]= lastbezt->vec[1][0];
		lastkey[1]= lastbezt->vec[1][1];
	}
	else if (fcu && fcu->fpt) {
		FPoint *prevfpt= fcu->fpt;
		FPoint *lastfpt= prevfpt + fcu->totvert-1;
		
		prevkey[0]= prevfpt->vec[0];
		prevkey[1]= prevfpt->vec[1];
		
		lastkey[0]= lastfpt->vec[0];
		lastkey[1]= lastfpt->vec[1];
	}
	else
		return;
		
	/* check if modifier will do anything
	 *	1) if in data range, definitely don't do anything
	 *	2) if before first frame or after last frame, make sure some cycling is in use
	 */
	if (evaltime < prevkey[0]) {
		if (data->before_mode) {
			side= -1;
			mode= data->before_mode;
			cycles= data->before_cycles;
		}
	}
	else if (evaltime > lastkey[0]) {
		if (data->after_mode) {
			side= 1;
			mode= data->after_mode;
			cycles= data->after_cycles;
		}
	}
	if ELEM(0, side, mode)
		return;
		
	/* extrapolation mode is 'cyclic' - find relative place within a cycle */
	// FIXME: adding the more fine-grained control of extrpolation mode
	{
		float cycdx=0, cycdy=0, ofs=0;
		
		/* ofs is start frame of cycle */
		ofs= prevkey[0];
		
		/* calculate period and amplitude (total height) of a cycle */
		cycdx= lastkey[0] - prevkey[0];
		cycdy= lastkey[1] - prevkey[1];
		
		/* check if cycle is infinitely small, to be point of being impossible to use */
		if (cycdx == 0)
			return;
		/* check that cyclic is still enabled for the specified time */
		if (cycles == 0) {
			/* catch this case so that we don't exit when we have cycles=0
			 * as this indicates infinite cycles...
			 */
		}
		else if ( ((float)side * (evaltime - ofs) / cycdx) > cycles )
			return;
		
		
		/* check if 'cyclic extrapolation', and thus calculate y-offset for this cycle */
		if (mode == FCM_EXTRAPOLATE_CYCLIC_OFFSET) {
			cycyofs = (float)floor((evaltime - ofs) / cycdx);
			cycyofs *= cycdy;
		}
		
		/* calculate where in the cycle we are (overwrite evaltime to reflect this) */
		evaltime= (float)(fmod(evaltime-ofs, cycdx) + ofs);
		if (evaltime < ofs) evaltime += cycdx;
	}
	
	
	/* store modifiers after (and including ourself) before recalculating curve with new evaltime */
	mods= fcu->modifiers;
	fcu->modifiers.first= fcu->modifiers.last= NULL;
	
	/* re-enter the evaluation loop (but without the burden of evaluating any modifiers, so 'should' be relatively quick) */
	new_value= evaluate_fcurve(fcu, evaltime);
	
	/* restore modifiers, and set new value (don't assume everything is still ok after being re-entrant) */
	fcu->modifiers= mods;
	*cvalue= new_value + cycyofs;
}

static FModifierTypeInfo FMI_CYCLES = {
	FMODIFIER_TYPE_CYCLES, /* type */
	sizeof(FMod_Cycles), /* size */
	FMI_TYPE_EXTRAPOLATION, /* action type */
	FMI_REQUIRES_ORIGINAL_DATA, /* requirements */
	"Cycles", /* name */
	"FMod_Cycles", /* struct name */
	NULL, /* free data */
	NULL, /* copy data */
	NULL, /* new data */
	fcm_cycles_evaluate /* evaluate */
};

/* Noise F-Curve Modifier  --------------------------- */

#if 0 // XXX not yet implemented 
static FModifierTypeInfo FMI_NOISE = {
	FMODIFIER_TYPE_NOISE, /* type */
	sizeof(FMod_Noise), /* size */
	FMI_TYPE_REPLACE_VALUES, /* action type */
	0, /* requirements */
	"Noise", /* name */
	"FMod_Noise", /* struct name */
	NULL, /* free data */
	NULL, /* copy data */
	fcm_noise_new_data, /* new data */
	fcm_noise_evaluate /* evaluate */
};
#endif // XXX not yet implemented

/* Filter F-Curve Modifier --------------------------- */

#if 0 // XXX not yet implemented 
static FModifierTypeInfo FMI_FILTER = {
	FMODIFIER_TYPE_FILTER, /* type */
	sizeof(FMod_Filter), /* size */
	FMI_TYPE_REPLACE_VALUES, /* action type */
	0, /* requirements */
	"Filter", /* name */
	"FMod_Filter", /* struct name */
	NULL, /* free data */
	NULL, /* copy data */
	NULL, /* new data */
	fcm_filter_evaluate /* evaluate */
};
#endif // XXX not yet implemented


/* Python F-Curve Modifier --------------------------- */

static void fcm_python_free (FModifier *fcm)
{
	FMod_Python *data= (FMod_Python *)fcm->data;
	
	/* id-properties */
	IDP_FreeProperty(data->prop);
	MEM_freeN(data->prop);
}

static void fcm_python_new_data (void *mdata) 
{
	FMod_Python *data= (FMod_Python *)mdata;
	
	/* everything should be set correctly by calloc, except for the prop->type constant.*/
	data->prop = MEM_callocN(sizeof(IDProperty), "PyFModifierProps");
	data->prop->type = IDP_GROUP;
}

static void fcm_python_copy (FModifier *fcm, FModifier *src)
{
	FMod_Python *pymod = (FMod_Python *)fcm->data;
	FMod_Python *opymod = (FMod_Python *)src->data;
	
	pymod->prop = IDP_CopyProperty(opymod->prop);
}

static void fcm_python_evaluate (FCurve *fcu, FModifier *fcm, float *cvalue, float evaltime)
{
#ifndef DISABLE_PYTHON
	//FMod_Python *data= (FMod_Python *)fcm->data;
	
	/* FIXME... need to implement this modifier...
	 *	It will need it execute a script using the custom properties 
	 */
#endif /* DISABLE_PYTHON */
}

static FModifierTypeInfo FMI_PYTHON = {
	FMODIFIER_TYPE_PYTHON, /* type */
	sizeof(FMod_Python), /* size */
	FMI_TYPE_GENERATE_CURVE, /* action type */
	FMI_REQUIRES_RUNTIME_CHECK, /* requirements */
	"Python", /* name */
	"FMod_Python", /* struct name */
	fcm_python_free, /* free data */
	fcm_python_copy, /* copy data */
	fcm_python_new_data, /* new data */
	fcm_python_evaluate /* evaluate */
};


/* F-Curve Modifier API --------------------------- */
/* All of the F-Curve Modifier api functions use FModifierTypeInfo structs to carry out
 * and operations that involve F-Curve modifier specifc code.
 */

/* These globals only ever get directly accessed in this file */
static FModifierTypeInfo *fmodifiersTypeInfo[FMODIFIER_NUM_TYPES];
static short FMI_INIT= 1; /* when non-zero, the list needs to be updated */

/* This function only gets called when FMI_INIT is non-zero */
static void fmods_init_typeinfo () 
{
	fmodifiersTypeInfo[0]=  NULL; 					/* 'Null' F-Curve Modifier */
	fmodifiersTypeInfo[1]=  &FMI_GENERATOR; 		/* Generator F-Curve Modifier */
	fmodifiersTypeInfo[2]=  &FMI_ENVELOPE;			/* Envelope F-Curve Modifier */
	fmodifiersTypeInfo[3]=  &FMI_CYCLES;			/* Cycles F-Curve Modifier */
	fmodifiersTypeInfo[4]=  NULL/*&FMI_NOISE*/;				/* Apply-Noise F-Curve Modifier */ // XXX unimplemented
	fmodifiersTypeInfo[5]=  NULL/*&FMI_FILTER*/;			/* Filter F-Curve Modifier */  // XXX unimplemented
	fmodifiersTypeInfo[6]=  &FMI_PYTHON;			/* Custom Python F-Curve Modifier */
}

/* This function should be used for getting the appropriate type-info when only
 * a F-Curve modifier type is known
 */
FModifierTypeInfo *get_fmodifier_typeinfo (int type)
{
	/* initialise the type-info list? */
	if (FMI_INIT) {
		fmods_init_typeinfo();
		FMI_INIT = 0;
	}
	
	/* only return for valid types */
	if ( (type >= FMODIFIER_TYPE_NULL) && 
		 (type <= FMODIFIER_NUM_TYPES ) ) 
	{
		/* there shouldn't be any segfaults here... */
		return fmodifiersTypeInfo[type];
	}
	else {
		printf("No valid F-Curve Modifier type-info data available. Type = %i \n", type);
	}
	
	return NULL;
} 
 
/* This function should always be used to get the appropriate type-info, as it
 * has checks which prevent segfaults in some weird cases.
 */
FModifierTypeInfo *fmodifier_get_typeinfo (FModifier *fcm)
{
	/* only return typeinfo for valid modifiers */
	if (fcm)
		return get_fmodifier_typeinfo(fcm->type);
	else
		return NULL;
}

/* API --------------------------- */

/* Add a new F-Curve Modifier to the given F-Curve of a certain type */
FModifier *fcurve_add_modifier (FCurve *fcu, int type)
{
	FModifierTypeInfo *fmi= get_fmodifier_typeinfo(type);
	FModifier *fcm;
	
	/* sanity checks */
	if ELEM(NULL, fcu, fmi)
		return NULL;
	
	/* special checks for whether modifier can be added */
	if ((fcu->modifiers.first) && (type == FMODIFIER_TYPE_CYCLES)) {
		/* cycles modifier must be first in stack, so for now, don't add if it can't be */
		// TODO: perhaps there is some better way, but for now, 
		printf("Error: Cannot add 'Cycles' modifier to F-Curve, as 'Cycles' modifier can only be first in stack. \n");
		return NULL;
	}
	
	/* add modifier itself */
	fcm= MEM_callocN(sizeof(FModifier), "F-Curve Modifier");
	fcm->type = type;
	fcm->flag = FMODIFIER_FLAG_EXPANDED;
	BLI_addtail(&fcu->modifiers, fcm);
	
	/* add modifier's data */
	fcm->data= MEM_callocN(fmi->size, "F-Curve Modifier Data");
		
	/* init custom settings if necessary */
	if (fmi->new_data)	
		fmi->new_data(fcm->data);
		
	/* return modifier for further editing */
	return fcm;
}

/* Duplicate all of the F-Curve Modifiers in the Modifier stacks */
void fcurve_copy_modifiers (ListBase *dst, ListBase *src)
{
	FModifier *fcm, *srcfcm;
	
	if ELEM(NULL, dst, src)
		return;
	
	dst->first= dst->last= NULL;
	BLI_duplicatelist(dst, src);
	
	for (fcm=dst->first, srcfcm=src->first; fcm && srcfcm; srcfcm=srcfcm->next, fcm=fcm->next) {
		FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
		
		/* make a new copy of the F-Modifier's data */
		fcm->data = MEM_dupallocN(fcm->data);
		
		/* only do specific constraints if required */
		if (fmi && fmi->copy_data)
			fmi->copy_data(fcm, srcfcm);
	}
}

/* Remove and free the given F-Curve Modifier from the given F-Curve's stack  */
void fcurve_remove_modifier (FCurve *fcu, FModifier *fcm)
{
	FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
	
	/* sanity check */
	if (fcm == NULL)
		return;
	
	/* free modifier's special data (stored inside fcm->data) */
	if (fmi && fmi->free_data)
		fmi->free_data(fcm);
		
	/* free modifier's data (fcm->data) */
	MEM_freeN(fcm->data);
	
	/* remove modifier from stack */
	if (fcu)
		BLI_freelinkN(&fcu->modifiers, fcm);
	else {
		// XXX this case can probably be removed some day, as it shouldn't happen...
		printf("fcurve_remove_modifier() - no fcurve \n");
		MEM_freeN(fcm);
	}
}

/* Remove all of a given F-Curve's modifiers */
void fcurve_free_modifiers (FCurve *fcu)
{
	FModifier *fcm, *fmn;
	
	/* sanity check */
	if (fcu == NULL)
		return;
	
	/* free each modifier in order - modifier is unlinked from list and freed */
	for (fcm= fcu->modifiers.first; fcm; fcm= fmn) {
		fmn= fcm->next;
		fcurve_remove_modifier(fcu, fcm);
	}
}

/* Bake modifiers for given F-Curve to curve sample data, in the frame range defined
 * by start and end (inclusive).
 */
void fcurve_bake_modifiers (FCurve *fcu, int start, int end)
{
	ChannelDriver *driver;
	
	/* sanity checks */
	// TODO: make these tests report errors using reports not printf's
	if ELEM(NULL, fcu, fcu->modifiers.first) {
		printf("Error: No F-Curve with F-Curve Modifiers to Bake\n");
		return;
	}
	
	/* temporarily, disable driver while we sample, so that they don't influence the outcome */
	driver= fcu->driver;
	fcu->driver= NULL;
	
	/* bake the modifiers, by sampling the curve at each frame */
	fcurve_store_samples(fcu, NULL, start, end, fcurve_samplingcb_evalcurve);
	
	/* free the modifiers now */
	fcurve_free_modifiers(fcu);
	
	/* restore driver */
	fcu->driver= driver;
}

/* Find the active F-Curve Modifier */
FModifier *fcurve_active_modifier (FCurve *fcu)
{
	FModifier *fcm;
	
	/* sanity checks */
	if ELEM(NULL, fcu, fcu->modifiers.first)
		return NULL;
	
	/* loop over modifiers until 'active' one is found */
	for (fcm= fcu->modifiers.first; fcm; fcm= fcm->next) {
		if (fcm->flag & FMODIFIER_FLAG_ACTIVE)
			return fcm;
	}
	
	/* no modifier is active */
	return NULL;
}

/* ***************************** F-Curve - Evaluation ********************************* */

/* Evaluate and return the value of the given F-Curve at the specified frame ("evaltime") 
 * Note: this is also used for drivers
 */
float evaluate_fcurve (FCurve *fcu, float evaltime) 
{
	FModifier *fcm;
	float cvalue = 0.0f;
	
	/* if there is a driver (only if this F-Curve is acting as 'driver'), evaluate it to find value to use as "evaltime" 
	 *	- this value will also be returned as the value of the 'curve', if there are no keyframes
	 */
	if (fcu->driver) {
		/* evaltime now serves as input for the curve */
		evaltime= cvalue= evaluate_driver(fcu->driver, evaltime);
	}
	
	/* evaluate curve-data */
	if (fcu->bezt)
		cvalue= fcurve_eval_keyframes(fcu, fcu->bezt, evaltime);
	else if (fcu->fpt)
		cvalue= fcurve_eval_samples(fcu, fcu->fpt, evaltime);
	
	/* evaluate modifiers */
	for (fcm= fcu->modifiers.first; fcm; fcm= fcm->next) {
		FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
		
		/* only evaluate if there's a callback for this */
		// TODO: implement the 'influence' control feature...
		if (fmi && fmi->evaluate_modifier) {
			if ((fcm->flag & FMODIFIER_FLAG_DISABLED) == 0)
				fmi->evaluate_modifier(fcu, fcm, &cvalue, evaltime);
		}
	}
	
	/* return evaluated value */
	return cvalue;
}

/* Calculate the value of the given F-Curve at the given frame, and set its curval */
// TODO: will this be necessary?
void calculate_fcurve (FCurve *fcu, float ctime)
{
	/* calculate and set curval (evaluates driver too) */
	fcu->curval= evaluate_fcurve(fcu, ctime);
}

