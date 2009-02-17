/* Testing code for 2.5 animation system 
 * Copyright 2009, Joshua Leung
 */
 
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_dynstr.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_utildefines.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_key.h"
#include "BKE_material.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

/* ************************************************** */
/* LOCAL TYPES AND DEFINES */

/* ----------- Common KeyData Sources ------------ */

/* temporary struct to gather data combos to keyframe */
typedef struct bCommonKeySrc {
	struct bCommonKeySrc *next, *prev;
		
		/* general data/destination-source settings */
	ID *id;					/* id-block this comes from */
	char *rna_path;			/* base path to use */	// xxx.... maybe we don't need this?
	
		/* specific cases */
	bPoseChannel *pchan;	/* only needed when doing recalcs... */
} bCommonKeySrc;

/* ******************************************* */
/* Animation Data Validation */

/* Get (or add relevant data to be able to do so) F-Curve from the Active Action, 
 * for the given Animation Data block. This assumes that all the destinations are valid.
 */
FCurve *verify_fcurve (ID *id, const char group[], const char rna_path[], const int array_index, short add)
{
	AnimData *adt;
	bAction *act;
	bActionGroup *grp;
	FCurve *fcu;
	
	/* sanity checks */
	if ELEM(NULL, id, rna_path)
		return NULL;
	
	/* init animdata if none available yet */
	adt= BKE_animdata_from_id(id);
	if ((adt == NULL) && (add))
		adt= BKE_id_add_animdata(id);
	if (adt == NULL) { 
		/* if still none (as not allowed to add, or ID doesn't have animdata for some reason) */
		return NULL;
	}
		
	/* init action if none available yet */
	// TODO: need some wizardry to handle NLA stuff correct
	if ((adt->action == NULL) && (add))
		adt->action= add_empty_action("Action");
	act= adt->action;
		
	/* try to find f-curve matching for this setting 
	 *	- add if not found and allowed to add one
	 *		TODO: add auto-grouping support? how this works will need to be resolved
	 */
	if (act)
		fcu= list_find_fcurve(&act->curves, rna_path, array_index);
	else
		fcu= NULL;
	
	if ((fcu == NULL) && (add)) {
		/* use default settings to make a F-Curve */
		fcu= MEM_callocN(sizeof(FCurve), "FCurve");
		
		fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
		if (act->curves.first==NULL) 
			fcu->flag |= FCURVE_ACTIVE;	/* first one added active */
			
		/* store path - make copy, and store that */
		fcu->rna_path= BLI_strdupn(rna_path, strlen(rna_path));
		fcu->array_index= array_index;
		
		/* if a group name has been provided, try to add or find a group, then add F-Curve to it */
		if (group) {
			/* try to find group */
			grp= action_groups_find_named(act, group);
			
			/* no matching groups, so add one */
			if (grp == NULL) {
				/* Add a new group, and make it active */
				grp= MEM_callocN(sizeof(bActionGroup), "bActionGroup");
				
				grp->flag |= (AGRP_ACTIVE|AGRP_SELECTED|AGRP_EXPANDED);
				BLI_snprintf(grp->name, 64, group);
				
				BLI_addtail(&act->groups, grp);
				BLI_uniquename(&act->groups, grp, "Group", offsetof(bActionGroup, name), 64);
				
				set_active_action_group(act, grp, 1);
			}
			
			/* add F-Curve to group */
			action_groups_add_channel(act, grp, fcu);
		}
		else {
			/* just add F-Curve to end of Action's list */
			BLI_addtail(&act->curves, fcu);
		}
	}
	
	/* return the F-Curve */
	return fcu;
}

/* ************************************************** */
/* KEYFRAME INSERTION */

/* -------------- BezTriple Insertion -------------------- */

/* threshold for inserting keyframes - threshold here should be good enough for now, but should become userpref */
#define BEZT_INSERT_THRESH 	0.00001f

/* Binary search algorithm for finding where to insert BezTriple. (for use by insert_bezt_icu)
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
static int binarysearch_bezt_index (BezTriple array[], float frame, int arraylen, short *replace)
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
		if (IS_EQT(frame, framenum, BEZT_INSERT_THRESH)) {
			*replace = 1;
			return 0;
		}
		else if (frame < framenum)
			return 0;
			
		/* 'Last' Keyframe */
		framenum= array[(arraylen-1)].vec[1][0];
		if (IS_EQT(frame, framenum, BEZT_INSERT_THRESH)) {
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
		int mid = (start + end) / 2;
		float midfra= array[mid].vec[1][0];
		
		/* check if exactly equal to midpoint */
		if (IS_EQT(frame, midfra, BEZT_INSERT_THRESH)) {
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

/* This function adds a given BezTriple to an F-Curve. It will allocate 
 * memory for the array if needed, and will insert the BezTriple into a
 * suitable place in chronological order.
 * 
 * NOTE: any recalculate of the F-Curve that needs to be done will need to 
 * 		be done by the caller.
 */
int insert_bezt_fcurve (FCurve *fcu, BezTriple *bezt)
{
	BezTriple *newb;
	int i= 0;
	
	if (fcu->bezt) {
		short replace = -1;
		i = binarysearch_bezt_index(fcu->bezt, bezt->vec[1][0], fcu->totvert, &replace);
		
		if (replace) {			
			/* sanity check: 'i' may in rare cases exceed arraylen */
			// FIXME: do not overwrite handletype if just replacing...?
			if ((i >= 0) && (i < fcu->totvert))
				*(fcu->bezt + i) = *bezt;
		}
		else {
			/* add new */
			newb= MEM_callocN((fcu->totvert+1)*sizeof(BezTriple), "beztriple");
			
			/* add the beztriples that should occur before the beztriple to be pasted (originally in ei->icu) */
			if (i > 0)
				memcpy(newb, fcu->bezt, i*sizeof(BezTriple));
			
			/* add beztriple to paste at index i */
			*(newb + i)= *bezt;
			
			/* add the beztriples that occur after the beztriple to be pasted (originally in icu) */
			if (i < fcu->totvert) 
				memcpy(newb+i+1, fcu->bezt+i, (fcu->totvert-i)*sizeof(BezTriple));
			
			/* replace (+ free) old with new */
			MEM_freeN(fcu->bezt);
			fcu->bezt= newb;
			
			fcu->totvert++;
		}
	}
	else {
		// TODO: need to check for old sample-data now...
		fcu->bezt= MEM_callocN(sizeof(BezTriple), "beztriple");
		*(fcu->bezt)= *bezt;
		fcu->totvert= 1;
	}
	
	
	/* we need to return the index, so that some tools which do post-processing can 
	 * detect where we added the BezTriple in the array
	 */
	return i;
}

/* This function is a wrapper for insert_bezt_icu, and should be used when
 * adding a new keyframe to a curve, when the keyframe doesn't exist anywhere
 * else yet. 
 * 
 * 'fast' - is only for the python API where importing BVH's would take an extreamly long time.
 */
void insert_vert_fcurve (FCurve *fcu, float x, float y, short fast)
{
	BezTriple beztr;
	int a;
	
	/* set all three points, for nicer start position */
	memset(&beztr, 0, sizeof(BezTriple));
	beztr.vec[0][0]= x; 
	beztr.vec[0][1]= y;
	beztr.vec[1][0]= x;
	beztr.vec[1][1]= y;
	beztr.vec[2][0]= x;
	beztr.vec[2][1]= y;
	beztr.ipo= U.ipo_new; /* use default interpolation mode here... */
	beztr.f1= beztr.f2= beztr.f3= SELECT;
	beztr.h1= beztr.h2= HD_AUTO; // XXX what about when we replace an old one?
	
	/* add temp beztriple to keyframes */
	a= insert_bezt_fcurve(fcu, &beztr);
	
	/* what if 'a' is a negative index? 
	 * for now, just exit to prevent any segfaults
	 */
	if (a < 0) return;
	
	/* don't recalculate handles if fast is set
	 *	- this is a hack to make importers faster
	 *	- we may calculate twice (see editipo_changed(), due to autohandle needing two calculations)
	 */
	if (!fast) calchandles_fcurve(fcu);
	
	/* set handletype and interpolation */
	if (fcu->totvert > 2) {
		BezTriple *bezt= (fcu->bezt + a);
		char h1, h2;
		
		/* set handles (autohandles by default) */
		h1= h2= HD_AUTO;
		
		if (a > 0) h1= (bezt-1)->h2;
		if (a < fcu->totvert-1) h2= (bezt+1)->h1;
		
		bezt->h1= h1;
		bezt->h2= h2;
		
		/* set interpolation from previous (if available) */
		if (a > 0) bezt->ipo= (bezt-1)->ipo;
		else if (a < fcu->totvert-1) bezt->ipo= (bezt+1)->ipo;
			
		/* don't recalculate handles if fast is set
		 *	- this is a hack to make importers faster
		 *	- we may calculate twice (see editipo_changed(), due to autohandle needing two calculations)
		 */
		if (!fast) calchandles_fcurve(fcu);
	}
}

/* -------------- 'Smarter' Keyframing Functions -------------------- */
/* return codes for new_key_needed */
enum {
	KEYNEEDED_DONTADD = 0,
	KEYNEEDED_JUSTADD,
	KEYNEEDED_DELPREV,
	KEYNEEDED_DELNEXT
} eKeyNeededStatus;

/* This helper function determines whether a new keyframe is needed */
/* Cases where keyframes should not be added:
 *	1. Keyframe to be added bewteen two keyframes with similar values
 *	2. Keyframe to be added on frame where two keyframes are already situated
 *	3. Keyframe lies at point that intersects the linear line between two keyframes
 */
static short new_key_needed (FCurve *fcu, float cFrame, float nValue) 
{
	BezTriple *bezt=NULL, *prev=NULL;
	int totCount, i;
	float valA = 0.0f, valB = 0.0f;
	
	/* safety checking */
	if (fcu == NULL) return KEYNEEDED_JUSTADD;
	totCount= fcu->totvert;
	if (totCount == 0) return KEYNEEDED_JUSTADD;
	
	/* loop through checking if any are the same */
	bezt= fcu->bezt;
	for (i=0; i<totCount; i++) {
		float prevPosi=0.0f, prevVal=0.0f;
		float beztPosi=0.0f, beztVal=0.0f;
			
		/* get current time+value */	
		beztPosi= bezt->vec[1][0];
		beztVal= bezt->vec[1][1];
			
		if (prev) {
			/* there is a keyframe before the one currently being examined */		
			
			/* get previous time+value */
			prevPosi= prev->vec[1][0];
			prevVal= prev->vec[1][1];
			
			/* keyframe to be added at point where there are already two similar points? */
			if (IS_EQ(prevPosi, cFrame) && IS_EQ(beztPosi, cFrame) && IS_EQ(beztPosi, prevPosi)) {
				return KEYNEEDED_DONTADD;
			}
			
			/* keyframe between prev+current points ? */
			if ((prevPosi <= cFrame) && (cFrame <= beztPosi)) {
				/* is the value of keyframe to be added the same as keyframes on either side ? */
				if (IS_EQ(prevVal, nValue) && IS_EQ(beztVal, nValue) && IS_EQ(prevVal, beztVal)) {
					return KEYNEEDED_DONTADD;
				}
				else {
					float realVal;
					
					/* get real value of curve at that point */
					realVal= evaluate_fcurve(fcu, cFrame);
					
					/* compare whether it's the same as proposed */
					if (IS_EQ(realVal, nValue)) 
						return KEYNEEDED_DONTADD;
					else 
						return KEYNEEDED_JUSTADD;
				}
			}
			
			/* new keyframe before prev beztriple? */
			if (cFrame < prevPosi) {
				/* A new keyframe will be added. However, whether the previous beztriple
				 * stays around or not depends on whether the values of previous/current
				 * beztriples and new keyframe are the same.
				 */
				if (IS_EQ(prevVal, nValue) && IS_EQ(beztVal, nValue) && IS_EQ(prevVal, beztVal))
					return KEYNEEDED_DELNEXT;
				else 
					return KEYNEEDED_JUSTADD;
			}
		}
		else {
			/* just add a keyframe if there's only one keyframe 
			 * and the new one occurs before the exisiting one does.
			 */
			if ((cFrame < beztPosi) && (totCount==1))
				return KEYNEEDED_JUSTADD;
		}
		
		/* continue. frame to do not yet passed (or other conditions not met) */
		if (i < (totCount-1)) {
			prev= bezt;
			bezt++;
		}
		else
			break;
	}
	
	/* Frame in which to add a new-keyframe occurs after all other keys
	 * -> If there are at least two existing keyframes, then if the values of the
	 *	 last two keyframes and the new-keyframe match, the last existing keyframe
	 *	 gets deleted as it is no longer required.
	 * -> Otherwise, a keyframe is just added. 1.0 is added so that fake-2nd-to-last
	 *	 keyframe is not equal to last keyframe.
	 */
	bezt= (fcu->bezt + (fcu->totvert - 1));
	valA= bezt->vec[1][1];
	
	if (prev)
		valB= prev->vec[1][1];
	else 
		valB= bezt->vec[1][1] + 1.0f; 
		
	if (IS_EQ(valA, nValue) && IS_EQ(valA, valB)) 
		return KEYNEEDED_DELPREV;
	else 
		return KEYNEEDED_JUSTADD;
}

/* ------------------ RNA Data-Access Functions ------------------ */

/* Try to read value using RNA-properties obtained already */
static float setting_get_rna_value (PointerRNA *ptr, PropertyRNA *prop, int index)
{
	float value= 0.0f;
	
	switch (RNA_property_type(ptr, prop)) {
		case PROP_BOOLEAN:
			if (RNA_property_array_length(ptr, prop))
				value= (float)RNA_property_boolean_get_index(ptr, prop, index);
			else
				value= (float)RNA_property_boolean_get(ptr, prop);
			break;
		case PROP_INT:
			if (RNA_property_array_length(ptr, prop))
				value= (float)RNA_property_int_get_index(ptr, prop, index);
			else
				value= (float)RNA_property_int_get(ptr, prop);
			break;
		case PROP_FLOAT:
			if (RNA_property_array_length(ptr, prop))
				value= RNA_property_float_get_index(ptr, prop, index);
			else
				value= RNA_property_float_get(ptr, prop);
			break;
		case PROP_ENUM:
			value= (float)RNA_property_enum_get(ptr, prop);
			break;
		default:
			break;
	}
	
	return value;
}

/* ------------------ 'Visual' Keyframing Functions ------------------ */

/* internal status codes for visualkey_can_use */
enum {
	VISUALKEY_NONE = 0,
	VISUALKEY_LOC,
	VISUALKEY_ROT,
};

/* This helper function determines if visual-keyframing should be used when  
 * inserting keyframes for the given channel. As visual-keyframing only works
 * on Object and Pose-Channel blocks, this should only get called for those 
 * blocktypes, when using "standard" keying but 'Visual Keying' option in Auto-Keying 
 * settings is on.
 */
static short visualkey_can_use (PointerRNA *ptr, PropertyRNA *prop)
{
	bConstraint *con= NULL;
	short searchtype= VISUALKEY_NONE;
	char *identifier= NULL;
	
	/* validate data */
	// TODO: this check is probably not needed, but it won't hurt
	if (ELEM3(NULL, ptr, ptr->data, prop))
		return 0;
		
	/* get first constraint and determine type of keyframe constraints to check for 
	 * 	- constraints can be on either Objects or PoseChannels, so we only check if the
	 *	  ptr->type is RNA_Object or RNA_PoseChannel, which are the RNA wrapping-info for
	 *  	  those structs, allowing us to identify the owner of the data 
	 */
	if (ptr->type == &RNA_Object) {
		/* Object */
		Object *ob= (Object *)ptr->data;
		
		con= ob->constraints.first;
		identifier= (char *)RNA_property_identifier(ptr, prop);
	}
	else if (ptr->type == &RNA_PoseChannel) {
		/* Pose Channel */
		bPoseChannel *pchan= (bPoseChannel *)ptr->data;
		
		con= pchan->constraints.first;
		identifier= (char *)RNA_property_identifier(ptr, prop);
	}
	
	/* check if any data to search using */
	if (ELEM(NULL, con, identifier))
		return 0;
		
	/* location or rotation identifiers only... */
	if (strstr(identifier, "location"))
		searchtype= VISUALKEY_LOC;
	else if (strstr(identifier, "rotation"))
		searchtype= VISUALKEY_ROT;
	else {
		printf("visualkey_can_use() failed: identifier - '%s' \n", identifier);
		return 0;
	}
	
	
	/* only search if a searchtype and initial constraint are available */
	if (searchtype && con) {
		for (; con; con= con->next) {
			/* only consider constraint if it is not disabled, and has influence */
			if (con->flag & CONSTRAINT_DISABLE) continue;
			if (con->enforce == 0.0f) continue;
			
			/* some constraints may alter these transforms */
			switch (con->type) {
				/* multi-transform constraints */
				case CONSTRAINT_TYPE_CHILDOF:
					return 1;
				case CONSTRAINT_TYPE_TRANSFORM:
					return 1;
				case CONSTRAINT_TYPE_FOLLOWPATH:
					return 1;
				case CONSTRAINT_TYPE_KINEMATIC:
					return 1;
					
				/* single-transform constraits  */
				case CONSTRAINT_TYPE_TRACKTO:
					if (searchtype==VISUALKEY_ROT) return 1;
					break;
				case CONSTRAINT_TYPE_ROTLIMIT:
					if (searchtype==VISUALKEY_ROT) return 1;
					break;
				case CONSTRAINT_TYPE_LOCLIMIT:
					if (searchtype==VISUALKEY_LOC) return 1;
					break;
				case CONSTRAINT_TYPE_ROTLIKE:
					if (searchtype==VISUALKEY_ROT) return 1;
					break;
				case CONSTRAINT_TYPE_DISTLIMIT:
					if (searchtype==VISUALKEY_LOC) return 1;
					break;
				case CONSTRAINT_TYPE_LOCLIKE:
					if (searchtype==VISUALKEY_LOC) return 1;
					break;
				case CONSTRAINT_TYPE_LOCKTRACK:
					if (searchtype==VISUALKEY_ROT) return 1;
					break;
				case CONSTRAINT_TYPE_MINMAX:
					if (searchtype==VISUALKEY_LOC) return 1;
					break;
				
				default:
					break;
			}
		}
	}
	
	/* when some condition is met, this function returns, so here it can be 0 */
	return 0;
}

/* This helper function extracts the value to use for visual-keyframing 
 * In the event that it is not possible to perform visual keying, try to fall-back
 * to using the default method. Assumes that all data it has been passed is valid.
 */
static float visualkey_get_value (PointerRNA *ptr, PropertyRNA *prop, int array_index)
{
	char *identifier= (char *)RNA_property_identifier(ptr, prop);
	
	/* handle for Objects or PoseChannels only 
	 * 	- constraints can be on either Objects or PoseChannels, so we only check if the
	 *	  ptr->type is RNA_Object or RNA_PoseChannel, which are the RNA wrapping-info for
	 *  	  those structs, allowing us to identify the owner of the data 
	 *	- assume that array_index will be sane
	 */
	if (ptr->type == &RNA_Object) {
		Object *ob= (Object *)ptr->data;
		
		/* parented objects are not supported, as the effects of the parent
		 * are included in the matrix, which kindof beats the point
		 */
		if (ob->parent == NULL) {
			/* only Location or Rotation keyframes are supported now */
			if (strstr(identifier, "location")) {
				return ob->obmat[3][array_index];
			}
			else if (strstr(identifier, "rotation")) {
				float eul[3];
				
				Mat4ToEul(ob->obmat, eul);
				return eul[array_index]*(5.72958f);
			}
		}
	}
	else if (ptr->type == &RNA_PoseChannel) {
		Object *ob= (Object *)ptr->id.data; /* we assume that this is always set, and is an object */
		bPoseChannel *pchan= (bPoseChannel *)ptr->data;
		float tmat[4][4];
		
		/* Although it is not strictly required for this particular space conversion, 
		 * arg1 must not be null, as there is a null check for the other conversions to
		 * be safe. Therefore, the active object is passed here, and in many cases, this
		 * will be what owns the pose-channel that is getting this anyway.
		 */
		Mat4CpyMat4(tmat, pchan->pose_mat);
		constraint_mat_convertspace(ob, pchan, tmat, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_LOCAL);
		
		/* Loc, Rot/Quat keyframes are supported... */
		if (strstr(identifier, "location")) {
			/* only use for non-connected bones */
			if ((pchan->bone->parent) && !(pchan->bone->flag & BONE_CONNECTED))
				return tmat[3][array_index];
			else if (pchan->bone->parent == NULL)
				return tmat[3][array_index];
		}
		else if (strstr(identifier, "euler_rotation")) {
			float eul[3];
			
			/* euler-rotation test before standard rotation, as standard rotation does quats */
			Mat4ToEul(tmat, eul);
			return eul[array_index];
		}
		else if (strstr(identifier, "rotation")) {
			float trimat[3][3], quat[4];
			
			Mat3CpyMat4(trimat, tmat);
			Mat3ToQuat_is_ok(trimat, quat);
			
			return quat[array_index];
		}
	}
	
	/* as the function hasn't returned yet, read value from system in the default way */
	return setting_get_rna_value(ptr, prop, array_index);
}

/* ------------------------- Insert Key API ------------------------- */

/* Main Keyframing API call:
 *	Use this when validation of necessary animation data isn't necessary as it
 *	already exists. It will insert a keyframe using the current value being keyframed.
 *	
 *	The flag argument is used for special settings that alter the behaviour of
 *	the keyframe insertion. These include the 'visual' keyframing modes, quick refresh,
 *	and extra keyframe filtering.
 */
short insertkey (ID *id, const char group[], const char rna_path[], int array_index, float cfra, short flag)
{	
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	FCurve *fcu;
	
	/* validate pointer first - exit if failure*/
	RNA_id_pointer_create(id, &id_ptr);
	if (RNA_path_resolve(&id_ptr, rna_path, &ptr, &prop) == 0 || prop == NULL) {
		printf("Insert Key: Could not insert keyframe, as RNA Path is invalid for the given ID (%s)\n", rna_path);
		return 0;
	}
	
	/* get F-Curve */
	fcu= verify_fcurve(id, group, rna_path, array_index, 1);
	
	/* only continue if we have an F-Curve to add keyframe to */
	if (fcu) {
		float curval= 0.0f;
		
		/* set additional flags for the F-Curve (i.e. only integer values) */
		if (RNA_property_type(&ptr, prop) != PROP_FLOAT)
			fcu->flag |= FCURVE_INT_VALUES;
		
		/* apply special time tweaking */
			// XXX check on this stuff...
		if (GS(id->name) == ID_OB) {
			//Object *ob= (Object *)id;
			
			/* apply NLA-scaling (if applicable) */
			//cfra= get_action_frame(ob, cfra);
			
			/* ancient time-offset cruft */
			//if ( (ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)) ) {
			//	/* actually frametofloat calc again! */
			//	cfra-= give_timeoffset(ob)*scene->r.framelen;
			//}
		}
		
		/* obtain value to give keyframe */
		if ( (flag & INSERTKEY_MATRIX) && 
			 (visualkey_can_use(&ptr, prop)) ) 
		{
			/* visual-keying is only available for object and pchan datablocks, as 
			 * it works by keyframing using a value extracted from the final matrix 
			 * instead of using the kt system to extract a value.
			 */
			curval= visualkey_get_value(&ptr, prop, array_index);
		}
		else {
			/* read value from system */
			curval= setting_get_rna_value(&ptr, prop, array_index);
		}
		
		/* only insert keyframes where they are needed */
		if (flag & INSERTKEY_NEEDED) {
			short insert_mode;
			
			/* check whether this curve really needs a new keyframe */
			insert_mode= new_key_needed(fcu, cfra, curval);
			
			/* insert new keyframe at current frame */
			if (insert_mode)
				insert_vert_fcurve(fcu, cfra, curval, (flag & INSERTKEY_FAST));
			
			/* delete keyframe immediately before/after newly added */
			switch (insert_mode) {
				case KEYNEEDED_DELPREV:
					delete_fcurve_key(fcu, fcu->totvert-2, 1);
					break;
				case KEYNEEDED_DELNEXT:
					delete_fcurve_key(fcu, 1, 1);
					break;
			}
			
			/* only return success if keyframe added */
			if (insert_mode)
				return 1;
		}
		else {
			/* just insert keyframe */
			insert_vert_fcurve(fcu, cfra, curval, (flag & INSERTKEY_FAST));
			
			/* return success */
			return 1;
		}
	}
	
	/* return failure */
	return 0;
}

/* ************************************************** */
/* KEYFRAME DELETION */

/* Main Keyframing API call:
 *	Use this when validation of necessary animation data isn't necessary as it
 *	already exists. It will delete a keyframe at the current frame.
 *	
 *	The flag argument is used for special settings that alter the behaviour of
 *	the keyframe deletion. These include the quick refresh options.
 */
short deletekey (ID *id, const char group[], const char rna_path[], int array_index, float cfra, short flag)
{
	AnimData *adt;
	FCurve *fcu;
	
	/* get F-Curve
	 * Note: here is one of the places where we don't want new Action + F-Curve added!
	 * 		so 'add' var must be 0
	 */
	/* we don't check the validity of the path here yet, but it should be ok... */
	fcu= verify_fcurve(id, group, rna_path, array_index, 0);
	adt= BKE_animdata_from_id(id);
	
	/* only continue if we have an ipo-curve to remove keyframes from */
	if (adt && adt->action && fcu) {
		bAction *act= adt->action;
		short found = -1;
		int i;
		
		/* apply special time tweaking */
		if (GS(id->name) == ID_OB) {
			//Object *ob= (Object *)id;
			
			/* apply NLA-scaling (if applicable) */
			//	cfra= get_action_frame(ob, cfra);
			
			/* ancient time-offset cruft */
			//if ( (ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)) ) {
			//	/* actually frametofloat calc again! */
			//	cfra-= give_timeoffset(ob)*scene->r.framelen;
			//}
		}
		
		/* try to find index of beztriple to get rid of */
		i = binarysearch_bezt_index(fcu->bezt, cfra, fcu->totvert, &found);
		if (found) {			
			/* delete the key at the index (will sanity check + do recalc afterwards) */
			delete_fcurve_key(fcu, i, 1);
			
			/* Only delete curve too if there are no points (we don't need to check for drivers, as they're kept separate) */
			// XXX how do we handle drivers then?
			if (fcu->totvert == 0) {
				BLI_remlink(&act->curves, fcu);
				free_fcurve(fcu);
			}
			
			/* return success */
			return 1;
		}
	}
	
	/* return failure */
	return 0;
}

/* ******************************************* */
/* KEYFRAME MODIFICATION */

/* mode for common_modifykey */
enum {
	COMMONKEY_MODE_INSERT = 0,
	COMMONKEY_MODE_DELETE,
} eCommonModifyKey_Modes;


/* Build menu-string of available keying-sets (allocates memory for string)
 * NOTE: mode must not be longer than 64 chars
 */
char *ANIM_build_keyingsets_menu (ListBase *list, short for_edit)
{
	DynStr *pupds= BLI_dynstr_new();
	KeyingSet *ks;
	char buf[64];
	char *str;
	int i;
	
	/* add title first */
	BLI_dynstr_append(pupds, "Keying Sets%t|");
	
	/* add dummy entries for none-active */
	if (for_edit) { 
		BLI_dynstr_append(pupds, "Add New%x-1|");
		BLI_dynstr_append(pupds, " %x0|");
	}
	else
		BLI_dynstr_append(pupds, "<No Keying Set Active>%x0|");
	
	/* loop through keyingsets, adding them */
	for (ks=list->first, i=1; ks; ks=ks->next, i++) {
		if (for_edit == 0)
			BLI_dynstr_append(pupds, "KS: ");
		
		BLI_dynstr_append(pupds, ks->name);
		BLI_snprintf( buf, 64, "%%x%d%s", i, ((ks->next)?"|":"") );
		BLI_dynstr_append(pupds, buf);
	}
	
	/* convert to normal MEM_malloc'd string */
	str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
	
	return str;
}

#if 0 // XXX old keyingsets code based on adrcodes... to be restored in due course

/* --------- KeyingSet Adrcode Getters ------------ */

/* initialise a channel-getter storage */
static void ks_adrcodegetter_init (bKS_AdrcodeGetter *kag, bKeyingSet *ks, bCommonKeySrc *cks)
{
	/* error checking */
	if (kag == NULL)
		return;
	
	if (ELEM(NULL, ks, cks)) {
		/* set invalid settings that won't cause harm */
		kag->ks= NULL;
		kag->cks= NULL;
		kag->index= -2;
		kag->tot= 0;
	}
	else {
		/* store settings */
		kag->ks= ks;
		kag->cks= cks;
		
		/* - index is -1, as that allows iterators to return first element
		 * - tot is chan_num by default, but may get overriden if -1 is encountered (for extension-type getters)
		 */
		kag->index= -1;
		kag->tot= ks->chan_num;
	}
}

/* 'default' channel-getter that will be used when iterating through keyingset's channels 
 *	 - iteration will stop when adrcode <= 0 is encountered, so we use that as escape
 */
static short ks_getnextadrcode_default (bKS_AdrcodeGetter *kag)
{	
	bKeyingSet *ks= (kag)? kag->ks : NULL;
	
	/* error checking */
	if (ELEM(NULL, kag, ks)) return 0;
	if (kag->tot <= 0) return 0;
	
	kag->index++;
	if ((kag->index < 0) || (kag->index >= kag->tot)) return 0;
	
	/* return the adrcode stored at index then */
	return ks->adrcodes[kag->index];
}

/* add map flag (for MTex channels, as certain ones need special offset) */
static short ks_getnextadrcode_addmap (bKS_AdrcodeGetter *kag)
{
	short adrcode= ks_getnextadrcode_default(kag);
	
	/* if there was an adrcode returned, assume that kag stuff is set ok */
	if (adrcode) {
		bCommonKeySrc *cks= kag->cks;
		bKeyingSet *ks= kag->ks;
		
		if (ELEM3(ks->blocktype, ID_MA, ID_LA, ID_WO)) {
			switch (adrcode) {
				case MAP_OFS_X: case MAP_OFS_Y: case MAP_OFS_Z:
				case MAP_SIZE_X: case MAP_SIZE_Y: case MAP_SIZE_Z:
				case MAP_R: case MAP_G: case MAP_B: case MAP_DVAR:
				case MAP_COLF: case MAP_NORF: case MAP_VARF: case MAP_DISP:
					adrcode += cks->map;
					break;
			}
		}
	}
	
	/* adrcode must be returned! */
	return adrcode;
}

/* extend posechannel keyingsets with rotation info (when KAG_CHAN_EXTEND is encountered) 
 *	- iteration will stop when adrcode <= 0 is encountered, so we use that as escape
 *	- when we encounter KAG_CHAN_EXTEND as adrcode, start returning our own
 */
static short ks_getnextadrcode_pchanrot (bKS_AdrcodeGetter *kag)
{	
	/* hardcoded adrcode channels used here only 
	 *	- length is keyed-channels + 1 (last item must be 0 to escape)
	 */
	static short quat_adrcodes[5] = {AC_QUAT_W, AC_QUAT_X, AC_QUAT_Y, AC_QUAT_Z, 0};
	static short eul_adrcodes[4] = {AC_EUL_X, AC_EUL_Y, AC_EUL_Z, 0};
		
	/* useful variables */
	bKeyingSet *ks= (kag)? kag->ks : NULL;
	bCommonKeySrc *cks= (kag) ? kag->cks : NULL;
	short index, adrcode;
	
	/* error checking */
	if (ELEM3(NULL, kag, ks, cks)) return 0;
	if (ks->chan_num <= 0) return 0;
	
	/* get index 
	 *	- if past the last item (kag->tot), return stuff from our static arrays
	 *	- otherwise, just keep returning stuff from the keyingset (but check out for -1!) 
	 */
	kag->index++;
	if (kag->index < 0)
		return 0;
	
	/* normal (static stuff) */
	if (kag->index < kag->tot) {
		/* get adrcode, and return if not KAG_CHAN_EXTEND (i.e. point for start of iteration) */
		adrcode= ks->adrcodes[kag->index];
		
		if (adrcode != KAG_CHAN_EXTEND) 
			return adrcode;
		else	
			kag->tot= kag->index;
	}
		
	/* based on current rotation-mode
	 *	- index can be at most 5, if we are to prevent segfaults
	 */
	index= kag->index - kag->tot;
	if ((index < 0) || (index > 5))
		return 0;
	
	if (cks->pchan && cks->pchan->rotmode)
		return eul_adrcodes[index];
	else
		return quat_adrcodes[index];
}

/* ------------- KeyingSet Defines ------------ */
/* Note: these must all be named with the defks_* prefix, otherwise the template macro will not work! */

/* macro for defining keyingset contexts */
#define KSC_TEMPLATE(ctx_name) {&defks_##ctx_name[0], NULL, sizeof(defks_##ctx_name)/sizeof(bKeyingSet)}

/* --- */

/* check if option not available for deleting keys */
static short incl_non_del_keys (bKeyingSet *ks, const char mode[])
{
	/* as optimisation, assume that it is sufficient to check only first letter
	 * of mode (int comparison should be faster than string!)
	 */
	//if (strcmp(mode, "Delete")==0)
	if (mode && mode[0]=='D')
		return 0;
	
	return 1;
}

/* Object KeyingSets  ------ */

/* check if include shapekey entry  */
static short incl_v3d_ob_shapekey (bKeyingSet *ks, const char mode[])
{
	//Object *ob= (G.obedit)? (G.obedit) : (OBACT); // XXX
	Object *ob= NULL;
	char *newname= NULL;
	
	if(ob==NULL)
		return 0;
	
	/* not available for delete mode */
	if (strcmp(mode, "Delete")==0)
		return 0;
	
	/* check if is geom object that can get shapekeys */
	switch (ob->type) {
		/* geometry? */
		case OB_MESH:		newname= "Mesh";		break;
		case OB_CURVE:		newname= "Curve";		break;
		case OB_SURF:		newname= "Surface";		break;
		case OB_LATTICE: 	newname= "Lattice";		break;
		
		/* not geometry! */
		default:
			return 0;
	}
	
	/* if ks is shapekey entry (this could be callled for separator before too!) */
	if (ks->flag == -3)
		BLI_strncpy(ks->name, newname, sizeof(ks->name));
	
	/* if it gets here, it's ok */
	return 1;
}

/* array for object keyingset defines */
bKeyingSet defks_v3d_object[] = 
{
	/* include_cb, adrcode-getter, name, blocktype, flag, chan_num, adrcodes */
	{NULL, "Loc", ID_OB, 0, 3, {OB_LOC_X,OB_LOC_Y,OB_LOC_Z}},
	{NULL, "Rot", ID_OB, 0, 3, {OB_ROT_X,OB_ROT_Y,OB_ROT_Z}},
	{NULL, "Scale", ID_OB, 0, 3, {OB_SIZE_X,OB_SIZE_Y,OB_SIZE_Z}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "LocRot", ID_OB, 0, 6, 
		{OB_LOC_X,OB_LOC_Y,OB_LOC_Z,
		 OB_ROT_X,OB_ROT_Y,OB_ROT_Z}},
		 
	{NULL, "LocScale", ID_OB, 0, 6, 
		{OB_LOC_X,OB_LOC_Y,OB_LOC_Z,
		 OB_SIZE_X,OB_SIZE_Y,OB_SIZE_Z}},
		 
	{NULL, "LocRotScale", ID_OB, 0, 9, 
		{OB_LOC_X,OB_LOC_Y,OB_LOC_Z,
		 OB_ROT_X,OB_ROT_Y,OB_ROT_Z,
		 OB_SIZE_X,OB_SIZE_Y,OB_SIZE_Z}},
		 
	{NULL, "RotScale", ID_OB, 0, 6, 
		{OB_ROT_X,OB_ROT_Y,OB_ROT_Z,
		 OB_SIZE_X,OB_SIZE_Y,OB_SIZE_Z}},
	
	{incl_non_del_keys, "%l", 0, -1, 0, {0}}, // separator
	
	{incl_non_del_keys, "VisualLoc", ID_OB, INSERTKEY_MATRIX, 3, {OB_LOC_X,OB_LOC_Y,OB_LOC_Z}},
	{incl_non_del_keys, "VisualRot", ID_OB, INSERTKEY_MATRIX, 3, {OB_ROT_X,OB_ROT_Y,OB_ROT_Z}},
	
	{incl_non_del_keys, "VisualLocRot", ID_OB, INSERTKEY_MATRIX, 6, 
		{OB_LOC_X,OB_LOC_Y,OB_LOC_Z,
		 OB_ROT_X,OB_ROT_Y,OB_ROT_Z}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Layer", ID_OB, 0, 1, {OB_LAY}}, // icky option...
	{NULL, "Available", ID_OB, -2, 0, {0}},
	
	{incl_v3d_ob_shapekey, "%l%l", 0, -1, 0, {0}}, // separator (linked to shapekey entry)
	{incl_v3d_ob_shapekey, "<ShapeKey>", ID_OB, -3, 0, {0}}
};

/* PoseChannel KeyingSets  ------ */

/* array for posechannel keyingset defines */
bKeyingSet defks_v3d_pchan[] = 
{
	/* include_cb, name, blocktype, flag, chan_num, adrcodes */
	{NULL, "Loc", ID_PO, 0, 3, {AC_LOC_X,AC_LOC_Y,AC_LOC_Z}},
	{NULL, "Rot", ID_PO, COMMONKEY_PCHANROT, 1, {KAG_CHAN_EXTEND}},
	{NULL, "Scale", ID_PO, 0, 3, {AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "LocRot", ID_PO, COMMONKEY_PCHANROT, 4, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z,
		 KAG_CHAN_EXTEND}},
		 
	{NULL, "LocScale", ID_PO, 0, 6, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z,
		 AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z}},
		 
	{NULL, "LocRotScale", ID_PO, COMMONKEY_PCHANROT, 7, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z,AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z, 
		 KAG_CHAN_EXTEND}},
		 
	{NULL, "RotScale", ID_PO, 0, 4, 
		{AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z, 
		 KAG_CHAN_EXTEND}},
	
	{incl_non_del_keys, "%l", 0, -1, 0, {0}}, // separator
	
	{incl_non_del_keys, "VisualLoc", ID_PO, INSERTKEY_MATRIX, 3, {AC_LOC_X,AC_LOC_Y,AC_LOC_Z}},
	{incl_non_del_keys, "VisualRot", ID_PO, INSERTKEY_MATRIX|COMMONKEY_PCHANROT, 1, {KAG_CHAN_EXTEND}},
	
	{incl_non_del_keys, "VisualLocRot", ID_PO, INSERTKEY_MATRIX|COMMONKEY_PCHANROT, 4, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z, KAG_CHAN_EXTEND}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Available", ID_PO, -2, 0, {0}}
};

/* Material KeyingSets  ------ */

/* array for material keyingset defines */
bKeyingSet defks_buts_shading_mat[] = 
{
	/* include_cb, name, blocktype, flag, chan_num, adrcodes */
	{NULL, "RGB", ID_MA, 0, 3, {MA_COL_R,MA_COL_G,MA_COL_B}},
	{NULL, "Alpha", ID_MA, 0, 1, {MA_ALPHA}},
	{NULL, "Halo Size", ID_MA, 0, 1, {MA_HASIZE}},
	{NULL, "Mode", ID_MA, 0, 1, {MA_MODE}}, // evil bitflags
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "All Color", ID_MA, 0, 18, 
		{MA_COL_R,MA_COL_G,MA_COL_B,
		 MA_ALPHA,MA_HASIZE, MA_MODE,
		 MA_SPEC_R,MA_SPEC_G,MA_SPEC_B,
		 MA_REF,MA_EMIT,MA_AMB,MA_SPEC,MA_HARD,
		 MA_MODE,MA_TRANSLU,MA_ADD}},
		 
	{NULL, "All Mirror", ID_MA, 0, 5, 
		{MA_RAYM,MA_FRESMIR,MA_FRESMIRI,
		 MA_FRESTRA,MA_FRESTRAI}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Ofs", ID_MA, COMMONKEY_ADDMAP, 3, {MAP_OFS_X,MAP_OFS_Y,MAP_OFS_Z}},
	{NULL, "Size", ID_MA, COMMONKEY_ADDMAP, 3, {MAP_SIZE_X,MAP_SIZE_Y,MAP_SIZE_Z}},
	
	{NULL, "All Mapping", ID_MA, COMMONKEY_ADDMAP, 14, 
		{MAP_OFS_X,MAP_OFS_Y,MAP_OFS_Z,
		 MAP_SIZE_X,MAP_SIZE_Y,MAP_SIZE_Z,
		 MAP_R,MAP_G,MAP_B,MAP_DVAR,
		 MAP_COLF,MAP_NORF,MAP_VARF,MAP_DISP}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Available", ID_MA, -2, 0, {0}}
};

/* World KeyingSets  ------ */

/* array for world keyingset defines */
bKeyingSet defks_buts_shading_wo[] = 
{
	/* include_cb, name, blocktype, flag, chan_num, adrcodes */
	{NULL, "Zenith RGB", ID_WO, 0, 3, {WO_ZEN_R,WO_ZEN_G,WO_ZEN_B}},
	{NULL, "Horizon RGB", ID_WO, 0, 3, {WO_HOR_R,WO_HOR_G,WO_HOR_B}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Mist", ID_WO, 0, 4, {WO_MISI,WO_MISTDI,WO_MISTSTA,WO_MISTHI}},
	{NULL, "Stars", ID_WO, 0, 5, {WO_STAR_R,WO_STAR_G,WO_STAR_B,WO_STARDIST,WO_STARSIZE}},
	
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Ofs", ID_WO, COMMONKEY_ADDMAP, 3, {MAP_OFS_X,MAP_OFS_Y,MAP_OFS_Z}},
	{NULL, "Size", ID_WO, COMMONKEY_ADDMAP, 3, {MAP_SIZE_X,MAP_SIZE_Y,MAP_SIZE_Z}},
	
	{NULL, "All Mapping", ID_WO, COMMONKEY_ADDMAP, 14, 
		{MAP_OFS_X,MAP_OFS_Y,MAP_OFS_Z,
		 MAP_SIZE_X,MAP_SIZE_Y,MAP_SIZE_Z,
		 MAP_R,MAP_G,MAP_B,MAP_DVAR,
		 MAP_COLF,MAP_NORF,MAP_VARF,MAP_DISP}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Available", ID_WO, -2, 0, {0}}
};

/* Lamp KeyingSets  ------ */

/* array for lamp keyingset defines */
bKeyingSet defks_buts_shading_la[] = 
{
	/* include_cb, name, blocktype, flag, chan_num, adrcodes */
	{NULL, "RGB", ID_LA, 0, 3, {LA_COL_R,LA_COL_G,LA_COL_B}},
	{NULL, "Energy", ID_LA, 0, 1, {LA_ENERGY}},
	{NULL, "Spot Size", ID_LA, 0, 1, {LA_SPOTSI}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Ofs", ID_LA, COMMONKEY_ADDMAP, 3, {MAP_OFS_X,MAP_OFS_Y,MAP_OFS_Z}},
	{NULL, "Size", ID_LA, COMMONKEY_ADDMAP, 3, {MAP_SIZE_X,MAP_SIZE_Y,MAP_SIZE_Z}},
	
	{NULL, "All Mapping", ID_LA, COMMONKEY_ADDMAP, 14, 
		{MAP_OFS_X,MAP_OFS_Y,MAP_OFS_Z,
		 MAP_SIZE_X,MAP_SIZE_Y,MAP_SIZE_Z,
		 MAP_R,MAP_G,MAP_B,MAP_DVAR,
		 MAP_COLF,MAP_NORF,MAP_VARF,MAP_DISP}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Available", ID_LA, -2, 0, {0}}
};

/* Texture KeyingSets  ------ */

/* array for texture keyingset defines */
bKeyingSet defks_buts_shading_tex[] = 
{
	/* include_cb, name, blocktype, flag, chan_num, adrcodes */
	{NULL, "Clouds", ID_TE, 0, 5, 
		{TE_NSIZE,TE_NDEPTH,TE_NTYPE,
		 TE_MG_TYP,TE_N_BAS1}},
	
	{NULL, "Marble", ID_TE, 0, 7, 
		{TE_NSIZE,TE_NDEPTH,TE_NTYPE,
		 TE_TURB,TE_MG_TYP,TE_N_BAS1,TE_N_BAS2}},
		 
	{NULL, "Stucci", ID_TE, 0, 5, 
		{TE_NSIZE,TE_NTYPE,TE_TURB,
		 TE_MG_TYP,TE_N_BAS1}},
		 
	{NULL, "Wood", ID_TE, 0, 6, 
		{TE_NSIZE,TE_NTYPE,TE_TURB,
		 TE_MG_TYP,TE_N_BAS1,TE_N_BAS2}},
		 
	{NULL, "Magic", ID_TE, 0, 2, {TE_NDEPTH,TE_TURB}},
	
	{NULL, "Blend", ID_TE, 0, 1, {TE_MG_TYP}},	
		
	{NULL, "Musgrave", ID_TE, 0, 6, 
		{TE_MG_TYP,TE_MGH,TE_MG_LAC,
		 TE_MG_OCT,TE_MG_OFF,TE_MG_GAIN}},
		 
	{NULL, "Voronoi", ID_TE, 0, 9, 
		{TE_VNW1,TE_VNW2,TE_VNW3,TE_VNW4,
		TE_VNMEXP,TE_VN_DISTM,TE_VN_COLT,
		TE_ISCA,TE_NSIZE}},
		
	{NULL, "Distorted Noise", ID_TE, 0, 4, 
		{TE_MG_OCT,TE_MG_OFF,TE_MG_GAIN,TE_DISTA}},
	
	{NULL, "Color Filter", ID_TE, 0, 5, 
		{TE_COL_R,TE_COL_G,TE_COL_B,TE_BRIGHT,TE_CONTRA}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Available", ID_TE, -2, 0, {0}}
};

/* Object Buttons KeyingSets  ------ */

/* check if include particles entry  */
static short incl_buts_ob (bKeyingSet *ks, const char mode[])
{
	//Object *ob= OBACT; // xxx
	Object *ob= NULL;
	/* only if object is mesh type */
	
	if(ob==NULL) return 0;
	return (ob->type == OB_MESH);
}

/* array for texture keyingset defines */
bKeyingSet defks_buts_object[] = 
{
	/* include_cb, name, blocktype, flag, chan_num, adrcodes */
	{incl_buts_ob, "Surface Damping", ID_OB, 0, 1, {OB_PD_SDAMP}},
	{incl_buts_ob, "Random Damping", ID_OB, 0, 1, {OB_PD_RDAMP}},
	{incl_buts_ob, "Permeability", ID_OB, 0, 1, {OB_PD_PERM}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Force Strength", ID_OB, 0, 1, {OB_PD_FSTR}},
	{NULL, "Force Falloff", ID_OB, 0, 1, {OB_PD_FFALL}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Available", ID_OB, -2, 0, {0}}  // this will include ob-transforms too!
};

/* Camera Buttons KeyingSets  ------ */

/* check if include internal-renderer entry  */
static short incl_buts_cam1 (bKeyingSet *ks, const char mode[])
{
	Scene *scene= NULL; // FIXME this will cause a crash, but we need an extra arg first!
	/* only if renderer is internal renderer */
	return (scene->r.renderer==R_INTERN);
}

/* check if include external-renderer entry  */
static short incl_buts_cam2 (bKeyingSet *ks, const char mode[])
{
	Scene *scene= NULL; // FIXME this will cause a crash, but we need an extra arg first!
	/* only if renderer is internal renderer */
	return (scene->r.renderer!=R_INTERN);
}

/* array for camera keyingset defines */
bKeyingSet defks_buts_cam[] = 
{
	/* include_cb, name, blocktype, flag, chan_num, adrcodes */
	{NULL, "Lens", ID_CA, 0, 1, {CAM_LENS}},
	{NULL, "Clipping", ID_CA, 0, 2, {CAM_STA,CAM_END}},
	{NULL, "Focal Distance", ID_CA, 0, 1, {CAM_YF_FDIST}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	
	{incl_buts_cam2, "Aperture", ID_CA, 0, 1, {CAM_YF_APERT}},
	{incl_buts_cam1, "Viewplane Shift", ID_CA, 0, 2, {CAM_SHIFT_X,CAM_SHIFT_Y}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "Available", ID_CA, -2, 0, {0}}
};

/* --- */

/* Keying Context Defines - Must keep in sync with enumeration (eKS_Contexts) */
bKeyingContext ks_contexts[] = 
{
	KSC_TEMPLATE(v3d_object),
	KSC_TEMPLATE(v3d_pchan),
	
	KSC_TEMPLATE(buts_shading_mat),
	KSC_TEMPLATE(buts_shading_wo),
	KSC_TEMPLATE(buts_shading_la),
	KSC_TEMPLATE(buts_shading_tex),

	KSC_TEMPLATE(buts_object),
	KSC_TEMPLATE(buts_cam)
};

/* Keying Context Enumeration - Must keep in sync with definitions*/
typedef enum eKS_Contexts {
	KSC_V3D_OBJECT = 0,
	KSC_V3D_PCHAN,
	
	KSC_BUTS_MAT,
	KSC_BUTS_WO,
	KSC_BUTS_LA,
	KSC_BUTS_TEX,
	
	KSC_BUTS_OB,
	KSC_BUTS_CAM,
	
		/* make sure this last one remains untouched! */
	KSC_TOT_TYPES
} eKS_Contexts;


/* ---------------- KeyingSet Tools ------------------- */

/* helper for commonkey_context_get() -  get keyingsets for 3d-view */
static void commonkey_context_getv3d (const bContext *C, ListBase *sources, bKeyingContext **ksc)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob;
	IpoCurve *icu;
	
	if ((OBACT) && (OBACT->flag & OB_POSEMODE)) {
		bPoseChannel *pchan;
		
		/* pose-level */
		ob= OBACT;
		*ksc= &ks_contexts[KSC_V3D_PCHAN];
			// XXX
		//set_pose_keys(ob);  /* sets pchan->flag to POSE_KEY if bone selected, and clears if not */
		
		/* loop through posechannels */
		for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next) {
			if (pchan->flag & POSE_KEY) {
				bCommonKeySrc *cks;
				
				/* add new keyframing destination */
				cks= MEM_callocN(sizeof(bCommonKeySrc), "bCommonKeySrc");
				BLI_addtail(sources, cks);
				
				/* set id-block to key to, and action */
				cks->id= (ID *)ob;
				cks->act= ob->action;
				
				/* set pchan */
				cks->pchan= pchan;
				cks->actname= pchan->name;
			}
		}
	}
	else {
		/* object-level */
		*ksc= &ks_contexts[KSC_V3D_OBJECT];
		
		/* loop through bases */
		// XXX but we're only supposed to do this on editable ones, not just selected ones!
		CTX_DATA_BEGIN(C, Base*, base, selected_bases) {
			bCommonKeySrc *cks;
			
			/* add new keyframing destination */
			cks= MEM_callocN(sizeof(bCommonKeySrc), "bCommonKeySrc");
			BLI_addtail(sources, cks);
			
			/* set id-block to key to */
			ob= base->object;
			cks->id= (ID *)ob;
			
			/* when ob's keyframes are in an action, default to using 'Object' as achan name */
			if (ob->ipoflag & OB_ACTION_OB)
				cks->actname= "Object";
			
			/* set ipo-flags */
			// TODO: add checks for lib-linked data
			if ((ob->ipo) || (ob->action)) {
				if (ob->ipo) {
					cks->ipo= ob->ipo;
				}
				else {
					bActionChannel *achan;
					
					cks->act= ob->action;
					achan= get_action_channel(ob->action, cks->actname);
					
					if (achan && achan->ipo)
						cks->ipo= achan->ipo;
				}
				/* cks->ipo can be NULL while editing */
				if(cks->ipo) {
					/* deselect all ipo-curves */
					for (icu= cks->ipo->curve.first; icu; icu= icu->next) {
						icu->flag &= ~IPO_SELECT;
					}
				}
			}
		}
		CTX_DATA_END;
	}
}

/* helper for commonkey_context_get() -  get keyingsets for buttons window */
static void commonkey_context_getsbuts (const bContext *C, ListBase *sources, bKeyingContext **ksc)
{
#if 0 // XXX dunno what's the future of this stuff...	
	bCommonKeySrc *cks;
	
	/* check on tab-type */
	switch (G.buts->mainb) {
	case CONTEXT_SHADING:	/* ------------- Shading buttons ---------------- */
		/* subtabs include "Material", "Texture", "Lamp", "World"*/
		switch (G.buts->tab[CONTEXT_SHADING]) {
			case TAB_SHADING_MAT: /* >------------- Material Tab -------------< */
			{
				Material *ma= editnode_get_active_material(G.buts->lockpoin);
				
				if (ma) {
					/* add new keyframing destination */
					cks= MEM_callocN(sizeof(bCommonKeySrc), "bCommonKeySrc");
					BLI_addtail(sources, cks); 
					
					/* set data */
					cks->id= (ID *)ma;
					cks->ipo= ma->ipo;
					cks->map= texchannel_to_adrcode(ma->texact);
					
					/* set keyingsets */
					*ksc= &ks_contexts[KSC_BUTS_MAT];
					return;
				}
			}
				break;
			case TAB_SHADING_WORLD: /* >------------- World Tab -------------< */
			{
				World *wo= G.buts->lockpoin;
				
				if (wo) {
					/* add new keyframing destination */
					cks= MEM_callocN(sizeof(bCommonKeySrc), "bCommonKeySrc");
					BLI_addtail(sources, cks); 
					
					/* set data */
					cks->id= (ID *)wo;
					cks->ipo= wo->ipo;
					cks->map= texchannel_to_adrcode(wo->texact);
					
					/* set keyingsets */
					*ksc= &ks_contexts[KSC_BUTS_WO];
					return;
				}
			}
				break;
			case TAB_SHADING_LAMP: /* >------------- Lamp Tab -------------< */
			{
				Lamp *la= G.buts->lockpoin;
				
				if (la) {
					/* add new keyframing destination */
					cks= MEM_callocN(sizeof(bCommonKeySrc), "bCommonKeySrc");
					BLI_addtail(sources, cks); 
					
					/* set data */
					cks->id= (ID *)la;
					cks->ipo= la->ipo;
					cks->map= texchannel_to_adrcode(la->texact);
					
					/* set keyingsets */
					*ksc= &ks_contexts[KSC_BUTS_LA];
					return;
				}
			}
				break;
			case TAB_SHADING_TEX: /* >------------- Texture Tab -------------< */
			{
				Tex *tex= G.buts->lockpoin;
				
				if (tex) {
					/* add new keyframing destination */
					cks= MEM_callocN(sizeof(bCommonKeySrc), "bCommonKeySrc");
					BLI_addtail(sources, cks); 
					
					/* set data */
					cks->id= (ID *)tex;
					cks->ipo= tex->ipo;
					
					/* set keyingsets */
					*ksc= &ks_contexts[KSC_BUTS_TEX];
					return;
				}
			}
				break;
		}
		break;
	
	case CONTEXT_OBJECT:	/* ------------- Object buttons ---------------- */
		{
			Object *ob= OBACT;
			
			if (ob) {
				/* add new keyframing destination */
				cks= MEM_callocN(sizeof(bCommonKeySrc), "bCommonKeySrc");
				BLI_addtail(sources, cks);
				
				/* set id-block to key to */
				cks->id= (ID *)ob;
				cks->ipo= ob->ipo;
				
				/* set keyingsets */
				*ksc= &ks_contexts[KSC_BUTS_OB];
				return;
			}
		}
		break;
	
	case CONTEXT_EDITING:	/* ------------- Editing buttons ---------------- */
		{
			Object *ob= OBACT;
			
			if ((ob) && (ob->type==OB_CAMERA) && (G.buts->lockpoin)) { /* >---------------- camera buttons ---------------< */
				Camera *ca= G.buts->lockpoin;
				
				/* add new keyframing destination */
				cks= MEM_callocN(sizeof(bCommonKeySrc), "bCommonKeySrc");
				BLI_addtail(sources, cks);
				
				/* set id-block to key to */
				cks->id= (ID *)ca;
				cks->ipo= ca->ipo;
				
				/* set keyingsets */
				*ksc= &ks_contexts[KSC_BUTS_CAM];
				return;
			}
		}
		break;
	}
#endif // XXX end of buttons stuff to port...
	
	/* if nothing happened... */
	*ksc= NULL;
}


/* get keyingsets for appropriate context */
static void commonkey_context_get (const bContext *C, ScrArea *sa, short mode, ListBase *sources, bKeyingContext **ksc)
{
	/* get current view if no view is defined */
	if (sa == NULL)
		sa= CTX_wm_area(C);
	
	/* check view type */
	switch (sa->spacetype) {
		/* 3d view - first one tested as most often used */
		case SPACE_VIEW3D:
		{
			commonkey_context_getv3d(C, sources, ksc);
		}
			break;
			
		/* buttons view */
		case SPACE_BUTS:
		{
			commonkey_context_getsbuts(C, sources, ksc);
		}
			break;
			
		/* spaces with their own methods */
		case SPACE_IPO:
			//if (mode == COMMONKEY_MODE_INSERT)
			//	insertkey_editipo(); // XXX old calls...
			return;
		case SPACE_ACTION:
			//if (mode == COMMONKEY_MODE_INSERT)
			//	insertkey_action(); // XXX old calls...
			return;
			
		/* timeline view - keyframe buttons */
		case SPACE_TIME:
		{
			bScreen *sc= CTX_wm_screen(C);
			ScrArea *sab;
			int bigarea= 0;
			
			/* try to find largest 3d-view available 
			 * (mostly of the time, this is what when user will want this,
			 *  as it's a standard feature in all other apps) 
			 */
			//sab= find_biggest_area_of_type(SPACE_VIEW3D);
			sab= NULL; // XXX for now...
			if (sab) {
				commonkey_context_getv3d(C, sources, ksc);
				return;
			}
			
			/* if not found, sab is now NULL, so perform own biggest area test */
			for (sa= sc->areabase.first; sa; sa= sa->next) { // XXX this has changed!
				int area= sa->winx * sa->winy;
				
				if (sa->spacetype != SPACE_TIME) {
					if ( (!sab) || (area > bigarea) ) {
						sab= sa;
						bigarea= area;
					}
				}
			}
			
			/* use whichever largest area was found (it shouldn't be a time window) */
			if (sab)
				commonkey_context_get(C, sab, mode, sources, ksc);
		}
			break;
	}
}

/* flush updates after all operations */
static void commonkey_context_finish (const bContext *C, ListBase *sources)
{
	ScrArea *curarea= CTX_wm_area(C);
	Scene *scene= CTX_data_scene(C);
	
	/* check view type */
	switch (curarea->spacetype) {
		/* 3d view - first one tested as most often used */
		case SPACE_VIEW3D:
		{
			/* either pose or object level */
			if (OBACT && (OBACT->pose)) {	
				//Object *ob= OBACT;
				
				/* recalculate ipo handles, etc. */
				// XXX this method has been removed!
				//if (ob->action)
				//	remake_action_ipos(ob->action);
				
				/* recalculate bone-paths on adding new keyframe? */
				// XXX missing function
				// TODO: currently, there is no setting to turn this on/off globally
				//if (ob->pose->flag & POSE_RECALCPATHS)
				//	pose_recalculate_paths(ob);
			}
			else {
				bCommonKeySrc *cks;
				
				/* loop over bases (as seen in sources) */
				for (cks= sources->first; cks; cks= cks->next) {
					Object *ob= (Object *)cks->id;
					
					/* simply set recalc flag */
					ob->recalc |= OB_RECALC_OB;
				}
			}
		}
			break;
	}
}

/* flush refreshes after undo */
static void commonkey_context_refresh (bContext *C)
{
	ScrArea *curarea= CTX_wm_area(C);
	
	/* check view type */
	switch (curarea->spacetype) {
		/* 3d view - first one tested as most often used */
		case SPACE_VIEW3D:
		{
			/* do refreshes */
			ED_anim_dag_flush_update(C);
		}
			break;
			
		/* buttons window */
		case SPACE_BUTS:
		{
			//allspace(REMAKEIPO, 0);
			//allqueue(REDRAWVIEW3D, 0);
			//allqueue(REDRAWMARKER, 0);
		}
			break;
	}
}

/* --- */

/* Get the keying set that was chosen by the user from the menu */
static bKeyingSet *get_keyingset_fromcontext (bKeyingContext *ksc, short index)
{
	/* check if index is valid */
	if (ELEM(NULL, ksc, ksc->keyingsets))
		return NULL;
	if ((index < 1) || (index > ksc->tot))
		return NULL;
		
	/* index starts from 1, and should directly correspond to keyingset in array */
	return (bKeyingSet *)(ksc->keyingsets + (index - 1));
}

/* ---------------- Keyframe Management API -------------------- */

/* Display a menu for handling the insertion of keyframes based on the active view */
void common_modifykey (bContext *C, short mode)
{
	ListBase dsources = {NULL, NULL};
	bKeyingContext *ksc= NULL;
	bCommonKeySrc *cks;
	bKeyingSet *ks = NULL;
	char *menustr, buf[64];
	short menu_nr;
	
	/* check if mode is valid */
	if (ELEM(mode, COMMONKEY_MODE_INSERT, COMMONKEY_MODE_DELETE)==0)
		return;
	
	/* delegate to other functions or get keyingsets to use 
	 *	- if the current area doesn't have its own handling, there will be data returned...
	 */
	commonkey_context_get(C, NULL, mode, &dsources, &ksc);
	
	/* check that there is data to operate on */
	if (ELEM(NULL, dsources.first, ksc)) {
		BLI_freelistN(&dsources);
		return;
	}
	
	/* get menu and process it */
	if (mode == COMMONKEY_MODE_DELETE)
		menustr= build_keyingsets_menu(ksc, "Delete");
	else
		menustr= build_keyingsets_menu(ksc, "Insert");
	// XXX: this goes to the invoke!
	//menu_nr= pupmenu(menustr);
	//if (menustr) MEM_freeN(menustr);
	menu_nr = -1; // XXX for now
	
	/* no item selected or shapekey entry? */
	if (menu_nr < 1) {
		/* free temp sources */
		BLI_freelistN(&dsources);
		
		/* check if insert new shapekey */
		// XXX missing function!
		//if ((menu_nr == 0) && (mode == COMMONKEY_MODE_INSERT))
		//	insert_shapekey(OBACT);
		//else 
			ksc->lastused= NULL;
			
		return;
	}
	else {
		/* try to get keyingset */
		ks= get_keyingset_fromcontext(ksc, menu_nr);
		
		if (ks == NULL) {
			BLI_freelistN(&dsources);
			return;
		}
	}
	
	/* loop over each destination, applying the keying set */
	for (cks= dsources.first; cks; cks= cks->next) {
		short success= 0;
		
		/* special hacks for 'available' option */
		if (ks->flag == -2) {
			IpoCurve *icu= NULL, *icn= NULL;
			
			/* get first IPO-curve */
			if (cks->act && cks->actname) {
				bActionChannel *achan= get_action_channel(cks->act, cks->actname);
				
				// FIXME: what about constraint channels?
				if (achan && achan->ipo)
					icu= achan->ipo->curve.first; 
			}
			else if(cks->ipo)
				icu= cks->ipo->curve.first;
				
			/* we get adrcodes directly from IPO curves (see method below...) */
			for (; icu; icu= icn) {
				short flag;
				
				/* get next ipo-curve in case current is deleted */
				icn= icu->next;
				
				/* insert mode or delete mode */
				if (mode == COMMONKEY_MODE_DELETE) {
					/* local flags only add on to global flags */
					flag = 0;
					
					/* delete keyframe */
					success += deletekey(cks->id, ks->blocktype, cks->actname, cks->constname, icu->adrcode, flag);
				}
				else {
					/* local flags only add on to global flags */
					flag = ks->flag;
					if (IS_AUTOKEY_FLAG(AUTOMATKEY)) flag |= INSERTKEY_MATRIX;
					if (IS_AUTOKEY_FLAG(INSERTNEEDED)) flag |= INSERTKEY_NEEDED;
					// if (IS_AUTOKEY_MODE(EDITKEYS)) flag |= INSERTKEY_REPLACE;
					
					/* insert keyframe */
					success += insertkey(cks->id, ks->blocktype, cks->actname, cks->constname, icu->adrcode, flag);
				}
			}
		}
		else {
			bKS_AdrcodeGetter kag;
			short (*get_next_adrcode)(bKS_AdrcodeGetter *);
			int adrcode;
			
			/* initialise keyingset channel iterator */
			ks_adrcodegetter_init(&kag, ks, cks);
			
			/* get iterator - only one can be in use at a time... the flags should be mutually exclusive in this regard */
			if (ks->flag & COMMONKEY_PCHANROT)
				get_next_adrcode= ks_getnextadrcode_pchanrot;
			else if (ks->flag & COMMONKEY_ADDMAP)
				get_next_adrcode= ks_getnextadrcode_addmap;
			else
				get_next_adrcode= ks_getnextadrcode_default;
			
			/* loop over channels available in keyingset */
			for (adrcode= get_next_adrcode(&kag); adrcode > 0; adrcode= get_next_adrcode(&kag)) {
				short flag;
				
				/* insert mode or delete mode */
				if (mode == COMMONKEY_MODE_DELETE) {
					/* local flags only add on to global flags */
					flag = 0;
					//flag &= ~COMMONKEY_MODES;
					
					/* delete keyframe */
					success += deletekey(cks->id, ks->blocktype, cks->actname, cks->constname, adrcode, flag);
				}
				else {
					/* local flags only add on to global flags */
					flag = ks->flag;
					if (IS_AUTOKEY_FLAG(AUTOMATKEY)) flag |= INSERTKEY_MATRIX;
					if (IS_AUTOKEY_FLAG(INSERTNEEDED)) flag |= INSERTKEY_NEEDED;
					// if (IS_AUTOKEY_MODE(EDITKEYS)) flag |= INSERTKEY_REPLACE;
					flag &= ~COMMONKEY_MODES;
					
					/* insert keyframe */
					success += insertkey(cks->id, ks->blocktype, cks->actname, cks->constname, adrcode, flag);
				}
			}
		}
		
		/* special handling for some key-sources */
		if (success) {
			/* set pose recalc-paths flag */
			if (cks->pchan) {
				Object *ob= (Object *)cks->id;
				bPoseChannel *pchan= cks->pchan;
				
				/* set flag to trigger path recalc */
				if (pchan->path) 
					ob->pose->flag |= POSE_RECALCPATHS;
					
				/* clear unkeyed flag (it doesn't matter if it's set or not) */
					// XXX old animation system
				//if (pchan->bone)
				//	pchan->bone->flag &= ~BONE_UNKEYED;
			}
			
			// XXX for new system, need to remove overrides
		}
	}
	
	/* apply post-keying flushes for this data sources */
	commonkey_context_finish(C, &dsources);
	
	/* free temp data */
	BLI_freelistN(&dsources);
	
	/* queue updates for contexts */
	commonkey_context_refresh(C);
}

#endif // XXX old keyingsets code based on adrcodes... to be restored in due course

/* Given a KeyingSet and context info (if required), modify keyframes for the channels specified
 * by the KeyingSet. This takes into account many of the different combinations of using KeyingSets.
 * Returns the number of channels that keyframes were added to
 */
static int commonkey_modifykey (ListBase *dsources, KeyingSet *ks, short mode, float cfra)
{
	KS_Path *ksp;
	int kflag=0, success= 0;
	char *groupname= NULL;
	
	/* get flags to use */
	if (mode == COMMONKEY_MODE_INSERT) {
		/* use KeyingSet's flags as base */
		kflag= ks->keyingflag;
		
		/* suppliment with info from the context */
		if (IS_AUTOKEY_FLAG(AUTOMATKEY)) kflag |= INSERTKEY_MATRIX;
		if (IS_AUTOKEY_FLAG(INSERTNEEDED)) kflag |= INSERTKEY_NEEDED;
		// if (IS_AUTOKEY_MODE(EDITKEYS)) flag |= INSERTKEY_REPLACE;
	}
	else if (mode == COMMONKEY_MODE_DELETE)
		kflag= 0;
	
	/* check if the KeyingSet is absolute or not (i.e. does it requires sources info) */
	if (ks->flag & KEYINGSET_ABSOLUTE) {
		/* Absolute KeyingSets are simpler to use, as all the destination info has already been
		 * provided by the user, and is stored, ready to use, in the KeyingSet paths.
		 */
		for (ksp= ks->paths.first; ksp; ksp= ksp->next) {
			int arraylen, i;
			
			/* get pointer to name of group to add channels to */
			if (ksp->groupmode == KSP_GROUP_NONE)
				groupname= NULL;
			else if (ksp->groupmode == KSP_GROUP_KSNAME)
				groupname= ks->name;
			else
				groupname= ksp->group;
			
			/* init arraylen and i - arraylen should be greater than i so that
			 * normal non-array entries get keyframed correctly
			 */
			i= ksp->array_index;
			arraylen= i+1;
			
			/* get length of array if whole array option is enabled */
			if (ksp->flag & KSP_FLAG_WHOLE_ARRAY) {
				PointerRNA id_ptr, ptr;
				PropertyRNA *prop;
				
				RNA_id_pointer_create(ksp->id, &id_ptr);
				if (RNA_path_resolve(&id_ptr, ksp->rna_path, &ptr, &prop) && prop)
					arraylen= RNA_property_array_length(&ptr, prop);
			}
			
			/* for each possible index, perform operation 
			 *	- assume that arraylen is greater than index
			 */
			for (; i < arraylen; i++) {
				/* action to take depends on mode */
				if (mode == COMMONKEY_MODE_INSERT)
					success+= insertkey(ksp->id, groupname, ksp->rna_path, i, cfra, kflag);
				else if (mode == COMMONKEY_MODE_DELETE)
					success+= deletekey(ksp->id, groupname,  ksp->rna_path, i, cfra, kflag);
			}
			
			// TODO: set recalc tags on the ID?
		}
	}
#if 0 // XXX still need to figure out how to get such keyingsets working
	else if (dsources) {
		/* for each one of the 'sources', resolve the template markers and expand arrays, then insert keyframes */
		bCommonKeySrc *cks;
		char *path = NULL;
		int index=0, tot=0;
		
		/* for each 'source' for keyframe data, resolve each of the paths from the KeyingSet */
		for (cks= dsources->first; cks; cks= cks->next) {
			
		}
	}
#endif // XXX still need to figure out how to get such 
	
	return success;
}

/* Insert Key Operator ------------------------ */

/* NOTE:
 * This is one of the 'simpler new-style' Insert Keyframe operators which relies on Keying Sets.
 * For now, these are absolute Keying Sets only, so there is very little context info involved.
 *
 * 	-- Joshua Leung, Feb 2009
 */

static int insert_key_exec (bContext *C, wmOperator *op)
{
	ListBase dsources = {NULL, NULL};
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks= NULL;
	float cfra= (float)CFRA; // XXX for now, don't bother about all the yucky offset crap
	short success;
	
	/* try to get KeyingSet */
	if (scene->active_keyingset > 0)
		ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	/* report failure */
	if (ks == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active Keying Set");
		return OPERATOR_CANCELLED;
	}
	
	/* try to insert keyframes for the channels specified by KeyingSet */
	success= commonkey_modifykey(&dsources, ks, COMMONKEY_MODE_INSERT, cfra);
	printf("KeyingSet '%s' - Successfully added %d Keyframes \n", ks->name, success);
	
	/* report failure? */
	if (success == 0)
		BKE_report(op->reports, RPT_WARNING, "Keying Set failed to insert any keyframes");
	
	/* send updates */
	ED_anim_dag_flush_update(C);	
	
	/* for now, only send ND_KEYS for KeyingSets */
	WM_event_add_notifier(C, ND_KEYS, NULL);
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_insert_keyframe (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Insert Keyframe";
	ot->idname= "ANIM_OT_insert_keyframe";
	
	/* callbacks */
	ot->exec= insert_key_exec; 
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* Delete Key Operator ------------------------ */

/* NOTE:
 * This is one of the 'simpler new-style' Insert Keyframe operators which relies on Keying Sets.
 * For now, these are absolute Keying Sets only, so there is very little context info involved.
 *
 * 	-- Joshua Leung, Feb 2009
 */
 
static int delete_key_exec (bContext *C, wmOperator *op)
{
	ListBase dsources = {NULL, NULL};
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks= NULL;
	float cfra= (float)CFRA; // XXX for now, don't bother about all the yucky offset crap
	short success;
	
	/* try to get KeyingSet */
	if (scene->active_keyingset > 0)
		ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	/* report failure */
	if (ks == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active Keying Set");
		return OPERATOR_CANCELLED;
	}
	
	/* try to insert keyframes for the channels specified by KeyingSet */
	success= commonkey_modifykey(&dsources, ks, COMMONKEY_MODE_DELETE, cfra);
	printf("KeyingSet '%s' - Successfully removed %d Keyframes \n", ks->name, success);
	
	/* report failure? */
	if (success == 0)
		BKE_report(op->reports, RPT_WARNING, "Keying Set failed to remove any keyframes");
	
	/* send updates */
	ED_anim_dag_flush_update(C);	
	
	/* for now, only send ND_KEYS for KeyingSets */
	WM_event_add_notifier(C, ND_KEYS, NULL);
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_delete_keyframe (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Keyframe";
	ot->idname= "ANIM_OT_delete_keyframe";
	
	/* callbacks */
	ot->exec= delete_key_exec; 
	
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* Insert Key Operator ------------------------ */

/* XXX WARNING:
 * This is currently just a basic operator, which work in 3d-view context on objects/bones only
 * and will insert keyframes for a few settings only. This is until it becomes clear how
 * to separate (or not) the process for RNA-path creation between context + keyingsets.
 * 
 * -- Joshua Leung, Jan 2009
 */

/* defines for basic insert-key testing operator  */
	// XXX this will definitely be replaced
EnumPropertyItem prop_insertkey_types[] = {
	{0, "KEYINGSET", "Active KeyingSet", ""},
	{1, "OBLOC", "Object Location", ""},
	{2, "OBROT", "Object Rotation", ""},
	{3, "OBSCALE", "Object Scale", ""},
	{4, "MAT_COL", "Active Material - Color", ""},
	{5, "PCHANLOC", "Pose-Channel Location", ""},
	{6, "PCHANROT", "Pose-Channel Rotation", ""},
	{7, "PCHANSCALE", "Pose-Channel Scale", ""},
	{0, NULL, NULL, NULL}
};

static int insert_key_old_invoke (bContext *C, wmOperator *op, wmEvent *event)
{
	Object *ob= CTX_data_active_object(C);
	uiMenuItem *head;
	
	if (ob == NULL)
		return OPERATOR_CANCELLED;
		
	head= uiPupMenuBegin("Insert Keyframe", 0);
	
	/* active keyingset */
	uiMenuItemEnumO(head, "", 0, "ANIM_OT_insert_keyframe_old", "type", 0);
	
	/* selective inclusion */
	if ((ob->pose) && (ob->flag & OB_POSEMODE)) {
		/* bone types */	
		uiMenuItemEnumO(head, "", 0, "ANIM_OT_insert_keyframe_old", "type", 5);
		uiMenuItemEnumO(head, "", 0, "ANIM_OT_insert_keyframe_old", "type", 6);
		uiMenuItemEnumO(head, "", 0, "ANIM_OT_insert_keyframe_old", "type", 7);
	}
	else {
		/* object types */
		uiMenuItemEnumO(head, "", 0, "ANIM_OT_insert_keyframe_old", "type", 1);
		uiMenuItemEnumO(head, "", 0, "ANIM_OT_insert_keyframe_old", "type", 2);
		uiMenuItemEnumO(head, "", 0, "ANIM_OT_insert_keyframe_old", "type", 3);
		uiMenuItemEnumO(head, "", 0, "ANIM_OT_insert_keyframe_old", "type", 4);
	}
	
	uiPupMenuEnd(C, head);
	
	return OPERATOR_CANCELLED;
}
 
static int insert_key_old_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	short mode= RNA_enum_get(op->ptr, "type");
	float cfra= (float)CFRA; // XXX for now, don't bother about all the yucky offset crap
	
	/* for now, handle 'active keyingset' one separately */
	if (mode == 0) {
		ListBase dsources = {NULL, NULL};
		KeyingSet *ks= NULL;
		short success;
		
		/* try to get KeyingSet */
		if (scene->active_keyingset > 0)
			ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
		/* report failure */
		if (ks == NULL) {
			BKE_report(op->reports, RPT_ERROR, "No active Keying Set");
			return OPERATOR_CANCELLED;
		}
		
		/* try to insert keyframes for the channels specified by KeyingSet */
		success= commonkey_modifykey(&dsources, ks, COMMONKEY_MODE_INSERT, cfra);
		printf("KeyingSet '%s' - Successfully added %d Keyframes \n", ks->name, success);
		
		/* report failure? */
		if (success == 0)
			BKE_report(op->reports, RPT_WARNING, "Keying Set failed to insert any keyframes");
	}
	else {
		// more comprehensive tests will be needed
		CTX_DATA_BEGIN(C, Base*, base, selected_bases) 
		{
			Object *ob= base->object;
			ID *id= (ID *)ob;
			short success= 0;
			
			/* check which keyframing mode chosen for this object */
			if (mode < 4) {
				/* object-based keyframes */
				switch (mode) {
				case 4: /* color of active material (only for geometry...) */
					// NOTE: this is just a demo... but ideally we'd go through materials instead of active one only so reference stays same
					// XXX no group for now
					success+= insertkey(id, NULL, "active_material.diffuse_color", 0, cfra, 0);
					success+= insertkey(id, NULL, "active_material.diffuse_color", 1, cfra, 0);
					success+= insertkey(id, NULL, "active_material.diffuse_color", 2, cfra, 0);
					break;
				case 3: /* object scale */
					success+= insertkey(id, "Object Transforms", "scale", 0, cfra, 0);
					success+= insertkey(id, "Object Transforms", "scale", 1, cfra, 0);
					success+= insertkey(id, "Object Transforms", "scale", 2, cfra, 0);
					break;
				case 2: /* object rotation */
					success+= insertkey(id, "Object Transforms", "rotation", 0, cfra, 0);
					success+= insertkey(id, "Object Transforms", "rotation", 1, cfra, 0);
					success+= insertkey(id, "Object Transforms", "rotation", 2, cfra, 0);
					break;
				case 1: /* object location */
					success+= insertkey(id, "Object Transforms", "location", 0, cfra, 0);
					success+= insertkey(id, "Object Transforms", "location", 1, cfra, 0);
					success+= insertkey(id, "Object Transforms", "location", 2, cfra, 0);
					break;
				}
				
				ob->recalc |= OB_RECALC_OB;
			}
			else if ((ob->pose) && (ob->flag & OB_POSEMODE)) {
				/* PoseChannel based keyframes */
				bPoseChannel *pchan;
				
				for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
					/* only if selected */
					if ((pchan->bone) && (pchan->bone->flag & BONE_SELECTED)) {
						char buf[512];
						
						switch (mode) {
						case 6: /* pchan scale */
							sprintf(buf, "pose.pose_channels[\"%s\"].scale", pchan->name);
							success+= insertkey(id, pchan->name, buf, 0, cfra, 0);
							success+= insertkey(id, pchan->name, buf, 1, cfra, 0);
							success+= insertkey(id, pchan->name, buf, 2, cfra, 0);
							break;
						case 5: /* pchan rotation */
							if (pchan->rotmode == PCHAN_ROT_QUAT) {
								sprintf(buf, "pose.pose_channels[\"%s\"].rotation", pchan->name);
								success+= insertkey(id, pchan->name, buf, 0, cfra, 0);
								success+= insertkey(id, pchan->name, buf, 1, cfra, 0);
								success+= insertkey(id, pchan->name, buf, 2, cfra, 0);
								success+= insertkey(id, pchan->name, buf, 3, cfra, 0);
							}
							else {
								sprintf(buf, "pose.pose_channels[\"%s\"].euler_rotation", pchan->name);
								success+= insertkey(id, pchan->name, buf, 0, cfra, 0);
								success+= insertkey(id, pchan->name, buf, 1, cfra, 0);
								success+= insertkey(id, pchan->name, buf, 2, cfra, 0);
							}
							break;
						default: /* pchan location */
							sprintf(buf, "pose.pose_channels[\"%s\"].location", pchan->name);
							success+= insertkey(id, pchan->name, buf, 0, cfra, 0);
							success+= insertkey(id, pchan->name, buf, 1, cfra, 0);
							success+= insertkey(id, pchan->name, buf, 2, cfra, 0);
							break;
						}
					}
				}
				
				ob->recalc |= OB_RECALC_OB;
			}
			
			printf("Ob '%s' - Successfully added %d Keyframes \n", id->name+2, success);
		}
		CTX_DATA_END;
	}
	
	/* send updates */
	ED_anim_dag_flush_update(C);	
	
	if (mode == 0) /* for now, only send ND_KEYS for KeyingSets */
		WM_event_add_notifier(C, ND_KEYS, NULL);
	else if (mode == 4) /* material color requires different notifiers */
		WM_event_add_notifier(C, NC_MATERIAL|ND_KEYS, NULL);
	else
		WM_event_add_notifier(C, NC_OBJECT|ND_KEYS, NULL);
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_insert_keyframe_old (wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name= "Insert Keyframe";
	ot->idname= "ANIM_OT_insert_keyframe_old";
	
	/* callbacks */
	ot->invoke= insert_key_old_invoke;
	ot->exec= insert_key_old_exec; 
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
		// XXX update this for the latest RNA stuff styles...
	prop= RNA_def_property(ot->srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_insertkey_types);
}

/* Delete Key Operator ------------------------ */

/* XXX WARNING:
 * This is currently just a basic operator, which work in 3d-view context on objects only. 
 * -- Joshua Leung, Jan 2009
 */
 
static int delete_key_old_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	float cfra= (float)CFRA; // XXX for now, don't bother about all the yucky offset crap
	
	// XXX more comprehensive tests will be needed
	CTX_DATA_BEGIN(C, Base*, base, selected_bases) 
	{
		Object *ob= base->object;
		ID *id= (ID *)ob;
		FCurve *fcu, *fcn;
		short success= 0;
		
		/* loop through all curves in animdata and delete keys on this frame */
		if (ob->adt) {
			AnimData *adt= ob->adt;
			bAction *act= adt->action;
			
			for (fcu= act->curves.first; fcu; fcu= fcn) {
				fcn= fcu->next;
				success+= deletekey(id, NULL, fcu->rna_path, fcu->array_index, cfra, 0);
			}
		}
		
		printf("Ob '%s' - Successfully removed %d keyframes \n", id->name+2, success);
		
		ob->recalc |= OB_RECALC_OB;
	}
	CTX_DATA_END;
	
	/* send updates */
	ED_anim_dag_flush_update(C);	
	
		// XXX what if it was a material keyframe?
	WM_event_add_notifier(C, NC_OBJECT|ND_KEYS, NULL);
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_delete_keyframe_old (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Keyframe";
	ot->idname= "ANIM_OT_delete_keyframe_old";
	
	/* callbacks */
	ot->invoke= WM_operator_confirm; // XXX we will need our own one eventually, to cope with the dynamic menus...
	ot->exec= delete_key_old_exec; 
	
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************************************* */
/* KEYFRAME DETECTION */

/* --------------- API/Per-Datablock Handling ------------------- */

/* Checks whether an IPO-block has a keyframe for a given frame 
 * Since we're only concerned whether a keyframe exists, we can simply loop until a match is found...
 */
short action_frame_has_keyframe (bAction *act, float frame, short filter)
{
	FCurve *fcu;
	
	/* can only find if there is data */
	if (act == NULL)
		return 0;
		
	/* if only check non-muted, check if muted */
	if ((filter & ANIMFILTER_KEYS_MUTED) || (act->flag & ACT_MUTED))
		return 0;
	
	/* loop over F-Curves, using binary-search to try to find matches 
	 *	- this assumes that keyframes are only beztriples
	 */
	for (fcu= act->curves.first; fcu; fcu= fcu->next) {
		/* only check if there are keyframes (currently only of type BezTriple) */
		if (fcu->bezt && fcu->totvert) {
			/* we either include all regardless of muting, or only non-muted  */
			if ((filter & ANIMFILTER_KEYS_MUTED) || (fcu->flag & FCURVE_MUTED)==0) {
				short replace = -1;
				int i = binarysearch_bezt_index(fcu->bezt, frame, fcu->totvert, &replace);
				
				/* binarysearch_bezt_index will set replace to be 0 or 1
				 * 	- obviously, 1 represents a match
				 */
				if (replace) {			
					/* sanity check: 'i' may in rare cases exceed arraylen */
					if ((i >= 0) && (i < fcu->totvert))
						return 1;
				}
			}
		}
	}
	
	/* nothing found */
	return 0;
}

/* Checks whether an Object has a keyframe for a given frame */
short object_frame_has_keyframe (Object *ob, float frame, short filter)
{
	/* error checking */
	if (ob == NULL)
		return 0;
	
	/* check own animation data - specifically, the action it contains */
	if ((ob->adt) && (ob->adt->action)) {
		if (action_frame_has_keyframe(ob->adt->action, frame, filter))
			return 1;
	}
	
	/* try shapekey keyframes (if available, and allowed by filter) */
	if ( !(filter & ANIMFILTER_KEYS_LOCAL) && !(filter & ANIMFILTER_KEYS_NOSKEY) ) {
		Key *key= ob_get_key(ob);
		
		/* shapekeys can have keyframes ('Relative Shape Keys') 
		 * or depend on time (old 'Absolute Shape Keys') 
		 */
		 
			/* 1. test for relative (with keyframes) */
		if (id_frame_has_keyframe((ID *)key, frame, filter))
			return 1;
			
			/* 2. test for time */
		// TODO... yet to be implemented (this feature may evolve before then anyway)
	}
	
	/* try materials */
	if ( !(filter & ANIMFILTER_KEYS_LOCAL) && !(filter & ANIMFILTER_KEYS_NOMAT) ) {
		/* if only active, then we can skip a lot of looping */
		if (filter & ANIMFILTER_KEYS_ACTIVE) {
			Material *ma= give_current_material(ob, (ob->actcol + 1));
			
			/* we only retrieve the active material... */
			if (id_frame_has_keyframe((ID *)ma, frame, filter))
				return 1;
		}
		else {
			int a;
			
			/* loop over materials */
			for (a=0; a<ob->totcol; a++) {
				Material *ma= give_current_material(ob, a+1);
				
				if (id_frame_has_keyframe((ID *)ma, frame, filter))
					return 1;
			}
		}
	}
	
	/* nothing found */
	return 0;
}

/* --------------- API ------------------- */

/* Checks whether a keyframe exists for the given ID-block one the given frame */
short id_frame_has_keyframe (ID *id, float frame, short filter)
{
	/* sanity checks */
	if (id == NULL)
		return 0;
	
	/* perform special checks for 'macro' types */
	switch (GS(id->name)) {
		case ID_OB: /* object */
			return object_frame_has_keyframe((Object *)id, frame, filter);
			break;
			
		case ID_SCE: /* scene */
		// XXX TODO... for now, just use 'normal' behaviour
		//	break;
		
		default: 	/* 'normal type' */
		{
			AnimData *adt= BKE_animdata_from_id(id);
			
			/* only check keyframes in active action */
			if (adt)
				return action_frame_has_keyframe(adt->action, frame, filter);
		}
			break;
	}
	
	
	/* no keyframe found */
	return 0;
}

/* ************************************************** */
