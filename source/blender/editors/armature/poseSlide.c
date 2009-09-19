/**
 * $Id: poseSlide.c 23179 2009-09-13 12:34:00Z aligorith $
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
 * The Original Code is Copyright (C) 2009, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_dlrbTree.h"

#include "DNA_listBase.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_object.h"

#include "BKE_global.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BIF_gl.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"
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
 * B) Pose Sculpting
 * This is yet to be implemented, but the idea here is to use
 * sculpting techniques to make it easier to pose rigs by allowing
 * rigs to be manipulated using a familiar paint-based interface. 
 */
/* **************************************************** */
/* A) Push & Relax, Breakdowner */

/* Temporary data shared between these operators */
typedef struct tPoseSlideOp {
	Scene *scene;		/* current scene */
	ARegion *ar;		/* region that we're operating in (needed for  */
	Object *ob;			/* active object that Pose Info comes from */
	bArmature *arm;		/* armature for pose */
	
	ListBase pfLinks;	/* links between posechannels and f-curves  */
	DLRBT_Tree keys;	/* binary tree for quicker searching for keyframes (when applicable) */
	
	KeyingSet *ks_loc;	/* builtin KeyingSet for keyframing locations */
	KeyingSet *ks_rot;	/* builtin KeyingSet for keyframing rotations */
	KeyingSet *ks_scale;/* builtin KeyingSet for keyframing scale */
	
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

/* Temporary data linking PoseChannels with the F-Curves they affect */
typedef struct tPChanFCurveLink {
	struct tPChanFCurveLink *next, *prev;
	
	ListBase fcurves;		/* F-Curves for this PoseChannel */
	bPoseChannel *pchan;	/* Pose Channel which data is attached to */
	char *pchan_path;		/* RNA Path to this Pose Channel (needs to be freed when we're done) */
} tPChanFCurveLink;

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
	pso->ob= CTX_data_active_object(C);
	pso->arm= (pso->ob)? pso->ob->data : NULL;
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
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pchans) 
	{
		ListBase curves = {NULL, NULL};
		int transFlags = action_get_item_transforms(act, pso->ob, pchan, &curves);
		
		pchan->flag &= ~(POSE_LOC|POSE_ROT|POSE_SIZE);
		
		/* check if any transforms found... */
		if (transFlags) {
			/* make new linkage data */
			tPChanFCurveLink *pfl= MEM_callocN(sizeof(tPChanFCurveLink), "tPChanFCurveLink");
			PointerRNA ptr;
			
			pfl->fcurves= curves;
			pfl->pchan= pchan;
			
			/* get the RNA path to this pchan - this needs to be freed! */
			RNA_pointer_create((ID *)pso->ob, &RNA_PoseChannel, pchan, &ptr);
			pfl->pchan_path= RNA_path_from_ID_to_struct(&ptr);
			
			/* add linkage data to operator data */
			BLI_addtail(&pso->pfLinks, pfl);
			
			/* set pchan's transform flags */
			if (transFlags & ACT_TRANS_LOC)
				pchan->flag |= POSE_LOC;
			if (transFlags & ACT_TRANS_ROT)
				pchan->flag |= POSE_ROT;
			if (transFlags & ACT_TRANS_SCALE)
				pchan->flag |= POSE_SIZE;
		}
	}
	CTX_DATA_END;
	
	/* set depsgraph flags */
		/* make sure the lock is set OK, unlock can be accidentally saved? */
	pso->ob->pose->flag |= POSE_LOCKED;
	pso->ob->pose->flag &= ~POSE_DO_UNLOCK;
	
	/* do basic initialise of RB-BST used for finding keyframes, but leave the filling of it up 
	 * to the caller of this (usually only invoke() will do it, to make things more efficient).
	 */
	BLI_dlrbTree_init(&pso->keys);
	
	/* get builtin KeyingSets */
	pso->ks_loc= ANIM_builtin_keyingset_get_named(NULL, "Location");
	pso->ks_rot= ANIM_builtin_keyingset_get_named(NULL, "Rotation");
	pso->ks_scale= ANIM_builtin_keyingset_get_named(NULL, "Scale");
	
	/* return status is whether we've got all the data we were requested to get */
	return 1;
}

/* exiting the operator - free data */
static void pose_slide_exit (bContext *C, wmOperator *op)
{
	tPoseSlideOp *pso= op->customdata;
	
	/* if data exists, clear its data and exit */
	if (pso) {
		tPChanFCurveLink *pfl, *pfln=NULL;
		
		/* free the temp pchan links and their data */
		for (pfl= pso->pfLinks.first; pfl; pfl= pfln) {
			pfln= pfl->next;
			
			/* free list of F-Curve reference links */
			BLI_freelistN(&pfl->fcurves);
			
			/* free pchan RNA Path */
			MEM_freeN(pfl->pchan_path);
			
			/* free link itself */
			BLI_freelinkN(&pso->pfLinks, pfl);
		}
		
		/* free RB-BST for keyframes (if it contained data) */
		BLI_dlrbTree_free(&pso->keys);
		
		/* free data itself */
		MEM_freeN(pso);
	}
	
	/* cleanup */
	op->customdata= NULL;
}

/* ------------------------------------ */

/* helper for apply() callabcks - find the next F-Curve with matching path... */
static LinkData *find_next_fcurve_link (ListBase *fcuLinks, LinkData *prev, char *path)
{
	LinkData *first= (prev)? prev->next : (fcuLinks)? fcuLinks->first : NULL;
	LinkData *ld;
	
	/* check each link to see if the linked F-Curve has a matching path */
	for (ld= first; ld; ld= ld->next) {
		FCurve *fcu= (FCurve *)ld->data;
		
		/* check if paths match */
		if (strcmp(path, fcu->rna_path) == 0)
			return ld;
	}	
	
	/* none found */
	return NULL;
}

/* helper for apply() - perform sliding for some 3-element vector */
static void pose_slide_apply_vec3 (tPoseSlideOp *pso, tPChanFCurveLink *pfl, float vec[3], char *propName)
{
	LinkData *ld=NULL;
	char *path=NULL;
	float cframe;
	
	/* get the path to use... */
	path= BLI_sprintfN("%s.%s", pfl->pchan_path, propName);
	
	/* get the current frame number */
	cframe= (float)pso->cframe;
	
	/* using this path, find each matching F-Curve for the variables we're interested in */
	while ( (ld= find_next_fcurve_link(&pfl->fcurves, ld, path)) ) {
		FCurve *fcu= (FCurve *)ld->data;
		float sVal, eVal;
		float w1, w2;
		int ch;
		
		/* get keyframe values for endpoint poses to blend with */
			/* previous/start */
		sVal= evaluate_fcurve(fcu, (float)pso->prevFrame);
			/* next/end */
		eVal= evaluate_fcurve(fcu, (float)pso->nextFrame);
		
		/* get channel index */
		ch= fcu->array_index;
		
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
		
		/* depending on the mode, */
		switch (pso->mode) {
			case POSESLIDE_PUSH: /* make the current pose more pronounced */
				// TODO: this is not interactively modifiable!
				vec[ch]= ( -((sVal * w2) + (eVal * w1)) + (vec[ch] * 6.0f) ) / 5.0f;
				break;
				
			case POSESLIDE_RELAX: /* make the current pose more like its surrounding ones */
				/* apply the value with a hard coded 6th */
				// TODO: this is not interactively modifiable!
				vec[ch]= ( ((sVal * w2) + (eVal * w1)) + (vec[ch] * 5.0f) ) / 6.0f;
				break;
				
			case POSESLIDE_BREAKDOWN: /* make the current pose slide around between the endpoints */
				/* perform simple linear interpolation - coefficient for start must come from pso->percentage... */
				vec[ch]= ((sVal * w2) + (eVal * w1));
				break;
		}
		
	}
	
	/* free the temp path we got */
	MEM_freeN(path);
}

/* helper for apply() - perform sliding for quaternion rotations (using quat blending) */
static void pose_slide_apply_quat (tPoseSlideOp *pso, tPChanFCurveLink *pfl)
{
	// TODO: this is quite evil stuff...
#if 0 // XXX port...
	/* get 2 quats */
	quat_prev[0] = eval_icu(icu_w, frame_prev);
	quat_prev[1] = eval_icu(icu_x, frame_prev);
	quat_prev[2] = eval_icu(icu_y, frame_prev);
	quat_prev[3] = eval_icu(icu_z, frame_prev);
	
	quat_next[0] = eval_icu(icu_w, frame_next);
	quat_next[1] = eval_icu(icu_x, frame_next);
	quat_next[2] = eval_icu(icu_y, frame_next);
	quat_next[3] = eval_icu(icu_z, frame_next);
	
#if 0
	/* apply the setting, completely smooth */
	QuatInterpol(pchan->quat, quat_prev, quat_next, (framef-frame_prev) / (frame_next-frame_prev) );
#else
	/* tricky interpolation */
	QuatInterpol(quat_interp, quat_prev, quat_next, (framef-frame_prev) / (frame_next-frame_prev) );
	QUATCOPY(quat_orig, pchan->quat);
	QuatInterpol(pchan->quat, quat_orig, quat_interp, 1.0f/6.0f);
	/* done */
#endif
#endif
}

/* helper for apply() - perform autokeyframing */
static void pose_slide_autoKeyframe (bContext *C, tPoseSlideOp *pso, bPoseChannel *pchan, KeyingSet *ks)
{
	/* insert keyframes as necessary if autokeyframing */
	if (autokeyframe_cfra_can_key(pso->scene, &pso->ob->id)) {
		bCommonKeySrc cks;
		ListBase dsources = {&cks, &cks};
		
		/* init common-key-source for use by KeyingSets */
		// TODO: for now, we don't clear it out, since it should be safe to do so...
		//memset(&cks, 0, sizeof(bCommonKeySrc));
		cks.id= &pso->ob->id;
		
		/* init cks for this PoseChannel, then use the relative KeyingSets to keyframe it */
		cks.pchan= pchan;
		
		/* insert keyframes */
		modify_keyframes(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)pso->cframe);
	}
}

/* apply() - perform the pose sliding based on weighting various poses */
static void pose_slide_apply (bContext *C, wmOperator *op, tPoseSlideOp *pso)
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
			/* insert keyframes if needed */
			pose_slide_autoKeyframe(C, pso, pchan, pso->ks_loc);
		}
		
		if (pchan->flag & POSE_SIZE) {
			/* calculate these for the 'scale' vector, and use scale curves */
			pose_slide_apply_vec3(pso, pfl, pchan->size, "scale");
			/* insert keyframes if needed */
			pose_slide_autoKeyframe(C, pso, pchan, pso->ks_scale);
		}
		
		if (pchan->flag & POSE_ROT) {
			/* everything depends on the rotation mode */
			if (pchan->rotmode > 0) {
				/* eulers - so calculate these for the 'eul' vector, and use euler_rotation curves */
				pose_slide_apply_vec3(pso, pfl, pchan->eul, "euler_rotation");
			}
			else if (pchan->rotmode == PCHAN_ROT_AXISANGLE) {
				// TODO: need to figure out how to do this!
			}
			else {
				/* quaternions - use quaternion blending */
				pose_slide_apply_quat(pso, pfl);
			}
			
			/* insert keyframes if needed */
			pose_slide_autoKeyframe(C, pso, pchan, pso->ks_rot);
		}
	}
	
	/* old optimize trick... this enforces to bypass the depgraph 
	 *	- note: code copied from transform_generics.c -> recalcData()
	 */
	// FIXME: shouldn't this use the builtin stuff?
	if ((pso->arm->flag & ARM_DELAYDEFORM)==0)
		DAG_id_flush_update(&pso->ob->id, OB_RECALC_DATA);  /* sets recalc flags */
	else
		where_is_pose(pso->scene, pso->ob);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, pso->ob);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
}

/* ------------------------------------ */

/* common code for invoke() methods */
static int pose_slide_invoke_common (bContext *C, wmOperator *op, tPoseSlideOp *pso)
{
	tPChanFCurveLink *pfl;
	AnimData *adt= pso->ob->adt;
	
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
		
		/* firstly, check if the current frame is a keyframe... */
		ak= cfra_find_actkeycolumn(pso->keys.root, pso->cframe);
		
		if (ak == NULL) {
			/* current frame is not a keyframe, so search */
			ActKeyColumn *pk= cfra_find_nearest_next_ak(pso->keys.root, pso->cframe, 0);
			ActKeyColumn *nk= cfra_find_nearest_next_ak(pso->keys.root, pso->cframe, 1);
			
			/* check if we found good keyframes */
			if ((pk == nk) && (pk != NULL)) {
				if (pk->cfra < pso->cframe)
					nk= nk->next;
				else if (nk->cfra > pso->cframe)
					pk= pk->prev;
			}
			
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
		BKE_report(op->reports, RPT_ERROR, "No keyframes to slide between.");
		return OPERATOR_CANCELLED;
	}
	
	// FIXME: for now, just do modal for breakdowns... 
	if (pso->mode == POSESLIDE_BREAKDOWN) {	
		/* initial apply for operator... */
		pose_slide_apply(C, op, pso);
		
		/* add a modal handler for this operator */
		WM_event_add_modal_handler(C, op);
		return OPERATOR_RUNNING_MODAL;
	}
	else {
		/* temp static operator code... until a way to include percentage in the formulation comes up */
		pose_slide_apply(C, op, pso);
		pose_slide_exit(C, op);
		return OPERATOR_FINISHED;
	}
}

/* common code for modal() */
static int pose_slide_modal (bContext *C, wmOperator *op, wmEvent *evt)
{
	tPoseSlideOp *pso= op->customdata;
	
	switch (evt->type) {
		case LEFTMOUSE:	/* confirm */
			pose_slide_exit(C, op);
			return OPERATOR_FINISHED;
		
		case ESCKEY:	/* cancel */
		case RIGHTMOUSE: 
			pose_slide_exit(C, op);
			return OPERATOR_CANCELLED;
			
		case MOUSEMOVE: /* calculate new position */
		{
			/* calculate percentage based on position of mouse (we only use x-axis for now.
			 * since this is more conveninent for users to do), and store new percentage value 
			 */
			pso->percentage= (evt->x - pso->ar->winrct.xmin) / ((float)pso->ar->winx);
			RNA_float_set(op->ptr, "percentage", pso->percentage);
			
			/* apply... */
			pose_slide_apply(C, op, pso);
		}
			break;
	}
	
	/* still running... */
	return OPERATOR_RUNNING_MODAL;
}

/* common code for cancel() */
static int pose_slide_cancel (bContext *C, wmOperator *op)
{
	/* cleanup and done */
	pose_slide_exit(C, op);
	return OPERATOR_CANCELLED;
}

/* common code for exec() methods */
static int pose_slide_exec_common (bContext *C, wmOperator *op, tPoseSlideOp *pso)
{
	/* settings should have been set up ok for applying, so just apply! */
	pose_slide_apply(C, op, pso);
	
	/* cleanup and done */
	pose_slide_exit(C, op);
	
	return OPERATOR_FINISHED;
}

/* common code for defining RNA properties */
static void pose_slide_opdef_properties (wmOperatorType *ot)
{
	RNA_def_int(ot->srna, "prev_frame", 0, MINAFRAME, MAXFRAME, "Previous Keyframe", "Frame number of keyframe immediately before the current frame.", 0, 50);
	RNA_def_int(ot->srna, "next_frame", 0, MINAFRAME, MAXFRAME, "Next Keyframe", "Frame number of keyframe immediately after the current frame.", 0, 50);
	RNA_def_float_percentage(ot->srna, "percentage", 0.5f, 0.0f, 1.0f, "Percentage", "Weighting factor for the sliding operation", 0.3, 0.7);
}

/* ------------------------------------ */

/* invoke() - for 'push' mode */
static int pose_slide_push_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	tPoseSlideOp *pso;
	
	/* initialise data  */
	if (pose_slide_init(C, op, POSESLIDE_PUSH) == 0) {
		pose_slide_exit(C, op);
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
	
	/* initialise data (from RNA-props) */
	if (pose_slide_init(C, op, POSESLIDE_PUSH) == 0) {
		pose_slide_exit(C, op);
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
	//ot->modal= pose_slide_modal;
	//ot->cancel= pose_slide_cancel;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;//|OPTYPE_BLOCKING;
	
	/* Properties */
	pose_slide_opdef_properties(ot);
}

/* ........................ */

/* invoke() - for 'relax' mode */
static int pose_slide_relax_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	tPoseSlideOp *pso;
	
	/* initialise data  */
	if (pose_slide_init(C, op, POSESLIDE_RELAX) == 0) {
		pose_slide_exit(C, op);
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
	
	/* initialise data (from RNA-props) */
	if (pose_slide_init(C, op, POSESLIDE_RELAX) == 0) {
		pose_slide_exit(C, op);
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
	ot->description= "Make the current pose more similar to its surrounding ones.";
	
	/* callbacks */
	ot->exec= pose_slide_relax_exec;
	ot->invoke= pose_slide_relax_invoke;
	//ot->modal= pose_slide_modal;
	//ot->cancel= pose_slide_cancel;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;//|OPTYPE_BLOCKING;
	
	/* Properties */
	pose_slide_opdef_properties(ot);
}

/* ........................ */

/* invoke() - for 'breakdown' mode */
static int pose_slide_breakdown_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	tPoseSlideOp *pso;
	
	/* initialise data  */
	if (pose_slide_init(C, op, POSESLIDE_BREAKDOWN) == 0) {
		pose_slide_exit(C, op);
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
	
	/* initialise data (from RNA-props) */
	if (pose_slide_init(C, op, POSESLIDE_BREAKDOWN) == 0) {
		pose_slide_exit(C, op);
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
	ot->description= "Create a suitable breakdown pose on the current frame.";
	
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
