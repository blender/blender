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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#ifdef RNA_RUNTIME

#include "BKE_animsys.h"

static KS_Path *rna_KeyingSet_add_path(KeyingSet *keyingset, ReportList *reports, 
		ID *id, char rna_path[], int index, int grouping_method, char group_name[])
{
	KS_Path *ksp = NULL;
	short flag = 0;
	
	/* special case when index = -1, we key the whole array (as with other places where index is used) */
	if (index == -1) {
		flag |= KSP_FLAG_WHOLE_ARRAY;
		index = 0;
	}
	
	/* if data is valid, call the API function for this */
	if (keyingset) {
		ksp= BKE_keyingset_add_path(keyingset, id, group_name, rna_path, index, flag, grouping_method);
		keyingset->active_path= BLI_countlist(&keyingset->paths); 
	}
	else {
		BKE_report(reports, RPT_ERROR, "Keying Set Path could not be added.");
	}
	
	/* return added path */
	return ksp;
}

static void rna_KeyingSet_remove_path(KeyingSet *keyingset, ReportList *reports, KS_Path *ksp)
{
	/* if data is valid, call the API function for this */
	if (keyingset && ksp) {
		/* remove the active path from the KeyingSet */
		BKE_keyingset_free_path(keyingset, ksp);
			
		/* the active path number will most likely have changed */
		// TODO: we should get more fancy and actually check if it was removed, but this will do for now
		keyingset->active_path = 0;
	}
	else {
		BKE_report(reports, RPT_ERROR, "Keying Set Path could not be removed.");
	}
}

static void rna_KeyingSet_remove_all_paths(KeyingSet *keyingset, ReportList *reports)
{
	/* if data is valid, call the API function for this */
	if (keyingset) {
		KS_Path *ksp, *kspn;
		
		/* free each path as we go to avoid looping twice */
		for (ksp= keyingset->paths.first; ksp; ksp= kspn) {
			kspn= ksp->next;
			BKE_keyingset_free_path(keyingset, ksp);
		}
			
		/* reset the active path, since there aren't any left */
		keyingset->active_path = 0;
	}
	else {
		BKE_report(reports, RPT_ERROR, "Keying Set Paths could not be removed.");
	}
}

#else

void RNA_api_keyingset(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;
	
	/* Add Path */
	func= RNA_def_function(srna, "add_path", "rna_KeyingSet_add_path");
	RNA_def_function_ui_description(func, "Add a new path for the Keying Set.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
		/* return arg */
	parm= RNA_def_pointer(func, "ksp", "KeyingSetPath", "New Path", "Path created and added to the Keying Set");
		RNA_def_function_return(func, parm);
		/* ID-block for target */
	parm= RNA_def_pointer(func, "target_id", "ID", "Target ID", "ID-Datablock for the destination."); 
		RNA_def_property_flag(parm, PROP_REQUIRED);
		/* rna-path */
	parm= RNA_def_string(func, "data_path", "", 256, "Data-Path", "RNA-Path to destination property."); // xxx hopefully this is long enough
		RNA_def_property_flag(parm, PROP_REQUIRED);
		/* index (defaults to -1 for entire array) */
	parm=RNA_def_int(func, "index", -1, 0, INT_MAX, "Index", "The index of the destination property (i.e. axis of Location/Rotation/etc.), or -1 for the entire array.", 0, INT_MAX);
		/* grouping */
	parm=RNA_def_enum(func, "grouping_method", keyingset_path_grouping_items, KSP_GROUP_KSNAME, "Grouping Method", "Method used to define which Group-name to use.");
	parm=RNA_def_string(func, "group_name", "", 64, "Group Name", "Name of Action Group to assign destination to (only if grouping mode is to use this name).");
	
	/* Remove Path */
	func= RNA_def_function(srna, "remove_path", "rna_KeyingSet_remove_path");
	RNA_def_function_ui_description(func, "Remove the given path from the Keying Set.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
		/* path to remove */
	parm= RNA_def_pointer(func, "path", "KeyingSetPath", "Path", ""); 
		RNA_def_property_flag(parm, PROP_REQUIRED);
		
	/* Remove All Paths */
	func= RNA_def_function(srna, "remove_all_paths", "rna_KeyingSet_remove_all_paths");
	RNA_def_function_ui_description(func, "Remove all the paths from the Keying Set.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
}

#endif

