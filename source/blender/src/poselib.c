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
#include <stddef.h>
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
#include "DNA_userdef_types.h"

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
	action_to_keylist(act, &keys, NULL, NULL);
	
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
			val= pupmenu("PoseLib Add Current Pose%t|Add New%x1|Add New (Current Frame)%x3|Replace Existing%x2");
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
		val= pupmenu_col(menustr, 20);
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
		
		/* get frame */
		if (val == 3)
			frame= CFRA;
		else /* if (val == 1) */
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
		
		/* validate name */
		BLI_uniquename(&act->markers, marker, "Pose", offsetof(TimeMarker, name), 64);
	}	
	
	/* loop through selected posechannels, keying their pose to the action */
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		/* check if available */
		if ((pchan->bone) && (arm->layer & pchan->bone->layer)) {
			if (pchan->bone->flag & (BONE_SELECTED|BONE_ACTIVE)) {
				/* make action-channel if needed (action groups are also created) */
				achan= verify_action_channel(act, pchan->name);
				verify_pchan2achan_grouping(act, pose, pchan->name);
				
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
		val= pupmenu_col(menustr, 20);
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
					delete_icu_key(icu, i, 1);
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
	val= pupmenu_col(menustr, 20);
	if (menustr) MEM_freeN(menustr);
	
	if (val <= 0) return;
	marker= BLI_findlink(&act->markers, val-1);
	if (marker == NULL) return;
	
	/* get name of pose */
	sprintf(name, marker->name);
	if (sbutton(name, 0, sizeof(name)-1, "Name: ") == 0)
		return;
	
	/* copy name and validate it */
	BLI_strncpy(marker->name, name, sizeof(marker->name));
	BLI_uniquename(&act->markers, marker, "Pose", offsetof(TimeMarker, name), 64);
	
	/* undo and update */
	BIF_undo_push("PoseLib Rename Pose");
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWACTION, 0);
}


/* ************************************************************* */

/* Simple struct for storing settings/data for use during PoseLib preview */
typedef struct tPoseLib_PreviewData {
	ListBase backups;		/* tPoseLib_Backup structs for restoring poses */
	ListBase searchp;		/* LinkData structs storing list of poses which match the current search-string */
	
	Object *ob;				/* object to work on */
	bArmature *arm;			/* object's armature data */
	bPose *pose;			/* object's pose */
	bAction *act;			/* poselib to use */
	TimeMarker *marker;		/* 'active' pose */
	
	short state;			/* state of main loop */
	short redraw;			/* redraw/update settings during main loop */
	short flag;				/* flags for various settings */
	
	int selcount;			/* number of selected elements to work on */
	int totcount;			/* total number of elements to work on */
	
	char headerstr[200];	/* Info-text to print in header */
	
	char searchstr[64];		/* (Part of) Name to search for to filter poses that get shown */
	char searchold[64];		/* Previously set searchstr (from last loop run), so that we can detected when to rebuild searchp */
	short search_cursor;	/* position of cursor in searchstr (cursor occurs before the item at the nominated index) */
} tPoseLib_PreviewData;

/* defines for tPoseLib_PreviewData->state values */
enum {
	PL_PREVIEW_ERROR = -1,
	PL_PREVIEW_RUNNING,
	PL_PREVIEW_CONFIRM,
	PL_PREVIEW_CANCEL,
	PL_PREVIEW_RUNONCE 
};

/* defines for tPoseLib_PreviewData->redraw values */
enum {
	PL_PREVIEW_NOREDRAW = 0,
	PL_PREVIEW_REDRAWALL,
	PL_PREVIEW_REDRAWHEADER,
};

/* defines for tPoseLib_PreviewData->flag values */
enum {
	PL_PREVIEW_FIRSTTIME	= (1<<0),
	PL_PREVIEW_SHOWORIGINAL	= (1<<1)
};

/* ---------------------------- */

/* simple struct for storing backup info */
typedef struct tPoseLib_Backup {
	struct tPoseLib_Backup *next, *prev;
	
	bPoseChannel *pchan;
	bPoseChannel olddata;
} tPoseLib_Backup;

/* Makes a copy of the current pose for restoration purposes - doesn't do constraints currently */
static void poselib_backup_posecopy (tPoseLib_PreviewData *pld)
{
	bActionChannel *achan;
	bPoseChannel *pchan;
	
	/* for each posechannel that has an actionchannel in */
	for (achan= pld->act->chanbase.first; achan; achan= achan->next) {
		/* try to find posechannel */
		pchan= get_pose_channel(pld->pose, achan->name);
		
		/* backup data if available */
		if (pchan) {
			tPoseLib_Backup *plb;
			
			/* store backup */
			plb= MEM_callocN(sizeof(tPoseLib_Backup), "tPoseLib_Backup");
			
			plb->pchan= pchan;
			memcpy(&plb->olddata, plb->pchan, sizeof(bPoseChannel));
			
			BLI_addtail(&pld->backups, plb);
			
			/* mark as being affected */
			if ((pchan->bone) && (pchan->bone->flag & BONE_SELECTED))
				pld->selcount++;
			pld->totcount++;
		}
	}
}

/* Restores original pose - doesn't do constraints currently */
static void poselib_backup_restore (tPoseLib_PreviewData *pld)
{
	tPoseLib_Backup *plb;
	
	for (plb= pld->backups.first; plb; plb= plb->next) {
		memcpy(plb->pchan, &plb->olddata, sizeof(bPoseChannel));
	}
}

/* ---------------------------- */

/* Applies the appropriate stored pose from the pose-library to the current pose
 *	- assumes that a valid object, with a poselib has been supplied
 *	- gets the string to print in the header
 * 	- this code is based on the code for extract_pose_from_action in blenkernel/action.c
 */
static void poselib_apply_pose (tPoseLib_PreviewData *pld)
{
	bPose *pose= pld->pose;
	bPoseChannel *pchan;
	bAction *act= pld->act;
	bActionChannel *achan;
	IpoCurve *icu;
	int frame;
	
	if (pld->marker)
		frame= pld->marker->frame;
	else
		return;	
	
	/* start applying - only those channels which have a key at this point in time! */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		short found= 0;
		
		/* apply this achan? */
		if (achan->ipo) {
			/* find a keyframe at this frame - users may not have defined the pose on every channel, so this is necessary */
			// TODO: this may be bad for user-defined poses...
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
				
				if (pchan) {	
					short ok= 0;
					
					if (pchan->bone) {
						if ( (pchan->bone->flag & (BONE_SELECTED|BONE_ACTIVE)) &&
							 (pchan->bone->flag & BONE_HIDDEN_P)==0 )
							ok = 1;
						else if (pld->selcount == 0)
							ok= 1;
					}
					else if (pld->selcount == 0)
						ok= 1;
					
					if (ok) {
						/* Evaluates and sets the internal ipo values	*/
						calc_ipo(achan->ipo, frame);
						/* This call also sets the pchan flags */
						execute_action_ipo(achan, pchan);
					}
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
static void poselib_keytag_pose (tPoseLib_PreviewData *pld)
{
	bPose *pose= pld->pose;
	bPoseChannel *pchan;
	bAction *act= pld->act;
	bActionChannel *achan;
	
	/* start tagging/keying */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		/* only for selected action channels */
		if (achan->flag & ACHAN_SELECTED) {
			pchan= get_pose_channel(pose, achan->name);
			
			if (pchan) {
				// TODO: use a standard autokeying function in future (to allow autokeying-editkeys to work)
				if (IS_AUTOKEY_MODE(NORMAL)) {
					ID *id= &pld->ob->id;
					
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
static void poselib_preview_get_next (tPoseLib_PreviewData *pld, int step)
{
	/* check if we no longer have search-string, but don't have any marker */
	if (pld->marker == NULL) {
		if ((step) && (pld->searchstr[0] == 0))
			pld->marker= pld->act->markers.first;
	}	
	
	/* the following operations assume that there is a starting point and direction */
	if ((pld->marker) && (step)) {
		/* search-string dictates a special approach */
		if (pld->searchstr[0]) {
			TimeMarker *marker;
			LinkData *ld, *ldn, *ldc;
			
			/* free and rebuild if needed (i.e. if search-str changed) */
			if (strcmp(pld->searchstr, pld->searchold)) {
				/* free list of temporary search matches */
				BLI_freelistN(&pld->searchp);
				
				/* generate a new list of search matches */
				for (marker= pld->act->markers.first; marker; marker= marker->next) {
					/* does the name partially match? 
					 * 	- don't worry about case, to make it easier for users to quickly input a name (or 
					 *	  part of one), which is the whole point of this feature
					 */
					if (BLI_strcasestr(marker->name, pld->searchstr)) {
						/* make link-data to store reference to it */
						ld= MEM_callocN(sizeof(LinkData), "PoseMatch");
						ld->data= marker;
						BLI_addtail(&pld->searchp, ld);
					}
				}
				
				/* set current marker to NULL (so that we start from first) */
				pld->marker= NULL;
			}
			
			/* check if any matches */
			if (pld->searchp.first == NULL) { 
				pld->marker= NULL;
				return;
			}
			
			/* find first match */
			for (ldc= pld->searchp.first; ldc; ldc= ldc->next) {
				if (ldc->data == pld->marker)
					break;
			}
			if (ldc == NULL)
				ldc= pld->searchp.first;
				
			/* Loop through the matches in a cyclic fashion, incrementing/decrementing step as appropriate 
			 * until step == 0. At this point, marker should be the correct marker.
			 */
			if (step > 0) {
				for (ld=ldc; ld && step; ld=ldn, step--)
					ldn= (ld->next) ? ld->next : pld->searchp.first;
			}
			else {
				for (ld=ldc; ld && step; ld=ldn, step++)
					ldn= (ld->prev) ? ld->prev : pld->searchp.last;
			}
			
			/* set marker */
			if (ld)
				pld->marker= ld->data;
		}
		else {
			TimeMarker *marker, *next;
			
			/* Loop through the markers in a cyclic fashion, incrementing/decrementing step as appropriate 
			 * until step == 0. At this point, marker should be the correct marker.
			 */
			if (step > 0) {
				for (marker=pld->marker; marker && step; marker=next, step--)
					next= (marker->next) ? marker->next : pld->act->markers.first;
			}
			else {
				for (marker=pld->marker; marker && step; marker=next, step++)
					next= (marker->prev) ? marker->prev : pld->act->markers.last;
			}
			
			/* it should be fairly impossible for marker to be NULL */
			if (marker)
				pld->marker= marker;
		}
	}
}

/* specially handle events for searching */
static void poselib_preview_handle_search (tPoseLib_PreviewData *pld, unsigned short event, char ascii)
{
	if (ascii) {
		/* character to add to the string */
		short index= pld->search_cursor;
		short len= (pld->searchstr[0]) ? strlen(pld->searchstr) : 0;
		short i;
		
		if (len) {
			for (i = len; i > index; i--)  
				pld->searchstr[i]= pld->searchstr[i-1];
		}
		else
			pld->searchstr[1]= 0;
			
		pld->searchstr[index]= ascii;
		pld->search_cursor++;
		
		poselib_preview_get_next(pld, 1);
		pld->redraw = PL_PREVIEW_REDRAWALL;
	}
	else {
		/* some form of string manipulation */
		switch (event) {
			case BACKSPACEKEY:
				if (pld->searchstr[0] && pld->search_cursor) {
					short len= strlen(pld->searchstr);
					short index= pld->search_cursor;
					short i;
					
					for (i = index; i <= len; i++) 
						pld->searchstr[i-1] = pld->searchstr[i];
					
					pld->search_cursor--;
					
					poselib_preview_get_next(pld, 1);
					pld->redraw = PL_PREVIEW_REDRAWALL;
				}	
				break;
				
			case DELKEY:
				if (pld->searchstr[0] && pld->searchstr[1]) {
					short len= strlen(pld->searchstr);
					short index= pld->search_cursor;
					int i;
					
					if (index < len) {
						for (i = index; i < len; i++) 
							pld->searchstr[i] = pld->searchstr[i+1];
							
						poselib_preview_get_next(pld, 1);
						pld->redraw = PL_PREVIEW_REDRAWALL;
					}
				}
				break;
		}
	}
}

/* handle events for poselib_preview_poses */
static void poselib_preview_handle_event (tPoseLib_PreviewData *pld, unsigned short event, char ascii)
{
	/* backup stuff that needs to occur before every operation
	 *	- make a copy of searchstr, so that we know if cache needs to be rebuilt
	 */
	strcpy(pld->searchold, pld->searchstr);
	
	/* if we're currently showing the original pose, only certain events are handled */
	if (pld->flag & PL_PREVIEW_SHOWORIGINAL) {
		switch (event) {
			/* exit - cancel */
			case ESCKEY:
			case RIGHTMOUSE:
				pld->state= PL_PREVIEW_CANCEL;
				break;
				
			/* exit - confirm */
			case LEFTMOUSE:
			case RETKEY:
			case PADENTER:
			case SPACEKEY:
				pld->state= PL_PREVIEW_CONFIRM;
				break;
			
			/* view manipulation */
			case MIDDLEMOUSE:
				// there's a little bug here that causes the normal header to get drawn while view is manipulated 
				handle_view_middlemouse();
				pld->redraw= PL_PREVIEW_REDRAWHEADER;
				break;
				
			/* view manipulation, or searching */
			case PAD0: case PAD1: case PAD2: case PAD3: case PAD4:
			case PAD5: case PAD6: case PAD7: case PAD8: case PAD9:
			case PADPLUSKEY: case PADMINUS:
				persptoetsen(event);
				pld->redraw= PL_PREVIEW_REDRAWHEADER;
				break;
				
			case TABKEY:
				pld->flag &= ~PL_PREVIEW_SHOWORIGINAL;
				pld->redraw= PL_PREVIEW_REDRAWALL;
				break;
		}
		
		/* EXITS HERE... */
		return;
	}
	
	/* NORMAL EVENT HANDLING... */
	/* searching takes priority over normal activity */
	switch (event) {
		/* exit - cancel */
		case ESCKEY:
		case RIGHTMOUSE:
			pld->state= PL_PREVIEW_CANCEL;
			break;
			
		/* exit - confirm */
		case LEFTMOUSE:
		case RETKEY:
		case PADENTER:
		case SPACEKEY:
			pld->state= PL_PREVIEW_CONFIRM;
			break;
			
		/* toggle between original pose and poselib pose*/
		case TABKEY:
			pld->flag |= PL_PREVIEW_SHOWORIGINAL;
			pld->redraw= PL_PREVIEW_REDRAWALL;
			break;
		
		/* change to previous pose (cyclic) */
		case PAGEUPKEY:
		case WHEELUPMOUSE:
			poselib_preview_get_next(pld, -1);
			pld->redraw= PL_PREVIEW_REDRAWALL;
			break;
		
		/* change to next pose (cyclic) */
		case PAGEDOWNKEY:
		case WHEELDOWNMOUSE:
			poselib_preview_get_next(pld, 1);
			pld->redraw= PL_PREVIEW_REDRAWALL;
			break;
		
		/* jump 5 poses (cyclic, back) */
		case DOWNARROWKEY:
			poselib_preview_get_next(pld, -5);
			pld->redraw= PL_PREVIEW_REDRAWALL;
			break;
		
		/* jump 5 poses (cyclic, forward) */
		case UPARROWKEY:
			poselib_preview_get_next(pld, 5);
			pld->redraw= PL_PREVIEW_REDRAWALL;
			break;
		
		/* change to next pose or searching cursor control */
		case RIGHTARROWKEY:
			if (pld->searchstr[0]) {
				/* move text-cursor to the right */
				if (pld->search_cursor < strlen(pld->searchstr))
					pld->search_cursor++;
				pld->redraw= PL_PREVIEW_REDRAWHEADER;
			}
			else {
				/* change to next pose (cyclic) */
				poselib_preview_get_next(pld, 1);
				pld->redraw= PL_PREVIEW_REDRAWALL;
			}
			break;
			
		/* change to next pose or searching cursor control */
		case LEFTARROWKEY:
			if (pld->searchstr[0]) {
				/* move text-cursor to the left */
				if (pld->search_cursor)
					pld->search_cursor--;
				pld->redraw= PL_PREVIEW_REDRAWHEADER;
			}
			else {
				/* change to previous pose (cyclic) */
				poselib_preview_get_next(pld, -1);
				pld->redraw= PL_PREVIEW_REDRAWALL;
			}
			break;
			
		/* change to first pose or start of searching string */
		case HOMEKEY:
			if (pld->searchstr[0]) {
				pld->search_cursor= 0;
				pld->redraw= PL_PREVIEW_REDRAWHEADER;
			}
			else {
				/* change to first pose */
				pld->marker= pld->act->markers.first;
				pld->act->active_marker= 1;
				
				pld->redraw= PL_PREVIEW_REDRAWALL;
			}
			break;
			
		/* change to last pose or start of searching string */
		case ENDKEY:
			if (pld->searchstr[0]) {
				pld->search_cursor= strlen(pld->searchstr);
				pld->redraw= PL_PREVIEW_REDRAWHEADER;
			}
			else {
				/* change to last pose */
				pld->marker= pld->act->markers.last;
				pld->act->active_marker= BLI_countlist(&pld->act->markers);
				
				pld->redraw= PL_PREVIEW_REDRAWALL;
			}
			break;
			
		/* view manipulation */
		case MIDDLEMOUSE:
			// there's a little bug here that causes the normal header to get drawn while view is manipulated 
			handle_view_middlemouse();
			pld->redraw= PL_PREVIEW_REDRAWHEADER;
			break;
			
		/* view manipulation, or searching */
		case PAD0: case PAD1: case PAD2: case PAD3: case PAD4:
		case PAD5: case PAD6: case PAD7: case PAD8: case PAD9:
		case PADPLUSKEY: case PADMINUS:
			if (pld->searchstr[0]) {
				poselib_preview_handle_search(pld, event, ascii);
			}
			else {
				persptoetsen(event);
				pld->redraw= PL_PREVIEW_REDRAWHEADER;
			}
			break;
			
		/* otherwise, assume that searching might be able to handle it */
		default:
			poselib_preview_handle_search(pld, event, ascii);
			break;
	}
}

/* ---------------------------- */

/* Init PoseLib Previewing data */
static void poselib_preview_init_data (tPoseLib_PreviewData *pld, Object *ob, short apply_active)
{
	/* clear pld first as it resides on the stack */
	memset(pld, 0, sizeof(tPoseLib_PreviewData));
	
	/* get basic data */
	pld->ob= ob;
	pld->arm= (ob) ? (ob->data) : NULL;
	pld->pose= (ob) ? (ob->pose) : NULL;
	pld->act= (ob) ? (ob->poselib) : NULL;
	pld->marker= poselib_get_active_pose(pld->act);
	
	/* check if valid poselib */
	if (ELEM3(NULL, pld->ob, pld->pose, pld->arm)) {
		error("PoseLib is only for Armatures in PoseMode");
		pld->state= PL_PREVIEW_ERROR;
		return;
	}
	if (pld->act == NULL) {
		error("Object doesn't have a valid PoseLib");
		pld->state= PL_PREVIEW_ERROR;
		return;
	}
	if (pld->marker == NULL) {
		if ((apply_active==0) && (pld->act->markers.first)) {
			/* just use first one then... */
			pld->marker= pld->act->markers.first;
			printf("PoseLib had no active pose\n");
		}
		else {
			error("PoseLib has no poses to preview/apply");
			pld->state= PL_PREVIEW_ERROR;
			return;
		}
	}
	
	/* make backups for restoring pose */
	poselib_backup_posecopy(pld);
	
	/* set flags for running */
	pld->state= (apply_active) ? PL_PREVIEW_RUNONCE : PL_PREVIEW_RUNNING;
	pld->redraw= PL_PREVIEW_REDRAWALL;
	pld->flag= PL_PREVIEW_FIRSTTIME;
	
	/* set depsgraph flags */
		/* make sure the lock is set OK, unlock can be accidentally saved? */
	pld->pose->flag |= POSE_LOCKED;
	pld->pose->flag &= ~POSE_DO_UNLOCK;
	
	/* clear strings + search */
	strcpy(pld->headerstr, "");
	strcpy(pld->searchstr, "");
	strcpy(pld->searchold, "");
	pld->search_cursor= 0;
}

/* After previewing poses */
static void poselib_preview_cleanup (tPoseLib_PreviewData *pld)
{
	Object *ob= pld->ob;
	bPose *pose= pld->pose;
	bArmature *arm= pld->arm;
	bAction *act= pld->act;
	TimeMarker *marker= pld->marker;
	
	/* this signal does one recalc on pose, then unlocks, so ESC or edit will work */
	pose->flag |= POSE_DO_UNLOCK;
	
	/* clear pose if cancelled */
	if (pld->state == PL_PREVIEW_CANCEL) {
		poselib_backup_restore(pld);
		
		/* old optimize trick... this enforces to bypass the depgraph 
		 *	- note: code copied from transform_generics.c -> recalcData()
		 */
		if ((arm->flag & ARM_DELAYDEFORM)==0)
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);  /* sets recalc flags */
		else
			where_is_pose(ob);
		
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
	}
	else if (pld->state == PL_PREVIEW_CONFIRM) {
		/* tag poses as appropriate */
		poselib_keytag_pose(pld);
		
		/* change active pose setting */
		act->active_marker= BLI_findindex(&act->markers, marker) + 1;
		action_set_activemarker(act, marker, 0);
		
		/* Update event for pose and deformation children */
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		
		/* updates */
		if (IS_AUTOKEY_MODE(NORMAL)) {
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
	BLI_freelistN(&pld->backups);
	BLI_freelistN(&pld->searchp);
}



/* This tool allows users to preview the pose from the pose-lib using the mouse-scrollwheel/pageupdown
 * It is also used to apply the active poselib pose only
 */
void poselib_preview_poses (Object *ob, short apply_active)
{
	tPoseLib_PreviewData pld;
	
	unsigned short event;
	short val=0;
	char ascii;
	
	/* check if valid poselib */
	poselib_preview_init_data(&pld, ob, apply_active);
	if (pld.state == PL_PREVIEW_ERROR)
		return;
		
	/* start preview loop */
	while (ELEM(pld.state, PL_PREVIEW_RUNNING, PL_PREVIEW_RUNONCE)) {
		/* preview a pose */
		if (pld.redraw) {
			/* only recalc pose (and its dependencies) if pose has changed */
			if (pld.redraw == PL_PREVIEW_REDRAWALL) {
				/* don't clear pose if firsttime */
				if ((pld.flag & PL_PREVIEW_FIRSTTIME)==0)
					poselib_backup_restore(&pld);
				else
					pld.flag &= ~PL_PREVIEW_FIRSTTIME;
					
				/* pose should be the right one to draw (unless we're temporarily not showing it) */
				if ((pld.flag & PL_PREVIEW_SHOWORIGINAL)==0)
					poselib_apply_pose(&pld);
				
				/* old optimize trick... this enforces to bypass the depgraph 
				 *	- note: code copied from transform_generics.c -> recalcData()
				 */
				if ((pld.arm->flag & ARM_DELAYDEFORM)==0)
					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);  /* sets recalc flags */
				else
					where_is_pose(ob);
			}
			
			/* do header print - if interactively previewing */
			if (pld.state == PL_PREVIEW_RUNNING) {
				if (pld.flag & PL_PREVIEW_SHOWORIGINAL) {
					sprintf(pld.headerstr, "PoseLib Previewing Pose: [Showing Original Pose] | Use Tab to start previewing poses again");
					headerprint(pld.headerstr);
				}
				else if (pld.searchstr[0]) {
					char tempstr[65];
					char markern[64];
					short index;
					
					/* get search-string */
					index= pld.search_cursor;
					
					if (IN_RANGE(index, 0, 64)) {
						memcpy(&tempstr[0], &pld.searchstr[0], index);
						tempstr[index]= '|';
						memcpy(&tempstr[index+1], &pld.searchstr[index], 64-index);
					}
					else {
						strncpy(tempstr, pld.searchstr, 64);
					}
					
					/* get marker name */
					if (pld.marker)
						strcpy(markern, pld.marker->name);
					else
						strcpy(markern, "No Matches");
					
					sprintf(pld.headerstr, "PoseLib Previewing Pose: Filter - [%s] | Current Pose - \"%s\"  | Use ScrollWheel or PageUp/Down to change", tempstr, markern);
					headerprint(pld.headerstr);
				}
				else {
					sprintf(pld.headerstr, "PoseLib Previewing Pose: \"%s\"  | Use ScrollWheel or PageUp/Down to change", pld.marker->name);
					headerprint(pld.headerstr);
				}
			}
			
			/* force drawing of view + clear redraw flag */
			force_draw(0);
			pld.redraw= PL_PREVIEW_NOREDRAW;
		}
		
		/* stop now if only running once */
		if (pld.state == PL_PREVIEW_RUNONCE) {
			pld.state = PL_PREVIEW_CONFIRM;
			break;
		}
		
		/* essential for idling subloop */
		if (qtest() == 0) 
			PIL_sleep_ms(2);
		
		/* emptying queue and reading events */
		while ( qtest() ) {
			event= extern_qread_ext(&val, &ascii);
			
			/* event processing */
			if (val) {
				poselib_preview_handle_event(&pld, event, ascii);
			}
		}
	}
	
	/* finish up */
	poselib_preview_cleanup(&pld);
	
	BIF_undo_push("PoseLib Apply Pose");
}
