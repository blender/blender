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
 * The Original Code is Copyright (C) 2007, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "DNA_listBase.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_depsgraph.h"
#include "BKE_ipo.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

//#include "BIF_keyframing.h"
#include "BSE_editipo.h"

#include "BDR_drawaction.h"
#include "BSE_time.h"

#include "BIF_poselib.h"
#include "BIF_interface.h"
#include "BIF_editaction.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"


#include "blendef.h"

#include "PIL_time.h"			/* sleep				*/
#include "mydevice.h"

/* ************************************************************* */
/* == POSE-LIBRARY TOOL FOR BLENDER == 
 *	
 * Overview: 
 * 	This tool allows animators to store a set of frequently used poses to dump into
 * 	the active action to help in "budget" productions to quickly block out new actions.
 * 	It acts as a kind of "glorified clipboard for poses", allowing for naming of poses.
 *
 * Features:
 *	- PoseLibs are simply normal Actions
 *	- Each "pose" is simply a set of keyframes that occur on a particular frame
 *		-> a set of TimeMarkers that belong to each Action, help 'label' where a 'pose' can be
 *		   found in the Action
 *	- The Scrollwheel or PageUp/Down buttons when used in a special mode or after pressing/holding
 *	  [a modifier] key, cycles through the poses available for the active pose's poselib, allowing the
 *	  animator to preview what action best suits that pose
 */
/* ************************************************************* */

/* gets list of poses in poselib as a string usable for pupmenu() */
char *poselib_build_poses_menu (bAction *act, char title[])
{
	DynStr *pupds= BLI_dynstr_new();
	TimeMarker *marker;
	char *str;
	char buf[64];
	int i;
	
	/* add title first */
	sprintf(buf, "%s%%t|", title);
	BLI_dynstr_append(pupds, buf);
	
	/* loop through markers, adding them */
	for (marker=act->markers.first, i=1; marker; marker=marker->next, i++) {
		BLI_dynstr_append(pupds, marker->name);
		
		sprintf(buf, "%%x%d", i);
		BLI_dynstr_append(pupds, buf);
		
		if (marker->next)
			BLI_dynstr_append(pupds, "|");
	}
	
	/* convert to normal MEM_malloc'd string */
	str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
	
	return str;
}

/* gets the first available frame in poselib to store a pose on 
 *	- frames start from 1, and a pose should occur on every frame... 0 is error!
 */
int poselib_get_free_index (bAction *act)
{
	TimeMarker *marker;
	int low=0, high=0;
	
	/* sanity checks */
	if (ELEM(NULL, act, act->markers.first)) return 1;
	
	/* loop over poses finding various values (poses are not stored in chronological order) */
	for (marker= act->markers.first; marker; marker= marker->next) {
		/* only increase low if value is 1 greater than low, to find "gaps" where
		 * poses were removed from the poselib
		 */
		if (marker->frame == (low + 1)) 
			low++;
		
		/* value replaces high if it is the highest value encountered yet */
		if (marker->frame > high) 
			high= marker->frame;
	}
	
	/* - if low is not equal to high, then low+1 is a gap 
	 * - if low is equal to high, then high+1 is the next index (add at end) 
	 */
	if (low < high) 
		return (low + 1);
	else 
		return (high + 1);
}

/* returns the active pose for a poselib */
TimeMarker *poselib_get_active_pose (bAction *act)
{	
	if ((act) && (act->active_marker))
		return BLI_findlink(&act->markers, act->active_marker-1);
	else
		return NULL;
}

/* ************************************************************* */

/* Initialise a new poselib (whether it is needed or not) */
bAction *poselib_init_new (Object *ob)
{
	/* sanity checks - only for armatures */
	if (ELEM(NULL, ob, ob->pose))
		return NULL;
	
	/* init object's poselib action (unlink old one if there) */
	if (ob->poselib)
		ob->poselib->id.us--;
	ob->poselib= add_empty_action("PoseLib");
	
	return ob->poselib;
}

/* Initialise a new poselib (checks if that needs to happen) */
bAction *poselib_validate (Object *ob)
{
	if (ELEM(NULL, ob, ob->pose))
		return NULL;
	else if (ob->poselib == NULL)
		return poselib_init_new(ob);
	else
		return ob->poselib;
}


/* This tool automagically generates/validates poselib data so that it corresponds to the data 
 * in the action. This is for use in making existing actions usable as poselibs.
 */
void poselib_validate_act (bAction *act)
{
	ListBase keys = {NULL, NULL};
	ActKeyColumn *ak;
	TimeMarker *marker, *markern;
	
	/* validate action and poselib */
	if (act == NULL)  {
		error("No Action to validate");
		return;
	}
	
	/* determine which frames have keys */
	action_to_keylist(act, &keys, NULL);
	
	/* for each key, make sure there is a correspnding pose */
	for (ak= keys.first; ak; ak= ak->next) {
		/* check if any pose matches this */
		for (marker= act->markers.first; marker; marker= marker->next) {
			if (IS_EQ(marker->frame, ak->cfra)) {
				marker->flag = -1;
				break;
			}
		}
		
		/* add new if none found */
		if (marker == NULL) {
			char name[64];
			
			/* add pose to poselib */
			marker= MEM_callocN(sizeof(TimeMarker), "ActionMarker");
			
			strcpy(name, "Pose");
			BLI_strncpy(marker->name, name, sizeof(marker->name));
			
			marker->frame= (int)ak->cfra;
			marker->flag= -1;
			
			BLI_addtail(&act->markers, marker);
		}
	}
	
	/* remove all untagged poses (unused), and remove all tags */
	for (marker= act->markers.first; marker; marker= markern) {
		markern= marker->next;
		
		if (marker->flag != -1)
			BLI_freelinkN(&act->markers, marker);
		else
			marker->flag = 0;
	}
	
	/* free temp memory */
	BLI_freelistN(&keys);
	
	BIF_undo_push("PoseLib Validate Action");
}

/* ************************************************************* */

/* This function adds an ipo-curve of the right type where it's needed */
static IpoCurve *poselib_verify_icu (Ipo *ipo, int adrcode)
{
	IpoCurve *icu;
	
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		if (icu->adrcode==adrcode) break;
	}
	if (icu==NULL) {
		icu= MEM_callocN(sizeof(IpoCurve), "ipocurve");
		
		icu->flag |= IPO_VISIBLE|IPO_AUTO_HORIZ;
		if (ipo->curve.first==NULL) icu->flag |= IPO_ACTIVE;	/* first one added active */
		
		icu->blocktype= ID_PO;
		icu->adrcode= adrcode;
		
		set_icu_vars(icu);
		
		BLI_addtail(&ipo->curve, icu);
	}
	
	return icu;
}

/* This tool adds the current pose to the poselib 
 *	Note: Standard insertkey cannot be used for this due to its limitations
 */
void poselib_add_current_pose (Object *ob, int val)
{
	bArmature *arm= (ob) ? ob->data : NULL;
	bPose *pose= (ob) ? ob->pose : NULL;
	bPoseChannel *pchan;
	TimeMarker *marker;
	bAction *act;
	bActionChannel *achan;
	IpoCurve *icu;
	int frame;
	char name[64];
	
	/* sanity check */
	if (ELEM3(NULL, ob, arm, pose)) 
		return;
	
	/* mode - add new or replace existing */
	if (val == 0) {
		if ((ob->poselib) && (ob->poselib->markers.first)) {
			val= pupmenu("PoseLib Add Current Pose%t|Add New%x1|Replace Existing%x2");
			if (val <= 0) return;
		}
		else 
			val= 1;
	}
	
	if ((ob->poselib) && (val == 2)) {
		char *menustr;
		
		/* get poselib */
		act= ob->poselib;
		
		/* get the pose to replace */
		menustr= poselib_build_poses_menu(act, "Replace PoseLib Pose");
		val= pupmenu(menustr);
		if (menustr) MEM_freeN(menustr);
		
		if (val <= 0) return;
		marker= BLI_findlink(&act->markers, val-1);
		if (marker == NULL) return;
		
		/* get the frame from the poselib */
		frame= marker->frame;
	}
	else {
		/* get name of pose */
		sprintf(name, "Pose");
		if (sbutton(name, 0, sizeof(name)-1, "Name: ") == 0)
			return;
			
		/* get/initialise poselib */
		act= poselib_validate(ob);
		
		/* validate name and get frame */
		frame= poselib_get_free_index(act);
		
		/* add pose to poselib - replaces any existing pose there */
		for (marker= act->markers.first; marker; marker= marker->next) {
			if (marker->frame == frame) {
				BLI_strncpy(marker->name, name, sizeof(marker->name));
				break;
			}
		}
		if (marker == NULL) {
			marker= MEM_callocN(sizeof(TimeMarker), "ActionMarker");
			
			BLI_strncpy(marker->name, name, sizeof(marker->name));
			marker->frame= frame;
			
			BLI_addtail(&act->markers, marker);
		}
	}	
	
	/* loop through selected posechannels, keying their pose to the action */
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		/* check if available */
		if ((pchan->bone) && (arm->layer & pchan->bone->layer)) {
			if (pchan->bone->flag & (BONE_SELECTED|BONE_ACTIVE)) {
				/* make action-channel if needed */
				achan= verify_action_channel(act, pchan->name);
				
				/* make ipo if needed... */
				if (achan->ipo == NULL)
					achan->ipo= add_ipo(achan->name, ID_PO);
					
				/* add missing ipo-curves and insert keys */
				#define INSERT_KEY_ICU(adrcode, data) {\
						icu= poselib_verify_icu(achan->ipo, adrcode); \
						insert_vert_icu(icu, frame, data, 1); \
					}
					
				INSERT_KEY_ICU(AC_LOC_X, pchan->loc[0])
				INSERT_KEY_ICU(AC_LOC_Y, pchan->loc[1])
				INSERT_KEY_ICU(AC_LOC_Z, pchan->loc[2])
				INSERT_KEY_ICU(AC_SIZE_X, pchan->size[0])
				INSERT_KEY_ICU(AC_SIZE_Y, pchan->size[1])
				INSERT_KEY_ICU(AC_SIZE_Z, pchan->size[2])
				INSERT_KEY_ICU(AC_QUAT_W, pchan->quat[0])
				INSERT_KEY_ICU(AC_QUAT_X, pchan->quat[1])
				INSERT_KEY_ICU(AC_QUAT_Y, pchan->quat[2])
				INSERT_KEY_ICU(AC_QUAT_Z, pchan->quat[3])
			}
		}
	}
	
	/* store new 'active' pose number */
	act->active_marker= BLI_countlist(&act->markers);
	
	BIF_undo_push("PoseLib Add Pose");
	allqueue(REDRAWBUTSEDIT, 0);
}


/* This tool removes the pose that the user selected from the poselib (or the provided pose) */
void poselib_remove_pose (Object *ob, TimeMarker *marker)
{
	bPose *pose= (ob) ? ob->pose : NULL;
	bAction *act= (ob) ? ob->poselib : NULL;
	bActionChannel *achan;
	char *menustr;
	int val;
	
	/* check if valid poselib */
	if (ELEM(NULL, ob, pose)) {
		error("PoseLib is only for Armatures in PoseMode");
		return;
	}
	if (act == NULL) {
		error("Object doesn't have PoseLib data");
		return;
	}
	
	/* get index (and pointer) of pose to remove */
	if (marker == NULL) {
		menustr= poselib_build_poses_menu(act, "Remove PoseLib Pose");
		val= pupmenu(menustr);
		if (menustr) MEM_freeN(menustr);
		
		if (val <= 0) return;
		marker= BLI_findlink(&act->markers, val-1);
		if (marker == NULL) return;
	}
	else {
		/* only continue if pose belongs to poselib */
		if (BLI_findindex(&act->markers, marker) == -1) 
			return;
	}
	
	/* remove relevant keyframes */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		Ipo *ipo= achan->ipo;
		IpoCurve *icu;
		BezTriple *bezt;
		int i;
		
		for (icu= ipo->curve.first; icu; icu= icu->next) {
			for (i=0, bezt=icu->bezt; i < icu->totvert; i++, bezt++) {
				/* check if remove... */
				if (IS_EQ(bezt->vec[1][0], marker->frame)) {
					delete_icu_key(icu, i);
					break;
				}
			}	
		}
	}
	
	/* remove poselib from list */
	BLI_freelinkN(&act->markers, marker);
	
	/* fix active pose number */
	act->active_marker= 0;
	
	/* undo + redraw */
	BIF_undo_push("PoseLib Remove Pose");
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWACTION, 0);
}


/* This tool renames the pose that the user selected from the poselib */
void poselib_rename_pose (Object *ob)
{
	bPose *pose= (ob) ? ob->pose : NULL;
	bAction *act= (ob) ? ob->poselib : NULL;
	TimeMarker *marker;
	char *menustr, name[64];
	int val;
	
	/* check if valid poselib */
	if (ELEM(NULL, ob, pose)) {
		error("PoseLib is only for Armatures in PoseMode");
		return;
	}
	if (act == NULL) {
		error("Object doesn't have a valid PoseLib");
		return;
	}
	
	/* get index of pose to remove */
	menustr= poselib_build_poses_menu(act, "Rename PoseLib Pose");
	val= pupmenu(menustr);
	if (menustr) MEM_freeN(menustr);
	
	if (val <= 0) return;
	marker= BLI_findlink(&act->markers, val-1);
	if (marker == NULL) return;
	
	/* get name of pose */
	sprintf(name, marker->name);
	if (sbutton(name, 0, sizeof(name)-1, "Name: ") == 0)
		return;
	
	/* copy name */
	BLI_strncpy(marker->name, name, sizeof(marker->name));
	
	/* undo and update */
	BIF_undo_push("PoseLib Rename Pose");
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWACTION, 0);
}


/* ************************************************************* */

/* simple struct for storing backup info */
typedef struct tPoseLib_Backup {
	struct tPoseLib_Backup *next, *prev;
	
	bPoseChannel *pchan;
	bPoseChannel olddata;
} tPoseLib_Backup;

/* Makes a copy of the current pose for restoration purposes - doesn't do constraints currently */
static void poselib_backup_posecopy (ListBase *backups, bPose *pose, bAction *act)
{
	bActionChannel *achan;
	bPoseChannel *pchan;
	
	/* for each posechannel that has an actionchannel in */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		/* try to find posechannel */
		pchan= get_pose_channel(pose, achan->name);
		
		/* backup data if available */
		if (pchan) {
			tPoseLib_Backup *plb;
			
			plb= MEM_callocN(sizeof(tPoseLib_Backup), "tPoseLib_Backup");
			
			plb->pchan= pchan;
			memcpy(&plb->olddata, plb->pchan, sizeof(bPoseChannel));
			
			BLI_addtail(backups, plb);
		}
	}
}

/* Restores original pose - doesn't do constraints currently */
static void poselib_backup_restore (ListBase *backups)
{
	tPoseLib_Backup *plb;
	
	for (plb= backups->first; plb; plb= plb->next) {
		memcpy(plb->pchan, &plb->olddata, sizeof(bPoseChannel));
	}
}


/* ---------------------------- */

/* Applies the appropriate stored pose from the pose-library to the current pose
 *	- assumes that a valid object, with a poselib has been supplied
 *	- gets the string to print in the header
 * 	- this code is based on the code for extract_pose_from_action in blenkernel/action.c
 */
static void poselib_apply_pose (Object *ob, TimeMarker *marker, char headerstr[])
{
	bPose *pose= ob->pose;
	bPoseChannel *pchan;
	bAction *act= ob->poselib;
	bActionChannel *achan;
	IpoCurve *icu;
	int frame= marker->frame;
	
	/* start applying - only those channels which have a key at this point in time! */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		short found= 0;
		
		/* apply this achan? */
		if (achan->ipo) {
			/* find a keyframe at this frame - users may not have defined the pose on every channel, so this is necessary */
			for (icu= achan->ipo->curve.first; icu; icu= icu->next) {
				BezTriple *bezt;
				int i;
				
				for (i=0, bezt=icu->bezt; i < icu->totvert; i++, bezt++) {
					if (IN_RANGE(bezt->vec[1][0], (frame-0.5f), (frame+0.5f))) {
						found= 1;
						break;
					}
				}
				
				if (found) break;
			}
			
			/* apply pose - only if posechannel selected? */
			if (found) {
				pchan= get_pose_channel(pose, achan->name);
				
				if ( (pchan) && (pchan->bone) && 
					 (pchan->bone->flag & (BONE_SELECTED|BONE_ACTIVE)) ) 
				{
					/* Evaluates and sets the internal ipo values	*/
					calc_ipo(achan->ipo, frame);
					/* This call also sets the pchan flags */
					execute_action_ipo(achan, pchan);
				}
			}
		}
		
		/* tag achan as having been used or not... */
		if (found)
			achan->flag |= ACHAN_SELECTED;
		else
			achan->flag &= ~ACHAN_SELECTED;
	}
}

/* Auto-keys/tags bones affected by the pose used from the poselib */
static void poselib_keytag_pose (Object *ob)
{
	bPoseChannel *pchan;
	bAction *act= ob->poselib;
	bActionChannel *achan;
	
	/* start tagging/keying */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		/* only for selected action channels */
		if (achan->flag & ACHAN_SELECTED) {
			pchan= get_pose_channel(ob->pose, achan->name);
			
			if (pchan) {
				if (G.flags & G_RECORDKEYS) {
					ID *id= &ob->id;
					
					/* Set keys on pose */
					if (pchan->flag & POSE_ROT) {
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_X, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Y, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Z, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_W, 0);
					}
					if (pchan->flag & POSE_SIZE) {
						insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_X, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_Y, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_Z, 0);
					}
					if (pchan->flag & POSE_LOC) {
						insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_X, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_Y, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_Z, 0);
					}
					
					/* clear any unkeyed tags */
					if (pchan->bone)
						pchan->bone->flag &= ~BONE_UNKEYED;
				}
				else {
					/* add unkeyed tags */
					if (pchan->bone)
						pchan->bone->flag |= BONE_UNKEYED;
				}
			}
		}
	}
}

/* ---------------------------- */

/* This helper function is called during poselib_preview_poses to find the 
 * pose to preview next (after a change event)
 */
static TimeMarker *poselib_preview_get_next (bAction *act, TimeMarker *current, int step)
{
	if (step) {
		TimeMarker *marker, *next;
		
		/* Loop through the markers in a cyclic fashion, incrementing/decrementing step as appropriate 
		 * until step == 0. At this point, marker should be the correct marker.
		 */
		if (step > 0) {
			for (marker=current; marker && step; marker=next, step--)
				next= (marker->next) ? marker->next : act->markers.first;
		}
		else {
			for (marker=current; marker && step; marker=next, step++)
				next= (marker->prev) ? marker->prev : act->markers.last;
		}
		
		/* don't go anywhere if for some reason an error occurred */
		return (marker) ? marker : current;
	}
	else
		return current;
}

/* ---------------------------- */

/* defines for poselib_preview_poses --> ret_val values */
enum {
	PL_PREVIEW_RUNNING = 0,
	PL_PREVIEW_CONFIRM,
	PL_PREVIEW_CANCEL,
	PL_PREVIEW_RUNONCE 
};

/* defines for poselib_preview_poses --> redraw values */
enum {
	PL_PREVIEW_NOREDRAW = 0,
	PL_PREVIEW_REDRAWALL,
	PL_PREVIEW_REDRAWHEADER,
};

/* This tool allows users to preview the pose from the pose-lib using the mouse-scrollwheel/pageupdown
 * It is also used to apply the active poselib pose only
 */
void poselib_preview_poses (Object *ob, short apply_active)
{
	ListBase backups = {NULL, NULL};
	
	bPose *pose= (ob) ? (ob->pose) : NULL;
	bArmature *arm= (ob) ? (ob->data) : NULL;
	bAction *act= (ob) ? (ob->poselib) : NULL; 
	TimeMarker *marker= poselib_get_active_pose(act);
	Base *base;
	
	short ret_val= (apply_active) ? PL_PREVIEW_RUNONCE : PL_PREVIEW_RUNNING;
	short val=0, redraw=1, firsttime=1;
	unsigned short event;
	char headerstr[200];
	
	/* check if valid poselib */
	if (ELEM3(NULL, ob, pose, arm)) {
		error("PoseLib is only for Armatures in PoseMode");
		return;
	}
	if (act == NULL) {
		error("Object doesn't have a valid PoseLib");
		return;
	}
	if (marker == NULL) {
		error("PoseLib has no poses to preview/apply");
		return;
	}
	
	/* make backup of current pose for restoring pose */
	poselib_backup_posecopy(&backups, pose, act);
	
	/* set depsgraph flags */
		/* make sure the lock is set OK, unlock can be accidentally saved? */
	pose->flag |= POSE_LOCKED;
	pose->flag &= ~POSE_DO_UNLOCK;

	
	/* start preview loop */
	while (ELEM(ret_val, PL_PREVIEW_RUNNING, PL_PREVIEW_RUNONCE)) {
		/* preview a pose */
		if (redraw) {
			/* only recalc pose (and its dependencies) if pose has changed */
			if (redraw == PL_PREVIEW_REDRAWALL) {
				/* don't clear pose if firsttime */
				if (firsttime == 0)
					poselib_backup_restore(&backups);
				else
					firsttime = 0;
					
				/* pose should be the right one to draw */
				poselib_apply_pose(ob, marker, headerstr);
				
				/* old optimize trick... this enforces to bypass the depgraph 
				 *	- note: code copied from transform_generics.c -> recalcData()
				 */
				if ((arm->flag & ARM_DELAYDEFORM)==0) {
					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);  /* sets recalc flags */
					
					/* bah, softbody exception... recalcdata doesnt reset */
					for (base= FIRSTBASE; base; base= base->next) {
						if (base->object->recalc & OB_RECALC_DATA)
							if (modifiers_isSoftbodyEnabled(base->object)) {
								base->object->softflag |= OB_SB_REDO;
						}
					}
				}
				else
					where_is_pose(ob);
			}
			
			/* do header print - if interactively previewing */
			if (ret_val == PL_PREVIEW_RUNNING) {
				sprintf(headerstr, "PoseLib Previewing Pose: \"%s\"  | Use ScrollWheel or PageUp/Down to change", marker->name);
				headerprint(headerstr);
			}
			
			/* force drawing of view + clear redraw flag */
			force_draw(0);
			redraw= PL_PREVIEW_NOREDRAW;
		}
		
		/* stop now if only running once */
		if (ret_val == PL_PREVIEW_RUNONCE) {
			ret_val = PL_PREVIEW_CONFIRM;
			break;
		}
		
		/* essential for idling subloop */
		if (qtest() == 0) 
			PIL_sleep_ms(2);
		
		/* emptying queue and reading events */
		while ( qtest() ) {
			event= extern_qread(&val);
			
			/* event processing */
			if (val) {
				switch (event) {
					/* exit - cancel */
					case ESCKEY:
					case RIGHTMOUSE:
						ret_val= PL_PREVIEW_CANCEL;
						break;
						
					/* exit - confirm */
					case LEFTMOUSE:
					case RETKEY:
					case SPACEKEY:
						ret_val= PL_PREVIEW_CONFIRM;
						break;
					
					/* change to previous pose (cyclic) */
					case PAGEUPKEY:
					case WHEELUPMOUSE:
					case RIGHTARROWKEY:
						marker= poselib_preview_get_next(act, marker, -1);
						redraw= PL_PREVIEW_REDRAWALL;
						break;
						
					/* change to next pose (cyclic) */
					case PAGEDOWNKEY:
					case WHEELDOWNMOUSE:
					case LEFTARROWKEY:
						marker= poselib_preview_get_next(act, marker, 1);
						redraw= PL_PREVIEW_REDRAWALL;
						break;
					
					/* jump 5 poses (cyclic, back) */
					case DOWNARROWKEY:
						marker= poselib_preview_get_next(act, marker, -5);
						redraw= PL_PREVIEW_REDRAWALL;
						break;
					
					/* jump 5 poses (cyclic, forward) */
					case UPARROWKEY:
						marker= poselib_preview_get_next(act, marker, 5);
						redraw= PL_PREVIEW_REDRAWALL;
						break;
						
					/* view manipulation */
					case MIDDLEMOUSE:
						// there's a little bug here that causes the normal header to get drawn while view is manipulated 
						handle_view_middlemouse();
						redraw= PL_PREVIEW_REDRAWHEADER;
						break;
						
					case PAD0: case PAD1: case PAD2: case PAD3: case PAD4:
					case PAD5: case PAD6: case PAD7: case PAD8: case PAD9:
					case PADPLUSKEY:
					case PADMINUS:
					case PADENTER:
						persptoetsen(event);
						redraw= PL_PREVIEW_REDRAWHEADER;
						break;
				}
			}
		}
	}
	
	/* this signal does one recalc on pose, then unlocks, so ESC or edit will work */
	pose->flag |= POSE_DO_UNLOCK;
	
	/* clear pose if cancelled */
	if (ret_val == PL_PREVIEW_CANCEL) {
		poselib_backup_restore(&backups);
		
		/* old optimize trick... this enforces to bypass the depgraph 
		 *	- note: code copied from transform_generics.c -> recalcData()
		 */
		if ((arm->flag & ARM_DELAYDEFORM)==0) {
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);  /* sets recalc flags */
			
			/* bah, softbody exception... recalcdata doesnt reset */
			for (base= FIRSTBASE; base; base= base->next) {
				if (base->object->recalc & OB_RECALC_DATA)
					if (modifiers_isSoftbodyEnabled(base->object)) {
						base->object->softflag |= OB_SB_REDO;
				}
			}
		}
		else
			where_is_pose(ob);
		
		allqueue(REDRAWVIEW3D, 0);
	}
	else if (ret_val == PL_PREVIEW_CONFIRM) {
		/* tag poses as appropriate */
		poselib_keytag_pose(ob);
		
		/* change active pose setting */
		act->active_marker= BLI_findindex(&act->markers, marker) + 1;
		
		/* Update event for pose and deformation children */
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		
		/* updates */
		if (G.flags & G_RECORDKEYS) {
			remake_action_ipos(ob->action);
			
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWACTION, 0);		
			allqueue(REDRAWNLA, 0);
		}
		else {
			/* need to trick depgraph, action is not allowed to execute on pose */
			where_is_pose(ob);
			ob->recalc= 0;
			
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSEDIT, 0);
		}
	}
	/* free memory used for backups */
	BLI_freelistN(&backups);
	
	BIF_undo_push("PoseLib Apply Pose");
}
