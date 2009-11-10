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
#include "BKE_modifier.h"
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

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"

#include "armature_intern.h"

/* ******* XXX ********** */

static void BIF_undo_push() {}
static void error() {}

static void action_set_activemarker() {}

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
// TODO: operatorfy me!
void poselib_validate_act (bAction *act)
{
	DLRBT_Tree keys = {NULL, NULL};
	ActKeyColumn *ak;
	TimeMarker *marker, *markern;
	
	/* validate action and poselib */
	if (act == NULL)  {
		error("No Action to validate");
		return;
	}
	
	/* determine which frames have keys */
	BLI_dlrbTree_init(&keys);
	action_to_keylist(NULL, act, &keys, NULL);
	BLI_dlrbTree_linkedlist_sync(&keys);
	
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
	BLI_freelistN((ListBase *)&keys);
	
	BIF_undo_push("PoseLib Validate Action");
}

/* ************************************************************* */

/* Pointers to the builtin KeyingSets that we want to use */
static KeyingSet *poselib_ks_locrotscale = NULL;		/* the only keyingset we'll need */

/* ----- */

static void poselib_add_menu_invoke__replacemenu (bContext *C, uiLayout *layout, void *arg)
{
	Object *ob= CTX_data_active_object(C);
	bAction *act= ob->poselib;
	TimeMarker *marker;
	
	/* set the operator execution context correctly */
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	
	/* add each marker to this menu */
	for (marker= act->markers.first; marker; marker= marker->next)
		uiItemIntO(layout, marker->name, ICON_ARMATURE_DATA, "POSELIB_OT_pose_add", "frame", marker->frame);
}

static int poselib_add_menu_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	bArmature *arm= (ob) ? ob->data : NULL;
	bPose *pose= (ob) ? ob->pose : NULL;
	uiPopupMenu *pup;
	uiLayout *layout;
	
	/* sanity check */
	if (ELEM3(NULL, ob, arm, pose)) 
		return OPERATOR_CANCELLED;
	
	/* start building */
	pup= uiPupMenuBegin(C, op->type->name, 0);
	layout= uiPupMenuLayout(pup);
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	
	/* add new (adds to the first unoccupied frame) */
	uiItemIntO(layout, "Add New", 0, "POSELIB_OT_pose_add", "frame", poselib_get_free_index(ob->poselib));
	
	/* check if we have any choices to add a new pose in any other way */
	if ((ob->poselib) && (ob->poselib->markers.first)) {
		/* add new (on current frame) */
		uiItemIntO(layout, "Add New (Current Frame)", 0, "POSELIB_OT_pose_add", "frame", CFRA);
		
		/* replace existing - submenu */
		uiItemMenuF(layout, "Replace Existing...", 0, poselib_add_menu_invoke__replacemenu, NULL);
	}
	
	uiPupMenuEnd(C, pup);
	
	/* this operator is only for a menu, not used further */
	return OPERATOR_CANCELLED;
}


static int poselib_add_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	bAction *act = poselib_validate(ob);
	bArmature *arm= (ob) ? ob->data : NULL;
	bPose *pose= (ob) ? ob->pose : NULL;
	bPoseChannel *pchan;
	TimeMarker *marker;
	int frame= RNA_int_get(op->ptr, "frame");
	char name[64];
	
	bCommonKeySrc cks;
	ListBase dsources = {&cks, &cks};
	
	/* sanity check (invoke should have checked this anyway) */
	if (ELEM3(NULL, ob, arm, pose)) 
		return OPERATOR_CANCELLED;
	
	/* get name to give to pose */
	RNA_string_get(op->ptr, "name", name);
	
	/* add pose to poselib - replaces any existing pose there
	 *	- for the 'replace' option, this should end up finding the appropriate marker,
	 *	  so no new one will be added
	 */
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
	BLI_uniquename(&act->markers, marker, "Pose", '.', offsetof(TimeMarker, name), 64);
	
	/* init common-key-source for use by KeyingSets */
	memset(&cks, 0, sizeof(bCommonKeySrc));
	cks.id= &ob->id;
	
	/* loop through selected posechannels, keying their pose to the action */
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		/* check if available */
		if ((pchan->bone) && (arm->layer & pchan->bone->layer)) {
			if (pchan->bone->flag & BONE_SELECTED || pchan->bone==arm->act_bone) {
				/* init cks for this PoseChannel, then use the relative KeyingSets to keyframe it */
				cks.pchan= pchan;
				
				/* KeyingSet to use depends on rotation mode (but that's handled by the templates code)  */
				if (poselib_ks_locrotscale == NULL)
					poselib_ks_locrotscale= ANIM_builtin_keyingset_get_named(NULL, "LocRotScale");
				modify_keyframes(scene, &dsources, act, poselib_ks_locrotscale, MODIFYKEY_MODE_INSERT, (float)frame);
			}
		}
	}
	
	/* store new 'active' pose number */
	act->active_marker= BLI_countlist(&act->markers);
	
	/* done */
	return OPERATOR_FINISHED;
}

void POSELIB_OT_pose_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "PoseLib Add Pose";
	ot->idname= "POSELIB_OT_pose_add";
	ot->description= "Add the current Pose to the active Pose Library";
	
	/* api callbacks */
	ot->invoke= poselib_add_menu_invoke;
	ot->exec= poselib_add_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "frame", 1, 0, INT_MAX, "Frame", "Frame to store pose on", 0, INT_MAX);
	RNA_def_string(ot->srna, "name", "Pose", 64, "Pose Name", "Name of newly added Pose");
}

/* ----- */

static EnumPropertyItem *poselib_stored_pose_itemf(bContext *C, PointerRNA *ptr, int *free)
{
	Object *ob= CTX_data_active_object(C);
	bAction *act= (ob) ? ob->poselib : NULL;
	TimeMarker *marker;
	EnumPropertyItem *item= NULL, item_tmp;
	int totitem= 0;
	int i= 0;

	memset(&item_tmp, 0, sizeof(item_tmp));
	
	/* check that the action exists */
	if (act) {
		/* add each marker to the list */
		for (marker=act->markers.first, i=0; marker; marker= marker->next, i++) {
			item_tmp.identifier= item_tmp.name= marker->name;
			item_tmp.icon= ICON_ARMATURE_DATA;
			item_tmp.value= i;
			RNA_enum_item_add(&item, &totitem, &item_tmp);
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*free= 1;

	return item;
}

static int poselib_remove_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bAction *act= (ob) ? ob->poselib : NULL;
	TimeMarker *marker;
	FCurve *fcu;
	
	/* check if valid poselib */
	if (act == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Object doesn't have PoseLib data");
		return OPERATOR_CANCELLED;
	}
	
	/* get index (and pointer) of pose to remove */
	marker= BLI_findlink(&act->markers, RNA_int_get(op->ptr, "pose"));
	if (marker == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Invalid index for Pose");
		return OPERATOR_CANCELLED;
	}
	
	/* remove relevant keyframes */
	for (fcu= act->curves.first; fcu; fcu= fcu->next) {
		BezTriple *bezt;
		int i;
		
		if (fcu->bezt) {
			for (i=0, bezt=fcu->bezt; i < fcu->totvert; i++, bezt++) {
				/* check if remove */
				if (IS_EQ(bezt->vec[1][0], marker->frame)) {
					delete_fcurve_key(fcu, i, 1);
					break;
				}
			}
		}
	}
	
	/* remove poselib from list */
	BLI_freelinkN(&act->markers, marker);
	
	/* fix active pose number */
	act->active_marker= 0;
	
	/* done */
	return OPERATOR_FINISHED;
}

void POSELIB_OT_pose_remove (wmOperatorType *ot)
{
	PropertyRNA *prop;
	static EnumPropertyItem prop_poses_dummy_types[] = {
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name= "PoseLib Remove Pose";
	ot->idname= "POSELIB_OT_pose_remove";
	ot->description= "Remove nth pose from the active Pose Library";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= poselib_remove_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "pose", prop_poses_dummy_types, 0, "Pose", "The pose to remove");
		RNA_def_enum_funcs(prop, poselib_stored_pose_itemf);
}

static int poselib_rename_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	Object *ob= CTX_data_active_object(C);
	bAction *act= (ob) ? ob->poselib : NULL;
	TimeMarker *marker;
	
	/* check if valid poselib */
	if (act == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Object doesn't have PoseLib data");
		return OPERATOR_CANCELLED;
	}
	
	/* get index (and pointer) of pose to remove */
	marker= BLI_findlink(&act->markers, act->active_marker-1);
	if (marker == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Invalid index for Pose");
		return OPERATOR_CANCELLED;
	}
	else {
		/* use the existing name of the marker as the name, and use the active marker as the one to rename */
		RNA_enum_set(op->ptr, "pose", act->active_marker-1);
		RNA_string_set(op->ptr, "name", marker->name);
	}
	
	/* part to sync with other similar operators... */
	return WM_operator_props_popup(C, op, evt);
}

static int poselib_rename_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bAction *act= (ob) ? ob->poselib : NULL;
	TimeMarker *marker;
	char newname[64];
	
	/* check if valid poselib */
	if (act == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Object doesn't have PoseLib data");
		return OPERATOR_CANCELLED;
	}
	
	/* get index (and pointer) of pose to remove */
	marker= BLI_findlink(&act->markers, RNA_int_get(op->ptr, "pose"));
	if (marker == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Invalid index for Pose");
		return OPERATOR_CANCELLED;
	}
	
	/* get new name */
	RNA_string_get(op->ptr, "name", newname);
	
	/* copy name and validate it */
	BLI_strncpy(marker->name, newname, sizeof(marker->name));
	BLI_uniquename(&act->markers, marker, "Pose", '.', offsetof(TimeMarker, name), 64);
	
	/* done */
	return OPERATOR_FINISHED;
}

void POSELIB_OT_pose_rename (wmOperatorType *ot)
{
	PropertyRNA *prop;
	static EnumPropertyItem prop_poses_dummy_types[] = {
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name= "PoseLib Rename Pose";
	ot->idname= "POSELIB_OT_pose_rename";
	ot->description= "Rename nth pose from the active Pose Library";
	
	/* api callbacks */
	ot->invoke= poselib_rename_invoke;
	ot->exec= poselib_rename_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "pose", prop_poses_dummy_types, 0, "Pose", "The pose to rename");
		RNA_def_enum_funcs(prop, poselib_stored_pose_itemf);
	RNA_def_string(ot->srna, "name", "RenamedPose", 64, "New Pose Name", "New name for pose");
}

/* ************************************************************* */

/* Simple struct for storing settings/data for use during PoseLib preview */
typedef struct tPoseLib_PreviewData {
	ListBase backups;		/* tPoseLib_Backup structs for restoring poses */
	ListBase searchp;		/* LinkData structs storing list of poses which match the current search-string */
	
	Scene *scene;			/* active scene */
	ScrArea *sa;			/* active area */
	
	PointerRNA rna_ptr;		/* RNA-Pointer to Object 'ob' */
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
	bActionGroup *agrp;
	bPoseChannel *pchan;
	
	/* for each posechannel that has an actionchannel in */
	for (agrp= pld->act->groups.first; agrp; agrp= agrp->next) {
		/* try to find posechannel */
		pchan= get_pose_channel(pld->pose, agrp->name);
		
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
	PointerRNA *ptr= &pld->rna_ptr;
	bArmature *arm= pld->arm;
	bPose *pose= pld->pose;
	bPoseChannel *pchan;
	bAction *act= pld->act;
	bActionGroup *agrp;
	
	BeztEditData bed;
	BeztEditFunc group_ok_cb;
	int frame= 1;
	
	/* get the frame */
	if (pld->marker)
		frame= pld->marker->frame;
	else
		return;	
	
	
	/* init settings for testing groups for keyframes */
	group_ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
	memset(&bed, 0, sizeof(BeztEditData)); 
	bed.f1= ((float)frame) - 0.5f;
	bed.f2= ((float)frame) + 0.5f;
	
	
	/* start applying - only those channels which have a key at this point in time! */
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		/* check if group has any keyframes */
		if (ANIM_animchanneldata_keys_bezier_loop(&bed, agrp, ALE_GROUP, NULL, group_ok_cb, NULL, 0)) {
			/* has keyframe on this frame, so try to get a PoseChannel with this name */
			pchan= get_pose_channel(pose, agrp->name);
			
			if (pchan) {	
				short ok= 0;
				
				/* check if this bone should get any animation applied */
				if (pld->selcount == 0) {
					/* if no bones are selected, then any bone is ok */
					ok= 1;
				}
				else if (pchan->bone) {
					/* only ok if bone is visible and selected */
					if ( (pchan->bone->flag & BONE_SELECTED || pchan->bone == arm->act_bone) &&
						 (pchan->bone->flag & BONE_HIDDEN_P)==0 &&
						 (pchan->bone->layer & arm->layer) )
						ok = 1;
				}
				
				if (ok) 
					animsys_evaluate_action_group(ptr, act, agrp, NULL, (float)frame);
			}
		}
	}
}

/* Auto-keys/tags bones affected by the pose used from the poselib */
static void poselib_keytag_pose (bContext *C, Scene *scene, tPoseLib_PreviewData *pld)
{
	bPose *pose= pld->pose;
	bPoseChannel *pchan;
	bAction *act= pld->act;
	bActionGroup *agrp;
	
	bCommonKeySrc cks;
	ListBase dsources = {&cks, &cks};
	
	/* init common-key-source for use by KeyingSets */
	memset(&cks, 0, sizeof(bCommonKeySrc));
	cks.id= &pld->ob->id;
	
	/* start tagging/keying */
	for (agrp= act->groups.first; agrp; agrp= agrp->next) {
		/* only for selected action channels */
		if (agrp->flag & AGRP_SELECTED) {
			pchan= get_pose_channel(pose, agrp->name);
			
			if (pchan) {
				// TODO: use a standard autokeying function in future (to allow autokeying-editkeys to work)
				if (IS_AUTOKEY_MODE(scene, NORMAL)) {
					/* Set keys on pose
					 *	- KeyingSet to use depends on rotation mode 
					 *	(but that's handled by the templates code)  
					 */
					// TODO: for getting the KeyingSet used, we should really check which channels were affected
					if (poselib_ks_locrotscale == NULL)
						poselib_ks_locrotscale= ANIM_builtin_keyingset_get_named(NULL, "LocRotScale");
					
					/* init cks for this PoseChannel, then use the relative KeyingSets to keyframe it */
					cks.pchan= pchan;
					
					/* now insert the keyframe */
					modify_keyframes(scene, &dsources, NULL, poselib_ks_locrotscale, MODIFYKEY_MODE_INSERT, (float)CFRA);
					
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
	
	/* send notifiers for this */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
}

/* Apply the relevant changes to the pose */
static void poselib_preview_apply (bContext *C, wmOperator *op)
{
	tPoseLib_PreviewData *pld= (tPoseLib_PreviewData *)op->customdata;
	
	/* only recalc pose (and its dependencies) if pose has changed */
	if (pld->redraw == PL_PREVIEW_REDRAWALL) {
		/* don't clear pose if firsttime */
		if ((pld->flag & PL_PREVIEW_FIRSTTIME)==0)
			poselib_backup_restore(pld);
		else
			pld->flag &= ~PL_PREVIEW_FIRSTTIME;
			
		/* pose should be the right one to draw (unless we're temporarily not showing it) */
		if ((pld->flag & PL_PREVIEW_SHOWORIGINAL)==0) {
			RNA_int_set(op->ptr, "pose_index", BLI_findindex(&pld->act->markers, pld->marker));
			poselib_apply_pose(pld);
		}
		else
			RNA_int_set(op->ptr, "pose_index", -2); /* -2 means don't apply any pose */
		
		/* old optimize trick... this enforces to bypass the depgraph 
		 *	- note: code copied from transform_generics.c -> recalcData()
		 */
		// FIXME: shouldn't this use the builtin stuff?
		if ((pld->arm->flag & ARM_DELAYDEFORM)==0)
			DAG_id_flush_update(&pld->ob->id, OB_RECALC_DATA);  /* sets recalc flags */
		else
			where_is_pose(pld->scene, pld->ob);
	}
	
	/* do header print - if interactively previewing */
	if (pld->state == PL_PREVIEW_RUNNING) {
		if (pld->flag & PL_PREVIEW_SHOWORIGINAL) {
			sprintf(pld->headerstr, "PoseLib Previewing Pose: [Showing Original Pose] | Use Tab to start previewing poses again");
			ED_area_headerprint(pld->sa, pld->headerstr);
		}
		else if (pld->searchstr[0]) {
			char tempstr[65];
			char markern[64];
			short index;
			
			/* get search-string */
			index= pld->search_cursor;
			
			if (IN_RANGE(index, 0, 64)) {
				memcpy(&tempstr[0], &pld->searchstr[0], index);
				tempstr[index]= '|';
				memcpy(&tempstr[index+1], &pld->searchstr[index], 64-index);
			}
			else {
				strncpy(tempstr, pld->searchstr, 64);
			}
			
			/* get marker name */
			if (pld->marker)
				strcpy(markern, pld->marker->name);
			else
				strcpy(markern, "No Matches");
			
			sprintf(pld->headerstr, "PoseLib Previewing Pose: Filter - [%s] | Current Pose - \"%s\"  | Use ScrollWheel or PageUp/Down to change", tempstr, markern);
			ED_area_headerprint(pld->sa, pld->headerstr);
		}
		else {
			sprintf(pld->headerstr, "PoseLib Previewing Pose: \"%s\"  | Use ScrollWheel or PageUp/Down to change", pld->marker->name);
			ED_area_headerprint(pld->sa, pld->headerstr);
		}
	}
	
	/* request drawing of view + clear redraw flag */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM|ND_POSE, pld->ob);
	pld->redraw= PL_PREVIEW_NOREDRAW;
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
	/* try doing some form of string manipulation first */
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
				return;
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
					return;
				}
			}
			break;
	}
	
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
}

/* handle events for poselib_preview_poses */
static int poselib_preview_handle_event (bContext *C, wmOperator *op, wmEvent *event)
{
	tPoseLib_PreviewData *pld= op->customdata; 
	int ret = OPERATOR_RUNNING_MODAL;
	
	/* backup stuff that needs to occur before every operation
	 *	- make a copy of searchstr, so that we know if cache needs to be rebuilt
	 */
	strcpy(pld->searchold, pld->searchstr);
	
	/* if we're currently showing the original pose, only certain events are handled */
	if (pld->flag & PL_PREVIEW_SHOWORIGINAL) {
		switch (event->type) {
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
			/* we add pass through here, so that the operators responsible for these can still run, 
			 * even though we still maintain control (as RUNNING_MODAL flag is still set too)
			 */
			case PAD0: case PAD1: case PAD2: case PAD3: case PAD4:
			case PAD5: case PAD6: case PAD7: case PAD8: case PAD9:
			case PADPLUSKEY: case PADMINUS: case MIDDLEMOUSE:
				//pld->redraw= PL_PREVIEW_REDRAWHEADER;
				ret |= OPERATOR_PASS_THROUGH;
				break;
				
			/* quicky compare to original */
			case TABKEY:
				/* only respond to one event */
				if (event->val == 0) {
					pld->flag &= ~PL_PREVIEW_SHOWORIGINAL;
					pld->redraw= PL_PREVIEW_REDRAWALL;
				}
				break;
		}
		
		/* EXITS HERE... */
		return ret;
	}
	
	/* NORMAL EVENT HANDLING... */
	/* searching takes priority over normal activity */
	switch (event->type) {
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
			/* only respond to one event */
			if (event->val == 0) {
				pld->flag |= PL_PREVIEW_SHOWORIGINAL;
				pld->redraw= PL_PREVIEW_REDRAWALL;
			}
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
		/* we add pass through here, so that the operators responsible for these can still run, 
		 * even though we still maintain control (as RUNNING_MODAL flag is still set too)
		 */
		case MIDDLEMOUSE:
			//pld->redraw= PL_PREVIEW_REDRAWHEADER;
			ret |= OPERATOR_PASS_THROUGH;
			break;
			
		/* view manipulation, or searching */
		case PAD0: case PAD1: case PAD2: case PAD3: case PAD4:
		case PAD5: case PAD6: case PAD7: case PAD8: case PAD9:
		case PADPLUSKEY: case PADMINUS:
			if (pld->searchstr[0]) {
				/* searching... */
				poselib_preview_handle_search(pld, event->type, event->ascii);
			}
			else {
				/* view manipulation (see above) */
				//pld->redraw= PL_PREVIEW_REDRAWHEADER;
				ret |= OPERATOR_PASS_THROUGH;
			}
			break;
			
		/* otherwise, assume that searching might be able to handle it */
		default:
			poselib_preview_handle_search(pld, event->type, event->ascii);
			break;
	}
	
	return ret;
}

/* ---------------------------- */

/* Init PoseLib Previewing data */
static void poselib_preview_init_data (bContext *C, wmOperator *op)
{
	tPoseLib_PreviewData *pld;
	Object *ob= CTX_data_active_object(C);
	int pose_index = RNA_int_get(op->ptr, "pose_index");
	
	/* set up preview state info */
	op->customdata= pld= MEM_callocN(sizeof(tPoseLib_PreviewData), "PoseLib Preview Data");
	
	/* get basic data */
	pld->ob= ob;
	pld->arm= (ob) ? (ob->data) : NULL;
	pld->pose= (ob) ? (ob->pose) : NULL;
	pld->act= (ob) ? (ob->poselib) : NULL;
	
	pld->scene= CTX_data_scene(C);
	pld->sa= CTX_wm_area(C);
	
	/* get starting pose based on RNA-props for this operator */
	if (pose_index == -1)
		pld->marker= poselib_get_active_pose(pld->act);
	else if (pose_index == -2)
		pld->flag |= PL_PREVIEW_SHOWORIGINAL;
	else
		pld->marker= (pld->act) ? BLI_findlink(&pld->act->markers, pose_index) : NULL;
	
	/* check if valid poselib */
	if (ELEM3(NULL, pld->ob, pld->pose, pld->arm)) {
		BKE_report(op->reports, RPT_ERROR, "PoseLib is only for Armatures in PoseMode");
		pld->state= PL_PREVIEW_ERROR;
		return;
	}
	if (pld->act == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Object doesn't have a valid PoseLib");
		pld->state= PL_PREVIEW_ERROR;
		return;
	}
	if (pld->marker == NULL) {
		if (pld->act->markers.first) {
			/* just use first one then... */
			pld->marker= pld->act->markers.first;
			if (pose_index > -2) printf("PoseLib had no active pose\n");
		}
		else {
			BKE_report(op->reports, RPT_ERROR, "PoseLib has no poses to preview/apply");
			pld->state= PL_PREVIEW_ERROR;
			return;
		}
	}
	
	/* get ID pointer for applying poses */
	RNA_id_pointer_create(&ob->id, &pld->rna_ptr);
	
	/* make backups for restoring pose */
	poselib_backup_posecopy(pld);
	
	/* set flags for running */
	pld->state= PL_PREVIEW_RUNNING;
	pld->redraw= PL_PREVIEW_REDRAWALL;
	pld->flag |= PL_PREVIEW_FIRSTTIME;
	
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
static void poselib_preview_cleanup (bContext *C, wmOperator *op)
{
	tPoseLib_PreviewData *pld= (tPoseLib_PreviewData *)op->customdata;
	Scene *scene= pld->scene;
	Object *ob= pld->ob;
	bPose *pose= pld->pose;
	bArmature *arm= pld->arm;
	bAction *act= pld->act;
	TimeMarker *marker= pld->marker;
	
	/* redraw the header so that it doesn't show any of our stuff anymore */
	ED_area_headerprint(pld->sa, NULL);
	
	/* this signal does one recalc on pose, then unlocks, so ESC or edit will work */
	pose->flag |= POSE_DO_UNLOCK;
	
	/* clear pose if cancelled */
	if (pld->state == PL_PREVIEW_CANCEL) {
		poselib_backup_restore(pld);
		
		/* old optimize trick... this enforces to bypass the depgraph 
		 *	- note: code copied from transform_generics.c -> recalcData()
		 */
		if ((arm->flag & ARM_DELAYDEFORM)==0)
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);  /* sets recalc flags */
		else
			where_is_pose(scene, ob);
		
	}
	else if (pld->state == PL_PREVIEW_CONFIRM) {
		/* tag poses as appropriate */
		poselib_keytag_pose(C, scene, pld);
		
		/* change active pose setting */
		act->active_marker= BLI_findindex(&act->markers, marker) + 1;
		action_set_activemarker(act, marker, 0);
		
		/* Update event for pose and deformation children */
		DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		
		/* updates */
		if (IS_AUTOKEY_MODE(scene, NORMAL)) {
			//remake_action_ipos(ob->action);
		}
		else {
			/* need to trick depgraph, action is not allowed to execute on pose */
			where_is_pose(scene, ob);
			ob->recalc= 0;
		}
	}
	
	/* free memory used for backups */
	BLI_freelistN(&pld->backups);
	BLI_freelistN(&pld->searchp);
	
	/* free temp data for operator */
	MEM_freeN(pld);
	op->customdata= NULL;
}

/* End previewing operation */
static int poselib_preview_exit (bContext *C, wmOperator *op)
{
	tPoseLib_PreviewData *pld= op->customdata;
	
	/* finish up */
	poselib_preview_cleanup(C, op);
	
	if (ELEM(pld->state, PL_PREVIEW_CANCEL, PL_PREVIEW_ERROR))
		return OPERATOR_CANCELLED;
	else
		return OPERATOR_FINISHED;
}

/* Cancel previewing operation (called when exiting Blender) */
static int poselib_preview_cancel (bContext *C, wmOperator *op)
{
	poselib_preview_exit(C, op);
	return OPERATOR_CANCELLED;
}

/* main modal status check */
static int poselib_preview_modal (bContext *C, wmOperator *op, wmEvent *event)
{
	tPoseLib_PreviewData *pld= op->customdata;
	int ret;
	
	/* 1) check state to see if we're still running */
	if (pld->state != PL_PREVIEW_RUNNING)
		return poselib_preview_exit(C, op);
	
	/* 2) handle events */
	ret= poselib_preview_handle_event(C, op, event);
	
	/* 3) apply changes and redraw, otherwise, confirming goes wrong */
	if (pld->redraw)
		poselib_preview_apply(C, op);
	
	return ret;
}

/* Modal Operator init */
static int poselib_preview_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	tPoseLib_PreviewData *pld;
	
	/* check if everything is ok, and init settings for modal operator */
	poselib_preview_init_data(C, op);
	pld= (tPoseLib_PreviewData *)op->customdata;
	
	if (pld->state == PL_PREVIEW_ERROR) {
		/* an error occurred, so free temp mem used */
		poselib_preview_cleanup(C, op);
		return OPERATOR_CANCELLED;
	}
	
	/* do initial apply to have something to look at */
	poselib_preview_apply(C, op);
	
	/* add temp handler if we're running as a modal operator */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* Repeat operator */
static int poselib_preview_exec (bContext *C, wmOperator *op)
{
	tPoseLib_PreviewData *pld;
	
	/* check if everything is ok, and init settings for modal operator */
	poselib_preview_init_data(C, op);
	pld= (tPoseLib_PreviewData *)op->customdata;
	
	if (pld->state == PL_PREVIEW_ERROR) {
		/* an error occurred, so free temp mem used */
		poselib_preview_cleanup(C, op);
		return OPERATOR_CANCELLED;
	}
	
	/* the exec() callback is effectively a 'run-once' scenario, so set the state to that
	 * so that everything draws correctly
	 */
	pld->state = PL_PREVIEW_RUNONCE;
	
	/* apply the active pose */
	poselib_preview_apply(C, op);
	
	/* now, set the status to exit */
	pld->state = PL_PREVIEW_CONFIRM;
	
	/* cleanup */
	return poselib_preview_exit(C, op);
}

void POSELIB_OT_browse_interactive (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "PoseLib Browse Poses";
	ot->idname= "POSELIB_OT_browse_interactive";
	ot->description= "Interactively browse poses in 3D-View";
	
	/* api callbacks */
	ot->invoke= poselib_preview_invoke;
	ot->modal= poselib_preview_modal;
	ot->cancel= poselib_preview_cancel;
	ot->exec= poselib_preview_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
	
	/* properties */	
	RNA_def_int(ot->srna, "pose_index", -1, -2, INT_MAX, "Pose", "Index of the pose to apply (-2 for no change to pose, -1 for poselib active pose)", 0, INT_MAX);
}
