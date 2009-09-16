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
 * Contributor(s): Blender Foundation (2008), Roland Hess, Joshua Leung
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
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include <string.h>

#include "BLI_arithb.h"

#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_idprop.h"

#include "ED_armature.h"

static void rna_Pose_update(bContext *C, PointerRNA *ptr)
{
	// XXX when to use this? ob->pose->flag |= (POSE_LOCKED|POSE_DO_UNLOCK);

	DAG_id_flush_update(ptr->id.data, OB_RECALC_DATA);
}

static char *rna_PoseChannel_path(PointerRNA *ptr)
{
	return BLI_sprintfN("pose.pose_channels[\"%s\"]", ((bPoseChannel*)ptr->data)->name);
}

static void rna_BoneGroup_color_set_set(PointerRNA *ptr, int value)
{
	bActionGroup *grp= ptr->data;
	
	/* if valid value, set the new enum value, then copy the relevant colours? */
	if ((value >= -1) && (value < 21))
		grp->customCol= value;
	else
		return;
	
	/* only do color copying if using a custom color (i.e. not default colour)  */
	if (grp->customCol) {
		if (grp->customCol > 0) {
			/* copy theme colors on-to group's custom color in case user tries to edit color */
			bTheme *btheme= U.themes.first;
			ThemeWireColor *col_set= &btheme->tarm[(grp->customCol - 1)];
			
			memcpy(&grp->cs, col_set, sizeof(ThemeWireColor));
		}
		else {
			/* init custom colors with a generic multi-color rgb set, if not initialised already (for custom color set) */
			if (grp->cs.solid[0] == 0) {
				/* define for setting colors in theme below */
				#define SETCOL(col, r, g, b, a)  col[0]=r; col[1]=g; col[2]= b; col[3]= a;
				
				SETCOL(grp->cs.solid, 0xff, 0x00, 0x00, 255);
				SETCOL(grp->cs.select, 0x81, 0xe6, 0x14, 255);
				SETCOL(grp->cs.active, 0x18, 0xb6, 0xe0, 255);
				
				#undef SETCOL
			}
		}
	}
}

static IDProperty *rna_PoseChannel_idproperties(PointerRNA *ptr, int create)
{
	bPoseChannel *pchan= ptr->data;

	if(create && !pchan->prop) {
		IDPropertyTemplate val = {0};
		pchan->prop= IDP_New(IDP_GROUP, val, "RNA_PoseChannel group");
	}

	return pchan->prop;
}

/* rotation - euler angles */
static void rna_PoseChannel_euler_rotation_get(PointerRNA *ptr, float *value)
{
	bPoseChannel *pchan= ptr->data;
	
	if(pchan->rotmode == PCHAN_ROT_AXISANGLE) /* default XYZ eulers */
		AxisAngleToEulO(&pchan->quat[1], pchan->quat[0], value, EULER_ORDER_DEFAULT);
	else if(pchan->rotmode == PCHAN_ROT_QUAT) /* default XYZ eulers  */
		QuatToEul(pchan->quat, value);
	else
		VECCOPY(value, pchan->eul);
}

/* rotation - euler angles */
static void rna_PoseChannel_euler_rotation_set(PointerRNA *ptr, const float *value)
{
	bPoseChannel *pchan= ptr->data;
	
	if(pchan->rotmode == PCHAN_ROT_AXISANGLE) /* default XYZ eulers */
		EulOToAxisAngle((float *)value, EULER_ORDER_DEFAULT, &pchan->quat[1], &pchan->quat[0]);
	else if(pchan->rotmode == PCHAN_ROT_QUAT) /* default XYZ eulers */
		EulToQuat((float*)value, pchan->quat);
	else
		VECCOPY(pchan->eul, value);
}

/* rotation - axis angle only */
static void rna_PoseChannel_rotation_axis_get(PointerRNA *ptr, float *value)
{
	bPoseChannel *pchan= ptr->data;
	
	if (pchan->rotmode == PCHAN_ROT_AXISANGLE) {
		/* axis is stord in quat for now */
		VecCopyf(value, &pchan->quat[1]);
	}
}

/* rotation - axis angle only */
static void rna_PoseChannel_rotation_axis_set(PointerRNA *ptr, const float *value)
{
	bPoseChannel *pchan= ptr->data;
	
	if (pchan->rotmode == PCHAN_ROT_AXISANGLE) {
		/* axis is stored in quat for now */
		VecCopyf(&pchan->quat[1], (float *)value);
	}
}

static void rna_PoseChannel_rotation_mode_set(PointerRNA *ptr, int value)
{
	bPoseChannel *pchan= ptr->data;
	
	/* check if any change - if so, need to convert data */
	// TODO: this needs to be generalised at some point to work for objects too...
	if (value > 0) { /* to euler */
		if (pchan->rotmode == PCHAN_ROT_AXISANGLE) {
			/* axis-angle to euler */
			AxisAngleToEulO(&pchan->quat[1], pchan->quat[0], pchan->eul, value);
		}
		else if (pchan->rotmode == PCHAN_ROT_QUAT) {
			/* quat to euler */
			QuatToEulO(pchan->quat, pchan->eul, value);
		}
		/* else { no conversion needed } */
	}
	else if (value == PCHAN_ROT_QUAT) { /* to quat */
		if (pchan->rotmode == PCHAN_ROT_AXISANGLE) {
			/* axis angle to quat */
			float q[4];
			
			/* copy to temp var first, since quats and axis-angle are stored in same place */
			QuatCopy(q, pchan->quat);
			AxisAngleToQuat(q, &pchan->quat[1], pchan->quat[0]);
		}
		else if (pchan->rotmode > 0) {
			/* euler to quat */
			EulOToQuat(pchan->eul, pchan->rotmode, pchan->quat);
		}
		/* else { no conversion needed } */
	}
	else { /* to axis-angle */
		if (pchan->rotmode > 0) {
			/* euler to axis angle */
			EulOToAxisAngle(pchan->eul, pchan->rotmode, &pchan->quat[1], &pchan->quat[0]);
		}
		else if (pchan->rotmode == PCHAN_ROT_QUAT) {
			/* quat to axis angle */
			float q[4];
			
			/* copy to temp var first, since quats and axis-angle are stored in same place */
			QuatCopy(q, pchan->quat);
			QuatToAxisAngle(q, &pchan->quat[1], &pchan->quat[0]);
		}
		
		/* when converting to axis-angle, we need a special exception for the case when there is no axis */
		if (IS_EQ(pchan->quat[1], pchan->quat[2]) && IS_EQ(pchan->quat[2], pchan->quat[3])) {
			/* for now, rotate around y-axis then (so that it simply becomes the roll) */
			pchan->quat[2]= 1.0f;
		}
	}
	
	/* finally, set the new rotation type */
	pchan->rotmode= value;
}

static void rna_PoseChannel_name_set(PointerRNA *ptr, const char *value)
{
	Object *ob= (Object*)ptr->id.data;
	bPoseChannel *pchan= (bPoseChannel*)ptr->data;
	char oldname[32], newname[32];
	
	/* need to be on the stack */
	BLI_strncpy(newname, value, 32);
	BLI_strncpy(oldname, pchan->name, 32);
	
	ED_armature_bone_rename(ob->data, oldname, newname);
}

static int rna_PoseChannel_has_ik_get(PointerRNA *ptr)
{
	Object *ob= (Object*)ptr->id.data;
	bPoseChannel *pchan= (bPoseChannel*)ptr->data;

	return ED_pose_channel_in_IK_chain(ob, pchan);
}

static PointerRNA rna_PoseChannel_bone_group_get(PointerRNA *ptr)
{
	Object *ob= (Object*)ptr->id.data;
	bPose *pose= (ob) ? ob->pose : NULL;
	bPoseChannel *pchan= (bPoseChannel*)ptr->data;
	bActionGroup *grp;
	
	if (pose)
		grp= BLI_findlink(&pose->agroups, pchan->agrp_index-1);
	else
		grp= NULL;
	
	return rna_pointer_inherit_refine(ptr, &RNA_BoneGroup, grp);
}

static void rna_PoseChannel_bone_group_set(PointerRNA *ptr, PointerRNA value)
{
	Object *ob= (Object*)ptr->id.data;
	bPose *pose= (ob) ? ob->pose : NULL;
	bPoseChannel *pchan= (bPoseChannel*)ptr->data;
	
	if (pose)
		pchan->agrp_index= BLI_findindex(&pose->agroups, value.data) + 1;
	else
		pchan->agrp_index= 0;
}

static int rna_PoseChannel_bone_group_index_get(PointerRNA *ptr)
{
	bPoseChannel *pchan= (bPoseChannel*)ptr->data;
	return MAX2(pchan->agrp_index-1, 0);
}

static void rna_PoseChannel_bone_group_index_set(PointerRNA *ptr, int value)
{
	bPoseChannel *pchan= (bPoseChannel*)ptr->data;
	pchan->agrp_index= value+1;
}

static void rna_PoseChannel_bone_group_index_range(PointerRNA *ptr, int *min, int *max)
{
	Object *ob= (Object*)ptr->id.data;
	bPose *pose= (ob) ? ob->pose : NULL;
	
	*min= 0;
	
	if (pose) {
		*max= BLI_countlist(&pose->agroups)-1;
		*max= MAX2(0, *max);
	}
	else
		*max= 0;
}

static PointerRNA rna_Pose_active_bone_group_get(PointerRNA *ptr)
{
	bPose *pose= (bPose*)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_BoneGroup, BLI_findlink(&pose->agroups, pose->active_group-1));
}

static void rna_Pose_active_bone_group_set(PointerRNA *ptr, PointerRNA value)
{
	bPose *pose= (bPose*)ptr->data;
	pose->active_group= BLI_findindex(&pose->agroups, value.data) + 1;
}

static int rna_Pose_active_bone_group_index_get(PointerRNA *ptr)
{
	bPose *pose= (bPose*)ptr->data;
	return MAX2(pose->active_group-1, 0);
}

static void rna_Pose_active_bone_group_index_set(PointerRNA *ptr, int value)
{
	bPose *pose= (bPose*)ptr->data;
	pose->active_group= value+1;
}

static void rna_Pose_active_bone_group_index_range(PointerRNA *ptr, int *min, int *max)
{
	bPose *pose= (bPose*)ptr->data;

	*min= 0;
	*max= BLI_countlist(&pose->agroups)-1;
	*max= MAX2(0, *max);
}

static void rna_pose_bgroup_name_index_get(PointerRNA *ptr, char *value, int index)
{
	bPose *pose= (bPose*)ptr->data;
	bActionGroup *grp;

	grp= BLI_findlink(&pose->agroups, index-1);

	if(grp) BLI_strncpy(value, grp->name, sizeof(grp->name));
	else BLI_strncpy(value, "", sizeof(grp->name)); // XXX if invalid pointer, won't this crash?
}

static int rna_pose_bgroup_name_index_length(PointerRNA *ptr, int index)
{
	bPose *pose= (bPose*)ptr->data;
	bActionGroup *grp;

	grp= BLI_findlink(&pose->agroups, index-1);
	return (grp)? strlen(grp->name): 0;
}

static void rna_pose_bgroup_name_index_set(PointerRNA *ptr, const char *value, short *index)
{
	bPose *pose= (bPose*)ptr->data;
	bActionGroup *grp;
	int a;
	
	for (a=1, grp=pose->agroups.first; grp; grp=grp->next, a++) {
		if (strcmp(grp->name, value) == 0) {
			*index= a;
			return;
		}
	}
	
	*index= 0;
}

static void rna_pose_pgroup_name_set(PointerRNA *ptr, const char *value, char *result, int maxlen)
{
	bPose *pose= (bPose*)ptr->data;
	bActionGroup *grp;
	
	for (grp= pose->agroups.first; grp; grp= grp->next) {
		if (strcmp(grp->name, value) == 0) {
			BLI_strncpy(result, value, maxlen);
			return;
		}
	}
	
	BLI_strncpy(result, "", maxlen);
}

#else

static void rna_def_bone_group(BlenderRNA *brna)
{
	static EnumPropertyItem prop_colorSets_items[] = {
		{0, "DEFAULT", 0, "Default Colors", ""},
		{1, "THEME01", 0, "01 - Theme Color Set", ""},
		{2, "THEME02", 0, "02 - Theme Color Set", ""},
		{3, "THEME03", 0, "03 - Theme Color Set", ""},
		{4, "THEME04", 0, "04 - Theme Color Set", ""},
		{5, "THEME05", 0, "05 - Theme Color Set", ""},
		{6, "THEME06", 0, "06 - Theme Color Set", ""},
		{7, "THEME07", 0, "07 - Theme Color Set", ""},
		{8, "THEME08", 0, "08 - Theme Color Set", ""},
		{9, "THEME09", 0, "09 - Theme Color Set", ""},
		{10, "THEME10", 0, "10 - Theme Color Set", ""},
		{11, "THEME11", 0, "11 - Theme Color Set", ""},
		{12, "THEME12", 0, "12 - Theme Color Set", ""},
		{13, "THEME13", 0, "13 - Theme Color Set", ""},
		{14, "THEME14", 0, "14 - Theme Color Set", ""},
		{15, "THEME15", 0, "15 - Theme Color Set", ""},
		{16, "THEME16", 0, "16 - Theme Color Set", ""},
		{17, "THEME17", 0, "17 - Theme Color Set", ""},
		{18, "THEME18", 0, "18 - Theme Color Set", ""},
		{19, "THEME19", 0, "19 - Theme Color Set", ""},
		{20, "THEME20", 0, "20 - Theme Color Set", ""},
		{-1, "CUSTOM", 0, "Custom Color Set", ""},
		{0, NULL, 0, NULL, NULL}};
	
	StructRNA *srna;
	PropertyRNA *prop;
	
	/* struct */
	srna= RNA_def_struct(brna, "BoneGroup", NULL);
	RNA_def_struct_sdna(srna, "bActionGroup");
	RNA_def_struct_ui_text(srna, "Bone Group", "Groups of Pose Channels (Bones).");
	RNA_def_struct_ui_icon(srna, ICON_GROUP_BONE);
	
	/* name */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	
	// TODO: add some runtime-collections stuff to access grouped bones 
	
	/* color set + colors */
	prop= RNA_def_property(srna, "color_set", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "customCol");
	RNA_def_property_enum_items(prop, prop_colorSets_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_BoneGroup_color_set_set", NULL);
	RNA_def_property_ui_text(prop, "Color Set", "Custom color set to use.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
	
		// TODO: editing the colors for this should result in changes to the color type...
	prop= RNA_def_property(srna, "colors", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "ThemeBoneColorSet");
	RNA_def_property_pointer_sdna(prop, NULL, "cs"); /* NOTE: the DNA data is not really a pointer, but this code works :) */
	RNA_def_property_ui_text(prop, "Colors", "Copy of the colors associated with the group's color set.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
}

static void rna_def_pose_channel(BlenderRNA *brna)
{
	static EnumPropertyItem prop_rotmode_items[] = {
		{PCHAN_ROT_QUAT, "QUATERNION", 0, "Quaternion (WXYZ)", "No Gimbal Lock (default)"},
		{PCHAN_ROT_XYZ, "XYZ", 0, "XYZ Euler", "XYZ Rotation Order. Prone to Gimbal Lock"},
		{PCHAN_ROT_XZY, "XZY", 0, "XZY Euler", "XZY Rotation Order. Prone to Gimbal Lock"},
		{PCHAN_ROT_YXZ, "YXZ", 0, "YXZ Euler", "YXZ Rotation Order. Prone to Gimbal Lock"},
		{PCHAN_ROT_YZX, "YZX", 0, "YZX Euler", "YZX Rotation Order. Prone to Gimbal Lock"},
		{PCHAN_ROT_ZXY, "ZXY", 0, "ZXY Euler", "ZXY Rotation Order. Prone to Gimbal Lock"},
		{PCHAN_ROT_ZYX, "ZYX", 0, "ZYX Euler", "ZYX Rotation Order. Prone to Gimbal Lock"},
		{PCHAN_ROT_AXISANGLE, "AXIS_ANGLE", 0, "Axis Angle", "Axis Angle (W+XYZ). Defines a rotation around some axis defined by 3D-Vector."},
		{0, NULL, 0, NULL, NULL}};
	
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "PoseChannel", NULL);
	RNA_def_struct_sdna(srna, "bPoseChannel");
	RNA_def_struct_ui_text(srna, "Pose Channel", "Channel defining pose data for a bone in a Pose.");
	RNA_def_struct_path_func(srna, "rna_PoseChannel_path");
	RNA_def_struct_idproperties_func(srna, "rna_PoseChannel_idproperties");
	
	/* Bone Constraints */
	prop= RNA_def_property(srna, "constraints", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Constraint");
	RNA_def_property_ui_text(prop, "Constraints", "Constraints that act on this PoseChannel."); 

	/* Name + Selection Status */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_PoseChannel_name_set");
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	
	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "selectflag", BONE_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "");

	/* Baked Bone Path cache data s*/
	prop= RNA_def_property(srna, "path_start_frame", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "pathsf");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Bone Paths Calculation Start Frame", "Starting frame of range of frames to use for Bone Path calculations.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE|ND_TRANSFORM, "rna_Pose_update");

	prop= RNA_def_property(srna, "path_end_frame", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "pathef");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Bone Paths Calculation End Frame", "End frame of range of frames to use for Bone Path calculations.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE|ND_TRANSFORM, "rna_Pose_update");
	
	/* Relationships to other bones */
	prop= RNA_def_property(srna, "bone", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Bone", "Bone associated with this Pose Channel.");

	prop= RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "PoseChannel");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent", "Parent of this pose channel.");

	prop= RNA_def_property(srna, "child", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "PoseChannel");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Child", "Child of this pose channel.");
	
	/* Transformation settings */
	prop= RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Pose_update");

	prop= RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_ui_text(prop, "Scale", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Pose_update");

	prop= RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_QUATERNION);
	RNA_def_property_float_sdna(prop, NULL, "quat");
	RNA_def_property_ui_text(prop, "Rotation", "Rotation in Quaternions.");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Pose_update");
	
	prop= RNA_def_property(srna, "rotation_angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "quat[0]");
	RNA_def_property_ui_text(prop, "Rotation Angle", "Angle of Rotation for Axis-Angle rotation representation.");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Pose_update");
	
	prop= RNA_def_property(srna, "rotation_axis", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "quat");
	RNA_def_property_float_funcs(prop, "rna_PoseChannel_rotation_axis_get", "rna_PoseChannel_rotation_axis_set", NULL);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Rotation Axis", "Axis for Axis-Angle rotation representation.");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Pose_update");
	
	prop= RNA_def_property(srna, "euler_rotation", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float_sdna(prop, NULL, "eul");
	RNA_def_property_float_funcs(prop, "rna_PoseChannel_euler_rotation_get", "rna_PoseChannel_euler_rotation_set", NULL);
	RNA_def_property_ui_text(prop, "Rotation (Euler)", "Rotation in Eulers.");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Pose_update");
	
	prop= RNA_def_property(srna, "rotation_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rotmode");
	RNA_def_property_enum_items(prop, prop_rotmode_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_PoseChannel_rotation_mode_set", NULL);
	RNA_def_property_ui_text(prop, "Rotation Mode", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	/* These three matrix properties await an implementation of the PROP_MATRIX subtype, which currently doesn't exist. */
/*	prop= RNA_def_property(srna, "channel_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_struct_type(prop, "chan_mat");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Channel Matrix", "4x4 matrix, before constraints.");*/

	/* kaito says this should be not user-editable; I disagree; power users should be able to force this in python; he's the boss. */
/*	prop= RNA_def_property(srna, "pose_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_struct_type(prop, "pose_mat");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); 
	RNA_def_property_ui_text(prop, "Pose Matrix", "Final 4x4 matrix for this channel.");

	prop= RNA_def_property(srna, "constraint_inverse_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_struct_type(prop, "constinv");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Constraint Inverse Matrix", "4x4 matrix, defines transform from final position to unconstrained position."); */
	
	/* Head/Tail Coordinates (in Pose Space) - Automatically calculated... */
	prop= RNA_def_property(srna, "pose_head", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Pose Head Position", "Location of head of the channel's bone.");

	prop= RNA_def_property(srna, "pose_tail", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Pose Tail Position", "Location of tail of the channel's bone.");
	
	/* IK Settings */
	prop= RNA_def_property(srna, "has_ik", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop,  "rna_PoseChannel_has_ik_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Has IK", "Is part of an IK chain.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_dof_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "ikflag", BONE_IK_NO_XDOF);
	RNA_def_property_ui_text(prop, "IK X DoF", "Allow movement around the X axis.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_dof_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "ikflag", BONE_IK_NO_YDOF);
	RNA_def_property_ui_text(prop, "IK Y DoF", "Allow movement around the Y axis.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_dof_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "ikflag", BONE_IK_NO_ZDOF);
	RNA_def_property_ui_text(prop, "IK Z DoF", "Allow movement around the Z axis.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_limit_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ikflag", BONE_IK_XLIMIT);
	RNA_def_property_ui_text(prop, "IK X Limit", "Limit movement around the X axis.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_limit_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ikflag", BONE_IK_YLIMIT);
	RNA_def_property_ui_text(prop, "IK Y Limit", "Limit movement around the Y axis.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_limit_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ikflag", BONE_IK_ZLIMIT);
	RNA_def_property_ui_text(prop, "IK Z Limit", "Limit movement around the Z axis.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
	
	prop= RNA_def_property(srna, "ik_min_x", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limitmin[0]");
	RNA_def_property_range(prop, -180.0f, 0.0f);
	RNA_def_property_ui_text(prop, "IK X Minimum", "Minimum angles for IK Limit");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_max_x", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limitmax[0]");
	RNA_def_property_range(prop, 0.0f, 180.0f);
	RNA_def_property_ui_text(prop, "IK X Maximum", "Maximum angles for IK Limit");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_min_y", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limitmin[1]");
	RNA_def_property_range(prop, -180.0f, 0.0f);
	RNA_def_property_ui_text(prop, "IK Y Minimum", "Minimum angles for IK Limit");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_max_y", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limitmax[1]");
	RNA_def_property_range(prop, 0.0f, 180.0f);
	RNA_def_property_ui_text(prop, "IK Y Maximum", "Maximum angles for IK Limit");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_min_z", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limitmin[2]");
	RNA_def_property_range(prop, -180.0f, 0.0f);
	RNA_def_property_ui_text(prop, "IK Z Minimum", "Minimum angles for IK Limit");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_max_z", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limitmax[2]");
	RNA_def_property_range(prop, 0.0f, 180.0f);
	RNA_def_property_ui_text(prop, "IK Z Maximum", "Maximum angles for IK Limit");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_stiffness_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "stiffness[0]");
	RNA_def_property_range(prop, 0.0f, 0.99f);
	RNA_def_property_ui_text(prop, "IK X Stiffness", "IK stiffness around the X axis.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_stiffness_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "stiffness[1]");
	RNA_def_property_range(prop, 0.0f, 0.99f);
	RNA_def_property_ui_text(prop, "IK Y Stiffness", "IK stiffness around the Y axis.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_stiffness_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "stiffness[2]");
	RNA_def_property_range(prop, 0.0f, 0.99f);
	RNA_def_property_ui_text(prop, "IK Z Stiffness", "IK stiffness around the Z axis.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "ik_stretch", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ikstretch");
	RNA_def_property_range(prop, 0.0f,1.0f);
	RNA_def_property_ui_text(prop, "IK Stretch", "Allow scaling of the bone for IK.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
	
	/* custom bone shapes */
	prop= RNA_def_property(srna, "custom_shape", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "custom");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Custom Object", "Object that defines custom draw type for this bone.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
	
	/* bone groups */
	prop= RNA_def_property(srna, "bone_group_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "agrp_index");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_PoseChannel_bone_group_index_get", "rna_PoseChannel_bone_group_index_set", "rna_PoseChannel_bone_group_index_range");
	RNA_def_property_ui_text(prop, "Bone Group Index", "Bone Group this pose channel belongs to (0=no group).");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
	
	prop= RNA_def_property(srna, "bone_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "BoneGroup");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_PoseChannel_bone_group_get", "rna_PoseChannel_bone_group_set", NULL);
	RNA_def_property_ui_text(prop, "Bone Group", "Bone Group this pose channel belongs to");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
	
	/* transform locks */
	prop= RNA_def_property(srna, "lock_location", PROP_BOOLEAN, PROP_XYZ);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_LOCX);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Lock Location", "Lock editing of location in the interface.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "lock_rotation", PROP_BOOLEAN, PROP_XYZ);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_ROTX);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Lock Rotation", "Lock editing of rotation in the interface.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
	
		// XXX this is sub-optimal - it really should be included above, but due to technical reasons we can't do this!
	prop= RNA_def_property(srna, "lock_rotation_w", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_ROTW);
	RNA_def_property_ui_text(prop, "Lock Rotation (4D Angle)", "Lock editing of 'angle' component of four-component rotations in the interface.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
		// XXX this needs a better name
	prop= RNA_def_property(srna, "lock_rotations_4d", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_ROT4D);
	RNA_def_property_ui_text(prop, "Lock Rotations (4D)", "Lock editing of four component rotations by components (instead of as Eulers).");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "lock_scale", PROP_BOOLEAN, PROP_XYZ);
	RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_SCALEX);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Lock Scale", "Lock editing of scale in the interface.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
}

static void rna_def_pose(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	/* struct definition */
	srna= RNA_def_struct(brna, "Pose", NULL);
	RNA_def_struct_sdna(srna, "bPose");
	RNA_def_struct_ui_text(srna, "Pose", "A collection of pose channels, including settings for animating bones.");

	/* pose channels */
	prop= RNA_def_property(srna, "pose_channels", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "chanbase", NULL);
	RNA_def_property_struct_type(prop, "PoseChannel");
	RNA_def_property_ui_text(prop, "Pose Channels", "Individual pose channels for the armature.");

	/* bone groups */
	prop= RNA_def_property(srna, "bone_groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "agroups", NULL);
	RNA_def_property_struct_type(prop, "BoneGroup");
	RNA_def_property_ui_text(prop, "Bone Groups", "Groups of the bones.");

	prop= RNA_def_property(srna, "active_bone_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "BoneGroup");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_Pose_active_bone_group_get", "rna_Pose_active_bone_group_set", NULL);
	RNA_def_property_ui_text(prop, "Active Bone Group", "Active bone group for this pose.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");

	prop= RNA_def_property(srna, "active_bone_group_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "active_group");
	RNA_def_property_int_funcs(prop, "rna_Pose_active_bone_group_index_get", "rna_Pose_active_bone_group_index_set", "rna_Pose_active_bone_group_index_range");
	RNA_def_property_ui_text(prop, "Active Bone Group Index", "Active index in bone groups array.");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Pose_update");
}

void RNA_def_pose(BlenderRNA *brna)
{
	rna_def_pose(brna);
	rna_def_pose_channel(brna);
	
	rna_def_bone_group(brna);
}

#endif
