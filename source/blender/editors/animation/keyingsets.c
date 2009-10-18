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
#include "BLI_arithb.h"
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
	
	if (IS_AUTOKEY_FLAG(AUTOMATKEY)) 
		keyingflag |= INSERTKEY_MATRIX;
	if (IS_AUTOKEY_FLAG(INSERTNEEDED)) 
		keyingflag |= INSERTKEY_NEEDED;
		
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
	ot->description= "Add a new (empty) Keying Set to the active Scene.";
	
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
	ot->description= "Remove the active Keying Set.";
	
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
			/* NOTE: sync this code with BKE_keyingset_free() */
			{
				/* free RNA-path info */
				MEM_freeN(ksp->rna_path);
				
				/* free path itself */
				BLI_freelinkN(&ks->paths, ksp);
			}
			
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
	ot->description= "Remove active Path from active Keying Set.";
	
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
		
		if (IS_AUTOKEY_FLAG(AUTOMATKEY)) 
			keyingflag |= INSERTKEY_MATRIX;
		if (IS_AUTOKEY_FLAG(INSERTNEEDED)) 
			keyingflag |= INSERTKEY_NEEDED;
			
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
	if (ptr.data && prop && RNA_property_animateable(ptr.data, prop)) {
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
			BKE_keyingset_add_destination(ks, ptr.id.data, NULL, path, index, pflag, KSP_GROUP_KSNAME);
			ks->active_path= BLI_countlist(&ks->paths);
			success= 1;
			
			/* free the temp path created */
			MEM_freeN(path);
		}
	}
	
	if (success) {
		/* send updates */
		ED_anim_dag_flush_update(C);	
		
		/* for now, only send ND_KEYS for KeyingSets */
		WM_event_add_notifier(C, NC_SCENE|ND_KEYINGSET, NULL);
	}
	
	return (success)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void ANIM_OT_add_keyingset_button (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add to Keying Set";
	ot->idname= "ANIM_OT_add_keyingset_button";
	
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
			ksp= BKE_keyingset_find_destination(ks, ptr.id.data, ks->name, path, index, KSP_GROUP_KSNAME);
			
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
		ED_anim_dag_flush_update(C);	
		
		/* for now, only send ND_KEYS for KeyingSets */
		WM_event_add_notifier(C, NC_SCENE|ND_KEYINGSET, NULL);
	}
	
	return (success)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void ANIM_OT_remove_keyingset_button (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove from Keying Set";
	ot->idname= "ANIM_OT_remove_keyingset_button";
	
	/* callbacks */
	ot->exec= remove_keyingset_button_exec; 
	//op->poll= ???
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************************************** */
/* KEYING SETS - EDITING API  */

/* UI API --------------------------------------------- */

/* Build menu-string of available keying-sets (allocates memory for string)
 * NOTE: mode must not be longer than 64 chars
 */
char *ANIM_build_keyingsets_menu (ListBase *list, short for_edit)
{
	DynStr *pupds= BLI_dynstr_new();
	KeyingSet *ks;
	char buf[64];
	char *str;
	int i;
	
	/* add title first */
	BLI_dynstr_append(pupds, "Keying Sets%t|");
	
	/* add dummy entries for none-active */
	if (for_edit) { 
		BLI_dynstr_append(pupds, "Add New%x-1|");
		BLI_dynstr_append(pupds, " %x0|");
	}
	else
		BLI_dynstr_append(pupds, "No Keying Set%x0|");
	
	/* loop through keyingsets, adding them */
	for (ks=list->first, i=1; ks; ks=ks->next, i++) {
		if (for_edit == 0)
			BLI_dynstr_append(pupds, "KS: ");
		
		BLI_dynstr_append(pupds, ks->name);
		BLI_snprintf( buf, 64, "%%x%d%s", i, ((ks->next)?"|":"") );
		BLI_dynstr_append(pupds, buf);
	}
	
	/* convert to normal MEM_malloc'd string */
	str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
	
	return str;
}


/* ******************************************* */
/* KEYING SETS - BUILTIN */

#if 0 // XXX old keyingsets code based on adrcodes... to be restored in due course

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
	//Object *ob= (G.obedit)? (G.obedit) : (OBACT); // XXX
	Object *ob= NULL;
	char *newname= NULL;
	
	if(ob==NULL)
		return 0;
	
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
		BLI_strncpy(ks->name, newname, sizeof(ks->name));
	
	/* if it gets here, it's ok */
	return 1;
}

/* array for object keyingset defines */
bKeyingSet defks_v3d_object[] = 
{
	/* include_cb, adrcode-getter, name, blocktype, flag, chan_num, adrcodes */
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
	{NULL, "Rot", ID_PO, COMMONKEY_PCHANROT, 1, {KAG_CHAN_EXTEND}},
	{NULL, "Scale", ID_PO, 0, 3, {AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z}},
	
	{NULL, "%l", 0, -1, 0, {0}}, // separator
	
	{NULL, "LocRot", ID_PO, COMMONKEY_PCHANROT, 4, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z,
		 KAG_CHAN_EXTEND}},
		 
	{NULL, "LocScale", ID_PO, 0, 6, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z,
		 AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z}},
		 
	{NULL, "LocRotScale", ID_PO, COMMONKEY_PCHANROT, 7, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z,AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z, 
		 KAG_CHAN_EXTEND}},
		 
	{NULL, "RotScale", ID_PO, 0, 4, 
		{AC_SIZE_X,AC_SIZE_Y,AC_SIZE_Z, 
		 KAG_CHAN_EXTEND}},
	
	{incl_non_del_keys, "%l", 0, -1, 0, {0}}, // separator
	
	{incl_non_del_keys, "VisualLoc", ID_PO, INSERTKEY_MATRIX, 3, {AC_LOC_X,AC_LOC_Y,AC_LOC_Z}},
	{incl_non_del_keys, "VisualRot", ID_PO, INSERTKEY_MATRIX|COMMONKEY_PCHANROT, 1, {KAG_CHAN_EXTEND}},
	
	{incl_non_del_keys, "VisualLocRot", ID_PO, INSERTKEY_MATRIX|COMMONKEY_PCHANROT, 4, 
		{AC_LOC_X,AC_LOC_Y,AC_LOC_Z, KAG_CHAN_EXTEND}},
	
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
	//Object *ob= OBACT; // xxx
	Object *ob= NULL;
	/* only if object is mesh type */
	
	if(ob==NULL) return 0;
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
	Scene *scene= NULL; // FIXME this will cause a crash, but we need an extra arg first!
	/* only if renderer is internal renderer */
	return (scene->r.renderer==R_INTERN);
}

/* check if include external-renderer entry  */
static short incl_buts_cam2 (bKeyingSet *ks, const char mode[])
{
	Scene *scene= NULL; // FIXME this will cause a crash, but we need an extra arg first!
	/* only if renderer is internal renderer */
	return (scene->r.renderer!=R_INTERN);
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


#endif // XXX old keyingsets code based on adrcodes... to be restored in due course

/* Macros for Declaring KeyingSets ------------------- */

/* A note about this system for declaring built-in Keying Sets:
 *	One may ask, "What is the purpose of all of these macros and static arrays?" and 
 * 	"Why not call the KeyingSets API defined in BKE_animsys.h?". The answer is two-fold.
 * 	
 * 	1) Firstly, we use static arrays of struct definitions instead of function calls, as
 *	   it reduces the start-up overhead and allocated-memory footprint of Blender. If we called
 *	   the KeyingSets API to build these sets, the overhead of checking for unique names, allocating
 *	   memory for each and every path and KeyingSet, scattered around in RAM, all of which would increase
 *	   the startup time (which is totally unacceptable) and could lead to fragmentation+slower access times.
 *	2) Since we aren't using function calls, we need a nice way of defining these KeyingSets in a way which
 *	   is easily readable and less prone to breakage from changes to the underlying struct definitions. Further,
 *	   adding additional entries SHOULD NOT require custom code to be written to access these new entries/sets. 
 *	   Therefore, here we have a system with nice, human-readable statements via macros, and static arrays which
 *	   are linked together using more special macros + struct definitions, allowing for such a generic + simple
 *	   initialisation function (init_builtin_keyingsets()) compared with that of something like the Nodes system.
 *
 * -- Joshua Leung, April 2009
 */

/* Struct type for declaring builtin KeyingSets in as entries in static arrays*/
typedef struct bBuiltinKeyingSet {
	KeyingSet ks;			/* the KeyingSet to build */
	int tot;				/* the total number of paths defined */
	KS_Path paths[64];		/* the paths for the KeyingSet to use */
} bBuiltinKeyingSet;

	/* WARNING: the following macros must be kept in sync with the 
	 * struct definitions in DNA_anim_types.h! 
	 */

/* macro for defining a builtin KeyingSet */
#define BI_KS_DEFINE_BEGIN(name, keyingflag) \
	{{NULL, NULL, {NULL, NULL}, name, KEYINGSET_BUILTIN, keyingflag},
	
/* macro to finish defining a builtin KeyingSet */
#define BI_KS_DEFINE_END \
	}
	
/* macro to start defining paths for a builtin KeyingSet */
#define BI_KS_PATHS_BEGIN(tot) \
	tot, {
	
/* macro to finish defining paths for a builtin KeyingSet */
#define BI_KS_PATHS_END \
	}
	
/* macro for defining a builtin KeyingSet's path */
#define BI_KSP_DEFINE(id_type, templates, prop_path, array_index, flag, groupflag) \
	{NULL, NULL, NULL, "", id_type, templates, prop_path, array_index, flag, groupflag}
	
/* macro for defining a builtin KeyingSet with no paths (use in place of BI_KS_PAHTS_BEGIN/END block) */
#define BI_KS_PATHS_NONE \
	0, {0}
	
/* ---- */

/* Struct type for finding all the arrays of builtin KeyingSets */
typedef struct bBuiltinKSContext {
	bBuiltinKeyingSet *bks;		/* array of KeyingSet definitions */
	int tot;					/* number of KeyingSets in this array */
} bBuiltinKSContext;

/* macro for defining builtin KeyingSet sets 
 * NOTE: all the arrays of sets must follow this naming convention!
 */
#define BKSC_TEMPLATE(ctx_name) {&def_builtin_keyingsets_##ctx_name[0], sizeof(def_builtin_keyingsets_##ctx_name)/sizeof(bBuiltinKeyingSet)}


/* 3D-View Builtin KeyingSets ------------------------ */

static bBuiltinKeyingSet def_builtin_keyingsets_v3d[] =
{
	/* Simple Keying Sets ************************************* */
	/* Keying Set - "Location" ---------- */
	BI_KS_DEFINE_BEGIN("Location", 0)
		BI_KS_PATHS_BEGIN(1)
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN, "location", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM) 
		BI_KS_PATHS_END
	BI_KS_DEFINE_END,

	/* Keying Set - "Rotation" ---------- */
	BI_KS_DEFINE_BEGIN("Rotation", 0)
		BI_KS_PATHS_BEGIN(1)
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN|KSP_TEMPLATE_ROT, "rotation", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM) 
		BI_KS_PATHS_END
	BI_KS_DEFINE_END,
	
	/* Keying Set - "Scaling" ---------- */
	BI_KS_DEFINE_BEGIN("Scaling", 0)
		BI_KS_PATHS_BEGIN(1)
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN, "scale", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM) 
		BI_KS_PATHS_END
	BI_KS_DEFINE_END,
	
	/* Compound Keying Sets *********************************** */
	/* Keying Set - "LocRot" ---------- */
	BI_KS_DEFINE_BEGIN("LocRot", 0)
		BI_KS_PATHS_BEGIN(2)
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN, "location", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM), 
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN|KSP_TEMPLATE_ROT, "rotation", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM) 
		BI_KS_PATHS_END
	BI_KS_DEFINE_END,
	
	/* Keying Set - "LocRotScale" ---------- */
	BI_KS_DEFINE_BEGIN("LocRotScale", 0)
		BI_KS_PATHS_BEGIN(3)
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN, "location", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM), 
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN|KSP_TEMPLATE_ROT, "rotation", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM), 
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN, "scale", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM) 
		BI_KS_PATHS_END
	BI_KS_DEFINE_END,
	
	/* Keying Sets with Keying Flags ************************* */
	/* Keying Set - "VisualLoc" ---------- */
	BI_KS_DEFINE_BEGIN("VisualLoc", INSERTKEY_MATRIX)
		BI_KS_PATHS_BEGIN(1)
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN, "location", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM) 
		BI_KS_PATHS_END
	BI_KS_DEFINE_END,

	/* Keying Set - "Rotation" ---------- */
	BI_KS_DEFINE_BEGIN("VisualRot", INSERTKEY_MATRIX)
		BI_KS_PATHS_BEGIN(1)
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN|KSP_TEMPLATE_ROT, "rotation", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM) 
		BI_KS_PATHS_END
	BI_KS_DEFINE_END,
	
	/* Keying Set - "VisualLocRot" ---------- */
	BI_KS_DEFINE_BEGIN("VisualLocRot", INSERTKEY_MATRIX)
		BI_KS_PATHS_BEGIN(2)
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN, "location", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM), 
			BI_KSP_DEFINE(ID_OB, KSP_TEMPLATE_OBJECT|KSP_TEMPLATE_PCHAN|KSP_TEMPLATE_ROT, "rotation", 0, KSP_FLAG_WHOLE_ARRAY, KSP_GROUP_TEMPLATE_ITEM) 
		BI_KS_PATHS_END
	BI_KS_DEFINE_END
};

/* All Builtin KeyingSets ------------------------ */

/* total number of builtin KeyingSet contexts */
#define MAX_BKSC_TYPES 	1

/* array containing all the available builtin KeyingSets definition sets 
 * 	- size of this is MAX_BKSC_TYPES+1 so that we don't smash the stack
 */
static bBuiltinKSContext def_builtin_keyingsets[MAX_BKSC_TYPES+1] =
{
	BKSC_TEMPLATE(v3d)
	/* add more contexts above this line... */
};


/* ListBase of these KeyingSets chained up ready for usage 
 * NOTE: this is exported to keyframing.c for use...
 */
ListBase builtin_keyingsets = {NULL, NULL};

/* Utility API ------------------------ */

/* Link up all of the builtin Keying Sets when starting up Blender
 * This is called from WM_init() in wm_init_exit.c
 */
void init_builtin_keyingsets (void)
{
	bBuiltinKSContext *bksc;
	bBuiltinKeyingSet *bks;
	int bksc_i, bks_i;
	
	/* loop over all the sets of KeyingSets, setting them up, and chaining them to the builtins list */
	for (bksc_i= 0, bksc= &def_builtin_keyingsets[0]; bksc_i < MAX_BKSC_TYPES; bksc_i++, bksc++)
	{
		/* for each set definitions for a builtin KeyingSet, chain the paths to that KeyingSet and add */
		for (bks_i= 0, bks= bksc->bks; bks_i < bksc->tot; bks_i++, bks++)
		{
			KeyingSet *ks= &bks->ks;
			KS_Path *ksp;
			int pIndex;
			
			/* loop over paths, linking them to the KeyingSet and each other */
			for (pIndex= 0, ksp= &bks->paths[0]; pIndex < bks->tot; pIndex++, ksp++)
				BLI_addtail(&ks->paths, ksp);
				
			/* add KeyingSet to builtin sets list */
			BLI_addtail(&builtin_keyingsets, ks);
		}
	}
}


/* Get the first builtin KeyingSet with the given name, which occurs after the given one (or start of list if none given) */
KeyingSet *ANIM_builtin_keyingset_get_named (KeyingSet *prevKS, char name[])
{
	KeyingSet *ks, *first=NULL;
	
	/* sanity checks - any name to check? */
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

/* ******************************************* */
/* KEYFRAME MODIFICATION */

/* KeyingSet Menu Helpers ------------ */

/* Extract the maximum set of requirements from the KeyingSet */
static int keyingset_relative_get_templates (KeyingSet *ks)
{
	KS_Path *ksp;
	int templates= 0;
	
	/* loop over the paths (could be slow to do for a number of KeyingSets)? */
	for (ksp= ks->paths.first; ksp; ksp= ksp->next) {
		/* add the destination's templates to the set of templates required for the set */
		templates |= ksp->templates;
	}
	
	return templates;
}

/* Check if context data is suitable for the given absolute Keying Set */
short keyingset_context_ok_poll (bContext *C, KeyingSet *ks)
{
	ScrArea *sa= CTX_wm_area(C);
	
	/* data retrieved from context depends on active editor */
	if (sa == NULL) return 0;
		
	switch (sa->spacetype) {
		case SPACE_VIEW3D:
		{
			Object *obact= CTX_data_active_object(C);
			
			/* if in posemode, check if 'pose-channels' requested for in KeyingSet */
			if ((obact && obact->pose) && (obact->mode & OB_MODE_POSE)) {
				/* check for posechannels */
				
			}
			else {
				/* check for selected object */
				
			}
		}
			break;
	}
	
	
	return 1;
}

/* KeyingSet Context Operations ------------ */

/* Get list of data-sources from context (in 3D-View) for inserting keyframes using the given relative Keying Set */
static short modifykey_get_context_v3d_data (bContext *C, ListBase *dsources, KeyingSet *ks)
{
	bCommonKeySrc *cks;
	Object *obact= CTX_data_active_object(C);
	int templates; 
	short ok= 0;
	
	/* get the templates in use in this KeyingSet which we should supply data for */
	templates = keyingset_relative_get_templates(ks);
	
	/* check if the active object is in PoseMode (i.e. only deal with bones) */
	// TODO: check with the templates to see what we really need to store 
	if ((obact && obact->pose) && (obact->mode & OB_MODE_POSE)) {
		/* Pose Mode: Selected bones */
#if 0
		//set_pose_keys(ob);  /* sets pchan->flag to POSE_KEY if bone selected, and clears if not */
		
		/* loop through posechannels */
		//for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next) {
		//	if (pchan->flag & POSE_KEY) {
		// 	}
		//}
#endif
		
		CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pchans)
		{
			/* add a new keying-source */
			cks= MEM_callocN(sizeof(bCommonKeySrc), "bCommonKeySrc");
			BLI_addtail(dsources, cks);
			
			/* set necessary info */
			cks->id= &obact->id;
			cks->pchan= pchan;
			
			if (templates & KSP_TEMPLATE_CONSTRAINT)
				cks->con= constraints_get_active(&pchan->constraints);
			
			ok= 1;
		}
		CTX_DATA_END;
	}
	else {
		/* Object Mode: Selected objects */
		CTX_DATA_BEGIN(C, Base*, base, selected_bases) 
		{
			Object *ob= base->object;
			
			/* add a new keying-source */
			cks= MEM_callocN(sizeof(bCommonKeySrc), "bCommonKeySrc");
			BLI_addtail(dsources, cks);
			
			/* set necessary info */
			cks->id= &ob->id;
			
			if (templates & KSP_TEMPLATE_CONSTRAINT)
				cks->con= constraints_get_active(&ob->constraints);
			
			ok= 1;
		}
		CTX_DATA_END;
	}
	
	/* return whether any data was extracted */
	return ok;
}

/* Get list of data-sources from context for inserting keyframes using the given relative Keying Set */
short modifykey_get_context_data (bContext *C, ListBase *dsources, KeyingSet *ks)
{
	ScrArea *sa= CTX_wm_area(C);
	
	/* for now, the active area is used to determine what set of contexts apply */
	if (sa == NULL)
		return 0;
		
	switch (sa->spacetype) {
		case SPACE_VIEW3D:	/* 3D-View: Selected Objects or Bones */
			return modifykey_get_context_v3d_data(C, dsources, ks);
	}
	
	/* nothing happened */
	return 0;
} 

/* KeyingSet Operations (Insert/Delete Keyframes) ------------ */

/* Given a KeyingSet and context info (if required), modify keyframes for the channels specified
 * by the KeyingSet. This takes into account many of the different combinations of using KeyingSets.
 * Returns the number of channels that keyframes were added to
 */
int modify_keyframes (Scene *scene, ListBase *dsources, bAction *act, KeyingSet *ks, short mode, float cfra)
{
	KS_Path *ksp;
	int kflag=0, success= 0;
	char *groupname= NULL;
	
	/* get flags to use */
	if (mode == MODIFYKEY_MODE_INSERT) {
		/* use KeyingSet's flags as base */
		kflag= ks->keyingflag;
		
		/* suppliment with info from the context */
		if (IS_AUTOKEY_FLAG(AUTOMATKEY)) kflag |= INSERTKEY_MATRIX;
		if (IS_AUTOKEY_FLAG(INSERTNEEDED)) kflag |= INSERTKEY_NEEDED;
		if (IS_AUTOKEY_MODE(scene, EDITKEYS)) kflag |= INSERTKEY_REPLACE;
	}
	else if (mode == MODIFYKEY_MODE_DELETE)
		kflag= 0;
	
	/* check if the KeyingSet is absolute or not (i.e. does it requires sources info) */
	if (ks->flag & KEYINGSET_ABSOLUTE) {
		/* Absolute KeyingSets are simpler to use, as all the destination info has already been
		 * provided by the user, and is stored, ready to use, in the KeyingSet paths.
		 */
		for (ksp= ks->paths.first; ksp; ksp= ksp->next) {
			int arraylen, i;
			
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
					success+= insert_keyframe(ksp->id, act, groupname, ksp->rna_path, i, cfra, kflag);
				else if (mode == MODIFYKEY_MODE_DELETE)
					success+= delete_keyframe(ksp->id, act, groupname, ksp->rna_path, i, cfra, kflag);
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
	}
	else if (dsources && dsources->first) {
		/* for each one of the 'sources', resolve the template markers and expand arrays, then insert keyframes */
		bCommonKeySrc *cks;
		
		/* for each 'source' for keyframe data, resolve each of the paths from the KeyingSet */
		for (cks= dsources->first; cks; cks= cks->next) {
			/* for each path in KeyingSet, construct a path using the templates */
			for (ksp= ks->paths.first; ksp; ksp= ksp->next) {
				DynStr *pathds= BLI_dynstr_new();
				char *path = NULL;
				int arraylen, i;
				
				/* set initial group name */
				if (cks->id == NULL) {
					printf("ERROR: Skipping 'Common-Key' Source. No valid ID present.\n");
					continue;
				}
				else
					groupname= cks->id->name+2;
				
				/* construct the path */
				// FIXME: this currently only works with a few hardcoded cases
				if ((ksp->templates & KSP_TEMPLATE_PCHAN) && (cks->pchan)) {
					/* add basic pose-channel path access */
					BLI_dynstr_append(pathds, "pose.pose_channels[\"");
					BLI_dynstr_append(pathds, cks->pchan->name);
					BLI_dynstr_append(pathds, "\"]");
					
					/* override default group name */
					groupname= cks->pchan->name;
				}
				if ((ksp->templates & KSP_TEMPLATE_CONSTRAINT) && (cks->con)) {
					/* add basic constraint path access */
					BLI_dynstr_append(pathds, "constraints[\"");
					BLI_dynstr_append(pathds, cks->con->name);
					BLI_dynstr_append(pathds, "\"]");
					
					/* override default group name */
					groupname= cks->con->name;
				}
				{
					/* add property stored in KeyingSet Path */
					if (BLI_dynstr_get_len(pathds))
						BLI_dynstr_append(pathds, ".");
						
					/* apply some further templates? */
					if (ksp->templates & KSP_TEMPLATE_ROT) {
						/* for builtin Keying Sets, this template makes the best fitting path for the 
						 * current rotation mode of the Object / PoseChannel to be used
						 */
						if (strcmp(ksp->rna_path, "rotation")==0) {
							/* get rotation mode */
							short rotmode= (cks->pchan)? (cks->pchan->rotmode) : 
										   (GS(cks->id->name)==ID_OB)? ( ((Object *)cks->id)->rotmode ) :
										   (0);
							
							/* determine path to build */
							if (rotmode == ROT_MODE_QUAT)
								BLI_dynstr_append(pathds, "rotation_quaternion");
							else if (rotmode == ROT_MODE_AXISANGLE)
								BLI_dynstr_append(pathds, "rotation_axis_angle");
							else
								BLI_dynstr_append(pathds, "rotation_euler");
						}
					}
					else {
						/* just directly use the path */
						BLI_dynstr_append(pathds, ksp->rna_path);
					}
					
					/* convert to C-string */
					path= BLI_dynstr_get_cstring(pathds);
					BLI_dynstr_free(pathds);
				}
				
				/* get pointer to name of group to add channels to 
				 *	- KSP_GROUP_TEMPLATE_ITEM is handled above while constructing the paths 
				 */
				if (ksp->groupmode == KSP_GROUP_NONE)
					groupname= NULL;
				else if (ksp->groupmode == KSP_GROUP_KSNAME)
					groupname= ks->name;
				else if (ksp->groupmode == KSP_GROUP_NAMED)
					groupname= ksp->group;
				
				/* init arraylen and i - arraylen should be greater than i so that
				 * normal non-array entries get keyframed correctly
				 */
				i= ksp->array_index;
				arraylen= i+1;
				
				/* get length of array if whole array option is enabled */
				if (ksp->flag & KSP_FLAG_WHOLE_ARRAY) {
					PointerRNA id_ptr, ptr;
					PropertyRNA *prop;
					
					RNA_id_pointer_create(cks->id, &id_ptr);
					if (RNA_path_resolve(&id_ptr, path, &ptr, &prop) && prop)
						arraylen= RNA_property_array_length(&ptr, prop);
				}
				
				/* for each possible index, perform operation 
				 *	- assume that arraylen is greater than index
				 */
				for (; i < arraylen; i++) {
					/* action to take depends on mode */
					if (mode == MODIFYKEY_MODE_INSERT)
						success+= insert_keyframe(cks->id, act, groupname, path, i, cfra, kflag);
					else if (mode == MODIFYKEY_MODE_DELETE)
						success+= delete_keyframe(cks->id, act, groupname, path, i, cfra, kflag);
				}
				
				/* free the path */
				MEM_freeN(path);
			}
			
			/* set recalc-flags */
			if (cks->id) {
				switch (GS(cks->id->name)) {
					case ID_OB: /* Object (or Object-Related) Keyframes */
					{
						Object *ob= (Object *)cks->id;
						
						ob->recalc |= OB_RECALC;
					}
						break;
				}
				
				/* send notifiers for updates (this doesn't require context to work!) */
				WM_main_add_notifier(NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
			}
		}
	}
	
	/* return the number of channels successfully affected */
	return success;
}

/* ************************************************** */
