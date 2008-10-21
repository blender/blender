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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender (with some old code)
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_blender.h"
#include "BKE_main.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_material.h"

#include "BIF_keyframing.h"
#include "BIF_butspace.h"
#include "BIF_editaction.h"
#include "BIF_editkey.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_poseobject.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_toets.h"

#include "BSE_editipo.h"
#include "BSE_node.h"
#include "BSE_time.h"
#include "BSE_view.h"

#include "blendef.h"

#include "PIL_time.h"			/* sleep				*/
#include "mydevice.h"

/* ************************************************** */
/* LOCAL TYPES AND DEFINES */

/* -------------- Keying Sets ------------------- */

/* keying set - a set of channels that will be keyframed together  */
// TODO: move this to a header to allow custom sets someday?
typedef struct bKeyingSet {
		/* callback func to consider if keyingset should be included 
		 * (by default, if this is undefined, item will be shown) 
		 */
	short (*include_cb)(struct bKeyingSet *, const char *);
	
	char name[48];				/* name of keyingset */
	int blocktype;				/* blocktype that all channels belong to */  // in future, this may be eliminated
	short flag;					/* flags to use when setting keyframes */
	
	short chan_num;				/* number of channels to insert keyframe in */
	short adrcodes[32];			/* adrcodes for channels to insert keys for (ideally would be variable-len, but limit of 32 will suffice) */
} bKeyingSet;

/* keying set context - an array of keying sets and the number of them */
typedef struct bKeyingContext {
	bKeyingSet *keyingsets;		/* array containing the keyingsets of interest */
	bKeyingSet *lastused;		/* item that was chosen last time*/
	int tot;					/* number of keyingsets in */
} bKeyingContext;


/* ----------- Common KeyData Sources ------------ */

/* temporary struct to gather data combos to keyframe */
typedef struct bCommonKeySrc {
	struct bCommonKeySrc *next, *prev;
		
		/* general data/destination-source settings */
	ID *id;					/* id-block this comes from */
	char *actname;			/* name of action channel */
	char *constname;		/* name of constraint channel */
	
		/* general destination source settings */
	Ipo *ipo;				/* ipo-block that id-block has (optional) */
	bAction *act;			/* action-block that id-block has (optional) */
	
		/* pose-level settings */
	bPoseChannel *pchan;	/* pose channel */
		
		/* buttons-window settings */
	int map;				/* offset to apply to certain adrcodes */
} bCommonKeySrc;

/* ************************************************** */
/* KEYFRAME INSERTION */

/* -------------- BezTriple Insertion -------------------- */

/* threshold for inserting keyframes - threshold here should be good enough for now, but should become userpref */
#define BEZT_INSERT_THRESH 	0.00001

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
		printf("Warning: binarysearch_bezt_index encountered invalid array \n");
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
		printf("Error: binarysearch_bezt_index was taking too long \n");
		
		// include debug info 
		printf("\tround = %d: start = %d, end = %d, arraylen = %d \n", loopbreaker, start, end, arraylen);
	}
	
	/* not found, so return where to place it */
	return start;
}

/* This function adds a given BezTriple to an IPO-Curve. It will allocate 
 * memory for the array if needed, and will insert the BezTriple into a
 * suitable place in chronological order.
 * 
 * NOTE: any recalculate of the IPO-Curve that needs to be done will need to 
 * 		be done by the caller.
 */
int insert_bezt_icu (IpoCurve *icu, BezTriple *bezt)
{
	BezTriple *newb;
	int i= 0;
	
	if (icu->bezt == NULL) {
		icu->bezt= MEM_callocN(sizeof(BezTriple), "beztriple");
		*(icu->bezt)= *bezt;
		icu->totvert= 1;
	}
	else {
		short replace = -1;
		i = binarysearch_bezt_index(icu->bezt, bezt->vec[1][0], icu->totvert, &replace);
		
		if (replace) {			
			/* sanity check: 'i' may in rare cases exceed arraylen */
			if ((i >= 0) && (i < icu->totvert))
				*(icu->bezt + i) = *bezt;
		}
		else {
			/* add new */
			newb= MEM_callocN((icu->totvert+1)*sizeof(BezTriple), "beztriple");
			
			/* add the beztriples that should occur before the beztriple to be pasted (originally in ei->icu) */
			if (i > 0)
				memcpy(newb, icu->bezt, i*sizeof(BezTriple));
			
			/* add beztriple to paste at index i */
			*(newb + i)= *bezt;
			
			/* add the beztriples that occur after the beztriple to be pasted (originally in icu) */
			if (i < icu->totvert) 
				memcpy(newb+i+1, icu->bezt+i, (icu->totvert-i)*sizeof(BezTriple));
			
			/* replace (+ free) old with new */
			MEM_freeN(icu->bezt);
			icu->bezt= newb;
			
			icu->totvert++;
		}
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
void insert_vert_icu (IpoCurve *icu, float x, float y, short fast)
{
	BezTriple beztr;
	int a, h1, h2;
	
	/* set all three points, for nicer start position */
	memset(&beztr, 0, sizeof(BezTriple));
	beztr.vec[0][0]= x; 
	beztr.vec[0][1]= y;
	beztr.vec[1][0]= x;
	beztr.vec[1][1]= y;
	beztr.vec[2][0]= x;
	beztr.vec[2][1]= y;
	beztr.hide= IPO_BEZ;
	beztr.f1= beztr.f2= beztr.f3= SELECT;
	beztr.h1= beztr.h2= HD_AUTO;
	
	/* add temp beztriple to keyframes */
	a= insert_bezt_icu(icu, &beztr);
	if (!fast) calchandles_ipocurve(icu);
	
	/* set handletype */
	if (icu->totvert > 2) {
		BezTriple *bezt;
		
		h1= h2= HD_AUTO;
		bezt= (icu->bezt + a);
		
		if (a > 0) h1= (bezt-1)->h2;
		if (a < icu->totvert-1) h2= (bezt+1)->h1;
		
		bezt->h1= h1;
		bezt->h2= h2;
		
		if (!fast) calchandles_ipocurve(icu);
	}
}

/* ------------------- Get Data ------------------------ */

/* Get pointer to use to get values from */
// FIXME: this should not be possible with Data-API
static void *get_context_ipo_poin (ID *id, int blocktype, char *actname, char *constname, IpoCurve *icu, int *vartype)
{
	switch (blocktype) {
		case ID_PO:  /* posechannel */
			if (GS(id->name)==ID_OB) {
				Object *ob= (Object *)id;
				bPoseChannel *pchan= get_pose_channel(ob->pose, actname);
				
				if (pchan) {
					*vartype= IPO_FLOAT;
					return get_pchan_ipo_poin(pchan, icu->adrcode);
				}
			}
			break;
			
		case ID_CO: /* constraint */
			if ((GS(id->name)==ID_OB) && (constname && constname[0])) {
				Object *ob= (Object *)id;
				bConstraint *con;
				
				/* assume that we only want the influence (as only used for Constraint Channels) */
				if ((ob->ipoflag & OB_ACTION_OB) && !strcmp(actname, "Object")) {
					for (con= ob->constraints.first; con; con= con->next) {
						if (strcmp(constname, con->name)==0) {
							*vartype= IPO_FLOAT;
							return &con->enforce;
						}
					}
				}
				else if (ob->pose) {
					bPoseChannel *pchan= get_pose_channel(ob->pose, actname);
					
					if (pchan) {
						for (con= pchan->constraints.first; con; con= con->next) {
							if (strcmp(constname, con->name)==0) {
								*vartype= IPO_FLOAT;
								return &con->enforce;
							}
						}
					}
				}
			}
			break;
			
		case ID_OB: /* object */
			/* hack: layer channels for object need to be keyed WITHOUT localview flag...
			 * tsk... tsk... why must we just dump bitflags upon users :/
			 */
			if ((GS(id->name)==ID_OB) && (icu->adrcode==OB_LAY)) {
				Object *ob= (Object *)id;
				static int layer = 0;
				
				/* init layer to be the object's layer var, then remove local view from it */
				layer = ob->lay;
				layer &= 0xFFFFFF;
				*vartype= IPO_INT_BIT;
				
				/* return pointer to this static var 
				 * 	- assumes that this pointer won't be stored for use later, so may not be threadsafe
				 *	  if multiple keyframe calls are made, but that is unlikely to happen in the near future
				 */
				return (void *)(&layer);
			}
			/* no break here for other ob channel-types - as they can be done normally */
		
		default: /* normal data-source */
			return get_ipo_poin(id, icu, vartype);
	}

	/* not valid... */
	return NULL;
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
static short new_key_needed (IpoCurve *icu, float cFrame, float nValue) 
{
	BezTriple *bezt=NULL, *prev=NULL;
	int totCount, i;
	float valA = 0.0f, valB = 0.0f;
	
	/* safety checking */
	if (icu == NULL) return KEYNEEDED_JUSTADD;
	totCount= icu->totvert;
	if (totCount == 0) return KEYNEEDED_JUSTADD;
	
	/* loop through checking if any are the same */
	bezt= icu->bezt;
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
					realVal= eval_icu(icu, cFrame);
					
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
	bezt= (icu->bezt + (icu->totvert - 1));
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

/* ------------------ 'Visual' Keyframing Functions ------------------ */

/* internal status codes for visualkey_can_use */
enum {
	VISUALKEY_NONE = 0,
	VISUALKEY_LOC,
	VISUALKEY_ROT
};

/* This helper function determines if visual-keyframing should be used when  
 * inserting keyframes for the given channel. As visual-keyframing only works
 * on Object and Pose-Channel blocks, this should only get called for those 
 * blocktypes, when using "standard" keying but 'Visual Keying' option in Auto-Keying 
 * settings is on.
 */
static short visualkey_can_use (ID *id, int blocktype, char *actname, char *constname, int adrcode)
{
	Object *ob= NULL;
	bConstraint *con= NULL;
	short searchtype= VISUALKEY_NONE;
	
	/* validate data */
	if ((id == NULL) || (GS(id->name)!=ID_OB) || !(ELEM(blocktype, ID_OB, ID_PO))) 
		return 0;
	
	/* get first constraint and determine type of keyframe constraints to check for*/
	ob= (Object *)id;
	
	if (blocktype == ID_OB) {
		con= ob->constraints.first;
		
		if (ELEM3(adrcode, OB_LOC_X, OB_LOC_Y, OB_LOC_Z))
			searchtype= VISUALKEY_LOC;
		else if (ELEM3(adrcode, OB_ROT_X, OB_ROT_Y, OB_ROT_Z))
			searchtype= VISUALKEY_ROT;
	}
	else if (blocktype == ID_PO) {
		bPoseChannel *pchan= get_pose_channel(ob->pose, actname);
		con= pchan->constraints.first;
		
		if (ELEM3(adrcode, AC_LOC_X, AC_LOC_Y, AC_LOC_Z))
			searchtype= VISUALKEY_LOC;
		else if (ELEM4(adrcode, AC_QUAT_W, AC_QUAT_X, AC_QUAT_Y, AC_QUAT_Z))
			searchtype= VISUALKEY_ROT;
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
 * to using the poin method. Assumes that all data it has been passed is valid.
 */
static float visualkey_get_value (ID *id, int blocktype, char *actname, char *constname, int adrcode, IpoCurve *icu)
{
	Object *ob;
	void *poin = NULL;
	int index, vartype;
	
	/* validate situtation */
	if ((id==NULL) || (GS(id->name)!=ID_OB) || (ELEM(blocktype, ID_OB, ID_PO)==0))
		return 0.0f;
		
	/* get object */
	ob= (Object *)id;
	
	/* only valid for objects or posechannels */
	if (blocktype == ID_OB) {
		/* parented objects are not supported, as the effects of the parent
		 * are included in the matrix, which kindof beats the point
		 */
		if (ob->parent == NULL) {
			/* only Location or Rotation keyframes are supported now */
			if (ELEM3(adrcode, OB_LOC_X, OB_LOC_Y, OB_LOC_Z)) {
				/* assumes that OB_LOC_Z > OB_LOC_Y > OB_LOC_X */
				index= adrcode - OB_LOC_X;
				
				return ob->obmat[3][index];
			}
			else if (ELEM3(adrcode, OB_ROT_X, OB_ROT_Y, OB_ROT_Z)) {
				float eul[3];
				
				/* assumes that OB_ROT_Z > OB_ROT_Y > OB_ROT_X */
				index= adrcode - OB_ROT_X;
				
				Mat4ToEul(ob->obmat, eul);
				return eul[index]*(5.72958f);
			}
		}
	}
	else if (blocktype == ID_PO) {
		bPoseChannel *pchan;
		float tmat[4][4];
		
		/* get data to use */
		pchan= get_pose_channel(ob->pose, actname);
		
		/* Although it is not strictly required for this particular space conversion, 
		 * arg1 must not be null, as there is a null check for the other conversions to
		 * be safe. Therefore, the active object is passed here, and in many cases, this
		 * will be what owns the pose-channel that is getting this anyway.
		 */
		Mat4CpyMat4(tmat, pchan->pose_mat);
		constraint_mat_convertspace(ob, pchan, tmat, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_LOCAL);
		
		/* Loc, Rot/Quat keyframes are supported... */
		if (ELEM3(adrcode, AC_LOC_X, AC_LOC_Y, AC_LOC_Z)) {
			/* assumes that AC_LOC_Z > AC_LOC_Y > AC_LOC_X */
			index= adrcode - AC_LOC_X;
			
			/* only use for non-connected bones */
			if ((pchan->bone->parent) && !(pchan->bone->flag & BONE_CONNECTED))
				return tmat[3][index];
			else if (pchan->bone->parent == NULL)
				return tmat[3][index];
		}
		else if (ELEM4(adrcode, AC_QUAT_W, AC_QUAT_X, AC_QUAT_Y, AC_QUAT_Z)) {
			float trimat[3][3], quat[4];
			
			/* assumes that AC_QUAT_Z > AC_QUAT_Y > AC_QUAT_X > AC_QUAT_W */
			index= adrcode - AC_QUAT_W;
			
			Mat3CpyMat4(trimat, tmat);
			Mat3ToQuat_is_ok(trimat, quat);
			
			return quat[index];
		}
	}
	
	/* as the function hasn't returned yet, try reading from poin */
	poin= get_context_ipo_poin(id, blocktype, actname, constname, icu, &vartype);
	if (poin)
		return read_ipo_poin(poin, vartype);
	else
		return 0.0;
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
short insertkey (ID *id, int blocktype, char *actname, char *constname, int adrcode, short flag)
{	
	IpoCurve *icu;
	
	/* get ipo-curve */
	icu= verify_ipocurve(id, blocktype, actname, constname, NULL, adrcode, 1);
	
	/* only continue if we have an ipo-curve to add keyframe to */
	if (icu) {
		float cfra = frame_to_float(CFRA);
		float curval= 0.0f;
		
		/* apply special time tweaking */
		if (GS(id->name) == ID_OB) {
			Object *ob= (Object *)id;
			
			/* apply NLA-scaling (if applicable) */
			if (actname && actname[0]) 
				cfra= get_action_frame(ob, cfra);
			
			/* ancient time-offset cruft */
			if ( (ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)) ) {
				/* actually frametofloat calc again! */
				cfra-= give_timeoffset(ob)*G.scene->r.framelen;
			}
		}
		
		/* obtain value to give keyframe */
		if ( (flag & INSERTKEY_MATRIX) && 
			 (visualkey_can_use(id, blocktype, actname, constname, adrcode)) ) 
		{
			/* visual-keying is only available for object and pchan datablocks, as 
			 * it works by keyframing using a value extracted from the final matrix 
			 * instead of using the kt system to extract a value.
			 */
			curval= visualkey_get_value(id, blocktype, actname, constname, adrcode, icu);
		}
		else {
			void *poin;
			int vartype;
			
			/* get pointer to data to read from */
			poin = get_context_ipo_poin(id, blocktype, actname, constname, icu, &vartype);
			if (poin == NULL) {
				printf("Insert Key: No pointer to variable obtained \n");
				return 0;
			}
			
			/* use kt's read_poin function to extract value (kt->read_poin should 
			 * exist in all cases, but it never hurts to check)
			 */
			curval= read_ipo_poin(poin, vartype);
		}
		
		/* only insert keyframes where they are needed */
		if (flag & INSERTKEY_NEEDED) {
			short insert_mode;
			
			/* check whether this curve really needs a new keyframe */
			insert_mode= new_key_needed(icu, cfra, curval);
			
			/* insert new keyframe at current frame */
			if (insert_mode)
				insert_vert_icu(icu, cfra, curval, (flag & INSERTKEY_FAST));
			
			/* delete keyframe immediately before/after newly added */
			switch (insert_mode) {
				case KEYNEEDED_DELPREV:
					delete_icu_key(icu, icu->totvert-2, 1);
					break;
				case KEYNEEDED_DELNEXT:
					delete_icu_key(icu, 1, 1);
					break;
			}
			
			/* only return success if keyframe added */
			if (insert_mode)
				return 1;
		}
		else {
			/* just insert keyframe */
			insert_vert_icu(icu, cfra, curval, (flag & INSERTKEY_FAST));
			
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
short deletekey (ID *id, int blocktype, char *actname, char *constname, int adrcode, short flag)
{
	Ipo *ipo;
	IpoCurve *icu;
	
	/* get ipo-curve 
	 * Note: here is one of the places where we don't want new ipo + ipo-curve added!
	 * 		so 'add' var must be 0
	 */
	ipo= verify_ipo(id, blocktype, actname, constname, NULL, 0);
	icu= verify_ipocurve(id, blocktype, actname, constname, NULL, adrcode, 0);
	
	/* only continue if we have an ipo-curve to remove keyframes from */
	if (icu) {
		float cfra = frame_to_float(CFRA);
		short found = -1;
		int i;
		
		/* apply special time tweaking */
		if (GS(id->name) == ID_OB) {
			Object *ob= (Object *)id;
			
			/* apply NLA-scaling (if applicable) */
			if (actname && actname[0]) 
				cfra= get_action_frame(ob, cfra);
			
			/* ancient time-offset cruft */
			if ( (ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)) ) {
				/* actually frametofloat calc again! */
				cfra-= give_timeoffset(ob)*G.scene->r.framelen;
			}
		}
		
		/* try to find index of beztriple to get rid of */
		i = binarysearch_bezt_index(icu->bezt, cfra, icu->totvert, &found);
		if (found) {			
			/* delete the key at the index (will sanity check + do recalc afterwards ) */
			delete_icu_key(icu, i, 1);
			
			/* Only delete curve too if there isn't an ipo-driver still hanging around on an empty curve */
			if (icu->totvert==0 && icu->driver==NULL) {
				BLI_remlink(&ipo->curve, icu);
				free_ipo_curve(icu);
			}
			
			/* return success */
			return 1;
		}
	}
	
	/* return failure */
	return 0;
}

/* ************************************************** */
/* COMMON KEYFRAME MANAGEMENT (common_insertkey/deletekey) */

/* mode for common_modifykey */
enum {
	COMMONKEY_MODE_INSERT = 0,
	COMMONKEY_MODE_DELETE,
} eCommonModifyKey_Modes;

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
	Object *ob= (G.obedit)? (G.obedit) : (OBACT);
	char *newname= NULL;
	
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
		sprintf(ks->name, newname);
	
	/* if it gets here, it's ok */
	return 1;
}

/* array for object keyingset defines */
bKeyingSet defks_v3d_object[] = 
{
	/* include_cb, name, blocktype, flag, chan_num, adrcodes */
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
	{NULL, "Rot", ID_PO, 0, 4, {AC_QUAT_W,AC_QUAT_X,AC_QUAT_Y,AC_QUAT_Z}},
	{NULL, "Scale", ID_PO, 0, 3, {AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "LocRot", ID_PO, 0, 7, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z,AC_QUAT_W,
		 AC_QUAT_X,AC_QUAT_Y,AC_QUAT_Z}},
		 
	{NULL, "LocScale", ID_PO, 0, 6, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z,
		 AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z}},
		 
	{NULL, "LocRotScale", ID_PO, 0, 10, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z,AC_QUAT_W,AC_QUAT_X,
		 AC_QUAT_Y,AC_QUAT_Z,AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z}},
		 
	{NULL, "RotScale", ID_PO, 0, 7, 
		{AC_QUAT_W,AC_QUAT_X,AC_QUAT_Y,AC_QUAT_Z,
		 AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z}},
	
	{incl_non_del_keys, "%l", 0, -1, 0, {0}}, // separator
	
	{incl_non_del_keys, "VisualLoc", ID_PO, INSERTKEY_MATRIX, 3, {AC_LOC_X,AC_LOC_Y,AC_LOC_Z}},
	{incl_non_del_keys, "VisualRot", ID_PO, INSERTKEY_MATRIX, 4, {AC_QUAT_W,AC_QUAT_X,AC_QUAT_Y,AC_QUAT_Z}},
	
	{incl_non_del_keys, "VisualLocRot", ID_PO, INSERTKEY_MATRIX, 7, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z,AC_QUAT_W,
		 AC_QUAT_X,AC_QUAT_Y,AC_QUAT_Z}},
	
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
	Object *ob= OBACT;
	/* only if object is mesh type */
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
	/* only if renderer is internal renderer */
	return (G.scene->r.renderer==R_INTERN);
}

/* check if include external-renderer entry  */
static short incl_buts_cam2 (bKeyingSet *ks, const char mode[])
{
	/* only if renderer is internal renderer */
	return (G.scene->r.renderer!=R_INTERN);
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
static void commonkey_context_getv3d (ListBase *sources, bKeyingContext **ksc)
{
	Object *ob;
	IpoCurve *icu;
	
	if ((OBACT) && (OBACT->flag & OB_POSEMODE)) {
		bPoseChannel *pchan;
		
		/* pose-level */
		ob= OBACT;
		*ksc= &ks_contexts[KSC_V3D_PCHAN];
		set_pose_keys(ob);  /* sets pchan->flag to POSE_KEY if bone selected, and clears if not */
		
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
		Base *base;
		
		/* object-level */
		*ksc= &ks_contexts[KSC_V3D_OBJECT];
		
		/* loop through bases */
		for (base= FIRSTBASE; base; base= base->next) {
			if (TESTBASELIB(base)) {
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
		}
	}
}

/* helper for commonkey_context_get() -  get keyingsets for buttons window */
static void commonkey_context_getsbuts (ListBase *sources, bKeyingContext **ksc)
{
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
	
	/* if nothing happened... */
	*ksc= NULL;
}


/* get keyingsets for appropriate context */
static void commonkey_context_get (ScrArea *sa, short mode, ListBase *sources, bKeyingContext **ksc)
{
	/* check view type */
	switch (sa->spacetype) {
		/* 3d view - first one tested as most often used */
		case SPACE_VIEW3D:
		{
			commonkey_context_getv3d(sources, ksc);
		}
			break;
			
		/* buttons view */
		case SPACE_BUTS:
		{
			commonkey_context_getsbuts(sources, ksc);
		}
			break;
			
		/* spaces with their own methods */
		case SPACE_IPO:
			if (mode == COMMONKEY_MODE_INSERT)
				insertkey_editipo();
			return;
		case SPACE_ACTION:
			if (mode == COMMONKEY_MODE_INSERT)
				insertkey_action();
			return;
			
		/* timeline view - keyframe buttons */
		case SPACE_TIME:
		{
			ScrArea *sab;
			int bigarea= 0;
			
			/* try to find largest 3d-view available 
			 * (mostly of the time, this is what when user will want this,
			 *  as it's a standard feature in all other apps) 
			 */
			sab= find_biggest_area_of_type(SPACE_VIEW3D);
			if (sab) {
				commonkey_context_getv3d(sources, ksc);
				return;
			}
			
			/* if not found, sab is now NULL, so perform own biggest area test */
			for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
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
				commonkey_context_get(sab, mode, sources, ksc);
		}
			break;
	}
}

/* flush updates after all operations */
static void commonkey_context_finish (ListBase *sources)
{
	/* check view type */
	switch (curarea->spacetype) {
		/* 3d view - first one tested as most often used */
		case SPACE_VIEW3D:
		{
			/* either pose or object level */
			if (OBACT && (OBACT->pose)) {	
				Object *ob= OBACT;
				
				/* recalculate ipo handles, etc. */
				if (ob->action)
					remake_action_ipos(ob->action);
				
				/* recalculate bone-paths on adding new keyframe? */
				// TODO: currently, there is no setting to turn this on/off globally
				if (ob->pose->flag & POSE_RECALCPATHS)
					pose_recalculate_paths(ob);
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
static void commonkey_context_refresh (void)
{
	/* check view type */
	switch (curarea->spacetype) {
		/* 3d view - first one tested as most often used */
		case SPACE_VIEW3D:
		{
			/* do refreshes */
			DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);
			
			allspace(REMAKEIPO, 0);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWMARKER, 0);
		}
			break;
			
		/* buttons window */
		case SPACE_BUTS:
		{
			allspace(REMAKEIPO, 0);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWMARKER, 0);
		}
			break;
	}
}

/* --- */

/* Build menu-string of available keying-sets (allocates memory for string)
 * NOTE: mode must not be longer than 64 chars
 */
static char *build_keyingsets_menu (bKeyingContext *ksc, const char mode[48])
{
	DynStr *pupds= BLI_dynstr_new();
	bKeyingSet *ks;
	char buf[64];
	char *str;
	int i, n;
	
	/* add title first */
	BLI_snprintf(buf, 64, "%s Key %%t|", mode);
	BLI_dynstr_append(pupds, buf);
	
	/* loop through keyingsets, adding them */
	for (ks=ksc->keyingsets, i=0, n=1; i < ksc->tot; ks++, i++, n++) {
		/* check if keyingset can be used */
		if (ks->flag == -1) {
			/* optional separator? */
			if (ks->include_cb) {
				if (ks->include_cb(ks, mode)) {
					BLI_snprintf( buf, 64, "%s%s", ks->name, ((n < ksc->tot)?"|":"") );
					BLI_dynstr_append(pupds, buf);
				}
			}
			else {
				BLI_snprintf( buf, 64, "%%l%s", ((n < ksc->tot)?"|":"") );
				BLI_dynstr_append(pupds, buf);
			}
		}
		else if ( (ks->include_cb==NULL) || (ks->include_cb(ks, mode)) ) {
			/* entry can be included */
			BLI_dynstr_append(pupds, ks->name);
			
			/* check if special "shapekey" entry */
			if (ks->flag == -3)
				BLI_snprintf( buf, 64, "%%x0%s", ((n < ksc->tot)?"|":"") );
			else
				BLI_snprintf( buf, 64, "%%x%d%s", n, ((n < ksc->tot)?"|":"") );
			BLI_dynstr_append(pupds, buf);
		}
	}
	
	/* convert to normal MEM_malloc'd string */
	str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
	
	return str;
}

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
// TODO: add back an option for repeating last keytype
void common_modifykey (short mode)
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
	commonkey_context_get(curarea, mode, &dsources, &ksc);
	
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
	menu_nr= pupmenu(menustr);
	if (menustr) MEM_freeN(menustr);
	
	/* no item selected or shapekey entry? */
	if (menu_nr < 1) {
		/* free temp sources */
		BLI_freelistN(&dsources);
		
		/* check if insert new shapekey */
		if ((menu_nr == 0) && (mode == COMMONKEY_MODE_INSERT))
			insert_shapekey(OBACT);
		else 
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
			int i;
			
			/* loop over channels available in keyingset */
			for (i=0; i < ks->chan_num; i++) {
				short flag, adrcode;
				
				/* get adrcode
				 *	- certain adrcodes (for MTEX channels need special offsets) 	// BAD CRUFT!!!
				 */
				adrcode= ks->adrcodes[i];
				if (ELEM3(ks->blocktype, ID_MA, ID_LA, ID_WO) && (ks->flag & COMMONKEY_ADDMAP)) {
					switch (adrcode) {
						case MAP_OFS_X: case MAP_OFS_Y: case MAP_OFS_Z:
						case MAP_SIZE_X: case MAP_SIZE_Y: case MAP_SIZE_Z:
						case MAP_R: case MAP_G: case MAP_B: case MAP_DVAR:
						case MAP_COLF: case MAP_NORF: case MAP_VARF: case MAP_DISP:
							adrcode += cks->map;
							break;
					}
				}
				
				/* insert mode or delete mode */
				if (mode == COMMONKEY_MODE_DELETE) {
					/* local flags only add on to global flags */
					flag = 0;
					//flag &= ~COMMONKEY_ADDMAP;
					
					/* delete keyframe */
					success += deletekey(cks->id, ks->blocktype, cks->actname, cks->constname, adrcode, flag);
				}
				else {
					/* local flags only add on to global flags */
					flag = ks->flag;
					if (IS_AUTOKEY_FLAG(AUTOMATKEY)) flag |= INSERTKEY_MATRIX;
					if (IS_AUTOKEY_FLAG(INSERTNEEDED)) flag |= INSERTKEY_NEEDED;
					// if (IS_AUTOKEY_MODE(EDITKEYS)) flag |= INSERTKEY_REPLACE;
					flag &= ~COMMONKEY_ADDMAP;
					
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
				if (pchan->bone)
					pchan->bone->flag &= ~BONE_UNKEYED;
			}
		}
	}
	
	/* apply post-keying flushes for this data sources */
	commonkey_context_finish(&dsources);
	ksc->lastused= ks;
	
	/* free temp data */
	BLI_freelistN(&dsources);
	
	/* undo pushes */
	if (mode == COMMONKEY_MODE_DELETE)
		BLI_snprintf(buf, 64, "Delete %s Key", ks->name);
	else
		BLI_snprintf(buf, 64, "Insert %s Key", ks->name);
	BIF_undo_push(buf);
	
	/* queue updates for contexts */
	commonkey_context_refresh();
}

/* ---- */

/* used to insert keyframes from any view */
void common_insertkey (void)
{
	common_modifykey(COMMONKEY_MODE_INSERT);
}

/* used to insert keyframes from any view */
void common_deletekey (void)
{
	common_modifykey(COMMONKEY_MODE_DELETE);
}

/* ************************************************** */
/* KEYFRAME DETECTION */

/* --------------- API/Per-Datablock Handling ------------------- */

/* Checks whether an IPO-block has a keyframe for a given frame 
 * Since we're only concerned whether a keyframe exists, we can simply loop until a match is found...
 */
short ipo_frame_has_keyframe (Ipo *ipo, float frame, short filter)
{
	IpoCurve *icu;
	
	/* can only find if there is data */
	if (ipo == NULL)
		return 0;
		
	/* if only check non-muted, check if muted */
	if ((filter & ANIMFILTER_MUTED) || (ipo->muteipo))
		return 0;
	
	/* loop over IPO-curves, using binary-search to try to find matches 
	 *	- this assumes that keyframes are only beztriples
	 */
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		/* only check if there are keyframes (currently only of type BezTriple) */
		if (icu->bezt) {
			/* we either include all regardless of muting, or only non-muted  */
			if ((filter & ANIMFILTER_MUTED) || (icu->flag & IPO_MUTE)==0) {
				short replace = -1;
				int i = binarysearch_bezt_index(icu->bezt, frame, icu->totvert, &replace);
				
				/* binarysearch_bezt_index will set replace to be 0 or 1
				 * 	- obviously, 1 represents a match
				 */
				if (replace) {			
					/* sanity check: 'i' may in rare cases exceed arraylen */
					if ((i >= 0) && (i < icu->totvert))
						return 1;
				}
			}
		}
	}
	
	/* nothing found */
	return 0;
}

/* Checks whether an action-block has a keyframe for a given frame 
 * Since we're only concerned whether a keyframe exists, we can simply loop until a match is found...
 */
short action_frame_has_keyframe (bAction *act, float frame, short filter)
{
	bActionChannel *achan;
	
	/* error checking */
	if (act == NULL)
		return 0;
		
	/* check thorugh action-channels for match */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		/* we either include all regardless of muting, or only non-muted 
		 *	- here we include 'hidden' channels in the muted definition
		 */
		if ((filter & ANIMFILTER_MUTED) || (achan->flag & ACHAN_HIDDEN)==0) {
			if (ipo_frame_has_keyframe(achan->ipo, frame, filter))
				return 1;
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
	
	/* check for an action - actions take priority over normal IPO's */
	if (ob->action) {
		float aframe;
		
		/* apply nla-action scaling if needed */
		if ((ob->nlaflag & OB_NLA_OVERRIDE) && (ob->nlastrips.first))
			aframe= get_action_frame(ob, frame);
		else
			aframe= frame;
		
		/* priority check here goes to pose-channel checks (for armatures) */
		if ((ob->pose) && (ob->flag & OB_POSEMODE)) {
			/* only relevant check here is to only show active... */
			if (filter & ANIMFILTER_ACTIVE) {
				bPoseChannel *pchan= get_active_posechannel(ob);
				bActionChannel *achan= (pchan) ? get_action_channel(ob->action, pchan->name) : NULL;
				
				/* since we're only interested in whether the selected one has any keyframes... */
				return (achan && ipo_frame_has_keyframe(achan->ipo, aframe, filter));
			}
		}
		
		/* for everything else, just use the standard test (only return if success) */
		if (action_frame_has_keyframe(ob->action, aframe, filter))
			return 1;
	}
	else if (ob->ipo) {
		/* only return if success */
		if (ipo_frame_has_keyframe(ob->ipo, frame, filter))
			return 1;
	}
	
	/* try shapekey keyframes (if available, and allowed by filter) */
	if ( !(filter & ANIMFILTER_LOCAL) && !(filter & ANIMFILTER_NOSKEY) ) {
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
	if ( !(filter & ANIMFILTER_LOCAL) && !(filter & ANIMFILTER_NOMAT) ) {
		/* if only active, then we can skip a lot of looping */
		if (filter & ANIMFILTER_ACTIVE) {
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
	/* error checking */
	if (id == NULL)
		return 0;
	
	/* check for a valid id-type */
	switch (GS(id->name)) {
			/* animation data-types */
		case ID_IP:	/* ipo */
			return ipo_frame_has_keyframe((Ipo *)id, frame, filter);
		case ID_AC: /* action */
			return action_frame_has_keyframe((bAction *)id, frame, filter);
			
		case ID_OB: /* object */
			return object_frame_has_keyframe((Object *)id, frame, filter);
			
		case ID_MA: /* material */
		{
			Material *ma= (Material *)id;
			
			/* currently, material's only have an ipo-block */
			return ipo_frame_has_keyframe(ma->ipo, frame, filter);
		}
			break;
			
		case ID_KE: /* shapekey */
		{
			Key *key= (Key *)id;
			
			/* currently, shapekey's only have an ipo-block */
			return ipo_frame_has_keyframe(key->ipo, frame, filter);
		}
			break;
	}
	
	/* no keyframe found */
	return 0;
}

/* ************************************************** */
