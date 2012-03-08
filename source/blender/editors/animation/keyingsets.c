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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/keyingsets.c
 *  \ingroup edanimation
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
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BKE_main.h"
#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_report.h"

#include "ED_keyframing.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

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

static int add_default_keyingset_exec (bContext *C, wmOperator *UNUSED(op))
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
	else if (scene->active_keyingset < 0) {
		BKE_report(op->reports, RPT_ERROR, "Cannot remove built in Keying Set");
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
	ksp->flag= KSP_FLAG_WHOLE_ARRAY;
	
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
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks = NULL;
	PropertyRNA *prop= NULL;
	PointerRNA ptr= {{NULL}};
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
		
		if (IS_AUTOKEY_FLAG(scene, XYZ2RGB)) 
			keyingflag |= INSERTKEY_XYZ2RGB;
			
		/* call the API func, and set the active keyingset index */
		ks= BKE_keyingset_add(&scene->keyingsets, "ButtonKeyingSet", flag, keyingflag);
		
		scene->active_keyingset= BLI_countlist(&scene->keyingsets);
	}
	else if (scene->active_keyingset < 0) {
		BKE_report(op->reports, RPT_ERROR, "Cannot add property to built in Keying Set");
		return OPERATOR_CANCELLED;
	}
	else
		ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	
	/* try to add to keyingset using property retrieved from UI */
	uiContextActiveProperty(C, &ptr, &prop, &index);
	
	/* check if property is able to be added */
	if (ptr.id.data && ptr.data && prop && RNA_property_animateable(&ptr, prop)) {
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
		DAG_ids_flush_update(bmain, 0);
		
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
	RNA_def_boolean(ot->srna, "all", 1, "All", "Add all elements of the array to a Keying Set");
}

/* Remove from KeyingSet Button Operator ------------------------ */

static int remove_keyingset_button_exec (bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks = NULL;
	PropertyRNA *prop= NULL;
	PointerRNA ptr= {{NULL}};
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
	else if (scene->active_keyingset < 0) {
		BKE_report(op->reports, RPT_ERROR, "Cannot remove property from built in Keying Set");
		return OPERATOR_CANCELLED;
	}
	else
		ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	
	/* try to add to keyingset using property retrieved from UI */
	uiContextActiveProperty(C, &ptr, &prop, &index);

	if (ptr.id.data && ptr.data && prop) {
		path= RNA_path_from_ID_to_property(&ptr, prop);
		
		if (path) {
			KS_Path *ksp;
			
			/* try to find a path matching this description */
			ksp= BKE_keyingset_find_path(ks, ptr.id.data, ks->name, path, index, KSP_GROUP_KSNAME);

			if (ksp) {
				BKE_keyingset_free_path(ks, ksp);
				success= 1;
			}
			
			/* free temp path used */
			MEM_freeN(path);
		}
	}
	
	
	if (success) {
		/* send updates */
		DAG_ids_flush_update(bmain, 0);
		
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

/* Change Active KeyingSet Operator ------------------------ */
/* This operator checks if a menu should be shown for choosing the KeyingSet to make the active one */

static int keyingset_active_menu_invoke (bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	/* call the menu, which will call this operator again, hence the canceled */
	ANIM_keying_sets_menu_setup(C, op->type->name, "ANIM_OT_keying_set_active_set");
	return OPERATOR_CANCELLED;
}

static int keyingset_active_menu_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	int type= RNA_int_get(op->ptr, "type");
	
	/* simply set the scene's active keying set index, unless the type == 0 
	 * (i.e. which happens if we want the current active to be maintained) 
	 */
	if (type)
		scene->active_keyingset= type;
		
	/* send notifiers */
	WM_event_add_notifier(C, NC_SCENE|ND_KEYINGSET, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ANIM_OT_keying_set_active_set (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Active Keying Set";
	ot->idname= "ANIM_OT_keying_set_active_set";
	
	/* callbacks */
	ot->invoke= keyingset_active_menu_invoke;
	ot->exec= keyingset_active_menu_exec; 
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* keyingset to use
	 *	- here the type is int not enum, since many of the indices here are determined dynamically
	 */
	RNA_def_int(ot->srna, "type", 0, INT_MIN, INT_MAX, "Keying Set Number", "Index (determined internally) of the Keying Set to use", 0, 1);
}

/* ******************************************* */
/* REGISTERED KEYING SETS */

/* Keying Set Type Info declarations */
static ListBase keyingset_type_infos = {NULL, NULL};

/* Built-In Keying Sets (referencing type infos)*/
ListBase builtin_keyingsets = {NULL, NULL};

/* --------------- */

/* Find KeyingSet type info given a name */
KeyingSetInfo *ANIM_keyingset_info_find_named (const char name[])
{
	/* sanity checks */
	if ((name == NULL) || (name[0] == 0))
		return NULL;
		
	/* search by comparing names */
	return BLI_findstring(&keyingset_type_infos, name, offsetof(KeyingSetInfo, idname));
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

	/* complain about missing keying sets on debug builds */
#ifndef NDEBUG
	printf("%s: '%s' not found\n", __func__, name);
#endif

	/* no matches found */
	return NULL;
}

/* --------------- */

/* Add the given KeyingSetInfo to the list of type infos, and create an appropriate builtin set too */
void ANIM_keyingset_info_register (KeyingSetInfo *ksi)
{
	KeyingSet *ks;
	
	/* create a new KeyingSet 
	 *	- inherit name and keyframing settings from the typeinfo
	 */
	ks = BKE_keyingset_add(&builtin_keyingsets, ksi->name, 1, ksi->keyingflag);
	
	/* link this KeyingSet with its typeinfo */
	memcpy(&ks->typeinfo, ksi->idname, sizeof(ks->typeinfo));
	
	/* add type-info to the list */
	BLI_addtail(&keyingset_type_infos, ksi);
}

/* Remove the given KeyingSetInfo from the list of type infos, and also remove the builtin set if appropriate */
void ANIM_keyingset_info_unregister (Main *bmain, KeyingSetInfo *ksi)
{
	KeyingSet *ks, *ksn;
	
	/* find relevant builtin KeyingSets which use this, and remove them */
	// TODO: this isn't done now, since unregister is really only used atm when we
	// reload the scripts, which kindof defeats the purpose of "builtin"?
	for (ks= builtin_keyingsets.first; ks; ks= ksn) {
		ksn = ks->next;
		
		/* remove if matching typeinfo name */
		if (strcmp(ks->typeinfo, ksi->idname) == 0) {
			Scene *scene;
			BKE_keyingset_free(ks);
			BLI_remlink(&builtin_keyingsets, ks);

			for(scene= bmain->scene.first; scene; scene= scene->id.next)
				BLI_remlink_safe(&scene->keyingsets, ks);

			MEM_freeN(ks);
		}
	}
	
	/* free the type info */
	BLI_freelinkN(&keyingset_type_infos, ksi);
}

/* --------------- */

void ANIM_keyingset_infos_exit (void)
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

/* Getters for Active/Indices ----------------------------- */

/* Get the active Keying Set for the Scene provided */
KeyingSet *ANIM_scene_get_active_keyingset (Scene *scene)
{
	/* if no scene, we've got no hope of finding the Keying Set */
	if (scene == NULL)
		return NULL;
	
	/* currently, there are several possibilities here:
	 *	-   0: no active keying set
	 *	- > 0: one of the user-defined Keying Sets, but indices start from 0 (hence the -1)
	 *	- < 0: a builtin keying set
	 */
	if (scene->active_keyingset > 0)
		return BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
	else
		return BLI_findlink(&builtin_keyingsets, (-scene->active_keyingset)-1);
}

/* Get the index of the Keying Set provided, for the given Scene */
int ANIM_scene_get_keyingset_index (Scene *scene, KeyingSet *ks)
{
	int index;
	
	/* if no KeyingSet provided, have none */
	if (ks == NULL)
		return 0;
	
	/* check if the KeyingSet exists in scene list */
	if (scene) {
		/* get index and if valid, return 
		 *	- (absolute) Scene KeyingSets are from (>= 1)
		 */
		index = BLI_findindex(&scene->keyingsets, ks);
		if (index != -1)
			return (index + 1);
	}
	
	/* still here, so try builtins list too 
	 *	- builtins are from (<= -1)
	 *	- none/invalid is (= 0)
	 */
	index = BLI_findindex(&builtin_keyingsets, ks);
	if (index != -1)
		return -(index + 1);
	else
		return 0;
}

/* Get Keying Set to use for Auto-Keyframing some transforms */
KeyingSet *ANIM_get_keyingset_for_autokeying(Scene *scene, const char *tranformKSName)
{
	/* get KeyingSet to use 
	 *	- use the active KeyingSet if defined (and user wants to use it for all autokeying), 
	 * 	  or otherwise key transforms only
	 */
	if (IS_AUTOKEY_FLAG(scene, ONLYKEYINGSET) && (scene->active_keyingset))
		return ANIM_scene_get_active_keyingset(scene);
	else if (IS_AUTOKEY_FLAG(scene, INSERTAVAIL))
		return ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_AVAILABLE_ID);
	else 
		return ANIM_builtin_keyingset_get_named(NULL, tranformKSName);
}

/* Menu of All Keying Sets ----------------------------- */

/* Dynamically populate an enum of Keying Sets */
EnumPropertyItem *ANIM_keying_sets_enum_itemf (bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), int *free)
{
	Scene *scene = CTX_data_scene(C);
	KeyingSet *ks;
	EnumPropertyItem *item= NULL, item_tmp= {0};
	int totitem= 0;
	int i= 0;

	if (C == NULL) {
		return DummyRNA_DEFAULT_items;
	}
	
	/* active Keying Set 
	 *	- only include entry if it exists
	 */
	if (scene->active_keyingset) {
		/* active Keying Set */
		item_tmp.identifier= item_tmp.name= "Active Keying Set";
		item_tmp.value= i++;
		RNA_enum_item_add(&item, &totitem, &item_tmp);
		
		/* separator */
		RNA_enum_item_add_separator(&item, &totitem);
	}
	else
		i++;
		
	/* user-defined Keying Sets 
	 *	- these are listed in the order in which they were defined for the active scene
	 */
	if (scene->keyingsets.first) {
		for (ks= scene->keyingsets.first; ks; ks= ks->next, i++) {
			if (ANIM_keyingset_context_ok_poll(C, ks)) {
				item_tmp.identifier= item_tmp.name= ks->name;
				item_tmp.value= i;
				RNA_enum_item_add(&item, &totitem, &item_tmp);
			}
		}
		
		/* separator */
		RNA_enum_item_add_separator(&item, &totitem);
	}
	
	/* builtin Keying Sets */
	i= -1;
	for (ks= builtin_keyingsets.first; ks; ks= ks->next, i--) {
		/* only show KeyingSet if context is suitable */
		if (ANIM_keyingset_context_ok_poll(C, ks)) {
			item_tmp.identifier= item_tmp.name= ks->name;
			item_tmp.value= i;
			RNA_enum_item_add(&item, &totitem, &item_tmp);
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*free= 1;

	return item;
}

/* Create (and show) a menu containing all the Keying Sets which can be used in the current context */
void ANIM_keying_sets_menu_setup (bContext *C, const char title[], const char op_name[])
{
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks;
	uiPopupMenu *pup;
	uiLayout *layout;
	int i = 0;
	
	pup= uiPupMenuBegin(C, title, ICON_NONE);
	layout= uiPupMenuLayout(pup);
	
	/* active Keying Set 
	 *	- only include entry if it exists
	 */
	if (scene->active_keyingset) {
		uiItemEnumO(layout, op_name, "Active Keying Set", ICON_NONE, "type", i++);
		uiItemS(layout);
	}
	else
		i++;
	
	/* user-defined Keying Sets 
	 *	- these are listed in the order in which they were defined for the active scene
	 */
	if (scene->keyingsets.first) {
		for (ks= scene->keyingsets.first; ks; ks=ks->next, i++) {
			if (ANIM_keyingset_context_ok_poll(C, ks))
				uiItemEnumO(layout, op_name, ks->name, ICON_NONE, "type", i);
		}
		uiItemS(layout);
	}
	
	/* builtin Keying Sets */
	i= -1;
	for (ks= builtin_keyingsets.first; ks; ks=ks->next, i--) {
		/* only show KeyingSet if context is suitable */
		if (ANIM_keyingset_context_ok_poll(C, ks))
			uiItemEnumO(layout, op_name, ks->name, ICON_NONE, "type", i);
	}
	
	uiPupMenuEnd(C, pup);
} 

/* ******************************************* */
/* KEYFRAME MODIFICATION */

/* Polling API ----------------------------------------------- */

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

/* Special 'Overrides' Iterator for Relative KeyingSets ------ */

/* 'Data Sources' for relative Keying Set 'overrides' 
 * 	- this is basically a wrapper for PointerRNA's in a linked list
 *	- do not allow this to be accessed from outside for now
 */
typedef struct tRKS_DSource {
	struct tRKS_DSource *next, *prev;
	PointerRNA ptr;		/* the whole point of this exercise! */
} tRKS_DSource;


/* Iterator used for overriding the behavior of iterators defined for 
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
	 *	- we must have somewhere to output the data
	 *	- we must have both srna+data (and with id too optionally), or id by itself only
	 */
	if (dsources == NULL)
		return;
	if (ELEM(NULL, srna, data) && (id == NULL))
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

/* Given a KeyingSet and context info, validate Keying Set's paths.
 * This is only really necessary with relative/built-in KeyingSets
 * where their list of paths is dynamically generated based on the
 * current context info.
 *
 * Returns 0 if succeeded, otherwise an error code: eModifyKey_Returns
 */
short ANIM_validate_keyingset (bContext *C, ListBase *dsources, KeyingSet *ks)
{
	/* sanity check */
	if (ks == NULL)
		return 0;
	
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
	
	/* succeeded; return 0 to tag error free */
	return 0;
} 

/* Given a KeyingSet and context info (if required), modify keyframes for the channels specified
 * by the KeyingSet. This takes into account many of the different combinations of using KeyingSets.
 * Returns the number of channels that keyframes were added to
 */
int ANIM_apply_keyingset (bContext *C, ListBase *dsources, bAction *act, KeyingSet *ks, short mode, float cfra)
{
	Scene *scene= CTX_data_scene(C);
	ReportList *reports = CTX_wm_reports(C);
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
	success = ANIM_validate_keyingset(C, dsources, ks);
	
	if (success != 0) {
		/* return error code if failed */
		return success;
	}
	
	/* apply the paths as specified in the KeyingSet now */
	for (ksp= ks->paths.first; ksp; ksp= ksp->next) { 
		int arraylen, i;
		short kflag2;
		
		/* skip path if no ID pointer is specified */
		if (ksp->id == NULL) {
			BKE_reportf(reports, RPT_WARNING,
				"Skipping path in Keying Set, as it has no ID (KS = '%s', Path = '%s'[%d])",
				ks->name, ksp->rna_path, ksp->array_index);
			continue;
		}
		
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
				success += insert_keyframe(reports, ksp->id, act, groupname, ksp->rna_path, i, cfra, kflag2);
			else if (mode == MODIFYKEY_MODE_DELETE)
				success += delete_keyframe(reports, ksp->id, act, groupname, ksp->rna_path, i, cfra, kflag2);
		}
		
		/* set recalc-flags */
		switch (GS(ksp->id->name)) {
			case ID_OB: /* Object (or Object-Related) Keyframes */
			{
				Object *ob= (Object *)ksp->id;
				
				ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME; // XXX: only object transforms only?
			}
				break;
		}
		
		/* send notifiers for updates (this doesn't require context to work!) */
		WM_main_add_notifier(NC_ANIMATION|ND_KEYFRAME|NA_EDITED, NULL);
	}
	
	/* return the number of channels successfully affected */
	return success;
}

/* ************************************************** */
