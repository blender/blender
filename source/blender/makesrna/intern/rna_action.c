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
 * Contributor(s): Blender Foundation (2008), Roland Hess
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"

#ifdef RNA_RUNTIME

static float rna_IK_Min_X_get(PointerRNA *ptr)
{
	bPoseChannel *pchan= (bPoseChannel*)ptr->id.data;

	return pchan->limitmin[0];
}

static float rna_IK_Min_Y_get(PointerRNA *ptr)
{
	bPoseChannel *pchan= (bPoseChannel*)ptr->id.data;

	return pchan->limitmin[1];
}

static float rna_IK_Min_Z_get(PointerRNA *ptr)
{
	bPoseChannel *pchan= (bPoseChannel*)ptr->id.data;

	return pchan->limitmin[2];
}

static void rna_IK_Min_X_set(PointerRNA *ptr, float value)
{
	bPoseChannel *pchan= (bPoseChannel*)ptr->id.data;
	
	pchan->limitmin[0] = value;
}

static void rna_IK_Min_Y_set(PointerRNA *ptr, float value)
{
	bPoseChannel *pchan= (bPoseChannel*)ptr->id.data;
	
	pchan->limitmin[1] = value;
}

static void rna_IK_Min_Z_set(PointerRNA *ptr, float value)
{
	bPoseChannel *pchan= (bPoseChannel*)ptr->id.data;
	
	pchan->limitmin[2] = value;
}

#else

/* users shouldn't be editing pose channel data directly -- better to set ipos and let blender calc pose_channel stuff */
/* it's going to be weird for users to find IK flags and other such here, instead of in bone where they would expect them
 	-- is there any way to put a doc in bone, pointing them here? */

static void RNA_def_pose_channel(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_iklimit_items[] = {
		{BONE_IK_NO_XDOF, "IKNOXDOF", "No X DoF", "Prevent motion around X axis."},
		{BONE_IK_NO_YDOF, "IKNOYDOF", "No Y DoF", "Prevent motion around Y axis."},
		{BONE_IK_NO_ZDOF, "IKNOZDOF", "No Z DoF", "Prevent motion around Z axis."},
		{BONE_IK_XLIMIT, "IKXLIMIT", "X Limit", "Limit motion around X axis."},
		{BONE_IK_YLIMIT, "IKYLIMIT", "Y Limit", "Limit motion around Y axis."},
		{BONE_IK_ZLIMIT, "IKZLIMIT", "Z Limit", "Limit motion around Z axis."},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "bPoseChannel", NULL);
	RNA_def_struct_ui_text(srna, "Pose Channel", "Member of the 'Pose' type.");

	/* cosntraints (collection) */
	prop= RNA_def_property(srna, "constraints", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "constraints", NULL);
	RNA_def_property_struct_type(prop, "bConstraint");
	RNA_def_property_ui_text(prop, "Constraints", "Constraints that act on this PoseChannel.");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "ikflag", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_iklimit_items);
	RNA_def_property_ui_text(prop, "IK Limits", "");
	
	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "selectflag", BONE_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "");
	
	prop= RNA_def_property(srna, "protected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", POSE_LOCKED);
	RNA_def_property_ui_text(prop, "Protected", "Protect channel from being transformed.");

	prop= RNA_def_property(srna, "action_group_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "agrp_index");
	RNA_def_property_ui_text(prop, "Action Group Index", "Action Group this pose channel belogs to (0=no group).");

	prop= RNA_def_property(srna, "path_start_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pathsf");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Bone Paths Calculation Start Frame", "Starting frame of range of frames to use for Bone Path calculations.");

	prop= RNA_def_property(srna, "path_end_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pathef");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Bone Paths Calculation End Frame", "End frame of range of frames to use for Bone Path calculations.");

	prop= RNA_def_property(srna, "bone", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Bone", "Bone associated with this Pose Channel.");

	prop= RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "bPoseChannel");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent", "Parent of this pose channel.");

	prop= RNA_def_property(srna, "child", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "bPoseChannel");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent", "Child of this pose channel.");

	prop= RNA_def_property(srna, "channel_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_struct_type(prop, "chan_mat");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Channel Matrix", "4x4 matrix, before constraints.");

	/* kaito says this should be not user-editable; I disagree; power users should be able to force this in python; he's the boss. */
	prop= RNA_def_property(srna, "pose_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_struct_type(prop, "pose_mat");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE); 
	RNA_def_property_ui_text(prop, "Pose Matrix", "Final 4x4 matrix for this channel.");

	prop= RNA_def_property(srna, "constraint_inverse_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_struct_type(prop, "constinv");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Constraint Inverse Matrix", "4x4 matrix, defines transform from final position to unconstrained position.");
	
	prop= RNA_def_property(srna, "pose_head", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Pose Head Position", "Location of head of the channel's bone.");

	prop= RNA_def_property(srna, "pose_tail", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Pose Tail Position", "Location of tail of the channel's bone.");

	prop= RNA_def_property(srna, "ik_min_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "limitmin");
	RNA_def_property_range(prop, -180.0f, 180.0f);
	RNA_def_property_float_funcs(prop, "rna_IK_Min_X_get", "rna_IK_Min_X_set", NULL);	
	RNA_def_property_ui_text(prop, "IK Minimum Limit X", "Minimum X angle for IK Limit");

	prop= RNA_def_property(srna, "ik_min_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "limitmin");
	RNA_def_property_range(prop, -180.0f, 180.0f);
	RNA_def_property_float_funcs(prop, "rna_IK_Min_Y_get", "rna_IK_Min_Y_set", NULL);	
	RNA_def_property_ui_text(prop, "IK Minimum Limit Y", "Minimum Y angle for IK Limit");

	prop= RNA_def_property(srna, "ik_min_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "limitmin");
	RNA_def_property_range(prop, -180.0f, 180.0f);
	RNA_def_property_float_funcs(prop, "rna_IK_Min_Z_get", "rna_IK_Min_Z_set", NULL);	
	RNA_def_property_ui_text(prop, "IK Minimum Limit Z", "Minimum Z angle for IK Limit");

	prop= RNA_def_property(srna, "ik_max_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "limitmax");
	RNA_def_property_range(prop, -180.0f, 180.0f);
	RNA_def_property_float_funcs(prop, "rna_IK_Max_X_get", "rna_IK_Max_X_set", NULL);	
	RNA_def_property_ui_text(prop, "IK Maximum Limit X", "Maximum X angle for IK Limit");

	prop= RNA_def_property(srna, "ik_max_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "limitmax");
	RNA_def_property_range(prop, -180.0f, 180.0f);
	RNA_def_property_float_funcs(prop, "rna_IK_Max_Y_get", "rna_IK_Max_Y_set", NULL);	
	RNA_def_property_ui_text(prop, "IK Maximum Limit Y", "Maximum Y angle for IK Limit");

	prop= RNA_def_property(srna, "ik_max_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "limitmax");
	RNA_def_property_range(prop, -180.0f, 180.0f);
	RNA_def_property_float_funcs(prop, "rna_IK_Max_Z_get", "rna_IK_Max_Z_set", NULL);	
	RNA_def_property_ui_text(prop, "IK Maximum Limit Z", "Maximum Z angle for IK Limit");

//	float		limitmin[3], limitmax[3];	/* DOF constraint */
//	float		stiffness[3];				/* DOF stiffness */
//	float		ikstretch;
	
//	float		*path;				/* totpath x 3 x float */
//	struct Object *custom;			/* draws custom object instead of this channel */
};

void RNA_def_action(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Action", "ID");
	RNA_def_struct_sdna(srna, "bAction");
	RNA_def_struct_ui_text(srna, "Action", "DOC_BROKEN");

}

#endif
