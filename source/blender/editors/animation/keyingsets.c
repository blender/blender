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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
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
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
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

#include "anim_intern.h"

/* ************************************************** */
/* KEYING SETS - OPERATORS (for use in UI panels) */
/* These operators are really duplication of existing functionality, but just for completeness,
 * they're here too, and will give the basic data needed...
 */

/* poll callback for adding default KeyingSet */
static int keyingset_poll_default_add (bContext *C)
{
	/* as long as there's an active Scene, it's fine */
	return (CTX_data_scene(C) != NULL);
}

/* poll callback for editing active KeyingSet */
static int keyingset_poll_active_edit (bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	
	if (scene == NULL)
		return 0;
	
	/* there must be an active KeyingSet (and KeyingSets) */
	return ((scene->active_keyingset > 0) && (scene->keyingsets.first));
}

/* poll callback for editing active KeyingSet Path */
static int keyingset_poll_activePath_edit (bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks;
	
	if (scene == NULL)
		return 0;
	if (scene->active_keyingset <= 0)
		return 0;
	else
		ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	
	/* there must be an active KeyingSet and an active path */
	return ((ks) && (ks->paths.first) && (ks->active_path > 0));
}

 
/* Add a Default (Empty) Keying Set ------------------------- */

static int add_default_keyingset_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	short flag=0, keyingflag=0;
	
	/* validate flags 
	 *	- absolute KeyingSets should be created by default
	 */
	flag |= KEYINGSET_ABSOLUTE;
	
	/* 2nd arg is 0 to indicate that we don't want to include autokeying mode related settings */
	keyingflag = ANIM_get_keyframing_flags(scene, 0);
	
	/* call the API func, and set the active keyingset index */
	BKE_keyingset_add(&scene->keyingsets, NULL, flag, keyingflag);
	
	scene->active_keyingset= BLI_countlist(&scene->keyingsets);
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_SCENE|ND_KEYINGSET, NULL);
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_keying_set_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Empty Keying Set";
	ot->idname= "ANIM_OT_keying_set_add";
	ot->description= "Add a new (empty) Keying Set to the active Scene";
	
	/* callbacks */
	ot->exec= add_default_keyingset_exec;
	ot->poll= keyingset_poll_default_add;
}

/* Remove 'Active' Keying Set ------------------------- */

static int remove_active_keyingset_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks;
	
	/* verify the Keying Set to use:
	 *	- use the active one
	 *	- return error if it doesn't exist
	 */
	if (scene->active_keyingset == 0) {
		BKE_report(op->reports, RPT_ERROR, "No active Keying Set to remove");
		return OPERATOR_CANCELLED;
	}
	else
		ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	
	/* free KeyingSet's data, then remove it from the scene */
	BKE_keyingset_free(ks);
	BLI_freelinkN(&scene->keyingsets, ks);
	
	/* the active one should now be the previously second-to-last one */
	scene->active_keyingset--;
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_SCENE|ND_KEYINGSET, NULL);
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_keying_set_remove (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Removed Active Keying Set";
	ot->idname= "ANIM_OT_keying_set_remove";
	ot->description= "Remove the active Keying Set";
	
	/* callbacks */
	ot->exec= remove_active_keyingset_exec;
	ot->poll= keyingset_poll_active_edit;
}

/* Add Empty Keying Set Path ------------------------- */

static int add_empty_ks_path_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks;
	KS_Path *ksp;
	
	/* verify the Keying Set to use:
	 *	- use the active one
	 *	- return error if it doesn't exist
	 */
	if (scene->active_keyingset == 0) {
		BKE_report(op->reports, RPT_ERROR, "No active Keying Set to add empty path to");
		return OPERATOR_CANCELLED;
	}
	else
		ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	
	/* don't use the API method for this, since that checks on values... */
	ksp= MEM_callocN(sizeof(KS_Path), "KeyingSetPath Empty");
	BLI_addtail(&ks->paths, ksp);
	ks->active_path= BLI_countlist(&ks->paths);
	
	ksp->groupmode= KSP_GROUP_KSNAME; // XXX?
	ksp->idtype= ID_OB;
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_keying_set_path_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Empty Keying Set Path";
	ot->idname= "ANIM_OT_keying_set_path_add";
	ot->description= "Add empty path to active Keying Set";
	
	/* callbacks */
	ot->exec= add_empty_ks_path_exec;
	ot->poll= keyingset_poll_active_edit;
}

/* Remove Active Keying Set Path ------------------------- */

static int remove_active_ks_path_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	
	/* if there is a KeyingSet, find the nominated path to remove */
	if (ks) {
		KS_Path *ksp= BLI_findlink(&ks->paths, ks->active_path-1);
		
		if (ksp) {
			/* remove the active path from the KeyingSet */
			BKE_keyingset_free_path(ks, ksp);
			
			/* the active path should now be the previously second-to-last active one */
			ks->active_path--;
		}
		else {
			BKE_report(op->reports, RPT_ERROR, "No active Keying Set Path to remove");
			return OPERATOR_CANCELLED;
		}
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "No active Keying Set to remove a path from");
		return OPERATOR_CANCELLED;
	}
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_keying_set_path_remove (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Active Keying Set Path";
	ot->idname= "ANIM_OT_keying_set_path_remove";
	ot->description= "Remove active Path from active Keying Set";
	
	/* callbacks */
	ot->exec= remove_active_ks_path_exec;
	ot->poll= keyingset_poll_activePath_edit;
}

/* ************************************************** */
/* KEYING SETS - OPERATORS (for use in UI menus) */

/* Add to KeyingSet Button Operator ------------------------ */

static int add_keyingset_button_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks = NULL;
	PropertyRNA *prop= NULL;
	PointerRNA ptr;
	char *path = NULL;
	short success= 0;
	int index=0, pflag=0;
	int all= RNA_boolean_get(op->ptr, "all");
	
	/* verify the Keying Set to use:
	 *	- use the active one for now (more control over this can be added later)
	 *	- add a new one if it doesn't exist 
	 */
	if (scene->active_keyingset == 0) {
		short flag=0, keyingflag=0;
		
		/* validate flags 
		 *	- absolute KeyingSets should be created by default
		 */
		flag |= KEYINGSET_ABSOLUTE;
		
		keyingflag |= ANIM_get_keyframing_flags(scene, 0);
		
		if (IS_AUTOKEY_FLAG(XYZ2RGB)) 
			keyingflag |= INSERTKEY_XYZ2RGB;
			
		/* call the API func, and set the active keyingset index */
		ks= BKE_keyingset_add(&scene->keyingsets, "ButtonKeyingSet", flag, keyingflag);
		
		scene->active_keyingset= BLI_countlist(&scene->keyingsets);
	}
	else
		ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	
	/* try to add to keyingset using property retrieved from UI */
	memset(&ptr, 0, sizeof(PointerRNA));
	uiAnimContextProperty(C, &ptr, &prop, &index);
	
	/* check if property is able to be added */
	if (ptr.data && prop && RNA_property_animateable(&ptr, prop)) {
		path= RNA_path_from_ID_to_property(&ptr, prop);
		
		if (path) {
			/* set flags */
			if (all) {
				pflag |= KSP_FLAG_WHOLE_ARRAY;
				
				/* we need to set the index for this to 0, even though it may break in some cases, this is 
				 * necessary if we want the entire array for most cases to get included without the user
				 * having to worry about where they clicked
				 */
				index= 0;
			}
				
			/* add path to this setting */
			BKE_keyingset_add_path(ks, ptr.id.data, NULL, path, index, pflag, KSP_GROUP_KSNAME);
			ks->active_path= BLI_countlist(&ks->paths);
			success= 1;
			
			/* free the temp path created */
			MEM_freeN(path);
		}
	}
	
	if (success) {
		/* send updates */
		DAG_ids_flush_update(0);
		
		/* for now, only send ND_KEYS for KeyingSets */
		WM_event_add_notifier(C, NC_SCENE|ND_KEYINGSET, NULL);
	}
	
	return (success)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void ANIM_OT_keyingset_button_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add to Keying Set";
	ot->idname= "ANIM_OT_keyingset_button_add";
	
	/* callbacks */
	ot->exec= add_keyingset_button_exec; 
	//op->poll= ???
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 1, "All", "Add all elements of the array to a Keying Set.");
}

/* Remove from KeyingSet Button Operator ------------------------ */

static int remove_keyingset_button_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks = NULL;
	PropertyRNA *prop= NULL;
	PointerRNA ptr;
	char *path = NULL;
	short success= 0;
	int index=0;
	
	/* verify the Keying Set to use:
	 *	- use the active one for now (more control over this can be added later)
	 *	- return error if it doesn't exist
	 */
	if (scene->active_keyingset == 0) {
		BKE_report(op->reports, RPT_ERROR, "No active Keying Set to remove property from");
		return OPERATOR_CANCELLED;
	}
	else
		ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	
	/* try to add to keyingset using property retrieved from UI */
	memset(&ptr, 0, sizeof(PointerRNA));
	uiAnimContextProperty(C, &ptr, &prop, &index);

	if (ptr.data && prop) {
		path= RNA_path_from_ID_to_property(&ptr, prop);
		
		if (path) {
			KS_Path *ksp;
			
			/* try to find a path matching this description */
			ksp= BKE_keyingset_find_path(ks, ptr.id.data, ks->name, path, index, KSP_GROUP_KSNAME);
			
			if (ksp) {
				/* just free it... */
				MEM_freeN(ksp->rna_path);
				BLI_freelinkN(&ks->paths, ksp);
				
				success= 1;
			}
			
			/* free temp path used */
			MEM_freeN(path);
		}
	}
	
	
	if (success) {
		/* send updates */
		DAG_ids_flush_update(0);
		
		/* for now, only send ND_KEYS for KeyingSets */
		WM_event_add_notifier(C, NC_SCENE|ND_KEYINGSET, NULL);
	}
	
	return (success)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void ANIM_OT_keyingset_button_remove (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove from Keying Set";
	ot->idname= "ANIM_OT_keyingset_button_remove";
	
	/* callbacks */
	ot->exec= remove_keyingset_button_exec; 
	//op->poll= ???
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************************************* */
/* REGISTERED KEYING SETS */

/* Keying Set Type Info declarations */
ListBase keyingset_type_infos = {NULL, NULL};

/* Built-In Keying Sets (referencing type infos)*/
ListBase builtin_keyingsets = {NULL, NULL};

/* --------------- */

/* Find KeyingSet type info given a name */
KeyingSetInfo *ANIM_keyingset_info_find_named (const char name[])
{
	KeyingSetInfo *ksi;
	
	/* sanity checks */
	if ((name == NULL) || (name[0] == 0))
		return NULL;
		
	/* search by comparing names */
	for (ksi = keyingset_type_infos.first; ksi; ksi = ksi->next) {
		if (strcmp(ksi->name, name) == 0)
			return ksi;
	}
	
	/* no matches found */
	return NULL;
}

/* Find builtin KeyingSet by name */
KeyingSet *ANIM_builtin_keyingset_get_named (KeyingSet *prevKS, const char name[])
{
	KeyingSet *ks, *first=NULL;
	
	/* sanity checks  any name to check? */
	if (name[0] == 0)
		return NULL;
	
	/* get first KeyingSet to use */
	if (prevKS && prevKS->next)
		first= prevKS->next;
	else
		first= builtin_keyingsets.first;
		
	/* loop over KeyingSets checking names */
	for (ks= first; ks; ks= ks->next) {
		if (strcmp(name, ks->name) == 0)
			return ks;
	}
	
	/* no matches found */
	return NULL;
}

/* --------------- */

/* Add the given KeyingSetInfo to the list of type infos, and create an appropriate builtin set too */
void ANIM_keyingset_info_register (const bContext *C, KeyingSetInfo *ksi)
{
	Scene *scene = CTX_data_scene(C);
	ListBase *list = NULL;
	KeyingSet *ks;
	
	/* determine the KeyingSet list to include the new KeyingSet in */
	if (ksi->builtin)
		list = &builtin_keyingsets;
	else
		list = &scene->keyingsets;
	
	/* create a new KeyingSet 
	 *	- inherit name and keyframing settings from the typeinfo
	 */
	ks = BKE_keyingset_add(list, ksi->name, ksi->builtin, ksi->keyingflag);
	
	/* link this KeyingSet with its typeinfo */
	memcpy(&ks->typeinfo, ksi->name, sizeof(ks->typeinfo));
	
	/* add type-info to the list */
	BLI_addtail(&keyingset_type_infos, ksi);
}

/* Remove the given KeyingSetInfo from the list of type infos, and also remove the builtin set if appropriate */
void ANIM_keyingset_info_unregister (const bContext *C, KeyingSetInfo *ksi)
{
	Scene *scene = CTX_data_scene(C);
	KeyingSet *ks, *ksn;
	
	/* find relevant scene KeyingSets which use this, and remove them */
	for (ks= scene->keyingsets.first; ks; ks= ksn) {
		ksn = ks->next;
		
		/* remove if matching typeinfo name */
		if (strcmp(ks->typeinfo, ksi->name) == 0) {
			BKE_keyingset_free(ks);
			BLI_freelinkN(&scene->keyingsets, ks);
		}
	}
	
	/* do the same with builtin sets? */
	// TODO: this isn't done now, since unregister is really only used atm when we
	// reload the scripts, which kindof defeats the purpose of "builtin"?
	
	
	/* free the type info */
	BLI_freelinkN(&keyingset_type_infos, ksi);
}

/* --------------- */

void ANIM_keyingset_infos_exit ()
{
	KeyingSetInfo *ksi, *next;
	
	/* free type infos */
	for (ksi=keyingset_type_infos.first; ksi; ksi=next) {
		next= ksi->next;
		
		/* free extra RNA data, and remove from list */
		if (ksi->ext.free)
			ksi->ext.free(ksi->ext.data);
		BLI_freelinkN(&keyingset_type_infos, ksi);
	}
	
	/* free builtin sets */
	BKE_keyingsets_free(&builtin_keyingsets);
}

/* ******************************************* */
/* KEYING SETS API (for UI) */

/* Get the active Keying Set for the Scene provided */
KeyingSet *ANIM_scene_get_active_keyingset (Scene *scene)
{
	if (ELEM(NULL, scene, scene->keyingsets.first))
		return NULL;
	
	/* currently, there are several possibilities here:
	 *	-   0: no active keying set
	 *	- > 0: one of the user-defined Keying Sets, but indices start from 0 (hence the -1)
	 *	- < 0: a builtin keying set (XXX this isn't enabled yet so that we don't get errors on reading back files)
	 */
	if (scene->active_keyingset > 0)
		return BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	else // for now...
		return NULL; 
}

/* Check if KeyingSet can be used in the current context */
short ANIM_keyingset_context_ok_poll (bContext *C, KeyingSet *ks)
{
	if ((ks->flag & KEYINGSET_ABSOLUTE) == 0) {
		KeyingSetInfo *ksi = ANIM_keyingset_info_find_named(ks->typeinfo);
		
		/* get the associated 'type info' for this KeyingSet */
		if (ksi == NULL)
			return 0;
		// TODO: check for missing callbacks!
		
		/* check if it can be used in the current context */
		return (ksi->poll(ksi, C));
	}
	
	return 1;
}

/* ******************************************* */
/* KEYFRAME MODIFICATION */

/* Special 'Overrides' Iterator for Relative KeyingSets ------ */

/* 'Data Sources' for relative Keying Set 'overrides' 
 * 	- this is basically a wrapper for PointerRNA's in a linked list
 *	- do not allow this to be accessed from outside for now
 */
typedef struct tRKS_DSource {
	struct tRKS_DSource *next, *prev;
	PointerRNA ptr;		/* the whole point of this exercise! */
} tRKS_DSource;


/* Iterator used for overriding the behaviour of iterators defined for 
 * relative Keying Sets, with the main usage of this being operators 
 * requiring Auto Keyframing. Internal Use Only!
 */
static void RKS_ITER_overrides_list (KeyingSetInfo *ksi, bContext *C, KeyingSet *ks, ListBase *dsources)
{
	tRKS_DSource *ds;
	
	for (ds = dsources->first; ds; ds = ds->next) {
		/* run generate callback on this data */
		ksi->generate(ksi, C, ks, &ds->ptr);
	}
}

/* Add new data source for relative Keying Sets */
void ANIM_relative_keyingset_add_source (ListBase *dsources, ID *id, StructRNA *srna, void *data)
{
	tRKS_DSource *ds;
	
	/* sanity checks 
	 *	we must have at least one valid data pointer to use 
	 */
	if (ELEM(NULL, dsources, srna) || ((id == data) && (id == NULL)))
		return;

	/* allocate new elem, and add to the list */
	ds = MEM_callocN(sizeof(tRKS_DSource), "tRKS_DSource");
	BLI_addtail(dsources, ds);
	
	/* depending on what data we have, create using ID or full pointer call */
	if (srna && data)
		RNA_pointer_create(id, srna, data, &ds->ptr);
	else
		RNA_id_pointer_create(id, &ds->ptr);
}

/* KeyingSet Operations (Insert/Delete Keyframes) ------------ */

/* Given a KeyingSet and context info (if required), modify keyframes for the channels specified
 * by the KeyingSet. This takes into account many of the different combinations of using KeyingSets.
 * Returns the number of channels that keyframes were added to
 */
int ANIM_apply_keyingset (bContext *C, ListBase *dsources, bAction *act, KeyingSet *ks, short mode, float cfra)
{
	Scene *scene= CTX_data_scene(C);
	KS_Path *ksp;
	int kflag=0, success= 0;
	char *groupname= NULL;
	
	/* sanity checks */
	if (ks == NULL)
		return 0;
	
	/* get flags to use */
	if (mode == MODIFYKEY_MODE_INSERT) {
		/* use KeyingSet's flags as base */
		kflag= ks->keyingflag;
		
		/* suppliment with info from the context */
		kflag |= ANIM_get_keyframing_flags(scene, 1);
	}
	else if (mode == MODIFYKEY_MODE_DELETE)
		kflag= 0;
	
	/* if relative Keying Sets, poll and build up the paths */
	if ((ks->flag & KEYINGSET_ABSOLUTE) == 0) {
		KeyingSetInfo *ksi = ANIM_keyingset_info_find_named(ks->typeinfo);
		
		/* clear all existing paths 
		 * NOTE: BKE_keyingset_free() frees all of the paths for the KeyingSet, but not the set itself
		 */
		BKE_keyingset_free(ks);
		
		/* get the associated 'type info' for this KeyingSet */
		if (ksi == NULL)
			return MODIFYKEY_MISSING_TYPEINFO;
		// TODO: check for missing callbacks!
		
		/* check if it can be used in the current context */
		if (ksi->poll(ksi, C)) {
			/* if a list of data sources are provided, run a special iterator over them,
			 * otherwise, just continue per normal
			 */
			if (dsources) 
				RKS_ITER_overrides_list(ksi, C, ks, dsources);
			else
				ksi->iter(ksi, C, ks);
				
			/* if we don't have any paths now, then this still qualifies as invalid context */
			if (ks->paths.first == NULL)
				return MODIFYKEY_INVALID_CONTEXT;
		}
		else {
			/* poll callback tells us that KeyingSet is useless in current context */
			return MODIFYKEY_INVALID_CONTEXT;
		}
	}
	
	/* apply the paths as specified in the KeyingSet now */
	for (ksp= ks->paths.first; ksp; ksp= ksp->next) { 
		int arraylen, i;
		short kflag2;
		
		/* since keying settings can be defined on the paths too, extend the path before using it */
		kflag2 = (kflag | ksp->keyingflag);
		
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
		arraylen= i;
		
		/* get length of array if whole array option is enabled */
		if (ksp->flag & KSP_FLAG_WHOLE_ARRAY) {
			PointerRNA id_ptr, ptr;
			PropertyRNA *prop;
			
			RNA_id_pointer_create(ksp->id, &id_ptr);
			if (RNA_path_resolve(&id_ptr, ksp->rna_path, &ptr, &prop) && prop)
				arraylen= RNA_property_array_length(&ptr, prop);
		}
		
		/* we should do at least one step */
		if (arraylen == i)
			arraylen++;
		
		/* for each possible index, perform operation 
		 *	- assume that arraylen is greater than index
		 */
		for (; i < arraylen; i++) {
			/* action to take depends on mode */
			if (mode == MODIFYKEY_MODE_INSERT)
				success += insert_keyframe(ksp->id, act, groupname, ksp->rna_path, i, cfra, kflag2);
			else if (mode == MODIFYKEY_MODE_DELETE)
				success += delete_keyframe(ksp->id, act, groupname, ksp->rna_path, i, cfra, kflag2);
		}
		
		/* set recalc-flags */
		if (ksp->id) {
			switch (GS(ksp->id->name)) {
				case ID_OB: /* Object (or Object-Related) Keyframes */
				{
					Object *ob= (Object *)ksp->id;
					
					ob->recalc |= OB_RECALC;
				}
					break;
			}
			
			/* send notifiers for updates (this doesn't require context to work!) */
			WM_main_add_notifier(NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
		}
	}
	
	/* return the number of channels successfully affected */
	return success;
}

/* ************************************************** */
