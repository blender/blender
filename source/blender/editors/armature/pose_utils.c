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

/** \file blender/editors/armature/pose_utils.c
 *  \ingroup edarmature
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_depsgraph.h"
#include "BKE_idprop.h"

#include "BKE_context.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"



#include "ED_armature.h"
#include "ED_keyframing.h"

#include "armature_intern.h"

/* *********************************************** */
/* Contents of this File:
 *
 * This file contains methods shared between Pose Slide and Pose Lib;
 * primarily the functions in question concern Animato <-> Pose 
 * convenience functions, such as applying/getting pose values
 * and/or inserting keyframes for these.
 */
/* *********************************************** */ 
/* FCurves <-> PoseChannels Links */

/* helper for poseAnim_mapping_get() -> get the relevant F-Curves per PoseChannel */
static void fcurves_to_pchan_links_get(ListBase *pfLinks, Object *ob, bAction *act, bPoseChannel *pchan)
{
	ListBase curves = {NULL, NULL};
	int transFlags = action_get_item_transforms(act, ob, pchan, &curves);
	
	pchan->flag &= ~(POSE_LOC | POSE_ROT | POSE_SIZE);
	
	/* check if any transforms found... */
	if (transFlags) {
		/* make new linkage data */
		tPChanFCurveLink *pfl = MEM_callocN(sizeof(tPChanFCurveLink), "tPChanFCurveLink");
		PointerRNA ptr;
		
		pfl->fcurves = curves;
		pfl->pchan = pchan;
		
		/* get the RNA path to this pchan - this needs to be freed! */
		RNA_pointer_create((ID *)ob, &RNA_PoseBone, pchan, &ptr);
		pfl->pchan_path = RNA_path_from_ID_to_struct(&ptr);
		
		/* add linkage data to operator data */
		BLI_addtail(pfLinks, pfl);
		
		/* set pchan's transform flags */
		if (transFlags & ACT_TRANS_LOC)
			pchan->flag |= POSE_LOC;
		if (transFlags & ACT_TRANS_ROT)
			pchan->flag |= POSE_ROT;
		if (transFlags & ACT_TRANS_SCALE)
			pchan->flag |= POSE_SIZE;
			
		/* store current transforms */
		copy_v3_v3(pfl->oldloc, pchan->loc);
		copy_v3_v3(pfl->oldrot, pchan->eul);
		copy_v3_v3(pfl->oldscale, pchan->size);
		copy_qt_qt(pfl->oldquat, pchan->quat);
		copy_v3_v3(pfl->oldaxis, pchan->rotAxis);
		pfl->oldangle = pchan->rotAngle;
		
		/* make copy of custom properties */
		if (pchan->prop && (transFlags & ACT_TRANS_PROP))
			pfl->oldprops = IDP_CopyProperty(pchan->prop);
	}
} 


/* get sets of F-Curves providing transforms for the bones in the Pose  */
void poseAnim_mapping_get(bContext *C, ListBase *pfLinks, Object *ob, bAction *act)
{	
	/* for each Pose-Channel which gets affected, get the F-Curves for that channel 
	 * and set the relevant transform flags...
	 */
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones)
	{
		fcurves_to_pchan_links_get(pfLinks, ob, act, pchan);
	}
	CTX_DATA_END;
	
	/* if no PoseChannels were found, try a second pass, doing visible ones instead
	 * i.e. if nothing selected, do whole pose
	 */
	if (BLI_listbase_is_empty(pfLinks)) {
		CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones)
		{
			fcurves_to_pchan_links_get(pfLinks, ob, act, pchan);
		}
		CTX_DATA_END;
	}
}

/* free F-Curve <-> PoseChannel links  */
void poseAnim_mapping_free(ListBase *pfLinks)
{
	tPChanFCurveLink *pfl, *pfln = NULL;
		
	/* free the temp pchan links and their data */
	for (pfl = pfLinks->first; pfl; pfl = pfln) {
		pfln = pfl->next;
		
		/* free custom properties */
		if (pfl->oldprops) {
			IDP_FreeProperty(pfl->oldprops);
			MEM_freeN(pfl->oldprops);
		}
		
		/* free list of F-Curve reference links */
		BLI_freelistN(&pfl->fcurves);
		
		/* free pchan RNA Path */
		MEM_freeN(pfl->pchan_path);
		
		/* free link itself */
		BLI_freelinkN(pfLinks, pfl);
	}
}

/* ------------------------- */

/* helper for apply() / reset() - refresh the data */
void poseAnim_mapping_refresh(bContext *C, Scene *scene, Object *ob)
{
	bArmature *arm = (bArmature *)ob->data;
	
	/* old optimize trick... this enforces to bypass the depgraph 
	 *	- note: code copied from transform_generics.c -> recalcData()
	 */
	/* FIXME: shouldn't this use the builtin stuff? */
	if ((arm->flag & ARM_DELAYDEFORM) == 0)
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);  /* sets recalc flags */
	else
		BKE_pose_where_is(scene, ob);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
}

/* reset changes made to current pose */
void poseAnim_mapping_reset(ListBase *pfLinks)
{
	tPChanFCurveLink *pfl;
	
	/* iterate over each pose-channel affected, restoring all channels to their original values */
	for (pfl = pfLinks->first; pfl; pfl = pfl->next) {
		bPoseChannel *pchan = pfl->pchan;
		
		/* just copy all the values over regardless of whether they changed or not */
		copy_v3_v3(pchan->loc, pfl->oldloc);
		copy_v3_v3(pchan->eul, pfl->oldrot);
		copy_v3_v3(pchan->size, pfl->oldscale);
		copy_qt_qt(pchan->quat, pfl->oldquat);
		copy_v3_v3(pchan->rotAxis, pfl->oldaxis);
		pchan->rotAngle = pfl->oldangle;
		
		/* just overwrite values of properties from the stored copies (there should be some) */
		if (pfl->oldprops)
			IDP_SyncGroupValues(pfl->pchan->prop, pfl->oldprops);
	}
}

/* perform autokeyframing after changes were made + confirmed */
void poseAnim_mapping_autoKeyframe(bContext *C, Scene *scene, Object *ob, ListBase *pfLinks, float cframe)
{
	/* insert keyframes as necessary if autokeyframing */
	if (autokeyframe_cfra_can_key(scene, &ob->id)) {
		KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_WHOLE_CHARACTER_ID);
		ListBase dsources = {NULL, NULL};
		tPChanFCurveLink *pfl;
		
		/* iterate over each pose-channel affected, tagging bones to be keyed */
		/* XXX: here we already have the information about what transforms exist, though 
		 * it might be easier to just overwrite all using normal mechanisms
		 */
		for (pfl = pfLinks->first; pfl; pfl = pfl->next) {
			bPoseChannel *pchan = pfl->pchan;
			
			/* add datasource override for the PoseChannel, to be used later */
			ANIM_relative_keyingset_add_source(&dsources, &ob->id, &RNA_PoseBone, pchan); 
			
			/* clear any unkeyed tags */
			if (pchan->bone)
				pchan->bone->flag &= ~BONE_UNKEYED;
		}
		
		/* insert keyframes for all relevant bones in one go */
		ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cframe);
		BLI_freelistN(&dsources);
		
		/* do the bone paths
		 *	- only do this if keyframes should have been added
		 *	- do not calculate unless there are paths already to update...
		 */
		if (ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) {
			//ED_pose_clear_paths(C, ob); // XXX for now, don't need to clear
			ED_pose_recalculate_paths(scene, ob);
		}
	}
}

/* ------------------------- */

/* find the next F-Curve for a PoseChannel with matching path... 
 *	- path is not just the pfl rna_path, since that path doesn't have property info yet
 */
LinkData *poseAnim_mapping_getNextFCurve(ListBase *fcuLinks, LinkData *prev, const char *path)
{
	LinkData *first = (prev) ? prev->next : (fcuLinks) ? fcuLinks->first : NULL;
	LinkData *ld;
	
	/* check each link to see if the linked F-Curve has a matching path */
	for (ld = first; ld; ld = ld->next) {
		FCurve *fcu = (FCurve *)ld->data;
		
		/* check if paths match */
		if (strcmp(path, fcu->rna_path) == 0)
			return ld;
	}
	
	/* none found */
	return NULL;
}

/* *********************************************** */  
