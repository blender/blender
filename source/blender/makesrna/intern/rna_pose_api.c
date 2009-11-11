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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"
#include "RNA_types.h"
#include "RNA_enum_types.h"

#include "DNA_object_types.h"

/* #include "BLO_sys_types.h" */

#ifdef RNA_RUNTIME

/* #include "DNA_anim_types.h" */
#include "DNA_action_types.h" /* bPose */

#include "BKE_constraint.h" /* bPose */

static bConstraint *rna_PoseChannel_constraints_add(bPoseChannel *pchan, bContext *C, int type)
{
	//WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT|NA_ADDED, object);
	// TODO, pass object also
	// TODO, new pose bones don't have updated draw flags
	return add_pose_constraint(NULL, pchan, NULL, type);
}

static int rna_PoseChannel_constraints_remove(bPoseChannel *pchan, bContext *C, int index)
{
	bConstraint *con= BLI_findlink(&pchan->constraints, index);

	if(con) {
		free_constraint_data(con);
		BLI_freelinkN(&pchan->constraints, con);

		//ED_object_constraint_set_active(object, NULL);
		//WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, object);

		return 1;
	}
	else {
		return 0;
	}
}

#else

void RNA_api_pose(StructRNA *srna)
{
	/* FunctionRNA *func; */
	/* PropertyRNA *parm; */
}

void RNA_api_pose_channel(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;


	/* Constraint collection */
	func= RNA_def_function(srna, "constraints__add", "rna_PoseChannel_constraints_add");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Add a constraint to this object");
	/* return type */
	parm= RNA_def_pointer(func, "constraint", "Constraint", "", "New constraint.");
	RNA_def_function_return(func, parm);
	/* object to add */
	parm= RNA_def_enum(func, "type", constraint_type_items, 1, "", "Constraint type to add.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "constraints__remove", "rna_PoseChannel_constraints_remove");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Remove a constraint from this object.");
	/* return type */
	parm= RNA_def_boolean(func, "success", 0, "Success", "Removed the constraint successfully.");
	RNA_def_function_return(func, parm);
	/* object to add */
	parm= RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);

}


#endif

