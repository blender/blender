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
#include "DNA_constraint_types.h"
#include "DNA_scene_types.h"

#ifdef RNA_RUNTIME

#else

/* users shouldn't be editing pose channel data directly -- better to set ipos and let blender calc pose_channel stuff */
/* it's going to be weird for users to find IK flags and other such here, instead of in bone where they would expect them
 	-- is there any way to put a doc in bone, pointing them here? */

static void rna_def_pose_channel(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "PoseChannel", NULL);
	RNA_def_struct_sdna(srna, "bPoseChannel");
	RNA_def_struct_ui_text(srna, "Pose Channel", "Member of the 'Pose' type.");

	prop= RNA_def_property(srna, "constraints", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "constraints", NULL);
	RNA_def_property_struct_type(prop, "Constraint");
	RNA_def_property_ui_text(prop, "Constraints", "Constraints that act on this PoseChannel."); 

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "ik_dof_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ikflag", BONE_IK_NO_XDOF);
	RNA_def_property_ui_text(prop, "IK X DoF", "Allow movement around the X axis.");

	prop= RNA_def_property(srna, "ik_dof_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ikflag", BONE_IK_NO_YDOF);
	RNA_def_property_ui_text(prop, "IK Y DoF", "Allow movement around the Y axis.");

	prop= RNA_def_property(srna, "ik_dof_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ikflag", BONE_IK_NO_ZDOF);
	RNA_def_property_ui_text(prop, "IK Z DoF", "Allow movement around the Z axis.");

	prop= RNA_def_property(srna, "ik_limit_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ikflag", BONE_IK_XLIMIT);
	RNA_def_property_ui_text(prop, "IK X Limit", "Limit movement around the X axis.");

	prop= RNA_def_property(srna, "ik_limit_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ikflag", BONE_IK_YLIMIT);
	RNA_def_property_ui_text(prop, "IK Y Limit", "Limit movement around the Y axis.");

	prop= RNA_def_property(srna, "ik_limit_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ikflag", BONE_IK_ZLIMIT);
	RNA_def_property_ui_text(prop, "IK Z Limit", "Limit movement around the Z axis.");
	
	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "selectflag", BONE_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "");
	
	prop= RNA_def_property(srna, "protected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", POSE_LOCKED);
	RNA_def_property_ui_text(prop, "Protected", "Protect channel from being transformed.");

		// XXX note: bone groups are stored internally as bActionGroups :) - Aligorith
	//prop= RNA_def_property(srna, "bone_group_index", PROP_INT, PROP_NONE);
	//RNA_def_property_int_sdna(prop, NULL, "agrp_index");
	//RNA_def_property_ui_text(prop, "Bone Group Index", "Bone Group this pose channel belongs to (0=no group).");

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
	RNA_def_property_struct_type(prop, "PoseChannel");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent", "Parent of this pose channel.");

	prop= RNA_def_property(srna, "child", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "PoseChannel");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent", "Child of this pose channel.");

	/* These three matrix properties await an implementation of the PROP_MATRIX subtype, which currently doesn't exist. */
/*	prop= RNA_def_property(srna, "channel_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_struct_type(prop, "chan_mat");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Channel Matrix", "4x4 matrix, before constraints.");*/

	/* kaito says this should be not user-editable; I disagree; power users should be able to force this in python; he's the boss. */
/*	prop= RNA_def_property(srna, "pose_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_struct_type(prop, "pose_mat");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE); 
	RNA_def_property_ui_text(prop, "Pose Matrix", "Final 4x4 matrix for this channel.");

	prop= RNA_def_property(srna, "constraint_inverse_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_struct_type(prop, "constinv");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Constraint Inverse Matrix", "4x4 matrix, defines transform from final position to unconstrained position."); */
	
	prop= RNA_def_property(srna, "pose_head", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Pose Head Position", "Location of head of the channel's bone.");

	prop= RNA_def_property(srna, "pose_tail", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Pose Tail Position", "Location of tail of the channel's bone.");

	prop= RNA_def_property(srna, "ik_minimums", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "limitmin");
	RNA_def_property_range(prop, -180.0f, 0.0f);
	RNA_def_property_ui_text(prop, "IK Minimum Limits", "Minimum angles for IK Limit");

	prop= RNA_def_property(srna, "ik_maximum", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "limitmax");
	RNA_def_property_range(prop, 0.0f, 180.0f);
	RNA_def_property_ui_text(prop, "IK Maximum Limits", "Maximum angles for IK Limit");

	prop= RNA_def_property(srna, "ik_stiffness", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "stiffness");
	RNA_def_property_range(prop, 0.0f,0.99f);
	RNA_def_property_ui_text(prop, "IK Stiffness", "Stiffness around the different axes.");

	prop= RNA_def_property(srna, "ik_stretch", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ikstretch");
	RNA_def_property_range(prop, 0.0f,1.0f);
	RNA_def_property_ui_text(prop, "IK Stretch", "Allow scaling of the bone for IK.");

	prop= RNA_def_property(srna, "custom", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Custom Object", "Object that defines custom draw type for this bone.");

}

void RNA_def_pose(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_pose_channel(brna);

	srna= RNA_def_struct(brna, "Pose", NULL);
	RNA_def_struct_sdna(srna, "bPose");
	RNA_def_struct_ui_text(srna, "Pose", "A collection of pose channels, including settings for animating bones.");

	prop= RNA_def_property(srna, "pose_channels", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "chanbase", NULL);
	RNA_def_property_struct_type(prop, "PoseChannel");
	RNA_def_property_ui_text(prop, "Pose Channels", "Individual pose channels for the armature.");

	/* commented for now... missing info... */
	/*prop= RNA_def_property(srna, "action_groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "agroups", NULL);
	RNA_def_property_struct_type(prop, "ActionGroup");
	RNA_def_property_ui_text(prop, "Action Groups", "Groups of bones.");*/
}

#endif
