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
 * The Original Code is Copyright (C) 2009, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/armature/poseSlide.c
 *  \ingroup edarmature
 */

 
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_dlrbTree.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.h"

#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_keyframes_draw.h"
#include "ED_markers.h"
#include "ED_screen.h"

#include "armature_intern.h"

/* **************************************************** */
/* == POSE 'SLIDING' TOOLS == 
 *
 * A) Push & Relax, Breakdowner
 * These tools provide the animator with various capabilities
 * for interactively controlling the spacing of poses, but also
 * for 'pushing' and/or 'relaxing' extremes as they see fit.
 *
 * B) Propagate
 * This tool copies elements of the selected pose to successive
 * keyframes, allowing the animator to go back and modify the poses
 * for some "static" pose controls, without having to repeatedly
 * doing a "next paste" dance.
 *
 * C) Pose Sculpting
 * This is yet to be implemented, but the idea here is to use
 * sculpting techniques to make it easier to pose rigs by allowing
 * rigs to be manipulated using a familiar paint-based interface. 
 */
/* **************************************************** */
/* A) Push & Relax, Breakdowner */

/* Temporary data shared between these operators */
typedef struct tPoseSlideOp {
	Scene *scene;		/* current scene */
	ScrArea *sa;		/* area that we're operating in (needed for modal()) */
	ARegion *ar;		/* region that we're operating in (needed for modal()) */
	Object *ob;			/* active object that Pose Info comes from */
	bArmature *arm;		/* armature for pose */
	
	ListBase pfLinks;	/* links between posechannels and f-curves  */
	DLRBT_Tree keys;	/* binary tree for quicker searching for keyframes (when applicable) */
	
	int cframe;			/* current frame number */
	int prevFrame;		/* frame before current frame (blend-from) */
	int nextFrame;		/* frame after current frame (blend-to) */
	
	int mode;			/* sliding mode (ePoseSlide_Modes) */
	int flag;			// unused for now, but can later get used for storing runtime settings....
	
	float percentage;	/* 0-1 value for determining the influence of whatever is relevant */
} tPoseSlideOp;

/* Pose Sliding Modes */
typedef enum ePoseSlide_Modes {
	POSESLIDE_PUSH	= 0,		/* exaggerate the pose... */
	POSESLIDE_RELAX,			/* soften the pose... */
	POSESLIDE_BREAKDOWN,		/* slide between the endpoint poses, finding a 'soft' spot */
} ePoseSlide_Modes;

/* ------------------------------------ */

/* operator init */
static int pose_slide_init (bContext *C, wmOperator *op, short mode)
{
	tPoseSlideOp *pso;
	bAction *act= NULL;
	
	/* init slide-op data */
	pso= op->customdata= MEM_callocN(sizeof(tPoseSlideOp), "tPoseSlideOp");
	
	/* get info from context */
	pso->scene= CTX_data_scene(C);
	pso->ob= object_pose_armature_get(CTX_data_active_object(C));
	pso->arm= (pso->ob)? pso->ob->data : NULL;
	pso->sa= CTX_wm_area(C); /* only really needed when doing modal() */
	pso->ar= CTX_wm_region(C); /* only really needed when doing modal() */
	
	pso->cframe= pso->scene->r.cfra;
	pso->mode= mode;
	
	/* set range info from property values - these may get overridden for the invoke() */
	pso->percentage= RNA_float_get(op->ptr, "percentage");
	pso->prevFrame= RNA_int_get(op->ptr, "prev_frame");
	pso->nextFrame= RNA_int_get(op->ptr, "next_frame");
	
	/* check the settings from the context */
	if (ELEM4(NULL, pso->ob, pso->arm, pso->ob->adt, pso->ob->adt->action))
		return 0;
	else
		act= pso->ob->adt->action;
	
	/* for each Pose-Channel which gets affected, get the F-Curves for that channel 
	 * and set the relevant transform flags...
	 */
	poseAnim_mapping_get(C, &pso->pfLinks, pso->ob, act);
	
	/* set depsgraph flags */
		/* make sure the lock is set OK, unlock can be accidentally saved? */
	pso->ob->pose->flag |= POSE_LOCKED;
	pso->ob->pose->flag &= ~POSE_DO_UNLOCK;
	
	/* do basic initialize of RB-BST used for finding keyframes, but leave the filling of it up 
	 * to the caller of this (usually only invoke() will do it, to make things more efficient).
	 */
	BLI_dlrbTree_init(&pso->keys);
	
	/* return status is whether we've got all the data we were requested to get */
	return 1;
}

/* exiting the operator - free data */
static void pose_slide_exit(wmOperator *op)
{
	tPoseSlideOp *pso= op->customdata;
	
	/* if data exists, clear its data and exit */
	if (pso) {
		/* free the temp pchan links and their data */
		poseAnim_mapping_free(&pso->pfLinks);
		
		/* free RB-BST for keyframes (if it contained data) */
		BLI_dlrbTree_free(&pso->keys);
		
		/* free data itself */
		MEM_freeN(pso);
	}
	
	/* cleanup */
	op->customdata= NULL;
}

/* ------------------------------------ */

/* helper for apply() / reset() - refresh the data */
static void pose_slide_refresh (bContext *C, tPoseSlideOp *pso)
{
	/* wrapper around the generic version, allowing us to add some custom stuff later still */
	poseAnim_mapping_refresh(C, pso->scene, pso->ob);
}

/* helper for apply() - perform sliding for some value */
static void pose_slide_apply_val (tPoseSlideOp *pso, FCurve *fcu, float *val)
{
	float cframe = (float)pso->cframe;
	float sVal, eVal;
	float w1, w2;
	
	/* get keyframe values for endpoint poses to blend with */
		/* previous/start */
	sVal= evaluate_fcurve(fcu, (float)pso->prevFrame);
		/* next/end */
	eVal= evaluate_fcurve(fcu, (float)pso->nextFrame);
	
	/* calculate the relative weights of the endpoints */
	if (pso->mode == POSESLIDE_BREAKDOWN) {
		/* get weights from the percentage control */
		w1= pso->percentage;	/* this must come second */
		w2= 1.0f - w1;			/* this must come first */
	}
	else {
		/*	- these weights are derived from the relative distance of these 
		 *	  poses from the current frame
		 *	- they then get normalised so that they only sum up to 1
		 */
		float wtot; 
		
		w1 = cframe - (float)pso->prevFrame;
		w2 = (float)pso->nextFrame - cframe;
		
		wtot = w1 + w2;
		w1 = (w1/wtot);
		w2 = (w2/wtot);
	}
	
	/* depending on the mode, calculate the new value
	 *	- in all of these, the start+end values are multiplied by w2 and w1 (respectively),
	 *	  since multiplication in another order would decrease the value the current frame is closer to
	 */
	switch (pso->mode) {
		case POSESLIDE_PUSH: /* make the current pose more pronounced */
		{
			/* perform a weighted average here, favoring the middle pose
			 *	- numerator should be larger than denominator to 'expand' the result
			 *	- perform this weighting a number of times given by the percentage...
			 */
			int iters= (int)ceil(10.0f*pso->percentage); // TODO: maybe a sensitivity ctrl on top of this is needed
			
			while (iters-- > 0) {
				(*val)= ( -((sVal * w2) + (eVal * w1)) + ((*val) * 6.0f) ) / 5.0f; 
			}
		}
			break;
			
		case POSESLIDE_RELAX: /* make the current pose more like its surrounding ones */
		{
			/* perform a weighted average here, favoring the middle pose
			 *	- numerator should be smaller than denominator to 'relax' the result
			 *	- perform this weighting a number of times given by the percentage...
			 */
			int iters= (int)ceil(10.0f*pso->percentage); // TODO: maybe a sensitivity ctrl on top of this is needed
			
			while (iters-- > 0) {
				(*val)= ( ((sVal * w2) + (eVal * w1)) + ((*val) * 5.0f) ) / 6.0f;
			}
		}
			break;
			
		case POSESLIDE_BREAKDOWN: /* make the current pose slide around between the endpoints */
		{
			/* perform simple linear interpolation - coefficient for start must come from pso->percentage... */
			// TODO: make this use some kind of spline interpolation instead?
			(*val)= ((sVal * w2) + (eVal * w1));
		}
			break;
	}
}

/* helper for apply() - perform sliding for some 3-element vector */
static void pose_slide_apply_vec3 (tPoseSlideOp *pso, tPChanFCurveLink *pfl, float vec[3], const char propName[])
{
	LinkData *ld=NULL;
	char *path=NULL;
	
	/* get the path to use... */
	path= BLI_sprintfN("%s.%s", pfl->pchan_path, propName);
	
	/* using this path, find each matching F-Curve for the variables we're interested in */
	while ( (ld= poseAnim_mapping_getNextFCurve(&pfl->fcurves, ld, path)) ) {
		FCurve *fcu= (FCurve *)ld->data;
		
		/* just work on these channels one by one... there's no interaction between values */
		pose_slide_apply_val(pso, fcu, &vec[fcu->array_index]);
	}
	
	/* free the temp path we got */
	MEM_freeN(path);
}

/* helper for apply() - perform sliding for custom properties */
static void pose_slide_apply_props (tPoseSlideOp *pso, tPChanFCurveLink *pfl)
{
	PointerRNA ptr = {{NULL}};
	LinkData *ld;
	int len = strlen(pfl->pchan_path);
	
	/* setup pointer RNA for resolving paths */
	RNA_pointer_create(NULL, &RNA_PoseBone, pfl->pchan, &ptr);
	
	/* custom properties are just denoted using ["..."][etc.] after the end of the base path, 
	 * so just check for opening pair after the end of the path
	 */
	for (ld = pfl->fcurves.first; ld; ld = ld->next) {
		FCurve *fcu = (FCurve *)ld->data;
		char *bPtr, *pPtr;
		
		if (fcu->rna_path == NULL)
			continue;
		
		/* do we have a match? 
		 *	- bPtr is the RNA Path with the standard part chopped off
		 *	- pPtr is the chunk of the path which is left over
		 */
		bPtr = strstr(fcu->rna_path, pfl->pchan_path) + len;
		pPtr = strstr(bPtr, "[\"");   /* dummy " for texteditor bugs */
		
		if (pPtr) {
			/* use RNA to try and get a handle on this property, then, assuming that it is just
			 * numerical, try and grab the value as a float for temp editing before setting back
			 */
			PropertyRNA *prop = RNA_struct_find_property(&ptr, pPtr);
			
			if (prop) {
				switch (RNA_property_type(prop)) {
					case PROP_FLOAT:
					{
						float tval = RNA_property_float_get(&ptr, prop);
						pose_slide_apply_val(pso, fcu, &tval);
						RNA_property_float_set(&ptr, prop, tval);
					}
						break;
					case PROP_BOOLEAN:
					case PROP_ENUM:
					case PROP_INT:
					{
						float tval = (float)RNA_property_int_get(&ptr, prop);
						pose_slide_apply_val(pso, fcu, &tval);
						RNA_property_int_set(&ptr, prop, (int)tval);
					}
						break;
					default:
						/* cannot handle */
						//printf("Cannot Pose Slide non-numerical property\n");
						break;
				}
			}
		}
	}
}

/* helper for apply() - perform sliding for quaternion rotations (using quat blending) */
static void pose_slide_apply_quat (tPoseSlideOp *pso, tPChanFCurveLink *pfl)
{
	FCurve *fcu_w=NULL, *fcu_x=NULL, *fcu_y=NULL, *fcu_z=NULL;
	bPoseChannel *pchan= pfl->pchan;
	LinkData *ld=NULL;
	char *path=NULL;
	float cframe;
	
	/* get the path to use - this should be quaternion rotations only (needs care) */
	path= BLI_sprintfN("%s.%s", pfl->pchan_path, "rotation_quaternion");
	
	/* get the current frame number */
	cframe= (float)pso->cframe;
	
	/* using this path, find each matching F-Curve for the variables we're interested in */
	while ( (ld= poseAnim_mapping_getNextFCurve(&pfl->fcurves, ld, path)) ) {
		FCurve *fcu= (FCurve *)ld->data;
		
		/* assign this F-Curve to one of the relevant pointers... */
		switch (fcu->array_index) {
			case 3: /* z */
				fcu_z= fcu;
				break;
			case 2: /* y */
				fcu_y= fcu;
				break;
			case 1: /* x */
				fcu_x= fcu;
				break;
			case 0: /* w */
				fcu_w= fcu;
				break;
		}
	}
	
	/* only if all channels exist, proceed */
	if (fcu_w && fcu_x && fcu_y && fcu_z) {
		float quat_prev[4], quat_next[4];
		
		/* get 2 quats */
		quat_prev[0] = evaluate_fcurve(fcu_w, pso->prevFrame);
		quat_prev[1] = evaluate_fcurve(fcu_x, pso->prevFrame);
		quat_prev[2] = evaluate_fcurve(fcu_y, pso->prevFrame);
		quat_prev[3] = evaluate_fcurve(fcu_z, pso->prevFrame);
		
		quat_next[0] = evaluate_fcurve(fcu_w, pso->nextFrame);
		quat_next[1] = evaluate_fcurve(fcu_x, pso->nextFrame);
		quat_next[2] = evaluate_fcurve(fcu_y, pso->nextFrame);
		quat_next[3] = evaluate_fcurve(fcu_z, pso->nextFrame);
		
		/* perform blending */
		if (pso->mode == POSESLIDE_BREAKDOWN) {
			/* just perform the interpol between quat_prev and quat_next using pso->percentage as a guide */
			interp_qt_qtqt(pchan->quat, quat_prev, quat_next, pso->percentage);
		}
		else if (pso->mode == POSESLIDE_PUSH) {
			float quat_diff[4], quat_orig[4];
			
			/* calculate the delta transform from the previous to the current */
			// TODO: investigate ways to favour one transform more?
			sub_qt_qtqt(quat_diff, pchan->quat, quat_prev);
			
			/* make a copy of the original rotation */
			copy_qt_qt(quat_orig, pchan->quat);
			
			/* increase the original by the delta transform, by an amount determined by percentage */
			add_qt_qtqt(pchan->quat, quat_orig, quat_diff, pso->percentage);
		}
		else {
			float quat_interp[4], quat_orig[4];
			int iters= (int)ceil(10.0f*pso->percentage); // TODO: maybe a sensitivity ctrl on top of this is needed
			
			/* perform this blending several times until a satisfactory result is reached */
			while (iters-- > 0) {
				/* calculate the interpolation between the endpoints */
				interp_qt_qtqt(quat_interp, quat_prev, quat_next, (cframe-pso->prevFrame) / (pso->nextFrame-pso->prevFrame) );
				
				/* make a copy of the original rotation */
				copy_qt_qt(quat_orig, pchan->quat);
				
				/* tricky interpolations - blending between original and new */
				interp_qt_qtqt(pchan->quat, quat_orig, quat_interp, 1.0f/6.0f);
			}
		}
	}
	
	/* free the path now */
	MEM_freeN(path);
}

/* apply() - perform the pose sliding based on weighting various poses */
static void pose_slide_apply(bContext *C, tPoseSlideOp *pso)
{
	tPChanFCurveLink *pfl;
	
	/* sanitise the frame ranges */
	if (pso->prevFrame == pso->nextFrame) {
		/* move out one step either side */
		pso->prevFrame--;
		pso->nextFrame++;
	}
	
	/* for each link, handle each set of transforms */
	for (pfl= pso->pfLinks.first; pfl; pfl= pfl->next) {
		/* valid transforms for each PoseChannel should have been noted already 
		 *	- sliding the pose should be a straightforward exercise for location+rotation, 
		 *	  but rotations get more complicated since we may want to use quaternion blending 
		 *	  for quaternions instead...
		 */
		bPoseChannel *pchan= pfl->pchan;
		 
		if (pchan->flag & POSE_LOC) {
			/* calculate these for the 'location' vector, and use location curves */
			pose_slide_apply_vec3(pso, pfl, pchan->loc, "location");
		}
		
		if (pchan->flag & POSE_SIZE) {
			/* calculate these for the 'scale' vector, and use scale curves */
			pose_slide_apply_vec3(pso, pfl, pchan->size, "scale");
		}
		
		if (pchan->flag & POSE_ROT) {
			/* everything depends on the rotation mode */
			if (pchan->rotmode > 0) {
				/* eulers - so calculate these for the 'eul' vector, and use euler_rotation curves */
				pose_slide_apply_vec3(pso, pfl, pchan->eul, "rotation_euler");
			}
			else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				// TODO: need to figure out how to do this!
			}
			else {
				/* quaternions - use quaternion blending */
				pose_slide_apply_quat(pso, pfl);
			}
		}
		
		if (pfl->oldprops) {
			/* not strictly a transform, but contributes to the pose produced in many rigs */
			pose_slide_apply_props(pso, pfl);
		}
	}
	
	/* depsgraph updates + redraws */
	pose_slide_refresh(C, pso);
}

/* perform autokeyframing after changes were made + confirmed */
static void pose_slide_autoKeyframe (bContext *C, tPoseSlideOp *pso)
{
	/* wrapper around the generic call */
	poseAnim_mapping_autoKeyframe(C, pso->scene, pso->ob, &pso->pfLinks, (float)pso->cframe);
}

/* reset changes made to current pose */
static void pose_slide_reset (tPoseSlideOp *pso)
{
	/* wrapper around the generic call, so that custom stuff can be added later */
	poseAnim_mapping_reset(&pso->pfLinks);
}

/* ------------------------------------ */

/* draw percentage indicator in header */
static void pose_slide_draw_status (tPoseSlideOp *pso)
{
	char status_str[32];
	char mode_str[32];
	
	switch (pso->mode) {
		case POSESLIDE_PUSH:
			strcpy(mode_str, "Push Pose");
			break;
		case POSESLIDE_RELAX:
			strcpy(mode_str, "Relax Pose");
			break;
		case POSESLIDE_BREAKDOWN:
			strcpy(mode_str, "Breakdown");
			break;
		
		default:
			// unknown
			strcpy(mode_str, "Sliding-Tool");
			break;
	}
	
	BLI_snprintf(status_str, sizeof(status_str), "%s: %d %%", mode_str, (int)(pso->percentage*100.0f));
	ED_area_headerprint(pso->sa, status_str);
}

/* common code for invoke() methods */
static int pose_slide_invoke_common (bContext *C, wmOperator *op, tPoseSlideOp *pso)
{
	tPChanFCurveLink *pfl;
	AnimData *adt= pso->ob->adt;
	wmWindow *win= CTX_wm_window(C);
	
	/* for each link, add all its keyframes to the search tree */
	for (pfl= pso->pfLinks.first; pfl; pfl= pfl->next) {
		LinkData *ld;
		
		/* do this for each F-Curve */
		for (ld= pfl->fcurves.first; ld; ld= ld->next) {
			FCurve *fcu= (FCurve *)ld->data;
			fcurve_to_keylist(adt, fcu, &pso->keys, NULL);
		}
	}
	
	/* consolidate these keyframes, and figure out the nearest ones */
	BLI_dlrbTree_linkedlist_sync(&pso->keys);
	
		/* cancel if no keyframes found... */
	if (pso->keys.root) {
		ActKeyColumn *ak;
		float cframe= (float)pso->cframe;
		
		/* firstly, check if the current frame is a keyframe... */
		ak= (ActKeyColumn *)BLI_dlrbTree_search_exact(&pso->keys, compare_ak_cfraPtr, &cframe);
		
		if (ak == NULL) {
			/* current frame is not a keyframe, so search */
			ActKeyColumn *pk= (ActKeyColumn *)BLI_dlrbTree_search_prev(&pso->keys, compare_ak_cfraPtr, &cframe);
			ActKeyColumn *nk= (ActKeyColumn *)BLI_dlrbTree_search_next(&pso->keys, compare_ak_cfraPtr, &cframe);
			
			/* new set the frames */
				/* prev frame */
			pso->prevFrame= (pk)? (pk->cfra) : (pso->cframe - 1);
			RNA_int_set(op->ptr, "prev_frame", pso->prevFrame);
				/* next frame */
			pso->nextFrame= (nk)? (nk->cfra) : (pso->cframe + 1);
			RNA_int_set(op->ptr, "next_frame", pso->nextFrame);
		}
		else {
			/* current frame itself is a keyframe, so just take keyframes on either side */
				/* prev frame */
			pso->prevFrame= (ak->prev)? (ak->prev->cfra) : (pso->cframe - 1);
			RNA_int_set(op->ptr, "prev_frame", pso->prevFrame);
				/* next frame */
			pso->nextFrame= (ak->next)? (ak->next->cfra) : (pso->cframe + 1);
			RNA_int_set(op->ptr, "next_frame", pso->nextFrame);
		}
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "No keyframes to slide between");
		pose_slide_exit(op);
		return OPERATOR_CANCELLED;
	}
	
	/* initial apply for operator... */
	// TODO: need to calculate percentage for initial round too...
	pose_slide_apply(C, pso);
	
	/* depsgraph updates + redraws */
	pose_slide_refresh(C, pso);
	
	/* set cursor to indicate modal */
	WM_cursor_modal(win, BC_EW_SCROLLCURSOR);
	
	/* header print */
	pose_slide_draw_status(pso);
	
	/* add a modal handler for this operator */
	WM_event_add_modal_handler(C, op);
	return OPERATOR_RUNNING_MODAL;
}

/* common code for modal() */
static int pose_slide_modal (bContext *C, wmOperator *op, wmEvent *evt)
{
	tPoseSlideOp *pso= op->customdata;
	wmWindow *win= CTX_wm_window(C);
	
	switch (evt->type) {
		case LEFTMOUSE:	/* confirm */
		{
			/* return to normal cursor and header status */
			ED_area_headerprint(pso->sa, NULL);
			WM_cursor_restore(win);
			
			/* insert keyframes as required... */
			pose_slide_autoKeyframe(C, pso);
			pose_slide_exit(op);
			
			/* done! */
			return OPERATOR_FINISHED;
		}
		
		case ESCKEY:	/* cancel */
		case RIGHTMOUSE: 
		{
			/* return to normal cursor and header status */
			ED_area_headerprint(pso->sa, NULL);
			WM_cursor_restore(win);
			
			/* reset transforms back to original state */
			pose_slide_reset(pso);
			
			/* depsgraph updates + redraws */
			pose_slide_refresh(C, pso);
			
			/* clean up temp data */
			pose_slide_exit(op);
			
			/* canceled! */
			return OPERATOR_CANCELLED;
		}
			
		case MOUSEMOVE: /* calculate new position */
		{
			/* calculate percentage based on position of mouse (we only use x-axis for now.
			 * since this is more convenient for users to do), and store new percentage value
			 */
			pso->percentage= (evt->x - pso->ar->winrct.xmin) / ((float)pso->ar->winx);
			RNA_float_set(op->ptr, "percentage", pso->percentage);
			
			/* update percentage indicator in header */
			pose_slide_draw_status(pso);
			
			/* reset transforms (to avoid accumulation errors) */
			pose_slide_reset(pso);
			
			/* apply... */
			pose_slide_apply(C, pso);
		}
			break;
			
		default: /* unhandled event (maybe it was some view manip? */
			/* allow to pass through */
			return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
	}
	
	/* still running... */
	return OPERATOR_RUNNING_MODAL;
}

/* common code for cancel() */
static int pose_slide_cancel (bContext *UNUSED(C), wmOperator *op)
{
	/* cleanup and done */
	pose_slide_exit(op);
	return OPERATOR_CANCELLED;
}

/* common code for exec() methods */
static int pose_slide_exec_common (bContext *C, wmOperator *op, tPoseSlideOp *pso)
{
	/* settings should have been set up ok for applying, so just apply! */
	pose_slide_apply(C, pso);
	
	/* insert keyframes if needed */
	pose_slide_autoKeyframe(C, pso);
	
	/* cleanup and done */
	pose_slide_exit(op);
	
	return OPERATOR_FINISHED;
}

/* common code for defining RNA properties */
static void pose_slide_opdef_properties (wmOperatorType *ot)
{
	RNA_def_int(ot->srna, "prev_frame", 0, MINAFRAME, MAXFRAME, "Previous Keyframe", "Frame number of keyframe immediately before the current frame", 0, 50);
	RNA_def_int(ot->srna, "next_frame", 0, MINAFRAME, MAXFRAME, "Next Keyframe", "Frame number of keyframe immediately after the current frame", 0, 50);
	RNA_def_float_percentage(ot->srna, "percentage", 0.5f, 0.0f, 1.0f, "Percentage", "Weighting factor for the sliding operation", 0.3, 0.7);
}

/* ------------------------------------ */

/* invoke() - for 'push' mode */
static int pose_slide_push_invoke (bContext *C, wmOperator *op, wmEvent *UNUSED(evt))
{
	tPoseSlideOp *pso;
	
	/* initialize data  */
	if (pose_slide_init(C, op, POSESLIDE_PUSH) == 0) {
		pose_slide_exit(op);
		return OPERATOR_CANCELLED;
	}
	else
		pso= op->customdata;
	
	/* do common setup work */
	return pose_slide_invoke_common(C, op, pso);
}

/* exec() - for push */
static int pose_slide_push_exec (bContext *C, wmOperator *op)
{
	tPoseSlideOp *pso;
	
	/* initialize data (from RNA-props) */
	if (pose_slide_init(C, op, POSESLIDE_PUSH) == 0) {
		pose_slide_exit(op);
		return OPERATOR_CANCELLED;
	}
	else
		pso= op->customdata;
		
	/* do common exec work */
	return pose_slide_exec_common(C, op, pso);
}

void POSE_OT_push (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Push Pose";
	ot->idname= "POSE_OT_push";
	ot->description= "Exaggerate the current pose";
	
	/* callbacks */
	ot->exec= pose_slide_push_exec;
	ot->invoke= pose_slide_push_invoke;
	ot->modal= pose_slide_modal;
	ot->cancel= pose_slide_cancel;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
	
	/* Properties */
	pose_slide_opdef_properties(ot);
}

/* ........................ */

/* invoke() - for 'relax' mode */
static int pose_slide_relax_invoke (bContext *C, wmOperator *op, wmEvent *UNUSED(evt))
{
	tPoseSlideOp *pso;
	
	/* initialize data  */
	if (pose_slide_init(C, op, POSESLIDE_RELAX) == 0) {
		pose_slide_exit(op);
		return OPERATOR_CANCELLED;
	}
	else
		pso= op->customdata;
	
	/* do common setup work */
	return pose_slide_invoke_common(C, op, pso);
}

/* exec() - for relax */
static int pose_slide_relax_exec (bContext *C, wmOperator *op)
{
	tPoseSlideOp *pso;
	
	/* initialize data (from RNA-props) */
	if (pose_slide_init(C, op, POSESLIDE_RELAX) == 0) {
		pose_slide_exit(op);
		return OPERATOR_CANCELLED;
	}
	else
		pso= op->customdata;
		
	/* do common exec work */
	return pose_slide_exec_common(C, op, pso);
}

void POSE_OT_relax (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Relax Pose";
	ot->idname= "POSE_OT_relax";
	ot->description= "Make the current pose more similar to its surrounding ones";
	
	/* callbacks */
	ot->exec= pose_slide_relax_exec;
	ot->invoke= pose_slide_relax_invoke;
	ot->modal= pose_slide_modal;
	ot->cancel= pose_slide_cancel;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
	
	/* Properties */
	pose_slide_opdef_properties(ot);
}

/* ........................ */

/* invoke() - for 'breakdown' mode */
static int pose_slide_breakdown_invoke (bContext *C, wmOperator *op, wmEvent *UNUSED(evt))
{
	tPoseSlideOp *pso;
	
	/* initialize data  */
	if (pose_slide_init(C, op, POSESLIDE_BREAKDOWN) == 0) {
		pose_slide_exit(op);
		return OPERATOR_CANCELLED;
	}
	else
		pso= op->customdata;
	
	/* do common setup work */
	return pose_slide_invoke_common(C, op, pso);
}

/* exec() - for breakdown */
static int pose_slide_breakdown_exec (bContext *C, wmOperator *op)
{
	tPoseSlideOp *pso;
	
	/* initialize data (from RNA-props) */
	if (pose_slide_init(C, op, POSESLIDE_BREAKDOWN) == 0) {
		pose_slide_exit(op);
		return OPERATOR_CANCELLED;
	}
	else
		pso= op->customdata;
		
	/* do common exec work */
	return pose_slide_exec_common(C, op, pso);
}

void POSE_OT_breakdown (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Pose Breakdowner";
	ot->idname= "POSE_OT_breakdown";
	ot->description= "Create a suitable breakdown pose on the current frame";
	
	/* callbacks */
	ot->exec= pose_slide_breakdown_exec;
	ot->invoke= pose_slide_breakdown_invoke;
	ot->modal= pose_slide_modal;
	ot->cancel= pose_slide_cancel;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
	
	/* Properties */
	pose_slide_opdef_properties(ot);
}

/* **************************************************** */
/* B) Pose Propagate */

/* "termination conditions" - i.e. when we stop */
typedef enum ePosePropagate_Termination {
		/* stop after the current hold ends */
	POSE_PROPAGATE_SMART_HOLDS = 0,
		/* only do on the last keyframe */
	POSE_PROPAGATE_LAST_KEY,
		/* stop after the next keyframe */
	POSE_PROPAGATE_NEXT_KEY,
		/* stop after the specified frame */
	POSE_PROPAGATE_BEFORE_FRAME,
		/* stop when we run out of keyframes */
	POSE_PROPAGATE_BEFORE_END,
	
		/* only do on the frames where markers are selected */
	POSE_PROPAGATE_SELECTED_MARKERS
} ePosePropagate_Termination;

/* termination data needed for some modes - assumes only one of these entries will be needed at a time */
typedef union tPosePropagate_ModeData {
	/* smart holds + before frame: frame number to stop on */
	float end_frame;
	
	/* selected markers: listbase for CfraElem's marking these frames */
	ListBase sel_markers;
} tPosePropagate_ModeData;

/* --------------------------------- */

/* get frame on which the "hold" for the bone ends 
 * XXX: this may not really work that well if a bone moves on some channels and not others
 * 		if this happens to be a major issue, scrap this, and just make this happen 
 *		independently per F-Curve
 */
static float pose_propagate_get_boneHoldEndFrame (Object *ob, tPChanFCurveLink *pfl, float startFrame)
{
	DLRBT_Tree keys, blocks;
	ActKeyBlock *ab;
	
	AnimData *adt= ob->adt;
	LinkData *ld;
	float endFrame = startFrame;
	
	/* set up optimized data-structures for searching for relevant keyframes + holds */
	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);
	
	for (ld = pfl->fcurves.first; ld; ld = ld->next) {
		FCurve *fcu = (FCurve *)ld->data;
		fcurve_to_keylist(adt, fcu, &keys, &blocks);
	}
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);
	
	/* find the long keyframe (i.e. hold), and hence obtain the endFrame value 
	 *	- the best case would be one that starts on the frame itself
	 */
	ab = (ActKeyBlock *)BLI_dlrbTree_search_exact(&blocks, compare_ab_cfraPtr, &startFrame);
	
	if (actkeyblock_is_valid(ab, &keys) == 0) {
		/* There are only two cases for no-exact match:
		 * 	1) the current frame is just before another key but not on a key itself
		 * 	2) the current frame is on a key, but that key doesn't link to the next
		 *
		 * If we've got the first case, then we can search for another block, 
		 * otherwise forget it, as we'd be overwriting some valid data.
		 */
		if (BLI_dlrbTree_search_exact(&keys, compare_ak_cfraPtr, &startFrame) == NULL) {
			/* we've got case 1, so try the one after */
			ab = (ActKeyBlock *)BLI_dlrbTree_search_next(&blocks, compare_ab_cfraPtr, &startFrame);
			
			if (actkeyblock_is_valid(ab, &keys) == 0) {
				/* try the block before this frame then as last resort */
				ab = (ActKeyBlock *)BLI_dlrbTree_search_prev(&blocks, compare_ab_cfraPtr, &startFrame);
				
				/* whatever happens, stop searching now... */
				if (actkeyblock_is_valid(ab, &keys) == 0) {
					/* restrict range to just the frame itself 
					 * i.e. everything is in motion, so no holds to safely overwrite
					 */
					ab = NULL;
				}
			}
		}
		else {
			/* we've got case 2 - set ab to NULL just in case, since we shouldn't do anything in this case */
			ab = NULL;
		}
	}
	
	/* check if we can go any further than we've already gone */
	if (ab) {
		/* go to next if it is also valid and meets "extension" criteria */
		while (ab->next) {
			ActKeyBlock *abn = (ActKeyBlock *)ab->next;
			
			/* must be valid */
			if (actkeyblock_is_valid(abn, &keys) == 0)
				break;
			/* should start on the same frame that the last ended on */
			if (ab->end != abn->start)
				break;
			/* should have the same number of curves */
			if (ab->totcurve != abn->totcurve)
				break;
			/* should have the same value 
			 * XXX: this may be a bit fuzzy on larger data sets, so be careful
			 */
			if (ab->val != abn->val)
				break;
				
			/* we can extend the bounds to the end of this "next" block now */
			ab = abn;
		}
		
		/* end frame can now take the value of the end of the block */
		endFrame = ab->end;
	}
	
	/* free temp memory */
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
	
	/* return the end frame we've found */
	return endFrame;
}

/* get reference value from F-Curve using RNA */
static short pose_propagate_get_refVal (Object *ob, FCurve *fcu, float *value)
{
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	short found= FALSE;
	
	/* base pointer is always the object -> id_ptr */
	RNA_id_pointer_create(&ob->id, &id_ptr);
	
	/* resolve the property... */
	if (RNA_path_resolve(&id_ptr, fcu->rna_path, &ptr, &prop)) {
		if (RNA_property_array_check(prop)) {
			/* array */
			if (fcu->array_index < RNA_property_array_length(&ptr, prop)) {
				found= TRUE;
				switch (RNA_property_type(prop)) {
					case PROP_BOOLEAN:
						*value= (float)RNA_property_boolean_get_index(&ptr, prop, fcu->array_index);
						break;
					case PROP_INT:
						*value= (float)RNA_property_int_get_index(&ptr, prop, fcu->array_index);
						break;
					case PROP_FLOAT:
						*value= RNA_property_float_get_index(&ptr, prop, fcu->array_index);
						break;
					default:
						found= FALSE;
						break;
				}
			}
		}
		else {
			/* not an array */
			found= TRUE;
			switch (RNA_property_type(prop)) {
				case PROP_BOOLEAN:
					*value= (float)RNA_property_boolean_get(&ptr, prop);
					break;
				case PROP_INT:
					*value= (float)RNA_property_int_get(&ptr, prop);
					break;
				case PROP_ENUM:
					*value= (float)RNA_property_enum_get(&ptr, prop);
					break;
				case PROP_FLOAT:
					*value= RNA_property_float_get(&ptr, prop);
					break;
				default:
					found= FALSE;
					break;
			}
		}
	}
	
	return found;
}

/* propagate just works along each F-Curve in turn */
static void pose_propagate_fcurve (wmOperator *op, Object *ob, FCurve *fcu, 
				float startFrame, tPosePropagate_ModeData modeData)
{
	const int mode = RNA_enum_get(op->ptr, "mode");
	
	BezTriple *bezt;
	float refVal = 0.0f;
	short keyExists;
	int i, match;
	short first=1;
	
	/* skip if no keyframes to edit */
	if ((fcu->bezt == NULL) || (fcu->totvert < 2))
		return;
		
	/* find the reference value from bones directly, which means that the user
	 * doesn't need to firstly keyframe the pose (though this doesn't mean that 
	 * they can't either)
	 */
	if( !pose_propagate_get_refVal(ob, fcu, &refVal))
		return;
	
	/* find the first keyframe to start propagating from 
	 *	- if there's a keyframe on the current frame, we probably want to save this value there too
	 *	  since it may be as of yet unkeyed
	 * 	- if starting before the starting frame, don't touch the key, as it may have had some valid 
	 *	  values
	 */
	match = binarysearch_bezt_index(fcu->bezt, startFrame, fcu->totvert, &keyExists);
	
	if (fcu->bezt[match].vec[1][0] < startFrame)
		i = match + 1;
	else
		i = match;
	
	for (bezt = &fcu->bezt[i]; i < fcu->totvert; i++, bezt++) {
		/* additional termination conditions based on the operator 'mode' property go here... */
		if (ELEM(mode, POSE_PROPAGATE_BEFORE_FRAME, POSE_PROPAGATE_SMART_HOLDS)) {
			/* stop if keyframe is outside the accepted range */
			if (bezt->vec[1][0] > modeData.end_frame)
				break; 
		}
		else if (mode == POSE_PROPAGATE_NEXT_KEY) {
			/* stop after the first keyframe has been processed */
			if (first == 0)
				break;
		}
		else if (mode == POSE_PROPAGATE_LAST_KEY) {
			/* only affect this frame if it will be the last one */
			if (i != (fcu->totvert-1))
				continue;
		}
		else if (mode == POSE_PROPAGATE_SELECTED_MARKERS) {
			/* only allow if there's a marker on this frame */
			CfraElem *ce = NULL;
			
			/* stop on matching marker if there is one */
			for (ce = modeData.sel_markers.first; ce; ce = ce->next) {
				if (ce->cfra == (int)(floor(bezt->vec[1][0] + 0.5f)))
					break;
			}
			
			/* skip this keyframe if no marker */
			if (ce == NULL)
				continue;
		}
		
		/* just flatten handles, since values will now be the same either side... */
		// TODO: perhaps a fade-out modulation of the value is required here (optional once again)?
		bezt->vec[0][1] = bezt->vec[1][1] = bezt->vec[2][1] = refVal;
		
		/* select keyframe to indicate that it's been changed */
		bezt->f2 |= SELECT;
		first = 0;
	}
}

/* --------------------------------- */

static int pose_propagate_exec (bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	bAction *act= (ob && ob->adt)? ob->adt->action : NULL;
	
	ListBase pflinks = {NULL, NULL};
	tPChanFCurveLink *pfl;
	
	tPosePropagate_ModeData modeData;
	const int mode = RNA_enum_get(op->ptr, "mode");
	
	/* sanity checks */
	if (ob == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No object to propagate poses for");
		return OPERATOR_CANCELLED;
	}
	if (act == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No keyframed poses to propagate to");
		return OPERATOR_CANCELLED;
	}
	
	/* isolate F-Curves related to the selected bones */
	poseAnim_mapping_get(C, &pflinks, ob, act);
	
	/* mode-specific data preprocessing (requiring no access to curves) */
	if (mode == POSE_PROPAGATE_SELECTED_MARKERS) {
		/* get a list of selected markers */
		ED_markers_make_cfra_list(&scene->markers, &modeData.sel_markers, SELECT);
	}
	else {
		/* assume everything else wants endFrame */
		modeData.end_frame = RNA_float_get(op->ptr, "end_frame");
	}
	
	/* for each bone, perform the copying required */
	for (pfl = pflinks.first; pfl; pfl = pfl->next) {
		LinkData *ld;
		
		/* mode-specific data preprocessing (requiring access to all curves) */
		if (mode == POSE_PROPAGATE_SMART_HOLDS) {
			/* we store in endFrame the end frame of the "long keyframe" (i.e. a held value) starting
			 * from the keyframe that occurs after the current frame
			 */
			modeData.end_frame = pose_propagate_get_boneHoldEndFrame(ob, pfl, (float)CFRA);
		}
		
		/* go through propagating pose to keyframes, curve by curve */
		for (ld = pfl->fcurves.first; ld; ld= ld->next)
			pose_propagate_fcurve(op, ob, (FCurve *)ld->data, (float)CFRA, modeData);
	}
	
	/* free temp data */
	poseAnim_mapping_free(&pflinks);
	
	if (mode == POSE_PROPAGATE_SELECTED_MARKERS)
		BLI_freelistN(&modeData.sel_markers);
	
	/* updates + notifiers */
	poseAnim_mapping_refresh(C, scene, ob);
	
	return OPERATOR_FINISHED;
}

/* --------------------------------- */

void POSE_OT_propagate (wmOperatorType *ot)
{
	static EnumPropertyItem terminate_items[]= {
		{POSE_PROPAGATE_SMART_HOLDS, "WHILE_HELD", 0, "While Held", "Propagate pose to all keyframes after current frame that don't change (Default behavior)"},
		{POSE_PROPAGATE_NEXT_KEY, "NEXT_KEY", 0, "To Next Keyframe", "Propagate pose to first keyframe following the current frame only"},
		{POSE_PROPAGATE_LAST_KEY, "LAST_KEY", 0, "To Last Keyframe", "Propagate pose to the last keyframe only (i.e. making action cyclic)"},
		{POSE_PROPAGATE_BEFORE_FRAME, "BEFORE_FRAME", 0, "Before Frame", "Propagate pose to all keyframes between current frame and 'Frame' property"},
		{POSE_PROPAGATE_BEFORE_END, "BEFORE_END", 0, "Before Last Keyframe", "Propagate pose to all keyframes from current frame until no more are found"},
		{POSE_PROPAGATE_SELECTED_MARKERS, "SELECTED_MARKERS", 0, "On Selected Markers", "Propagate pose to all keyframes occurring on frames with Scene Markers after the current frame"},
		{0, NULL, 0, NULL, NULL}};
		
	/* identifiers */
	ot->name= "Propagate Pose";
	ot->idname= "POSE_OT_propagate";
	ot->description= "Copy selected aspects of the current pose to subsequent poses already keyframed";
	
	/* callbacks */
	ot->exec= pose_propagate_exec;
	ot->poll= ED_operator_posemode; // XXX: needs selected bones!
	
	/* flag */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	// TODO: add "fade out" control for tapering off amount of propagation as time goes by?
	ot->prop= RNA_def_enum(ot->srna, "mode", terminate_items, POSE_PROPAGATE_SMART_HOLDS, "Terminate Mode", "Method used to determine when to stop propagating pose to keyframes");
	RNA_def_float(ot->srna, "end_frame", 250.0, FLT_MIN, FLT_MAX, "End Frame", "Frame to stop propagating frames to (for 'Before Frame' mode)", 1.0, 250.0);
}

/* **************************************************** */
