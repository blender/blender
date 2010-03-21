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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "BLI_math.h"
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
static void fcurves_to_pchan_links_get (ListBase *pfLinks, Object *ob, bAction *act, bPoseChannel *pchan)
{
	ListBase curves = {NULL, NULL};
	int transFlags = action_get_item_transforms(act, ob, pchan, &curves);
	
	pchan->flag &= ~(POSE_LOC|POSE_ROT|POSE_SIZE);
	
	/* check if any transforms found... */
	if (transFlags) {
		/* make new linkage data */
		tPChanFCurveLink *pfl= MEM_callocN(sizeof(tPChanFCurveLink), "tPChanFCurveLink");
		PointerRNA ptr;
		
		pfl->fcurves= curves;
		pfl->pchan= pchan;
		
		/* get the RNA path to this pchan - this needs to be freed! */
		RNA_pointer_create((ID *)ob, &RNA_PoseBone, pchan, &ptr);
		pfl->pchan_path= RNA_path_from_ID_to_struct(&ptr);
		
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
		// TODO: store axis-angle too?
		VECCOPY(pfl->oldloc, pchan->loc);
		VECCOPY(pfl->oldrot, pchan->eul);
		VECCOPY(pfl->oldscale, pchan->size);
		QUATCOPY(pfl->oldquat, pchan->quat);
	}
} 


/* get sets of F-Curves providing transforms for the bones in the Pose  */
// TODO: separate the inner workings out to another helper func, since we need option of whether to take selected or visible bones...
void poseAnim_mapping_get (bContext *C, ListBase *pfLinks, Object *ob, bAction *act)
{	
	/* for each Pose-Channel which gets affected, get the F-Curves for that channel 
	 * and set the relevant transform flags...
	 */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones) 
	{
		fcurves_to_pchan_links_get(pfLinks, ob, act, pchan);
	}
	CTX_DATA_END;
}

/* free F-Curve <-> PoseChannel links  */
void poseAnim_mapping_free (ListBase *pfLinks)
{
	tPChanFCurveLink *pfl, *pfln=NULL;
		
	/* free the temp pchan links and their data */
	for (pfl= pfLinks->first; pfl; pfl= pfln) {
		pfln= pfl->next;
		
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
void poseAnim_mapping_refresh (bContext *C, Scene *scene, Object *ob)
{
	bArmature *arm= (bArmature *)ob->data;
	
	/* old optimize trick... this enforces to bypass the depgraph 
	 *	- note: code copied from transform_generics.c -> recalcData()
	 */
	// FIXME: shouldn't this use the builtin stuff?
	if ((arm->flag & ARM_DELAYDEFORM)==0)
		DAG_id_flush_update(&ob->id, OB_RECALC_DATA);  /* sets recalc flags */
	else
		where_is_pose(scene, ob);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
}

/* reset changes made to current pose */
void poseAnim_mapping_reset (ListBase *pfLinks)
{
	tPChanFCurveLink *pfl;
	
	/* iterate over each pose-channel affected, restoring all channels to their original values */
	for (pfl= pfLinks->first; pfl; pfl= pfl->next) {
		bPoseChannel *pchan= pfl->pchan;
		
		/* just copy all the values over regardless of whether they changed or not */
		// TODO; include axis-angle here too?
		VECCOPY(pchan->loc, pfl->oldloc);
		VECCOPY(pchan->eul, pfl->oldrot);
		VECCOPY(pchan->size, pfl->oldscale);
		QUATCOPY(pchan->quat, pfl->oldquat);
	}
}

/* perform autokeyframing after changes were made + confirmed */
void poseAnim_mapping_autoKeyframe (bContext *C, Scene *scene, Object *ob, ListBase *pfLinks, float cframe)
{
	static short keyingsets_need_init = 1;
	static KeyingSet *ks_loc = NULL;
	static KeyingSet *ks_rot = NULL;
	static KeyingSet *ks_scale = NULL;
	
	/* get keyingsets the first time this is run? 
	 * NOTE: it should be safe to store these static, since they're currently builtin ones
	 * but maybe later this may change, in which case this code needs to be revised!
	 */
	if (keyingsets_need_init) {
		ks_loc= ANIM_builtin_keyingset_get_named(NULL, "Location");
		ks_rot= ANIM_builtin_keyingset_get_named(NULL, "Rotation");
		ks_scale= ANIM_builtin_keyingset_get_named(NULL, "Scaling");
		
		keyingsets_need_init = 0;
	}
	
	/* insert keyframes as necessary if autokeyframing */
	if (autokeyframe_cfra_can_key(scene, &ob->id)) {
		tPChanFCurveLink *pfl;
		
		/* iterate over each pose-channel affected, applying the changes */
		for (pfl= pfLinks->first; pfl; pfl= pfl->next) {
			ListBase dsources = {NULL, NULL};
			bPoseChannel *pchan= pfl->pchan;
			
			/* add datasource override for the PoseChannel so KeyingSet will do right thing */
			ANIM_relative_keyingset_add_source(&dsources, &ob->id, &RNA_PoseBone, pchan); 
			
			/* insert keyframes 
			 * 	- these keyingsets here use dsources, since we need to specify exactly which keyframes get affected
			 */
			if (pchan->flag & POSE_LOC)
				ANIM_apply_keyingset(C, &dsources, NULL, ks_loc, MODIFYKEY_MODE_INSERT, cframe);
			if (pchan->flag & POSE_ROT)
				ANIM_apply_keyingset(C, &dsources, NULL, ks_rot, MODIFYKEY_MODE_INSERT, cframe);
			if (pchan->flag & POSE_SIZE)
				ANIM_apply_keyingset(C, &dsources, NULL, ks_scale, MODIFYKEY_MODE_INSERT, cframe);
				
			/* free the temp info */
			BLI_freelistN(&dsources);
		}
	}
}

/* ------------------------- */

/* find the next F-Curve for a PoseChannel with matching path... 
 *	- path is not just the pfl rna_path, since that path doesn't have property info yet
 */
LinkData *poseAnim_mapping_getNextFCurve (ListBase *fcuLinks, LinkData *prev, char *path)
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

/* *********************************************** */  
