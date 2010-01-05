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
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_FCURVE_H
#define BKE_FCURVE_H

struct FCurve;
struct FModifier;
struct ChannelDriver;
struct DriverTarget;

struct BezTriple;
struct StructRNA;

#include "DNA_curve_types.h"

/* ************** Keyframe Tools ***************** */

// XXX this stuff is defined in BKE_ipo.h too, so maybe skip for now?
typedef struct CfraElem {
	struct CfraElem *next, *prev;
	float cfra;
	int sel;
} CfraElem;

void bezt_add_to_cfra_elem(ListBase *lb, struct BezTriple *bezt);

/* ************** F-Curve Drivers ***************** */

void fcurve_free_driver(struct FCurve *fcu);
struct ChannelDriver *fcurve_copy_driver(struct ChannelDriver *driver);

void driver_free_target(struct ChannelDriver *driver, struct DriverTarget *dtar);
struct DriverTarget *driver_add_new_target(struct ChannelDriver *driver);

float driver_get_target_value(struct ChannelDriver *driver, struct DriverTarget *dtar);

/* ************** F-Curve Modifiers *************** */

/* F-Curve Modifier Type-Info (fmi):
 *  This struct provides function pointers for runtime, so that functions can be
 *  written more generally (with fewer/no special exceptions for various modifiers).
 *
 *  Callers of these functions must check that they actually point to something useful,
 *  as some constraints don't define some of these.
 *
 *  Warning: it is not too advisable to reorder order of members of this struct,
 *			as you'll have to edit quite a few ($FMODIFIER_NUM_TYPES) of these
 *			structs.
 */
typedef struct FModifierTypeInfo {
	/* admin/ident */
	short type;				/* FMODIFIER_TYPE_### */
	short size;				/* size in bytes of the struct */
	short acttype;			/* eFMI_Action_Types */
	short requires;			/* eFMI_Requirement_Flags */
	char name[64]; 			/* name of modifier in interface */
	char structName[64];	/* name of struct for SDNA */
	
	/* data management function pointers - special handling */
		/* free any data that is allocated separately (optional) */
	void (*free_data)(struct FModifier *fcm);
		/* copy any special data that is allocated separately (optional) */
	void (*copy_data)(struct FModifier *fcm, struct FModifier *src);
		/* set settings for data that will be used for FCuModifier.data (memory already allocated using MEM_callocN) */
	void (*new_data)(void *mdata);
		/* verifies that the modifier settings are valid */
	void (*verify_data)(struct FModifier *fcm);
	
	/* evaluation */
		/* evaluate time that the modifier requires the F-Curve to be evaluated at */
	float (*evaluate_modifier_time)(struct FCurve *fcu, struct FModifier *fcm, float cvalue, float evaltime);
		/* evaluate the modifier for the given time and 'accumulated' value */
	void (*evaluate_modifier)(struct FCurve *fcu, struct FModifier *fcm, float *cvalue, float evaltime);
} FModifierTypeInfo;

/* Values which describe the behaviour of a FModifier Type */
typedef enum eFMI_Action_Types {
		/* modifier only modifies values outside of data range */
	FMI_TYPE_EXTRAPOLATION = 0,
		/* modifier leaves data-points alone, but adjusts the interpolation between and around them */
	FMI_TYPE_INTERPOLATION,
		/* modifier only modifies the values of points (but times stay the same) */
	FMI_TYPE_REPLACE_VALUES,
		/* modifier generates a curve regardless of what came before */
	FMI_TYPE_GENERATE_CURVE,
} eFMI_Action_Types;

/* Flags for the requirements of a FModifier Type */
typedef enum eFMI_Requirement_Flags {
		/* modifier requires original data-points (kindof beats the purpose of a modifier stack?) */
	FMI_REQUIRES_ORIGINAL_DATA		= (1<<0),
		/* modifier doesn't require on any preceeding data (i.e. it will generate a curve). 
		 * Use in conjunction with FMI_TYPE_GENRATE_CURVE 
		 */
	FMI_REQUIRES_NOTHING			= (1<<1),
		/* refer to modifier instance */
	FMI_REQUIRES_RUNTIME_CHECK		= (1<<2),
} eFMI_Requirement_Flags;

/* Function Prototypes for FModifierTypeInfo's */
FModifierTypeInfo *fmodifier_get_typeinfo(struct FModifier *fcm);
FModifierTypeInfo *get_fmodifier_typeinfo(int type);

/* ---------------------- */

struct FModifier *add_fmodifier(ListBase *modifiers, int type);
void copy_fmodifiers(ListBase *dst, ListBase *src);
int remove_fmodifier(ListBase *modifiers, struct FModifier *fcm);
int remove_fmodifier_index(ListBase *modifiers, int index);
void free_fmodifiers(ListBase *modifiers);

struct FModifier *find_active_fmodifier(ListBase *modifiers);
void set_active_fmodifier(ListBase *modifiers, struct FModifier *fcm);

short list_has_suitable_fmodifier(ListBase *modifiers, int mtype, short acttype);

float evaluate_time_fmodifiers(ListBase *modifiers, struct FCurve *fcu, float cvalue, float evaltime);
void evaluate_value_fmodifiers(ListBase *modifiers, struct FCurve *fcu, float *cvalue, float evaltime);

void fcurve_bake_modifiers(struct FCurve *fcu, int start, int end);

/* ************** F-Curves API ******************** */

/* -------- Data Managemnt  --------  */

void free_fcurve(struct FCurve *fcu);
struct FCurve *copy_fcurve(struct FCurve *fcu);

void free_fcurves(ListBase *list);
void copy_fcurves(ListBase *dst, ListBase *src);

/* find matching F-Curve in the given list of F-Curves */
struct FCurve *list_find_fcurve(ListBase *list, const char rna_path[], const int array_index);

/* high level function to get an fcurve from C without having the rna */
struct FCurve *id_data_find_fcurve(ID *id, void *data, struct StructRNA *type, char *prop_name, int index);

/* Get list of LinkData's containing pointers to the F-Curves which control the types of data indicated 
 *	e.g.  numMatches = list_find_data_fcurves(matches, &act->curves, "pose.bones[", "MyFancyBone");
 */
int list_find_data_fcurves(ListBase *dst, ListBase *src, const char *dataPrefix, const char *dataName);

/* Binary search algorithm for finding where to 'insert' BezTriple with given frame number.
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
int binarysearch_bezt_index(struct BezTriple array[], float frame, int arraylen, short *replace);

/* get the time extents for F-Curve */
void calc_fcurve_range(struct FCurve *fcu, float *min, float *max);

/* get the bounding-box extents for F-Curve */
void calc_fcurve_bounds(struct FCurve *fcu, float *xmin, float *xmax, float *ymin, float *ymax);

/* -------- Curve Sanity --------  */

void calchandles_fcurve(struct FCurve *fcu);
void testhandles_fcurve(struct FCurve *fcu);
void sort_time_fcurve(struct FCurve *fcu);
short test_time_fcurve(struct FCurve *fcu);

void correct_bezpart(float *v1, float *v2, float *v3, float *v4);

/* -------- Evaluation --------  */

/* evaluate fcurve */
float evaluate_fcurve(struct FCurve *fcu, float evaltime);
/* evaluate fcurve and store value */
void calculate_fcurve(struct FCurve *fcu, float ctime);

/* ************* F-Curve Samples API ******************** */

/* -------- Defines --------  */

/* Basic signature for F-Curve sample-creation function 
 *	- fcu: the F-Curve being operated on
 *	- data: pointer to some specific data that may be used by one of the callbacks
 */
typedef float (*FcuSampleFunc)(struct FCurve *fcu, void *data, float evaltime);

/* ----- Sampling Callbacks ------  */

/* Basic sampling callback which acts as a wrapper for evaluate_fcurve() */
float fcurve_samplingcb_evalcurve(struct FCurve *fcu, void *data, float evaltime);

/* -------- Main Methods --------  */

/* Main API function for creating a set of sampled curve data, given some callback function 
 * used to retrieve the values to store.
 */
void fcurve_store_samples(struct FCurve *fcu, void *data, int start, int end, FcuSampleFunc sample_cb);

#endif /* BKE_FCURVE_H*/
