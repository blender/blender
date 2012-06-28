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
 * Contributor(s): Blender Foundation (2008), Joshua Leung, Roland Hess
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_constraint.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "BLI_math.h"

#include "BLF_translation.h"

#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ED_object.h"
#include "WM_types.h"

/* please keep the names in sync with constraint.c */
EnumPropertyItem constraint_type_items[] = {
	{0, "", 0, N_("Motion Tracking"), ""},
	{CONSTRAINT_TYPE_CAMERASOLVER, "CAMERA_SOLVER", ICON_CONSTRAINT_DATA, "Camera Solver", ""},
	{CONSTRAINT_TYPE_OBJECTSOLVER, "OBJECT_SOLVER", ICON_CONSTRAINT_DATA, "Object Solver", ""},
	{CONSTRAINT_TYPE_FOLLOWTRACK, "FOLLOW_TRACK", ICON_CONSTRAINT_DATA, "Follow Track", ""},
	{0, "", 0, N_("Transform"), ""},
	{CONSTRAINT_TYPE_LOCLIKE, "COPY_LOCATION", ICON_CONSTRAINT_DATA, "Copy Location", ""},
	{CONSTRAINT_TYPE_ROTLIKE, "COPY_ROTATION", ICON_CONSTRAINT_DATA, "Copy Rotation", ""},
	{CONSTRAINT_TYPE_SIZELIKE, "COPY_SCALE", ICON_CONSTRAINT_DATA, "Copy Scale", ""},
	{CONSTRAINT_TYPE_TRANSLIKE, "COPY_TRANSFORMS", ICON_CONSTRAINT_DATA, "Copy Transforms", ""},
	{CONSTRAINT_TYPE_DISTLIMIT, "LIMIT_DISTANCE", ICON_CONSTRAINT_DATA, "Limit Distance", ""},
	{CONSTRAINT_TYPE_LOCLIMIT, "LIMIT_LOCATION", ICON_CONSTRAINT_DATA, "Limit Location", ""},
	{CONSTRAINT_TYPE_ROTLIMIT, "LIMIT_ROTATION", ICON_CONSTRAINT_DATA, "Limit Rotation", ""},
	{CONSTRAINT_TYPE_SIZELIMIT, "LIMIT_SCALE", ICON_CONSTRAINT_DATA, "Limit Scale", ""},
	{CONSTRAINT_TYPE_SAMEVOL, "MAINTAIN_VOLUME", ICON_CONSTRAINT_DATA, "Maintain Volume", ""},
	{CONSTRAINT_TYPE_TRANSFORM, "TRANSFORM", ICON_CONSTRAINT_DATA, "Transformation", ""},
	{0, "", 0, N_("Tracking"), ""},
	{CONSTRAINT_TYPE_CLAMPTO, "CLAMP_TO", ICON_CONSTRAINT_DATA, "Clamp To", ""},
	{CONSTRAINT_TYPE_DAMPTRACK, "DAMPED_TRACK", ICON_CONSTRAINT_DATA, "Damped Track",
	                            "Tracking by taking the shortest path"},
	{CONSTRAINT_TYPE_KINEMATIC, "IK", ICON_CONSTRAINT_DATA, "Inverse Kinematics", ""},
	{CONSTRAINT_TYPE_LOCKTRACK, "LOCKED_TRACK", ICON_CONSTRAINT_DATA, "Locked Track",
	                            "Tracking along a single axis"},
	{CONSTRAINT_TYPE_SPLINEIK, "SPLINE_IK", ICON_CONSTRAINT_DATA, "Spline IK", ""},
	{CONSTRAINT_TYPE_STRETCHTO, "STRETCH_TO", ICON_CONSTRAINT_DATA, "Stretch To", ""},
	{CONSTRAINT_TYPE_TRACKTO, "TRACK_TO", ICON_CONSTRAINT_DATA, "Track To",
	                          "Legacy tracking constraint prone to twisting artifacts"},
	{0, "", 0, N_("Relationship"), ""},
	{CONSTRAINT_TYPE_ACTION, "ACTION", ICON_CONSTRAINT_DATA, "Action", ""},
	{CONSTRAINT_TYPE_CHILDOF, "CHILD_OF", ICON_CONSTRAINT_DATA, "Child Of", ""},
	{CONSTRAINT_TYPE_MINMAX, "FLOOR", ICON_CONSTRAINT_DATA, "Floor", ""},
	{CONSTRAINT_TYPE_FOLLOWPATH, "FOLLOW_PATH", ICON_CONSTRAINT_DATA, "Follow Path", ""},
	{CONSTRAINT_TYPE_PIVOT, "PIVOT", ICON_CONSTRAINT_DATA, "Pivot", ""},
	{CONSTRAINT_TYPE_RIGIDBODYJOINT, "RIGID_BODY_JOINT", ICON_CONSTRAINT_DATA, "Rigid Body Joint", ""},
	{CONSTRAINT_TYPE_PYTHON, "SCRIPT", ICON_CONSTRAINT_DATA, "Script", ""},
	{CONSTRAINT_TYPE_SHRINKWRAP, "SHRINKWRAP", ICON_CONSTRAINT_DATA, "Shrinkwrap", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem target_space_pchan_items[] = {
	{CONSTRAINT_SPACE_WORLD,    "WORLD", 0, "World Space",
	                            "The transformation of the target is evaluated relative to the world "
	                            "coordinate system"},
	{CONSTRAINT_SPACE_POSE,     "POSE", 0, "Pose Space",
	                            "The transformation of the target is only evaluated in the Pose Space, "
	                            "the target armature object transformation is ignored"},
	{CONSTRAINT_SPACE_PARLOCAL, "LOCAL_WITH_PARENT", 0, "Local With Parent",
	                            "The transformation of the target bone is evaluated relative its local "
	                            "coordinate system, with the parent transformation added"},
	{CONSTRAINT_SPACE_LOCAL,    "LOCAL", 0, "Local Space",
	                            "The transformation of the target is evaluated relative to its local "
	                            "coordinate system"},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem owner_space_pchan_items[] = {
	{CONSTRAINT_SPACE_WORLD,    "WORLD", 0, "World Space",
	                            "The constraint is applied relative to the world coordinate system"},
	{CONSTRAINT_SPACE_POSE,     "POSE", 0, "Pose Space",
	                            "The constraint is applied in Pose Space, the object transformation is ignored"},
	{CONSTRAINT_SPACE_PARLOCAL, "LOCAL_WITH_PARENT", 0, "Local With Parent",
	                            "The constraint is applied relative to the local coordinate system of the object, "
	                            "with the parent transformation added"},
	{CONSTRAINT_SPACE_LOCAL,    "LOCAL", 0, "Local Space",
	                            "The constraint is applied relative to the local coordinate sytem of the object"},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

static EnumPropertyItem space_object_items[] = {
	{CONSTRAINT_SPACE_WORLD, "WORLD", 0, "World Space",
	                         "The transformation of the target is evaluated relative to the world coordinate system"},
	{CONSTRAINT_SPACE_LOCAL, "LOCAL", 0, "Local Space",
	                         "The transformation of the target is evaluated relative to its local coordinate system"},
	{0, NULL, 0, NULL, NULL}
};

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"


static StructRNA *rna_ConstraintType_refine(struct PointerRNA *ptr)
{
	bConstraint *con = (bConstraint *)ptr->data;

	switch (con->type) {
		case CONSTRAINT_TYPE_CHILDOF:
			return &RNA_ChildOfConstraint;
		case CONSTRAINT_TYPE_TRACKTO:
			return &RNA_TrackToConstraint;
		case CONSTRAINT_TYPE_KINEMATIC:
			return &RNA_KinematicConstraint;
		case CONSTRAINT_TYPE_FOLLOWPATH:
			return &RNA_FollowPathConstraint;
		case CONSTRAINT_TYPE_ROTLIKE:
			return &RNA_CopyRotationConstraint;
		case CONSTRAINT_TYPE_LOCLIKE:
			return &RNA_CopyLocationConstraint;
		case CONSTRAINT_TYPE_SIZELIKE:
			return &RNA_CopyScaleConstraint;
		case CONSTRAINT_TYPE_SAMEVOL:
			return &RNA_MaintainVolumeConstraint;
		case CONSTRAINT_TYPE_PYTHON:
			return &RNA_PythonConstraint;
		case CONSTRAINT_TYPE_ACTION:
			return &RNA_ActionConstraint;
		case CONSTRAINT_TYPE_LOCKTRACK:
			return &RNA_LockedTrackConstraint;
		case CONSTRAINT_TYPE_STRETCHTO:
			return &RNA_StretchToConstraint;
		case CONSTRAINT_TYPE_MINMAX:
			return &RNA_FloorConstraint;
		case CONSTRAINT_TYPE_RIGIDBODYJOINT:
			return &RNA_RigidBodyJointConstraint;
		case CONSTRAINT_TYPE_CLAMPTO:
			return &RNA_ClampToConstraint;
		case CONSTRAINT_TYPE_TRANSFORM:
			return &RNA_TransformConstraint;
		case CONSTRAINT_TYPE_ROTLIMIT:
			return &RNA_LimitRotationConstraint;
		case CONSTRAINT_TYPE_LOCLIMIT:
			return &RNA_LimitLocationConstraint;
		case CONSTRAINT_TYPE_SIZELIMIT:
			return &RNA_LimitScaleConstraint;
		case CONSTRAINT_TYPE_DISTLIMIT:
			return &RNA_LimitDistanceConstraint;
		case CONSTRAINT_TYPE_SHRINKWRAP:
			return &RNA_ShrinkwrapConstraint;
		case CONSTRAINT_TYPE_DAMPTRACK:
			return &RNA_DampedTrackConstraint;
		case CONSTRAINT_TYPE_SPLINEIK:
			return &RNA_SplineIKConstraint;
		case CONSTRAINT_TYPE_TRANSLIKE:
			return &RNA_CopyTransformsConstraint;
		case CONSTRAINT_TYPE_PIVOT:
			return &RNA_PivotConstraint;
		case CONSTRAINT_TYPE_FOLLOWTRACK:
			return &RNA_FollowTrackConstraint;
		case CONSTRAINT_TYPE_CAMERASOLVER:
			return &RNA_CameraSolverConstraint;
		case CONSTRAINT_TYPE_OBJECTSOLVER:
			return &RNA_ObjectSolverConstraint;
		default:
			return &RNA_UnknownType;
	}
}

static void rna_Constraint_name_set(PointerRNA *ptr, const char *value)
{
	bConstraint *con = ptr->data;
	char oldname[sizeof(con->name)];
	
	/* make a copy of the old name first */
	BLI_strncpy(oldname, con->name, sizeof(con->name));
	
	/* copy the new name into the name slot */
	BLI_strncpy_utf8(con->name, value, sizeof(con->name));
	
	/* make sure name is unique */
	if (ptr->id.data) {
		Object *ob = ptr->id.data;
		ListBase *list = get_constraint_lb(ob, con, NULL);
		
		/* if we have the list, check for unique name, otherwise give up */
		if (list)
			unique_constraint_name(con, list);
	}
	
	/* fix all the animation data which may link to this */
	BKE_all_animdata_fix_paths_rename(NULL, "constraints", oldname, con->name);
}

static char *rna_Constraint_path(PointerRNA *ptr)
{
	Object *ob = ptr->id.data;
	bConstraint *con = ptr->data;
	bPoseChannel *pchan;
	ListBase *lb = get_constraint_lb(ob, con, &pchan);

	if (lb == NULL)
		printf("rna_Constraint_path: internal error, constraint '%s' not found in object '%s'\n",
		       con->name, ob->id.name);

	if (pchan) {
		return BLI_sprintfN("pose.bones[\"%s\"].constraints[\"%s\"]", pchan->name, con->name);
	}
	
	return BLI_sprintfN("constraints[\"%s\"]", con->name);
}

static void rna_Constraint_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ED_object_constraint_update(ptr->id.data);
}

static void rna_Constraint_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ED_object_constraint_dependency_update(bmain, scene, ptr->id.data);
}

static void rna_Constraint_influence_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob = ptr->id.data;

	if (ob->pose)
		ob->pose->flag |= (POSE_LOCKED | POSE_DO_UNLOCK);
	
	rna_Constraint_update(bmain, scene, ptr);
}

static void rna_Constraint_ik_type_set(struct PointerRNA *ptr, int value)
{
	bConstraint *con = ptr->data;
	bKinematicConstraint *ikdata = con->data;

	if (ikdata->type != value) {
		/* the type of IK constraint has changed, set suitable default values */
		/* in case constraints reuse same fields incompatible */
		switch (value) {
			case CONSTRAINT_IK_COPYPOSE:
				break;
			case CONSTRAINT_IK_DISTANCE:
				break;
		}
		ikdata->type = value;
	}
}

static EnumPropertyItem *rna_Constraint_owner_space_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                          PropertyRNA *UNUSED(prop), int *UNUSED(free))
{
	Object *ob = (Object *)ptr->id.data;
	bConstraint *con = (bConstraint *)ptr->data;
	
	if (BLI_findindex(&ob->constraints, con) == -1)
		return owner_space_pchan_items;
	else /* object */
		return space_object_items;
}

static EnumPropertyItem *rna_Constraint_target_space_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                           PropertyRNA *UNUSED(prop), int *UNUSED(free))
{
	bConstraint *con = (bConstraint *)ptr->data;
	bConstraintTypeInfo *cti = constraint_get_typeinfo(con);
	ListBase targets = {NULL, NULL};
	bConstraintTarget *ct;
	
	if (cti && cti->get_constraint_targets) {
		cti->get_constraint_targets(con, &targets);
		
		for (ct = targets.first; ct; ct = ct->next)
			if (ct->tar && ct->tar->type == OB_ARMATURE)
				break;
		
		if (cti->flush_constraint_targets)
			cti->flush_constraint_targets(con, &targets, 1);

		if (ct)
			return target_space_pchan_items;
	}

	return space_object_items;
}

static void rna_ActionConstraint_minmax_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	bConstraint *con = (bConstraint *)ptr->data;
	bActionConstraint *acon = (bActionConstraint *)con->data;

	/* 0, 1, 2 = magic numbers for rotX, rotY, rotZ */
	if (ELEM3(acon->type, 0, 1, 2)) {
		*min = -180.0f;
		*max = 180.0f;
	}
	else {
		*min = -1000.f;
		*max = 1000.f;
	}
}

static int rna_SplineIKConstraint_joint_bindings_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	bConstraint *con = (bConstraint *)ptr->data;
	bSplineIKConstraint *ikData = (bSplineIKConstraint *)con->data;

	if (ikData)
		length[0] = ikData->numpoints;
	else
		length[0] = 256;  /* for raw_access, untested */

	return length[0];
}

static void rna_SplineIKConstraint_joint_bindings_get(PointerRNA *ptr, float *values)
{
	bConstraint *con = (bConstraint *)ptr->data;
	bSplineIKConstraint *ikData = (bSplineIKConstraint *)con->data;
	
	memcpy(values, ikData->points, ikData->numpoints * sizeof(float));
}

static void rna_SplineIKConstraint_joint_bindings_set(PointerRNA *ptr, const float *values)
{
	bConstraint *con = (bConstraint *)ptr->data;
	bSplineIKConstraint *ikData = (bSplineIKConstraint *)con->data;
	
	memcpy(ikData->points, values, ikData->numpoints * sizeof(float));
}

static int rna_Constraint_cameraObject_poll(PointerRNA *ptr, PointerRNA value)
{
	Object *ob = (Object *)value.data;

	if (ob) {
		if (ob->type == OB_CAMERA && ob != (Object *)ptr->id.data) {
			return 1;
		}
	}

	return 0;
}

static void rna_Constraint_followTrack_camera_set(PointerRNA *ptr, PointerRNA value)
{
	bConstraint *con = (bConstraint *)ptr->data;
	bFollowTrackConstraint *data = (bFollowTrackConstraint *)con->data;
	Object *ob = (Object *)value.data;

	if (ob) {
		if (ob->type == OB_CAMERA && ob != (Object *)ptr->id.data) {
			data->camera = ob;
		}
	}
	else {
		data->camera = NULL;
	}
}

static void rna_Constraint_followTrack_depthObject_set(PointerRNA *ptr, PointerRNA value)
{
	bConstraint *con = (bConstraint *)ptr->data;
	bFollowTrackConstraint *data = (bFollowTrackConstraint *)con->data;
	Object *ob = (Object *)value.data;

	if (ob) {
		if (ob->type == OB_MESH && ob != (Object *)ptr->id.data) {
			data->depth_ob = ob;
		}
	}
	else {
		data->depth_ob = NULL;
	}
}

static int rna_Constraint_followTrack_depthObject_poll(PointerRNA *ptr, PointerRNA value)
{
	Object *ob = (Object *)value.data;

	if (ob) {
		if (ob->type == OB_MESH && ob != (Object *)ptr->id.data) {
			return 1;
		}
	}

	return 0;
}

static void rna_Constraint_objectSolver_camera_set(PointerRNA *ptr, PointerRNA value)
{
	bConstraint *con = (bConstraint *)ptr->data;
	bObjectSolverConstraint *data = (bObjectSolverConstraint *)con->data;
	Object *ob = (Object *)value.data;

	if (ob) {
		if (ob->type == OB_CAMERA && ob != (Object *)ptr->id.data) {
			data->camera = ob;
		}
	}
	else {
		data->camera = NULL;
	}
}

#else

EnumPropertyItem constraint_distance_items[] = {
	{LIMITDIST_INSIDE, "LIMITDIST_INSIDE", 0, "Inside",
	                   "The object is constrained inside a virtual sphere around the target object, "
	                   "with a radius defined by the limit distance"},
	{LIMITDIST_OUTSIDE, "LIMITDIST_OUTSIDE", 0, "Outside",
	                    "The object is constrained outside a virtual sphere around the target object, "
	                    "with a radius defined by the limit distance"},
	{LIMITDIST_ONSURFACE, "LIMITDIST_ONSURFACE", 0, "On Surface",
	                      "The object is constrained on the surface of a virtual sphere around the target object, "
	                      "with a radius defined by the limit distance"},
	{0, NULL, 0, NULL, NULL}
};


static void rna_def_constrainttarget(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ConstraintTarget", NULL);
	RNA_def_struct_ui_text(srna, "Constraint Target", "Target object for multi-target constraints");
	RNA_def_struct_sdna(srna, "bConstraintTarget");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	/* space, flag and type still to do  */
}

static void rna_def_constraint_childof(BlenderRNA *brna)
{
	static int rna_matrix_dimsize_4x4[] = {4, 4};
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ChildOfConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Child Of Constraint", "Create constraint-based parent-child relationship");
	RNA_def_struct_sdna_from(srna, "bChildOfConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "use_location_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CHILDOF_LOCX);
	RNA_def_property_ui_text(prop, "Location X", "Use X Location of Parent");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_location_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CHILDOF_LOCY);
	RNA_def_property_ui_text(prop, "Location Y", "Use Y Location of Parent");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_location_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CHILDOF_LOCZ);
	RNA_def_property_ui_text(prop, "Location Z", "Use Z Location of Parent");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_rotation_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CHILDOF_ROTX);
	RNA_def_property_ui_text(prop, "Rotation X", "Use X Rotation of Parent");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_rotation_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CHILDOF_ROTY);
	RNA_def_property_ui_text(prop, "Rotation Y", "Use Y Rotation of Parent");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_rotation_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CHILDOF_ROTZ);
	RNA_def_property_ui_text(prop, "Rotation Z", "Use Z Rotation of Parent");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_scale_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CHILDOF_SIZEX);
	RNA_def_property_ui_text(prop, "Scale X", "Use X Scale of Parent");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_scale_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CHILDOF_SIZEY);
	RNA_def_property_ui_text(prop, "Scale Y", "Use Y Scale of Parent");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_scale_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CHILDOF_SIZEZ);
	RNA_def_property_ui_text(prop, "Scale Z", "Use Z Scale of Parent");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "inverse_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "invmat");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Inverse Matrix", "Transformation matrix to apply before");
	
}

static void rna_def_constraint_python(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "PythonConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Python Constraint", "Use Python script for constraint evaluation");
	RNA_def_struct_sdna_from(srna, "bPythonConstraint", "data");

	prop = RNA_def_property(srna, "targets", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "targets", NULL);
	RNA_def_property_struct_type(prop, "ConstraintTarget");
	RNA_def_property_ui_text(prop, "Targets", "Target Objects");

	prop = RNA_def_property(srna, "target_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "tarnum");
	RNA_def_property_ui_text(prop, "Number of Targets", "Usually only 1-3 are needed");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "text", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Script", "The text object that contains the Python script");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_targets", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PYCON_USETARGETS);
	RNA_def_property_ui_text(prop, "Use Targets", "Use the targets indicated in the constraint panel");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "has_script_error", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PYCON_SCRIPTERROR);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Script Error", "The linked Python script has thrown an error");
}

static void rna_def_constraint_kinematic(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem constraint_ik_axisref_items[] = {
		{0, "BONE", 0, "Bone", ""},
		{CONSTRAINT_IK_TARGETAXIS, "TARGET", 0, "Target", ""},
		{0, NULL, 0, NULL, NULL},
	};

	static EnumPropertyItem constraint_ik_type_items[] = {
		{CONSTRAINT_IK_COPYPOSE, "COPY_POSE", 0, "Copy Pose", ""},
		{CONSTRAINT_IK_DISTANCE, "DISTANCE", 0, "Distance", ""},
		{0, NULL, 0, NULL, NULL},
	};

	srna = RNA_def_struct(brna, "KinematicConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Kinematic Constraint", "Inverse Kinematics");
	RNA_def_struct_sdna_from(srna, "bKinematicConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, 10000);
	RNA_def_property_ui_text(prop, "Iterations", "Maximum number of solving iterations");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "pole_target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "poletar");
	RNA_def_property_ui_text(prop, "Pole Target", "Object for pole rotation");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "pole_subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "polesubtarget");
	RNA_def_property_ui_text(prop, "Pole Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "pole_angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "poleangle");
	RNA_def_property_range(prop, -M_PI, M_PI);
	RNA_def_property_ui_text(prop, "Pole Angle", "Pole rotation offset");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 1.f);
	RNA_def_property_ui_text(prop, "Weight", "For Tree-IK: Weight of position control for this target");

	prop = RNA_def_property(srna, "orient_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "orientweight");
	RNA_def_property_range(prop, 0.01, 1.f);
	RNA_def_property_ui_text(prop, "Orientation Weight", "For Tree-IK: Weight of orientation control for this target");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "chain_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "rootbone");
	RNA_def_property_range(prop, 0, 255);
	RNA_def_property_ui_text(prop, "Chain Length", "How many bones are included in the IK effect - 0 uses all bones");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "use_tail", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_IK_TIP);
	RNA_def_property_ui_text(prop, "Use Tail", "Include bone's tail as last element in chain");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "reference_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, constraint_ik_axisref_items);
	RNA_def_property_ui_text(prop, "Axis Reference",
	                         "Constraint axis Lock options relative to Bone or Target reference");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "use_location", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_IK_POS);
	RNA_def_property_ui_text(prop, "Position", "Chain follows position of target");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "lock_location_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CONSTRAINT_IK_NO_POS_X);
	RNA_def_property_ui_text(prop, "Lock X Pos", "Constraint position along X axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "lock_location_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CONSTRAINT_IK_NO_POS_Y);
	RNA_def_property_ui_text(prop, "Lock Y Pos", "Constraint position along Y axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "lock_location_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CONSTRAINT_IK_NO_POS_Z);
	RNA_def_property_ui_text(prop, "Lock Z Pos", "Constraint position along Z axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "use_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_IK_ROT);
	RNA_def_property_ui_text(prop, "Rotation", "Chain follows rotation of target");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "lock_rotation_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CONSTRAINT_IK_NO_ROT_X);
	RNA_def_property_ui_text(prop, "Lock X Rot", "Constraint rotation along X axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "lock_rotation_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CONSTRAINT_IK_NO_ROT_Y);
	RNA_def_property_ui_text(prop, "Lock Y Rot", "Constraint rotation along Y axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "lock_rotation_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CONSTRAINT_IK_NO_ROT_Z);
	RNA_def_property_ui_text(prop, "Lock Z Rot", "Constraint rotation along Z axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "use_target", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CONSTRAINT_IK_AUTO);
	RNA_def_property_ui_text(prop, "Target", "Disable for targetless IK");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "use_stretch", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_IK_STRETCH);
	RNA_def_property_ui_text(prop, "Stretch", "Enable IK Stretching");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "ik_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_funcs(prop, NULL, "rna_Constraint_ik_type_set", NULL);
	RNA_def_property_enum_items(prop, constraint_ik_type_items);
	RNA_def_property_ui_text(prop, "IK Type", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "limit_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, constraint_distance_items);
	RNA_def_property_ui_text(prop, "Limit Mode", "Distances in relation to sphere of influence to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_range(prop, 0.0, 100.f);
	RNA_def_property_ui_text(prop, "Distance", "Radius of limiting sphere");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_track_to(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem track_items[] = {
		{TRACK_X, "TRACK_X", 0, "X", ""},
		{TRACK_Y, "TRACK_Y", 0, "Y", ""},
		{TRACK_Z, "TRACK_Z", 0, "Z", ""},
		{TRACK_nX, "TRACK_NEGATIVE_X", 0, "-X", ""},
		{TRACK_nY, "TRACK_NEGATIVE_Y", 0, "-Y", ""},
		{TRACK_nZ, "TRACK_NEGATIVE_Z", 0, "-Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem up_items[] = {
		{TRACK_X, "UP_X", 0, "X", ""},
		{TRACK_Y, "UP_Y", 0, "Y", ""},
		{TRACK_Z, "UP_Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "TrackToConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Track To Constraint", "Aim the constrained object toward the target");

	prop = RNA_def_property(srna, "head_tail", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, "bConstraint", "headtail");
	RNA_def_property_ui_text(prop, "Head/Tail", "Target along length of bone: Head=0, Tail=1");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	RNA_def_struct_sdna_from(srna, "bTrackToConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "track_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "reserved1");
	RNA_def_property_enum_items(prop, track_items);
	RNA_def_property_ui_text(prop, "Track Axis", "Axis that points to the target object");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "up_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "reserved2");
	RNA_def_property_enum_items(prop, up_items);
	RNA_def_property_ui_text(prop, "Up Axis", "Axis that points upward");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_target_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", TARGET_Z_UP);
	RNA_def_property_ui_text(prop, "Target Z", "Target's Z axis, not World Z axis, will constraint the Up direction");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_locate_like(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "CopyLocationConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Copy Location Constraint", "Copy the location of the target");

	prop = RNA_def_property(srna, "head_tail", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, "bConstraint", "headtail");
	RNA_def_property_ui_text(prop, "Head/Tail", "Target along length of bone: Head=0, Tail=1");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	RNA_def_struct_sdna_from(srna, "bLocateLikeConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LOCLIKE_X);
	RNA_def_property_ui_text(prop, "Copy X", "Copy the target's X location");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LOCLIKE_Y);
	RNA_def_property_ui_text(prop, "Copy Y", "Copy the target's Y location");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LOCLIKE_Z);
	RNA_def_property_ui_text(prop, "Copy Z", "Copy the target's Z location");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "invert_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LOCLIKE_X_INVERT);
	RNA_def_property_ui_text(prop, "Invert X", "Invert the X location");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "invert_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LOCLIKE_Y_INVERT);
	RNA_def_property_ui_text(prop, "Invert Y", "Invert the Y location");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "invert_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LOCLIKE_Z_INVERT);
	RNA_def_property_ui_text(prop, "Invert Z", "Invert the Z location");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LOCLIKE_OFFSET);
	RNA_def_property_ui_text(prop, "Offset", "Add original location into copied location");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_rotate_like(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "CopyRotationConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Copy Rotation Constraint", "Copy the rotation of the target");
	RNA_def_struct_sdna_from(srna, "bRotateLikeConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ROTLIKE_X);
	RNA_def_property_ui_text(prop, "Copy X", "Copy the target's X rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ROTLIKE_Y);
	RNA_def_property_ui_text(prop, "Copy Y", "Copy the target's Y rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ROTLIKE_Z);
	RNA_def_property_ui_text(prop, "Copy Z", "Copy the target's Z rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "invert_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ROTLIKE_X_INVERT);
	RNA_def_property_ui_text(prop, "Invert X", "Invert the X rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "invert_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ROTLIKE_Y_INVERT);
	RNA_def_property_ui_text(prop, "Invert Y", "Invert the Y rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "invert_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ROTLIKE_Z_INVERT);
	RNA_def_property_ui_text(prop, "Invert Z", "Invert the Z rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ROTLIKE_OFFSET);
	RNA_def_property_ui_text(prop, "Offset", "Add original rotation into copied rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_size_like(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "CopyScaleConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Copy Scale Constraint", "Copy the scale of the target");
	RNA_def_struct_sdna_from(srna, "bSizeLikeConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SIZELIKE_X);
	RNA_def_property_ui_text(prop, "Copy X", "Copy the target's X scale");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SIZELIKE_Y);
	RNA_def_property_ui_text(prop, "Copy Y", "Copy the target's Y scale");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SIZELIKE_Z);
	RNA_def_property_ui_text(prop, "Copy Z", "Copy the target's Z scale");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SIZELIKE_OFFSET);
	RNA_def_property_ui_text(prop, "Offset", "Add original scale into copied scale");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_same_volume(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem volume_items[] = {
		{SAMEVOL_X, "SAMEVOL_X", 0, "X", ""},
		{SAMEVOL_Y, "SAMEVOL_Y", 0, "Y", ""},
		{SAMEVOL_Z, "SAMEVOL_Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "MaintainVolumeConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Maintain Volume Constraint",
	                       "Maintain a constant volume along a single scaling axis");
	RNA_def_struct_sdna_from(srna, "bSameVolumeConstraint", "data");

	prop = RNA_def_property(srna, "free_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, volume_items);
	RNA_def_property_ui_text(prop, "Free Axis", "The free scaling axis of the object");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "volume", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0.001, 100.f);
	RNA_def_property_ui_text(prop, "Volume", "Volume of the bone at rest");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

}

static void rna_def_constraint_transform_like(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "CopyTransformsConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Copy Transforms Constraint", "Copy all the transforms of the target");
	
	prop = RNA_def_property(srna, "head_tail", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, "bConstraint", "headtail");
	RNA_def_property_ui_text(prop, "Head/Tail", "Target along length of bone: Head=0, Tail=1");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	RNA_def_struct_sdna_from(srna, "bTransLikeConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
}

static void rna_def_constraint_minmax(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem minmax_items[] = {
		{TRACK_X, "FLOOR_X", 0, "X", ""},
		{TRACK_Y, "FLOOR_Y", 0, "Y", ""},
		{TRACK_Z, "FLOOR_Z", 0, "Z", ""},
		{TRACK_nX, "FLOOR_NEGATIVE_X", 0, "-X", ""},
		{TRACK_nY, "FLOOR_NEGATIVE_Y", 0, "-Y", ""},
		{TRACK_nZ, "FLOOR_NEGATIVE_Z", 0, "-Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "FloorConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Floor Constraint", "Use the target object for location limitation");
	RNA_def_struct_sdna_from(srna, "bMinMaxConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "floor_location", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "minmaxflag");
	RNA_def_property_enum_items(prop, minmax_items);
	RNA_def_property_ui_text(prop, "Floor Location", "Location of target that object will not pass through");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_sticky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MINMAX_STICKY);
	RNA_def_property_ui_text(prop, "Sticky", "Immobilize object while constrained");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MINMAX_USEROT);
	RNA_def_property_ui_text(prop, "Use Rotation", "Use the target's rotation to determine floor");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0.0, 100.f);
	RNA_def_property_ui_text(prop, "Offset", "Offset of floor from object origin");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_action(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem transform_channel_items[] = {
		{20, "LOCATION_X", 0, "X Location", ""},
		{21, "LOCATION_Y", 0, "Y Location", ""},
		{22, "LOCATION_Z", 0, "Z Location", ""},
		{00, "ROTATION_X", 0, "X Rotation", ""},
		{01, "ROTATION_Y", 0, "Y Rotation", ""},
		{02, "ROTATION_Z", 0, "Z Rotation", ""},
		{10, "SCALE_X", 0, "X Scale", ""},
		{11, "SCALE_Y", 0, "Y Scale", ""},
		{12, "SCALE_Z", 0, "Z Scale", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ActionConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Action Constraint", "Map an action to the transform axes of a bone");
	RNA_def_struct_sdna_from(srna, "bActionConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "transform_channel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, transform_channel_items);
	RNA_def_property_ui_text(prop, "Transform Channel",
	                         "Transformation channel from the target that is used to key the Action");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "act");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Action_id_poll");
	RNA_def_property_ui_text(prop, "Action", "The constraining action");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_bone_object_action", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACTCON_BONE_USE_OBJECT_ACTION);
	RNA_def_property_ui_text(prop, "Object Action",
	                         "Bones only: apply the object's transformation channels of the action "
	                         "to the constrained bone, instead of bone's channels");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "start");
	RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Start Frame", "First frame of the Action to use");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "end");
	RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "End Frame", "Last frame of the Action to use");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_range(prop, -1000.f, 1000.f);
	RNA_def_property_ui_text(prop, "Maximum", "Maximum value for target channel range");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_ActionConstraint_minmax_range");

	prop = RNA_def_property(srna, "min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_range(prop, -1000.f, 1000.f);
	RNA_def_property_ui_text(prop, "Minimum", "Minimum value for target channel range");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_ActionConstraint_minmax_range");
}

static void rna_def_constraint_locked_track(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem locktrack_items[] = {
		{TRACK_X, "TRACK_X", 0, "X", ""},
		{TRACK_Y, "TRACK_Y", 0, "Y", ""},
		{TRACK_Z, "TRACK_Z", 0, "Z", ""},
		{TRACK_nX, "TRACK_NEGATIVE_X", 0, "-X", ""},
		{TRACK_nY, "TRACK_NEGATIVE_Y", 0, "-Y", ""},
		{TRACK_nZ, "TRACK_NEGATIVE_Z", 0, "-Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem lock_items[] = {
		{TRACK_X, "LOCK_X", 0, "X", ""},
		{TRACK_Y, "LOCK_Y", 0, "Y", ""},
		{TRACK_Z, "LOCK_Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "LockedTrackConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Locked Track Constraint",
	                       "Point toward the target along the track axis, while locking the other axis");
	
	prop = RNA_def_property(srna, "head_tail", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, "bConstraint", "headtail");
	RNA_def_property_ui_text(prop, "Head/Tail", "Target along length of bone: Head=0, Tail=1");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	RNA_def_struct_sdna_from(srna, "bLockTrackConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "track_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "trackflag");
	RNA_def_property_enum_items(prop, locktrack_items);
	RNA_def_property_ui_text(prop, "Track Axis", "Axis that points to the target object");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "lock_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "lockflag");
	RNA_def_property_enum_items(prop, lock_items);
	RNA_def_property_ui_text(prop, "Locked Axis", "Axis that points upward");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_follow_path(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem forwardpath_items[] = {
		{TRACK_X, "FORWARD_X", 0, "X", ""},
		{TRACK_Y, "FORWARD_Y", 0, "Y", ""},
		{TRACK_Z, "FORWARD_Z", 0, "Z", ""},
		{TRACK_nX, "TRACK_NEGATIVE_X", 0, "-X", ""},
		{TRACK_nY, "TRACK_NEGATIVE_Y", 0, "-Y", ""},
		{TRACK_nZ, "TRACK_NEGATIVE_Z", 0, "-Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem pathup_items[] = {
		{TRACK_X, "UP_X", 0, "X", ""},
		{TRACK_Y, "UP_Y", 0, "Y", ""},
		{TRACK_Z, "UP_Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "FollowPathConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Follow Path Constraint", "Lock motion to the target path");
	RNA_def_struct_sdna_from(srna, "bFollowPathConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Curve_object_poll");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_TIME);
	RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Offset", "Offset from the position corresponding to the time frame");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "offset_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "offset_fac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Offset Factor", "Percentage value defining target position along length of bone");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "forward_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "trackflag");
	RNA_def_property_enum_items(prop, forwardpath_items);
	RNA_def_property_ui_text(prop, "Forward Axis", "Axis that points forward along the path");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "up_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "upflag");
	RNA_def_property_enum_items(prop, pathup_items);
	RNA_def_property_ui_text(prop, "Up Axis", "Axis that points upward");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_curve_follow", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "followflag", FOLLOWPATH_FOLLOW);
	RNA_def_property_ui_text(prop, "Follow Curve", "Object will follow the heading and banking of the curve");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_fixed_location", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "followflag", FOLLOWPATH_STATIC);
	RNA_def_property_ui_text(prop, "Fixed Position",
	                         "Object will stay locked to a single point somewhere along the length of the curve "
	                         "regardless of time");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_curve_radius", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "followflag", FOLLOWPATH_RADIUS);
	RNA_def_property_ui_text(prop, "Curve Radius", "Object is scaled by the curve radius");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_stretch_to(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem volume_items[] = {
		{VOLUME_XZ, "VOLUME_XZX", 0, "XZ", ""},
		{VOLUME_X, "VOLUME_X", 0, "X", ""},
		{VOLUME_Z, "VOLUME_Z", 0, "Z", ""},
		{NO_VOLUME, "NO_VOLUME", 0, "None", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem plane_items[] = {
		{PLANE_X, "PLANE_X", 0, "X", "Keep X Axis"},
		{PLANE_Z, "PLANE_Z", 0, "Z", "Keep Z Axis"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "StretchToConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Stretch To Constraint", "Stretch to meet the target object");

	prop = RNA_def_property(srna, "head_tail", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, "bConstraint", "headtail");
	RNA_def_property_ui_text(prop, "Head/Tail", "Target along length of bone: Head=0, Tail=1");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	RNA_def_struct_sdna_from(srna, "bStretchToConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
	
	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "volume", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "volmode");
	RNA_def_property_enum_items(prop, volume_items);
	RNA_def_property_ui_text(prop, "Maintain Volume", "Maintain the object's volume as it stretches");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "keep_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "plane");
	RNA_def_property_enum_items(prop, plane_items);
	RNA_def_property_ui_text(prop, "Keep Axis", "Axis to maintain during stretch");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "rest_length", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "orglength");
	RNA_def_property_range(prop, 0.0, 100.f);
	RNA_def_property_ui_text(prop, "Original Length", "Length at rest position");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "bulge", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.f);
	RNA_def_property_ui_text(prop, "Volume Variation", "Factor between volume variation and stretching");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_rigid_body_joint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem pivot_items[] = {
		{CONSTRAINT_RB_BALL, "BALL", 0, "Ball", "Allow rotations around all axes"},
		{CONSTRAINT_RB_HINGE, "HINGE", 0, "Hinge", "Work in one plane, allow rotations around one axis only"},
		{CONSTRAINT_RB_CONETWIST, "CONE_TWIST", 0, "Cone Twist",
		                          "Allow rotations around all axes with limits for the cone and twist axes"},
		{CONSTRAINT_RB_GENERIC6DOF, "GENERIC_6_DOF", 0, "Generic 6 DoF",
		                            "No constraints by default, limits can be set individually"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "RigidBodyJointConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Rigid Body Joint Constraint", "For use with the Game Engine");
	RNA_def_struct_sdna_from(srna, "bRigidBodyJointConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "child", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Child Object", "Child object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "pivot_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, pivot_items);
	RNA_def_property_ui_text(prop, "Pivot Type", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "pivot_x", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "pivX");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Pivot X", "Offset pivot on X");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "pivot_y", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "pivY");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Pivot Y", "Offset pivot on Y");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "pivot_z", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "pivZ");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Pivot Z", "Offset pivot on Z");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "axis_x", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "axX");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_ui_text(prop, "Axis X", "Rotate pivot on X axis in degrees");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "axis_y", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "axY");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_ui_text(prop, "Axis Y", "Rotate pivot on Y axis in degrees");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "axis_z", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "axZ");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_ui_text(prop, "Axis Z", "Rotate pivot on Z axis in degrees");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_linked_collision", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_DISABLE_LINKED_COLLISION);
	RNA_def_property_ui_text(prop, "Disable Linked Collision", "Disable collision between linked bodies");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "show_pivot", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_DRAW_PIVOT);
	RNA_def_property_ui_text(prop, "Draw Pivot", "Display the pivot point and rotation in 3D view");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	
	/* Limits */
	/* Limit Min/Max */
	prop = RNA_def_property(srna, "limit_min_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "minLimit[0]");
	RNA_def_property_ui_text(prop, "Minimum Limit X", "");

	prop = RNA_def_property(srna, "limit_min_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "minLimit[1]");
	RNA_def_property_ui_text(prop, "Minimum Limit Y", "");

	prop = RNA_def_property(srna, "limit_min_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "minLimit[2]");
	RNA_def_property_ui_text(prop, "Minimum Limit Z", "");

	prop = RNA_def_property(srna, "limit_max_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxLimit[0]");
	RNA_def_property_ui_text(prop, "Maximum Limit X", "");

	prop = RNA_def_property(srna, "limit_max_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxLimit[1]");
	RNA_def_property_ui_text(prop, "Maximum Limit Y", "");

	prop = RNA_def_property(srna, "limit_max_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxLimit[2]");
	RNA_def_property_ui_text(prop, "Maximum Limit Z", "");

	/* Limit Min/Max for angle */
	prop = RNA_def_property(srna, "limit_angle_min_x", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "minLimit[3]");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_ui_text(prop, "Minimum Angular Limit X", "");

	prop = RNA_def_property(srna, "limit_angle_min_y", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "minLimit[4]");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_ui_text(prop, "Minimum Angular Limit Y", "");

	prop = RNA_def_property(srna, "limit_angle_min_z", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "minLimit[5]");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_ui_text(prop, "Minimum Angular Limit Z", "");

	prop = RNA_def_property(srna, "limit_angle_max_x", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "maxLimit[3]");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_ui_text(prop, "Maximum Angular Limit X", "");
	
	prop = RNA_def_property(srna, "limit_angle_max_y", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "maxLimit[4]");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_ui_text(prop, "Maximum Angular Limit Y", "");

	prop = RNA_def_property(srna, "limit_angle_max_z", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "maxLimit[5]");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_ui_text(prop, "Maximum Angular Limit Z", "");

	/* Limit Booleans */
	prop = RNA_def_property(srna, "use_limit_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 1);
	RNA_def_property_ui_text(prop, "Limit X", "Use minimum/maximum X limit");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_limit_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 2);
	RNA_def_property_ui_text(prop, "Limit Y", "Use minimum/maximum y limit");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_limit_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 4);
	RNA_def_property_ui_text(prop, "Limit Z", "Use minimum/maximum z limit");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_angular_limit_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 8);
	RNA_def_property_ui_text(prop, "Angular X Limit", "Use minimum/maximum X angular limit");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_angular_limit_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 16);
	RNA_def_property_ui_text(prop, "Angular Y Limit", "Use minimum/maximum Y angular limit");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_angular_limit_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 32);
	RNA_def_property_ui_text(prop, "Angular Z Limit", "Use minimum/maximum Z angular limit");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_clamp_to(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem clamp_items[] = {
		{CLAMPTO_AUTO, "CLAMPTO_AUTO", 0, "Auto", ""},
		{CLAMPTO_X, "CLAMPTO_X", 0, "X", ""},
		{CLAMPTO_Y, "CLAMPTO_Y", 0, "Y", ""},
		{CLAMPTO_Z, "CLAMPTO_Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ClampToConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Clamp To Constraint",
	                       "Constrain an object's location to the nearest point along the target path");
	RNA_def_struct_sdna_from(srna, "bClampToConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Curve_object_poll");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "main_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, clamp_items);
	RNA_def_property_ui_text(prop, "Main Axis", "Main axis of movement");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag2", CLAMPTO_CYCLIC);
	RNA_def_property_ui_text(prop, "Cyclic", "Treat curve as cyclic curve (no clamping to curve bounding box)");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_transform(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem transform_items[] = {
		{0, "LOCATION", 0, "Loc", ""},
		{1, "ROTATION", 0, "Rot", ""},
		{2, "SCALE", 0, "Scale", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem axis_map_items[] = {
		{0, "X", 0, "X", ""},
		{1, "Y", 0, "Y", ""},
		{2, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "TransformConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Transformation Constraint", "Map transformations of the target to the object");
	RNA_def_struct_sdna_from(srna, "bTransformConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "map_from", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "from");
	RNA_def_property_enum_items(prop, transform_items);
	RNA_def_property_ui_text(prop, "Map From", "The transformation type to use from the target");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "map_to", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "to");
	RNA_def_property_enum_items(prop, transform_items);
	RNA_def_property_ui_text(prop, "Map To", "The transformation type to affect of the constrained object");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "map_to_x_from", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "map[0]");
	RNA_def_property_enum_items(prop, axis_map_items);
	RNA_def_property_ui_text(prop, "Map To X From", "The source axis constrained object's X axis uses");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "map_to_y_from", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "map[1]");
	RNA_def_property_enum_items(prop, axis_map_items);
	RNA_def_property_ui_text(prop, "Map To Y From", "The source axis constrained object's Y axis uses");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "map_to_z_from", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "map[2]");
	RNA_def_property_enum_items(prop, axis_map_items);
	RNA_def_property_ui_text(prop, "Map To Z From", "The source axis constrained object's Z axis uses");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_motion_extrapolate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "expo", CLAMPTO_CYCLIC);
	RNA_def_property_ui_text(prop, "Extrapolate Motion", "Extrapolate ranges");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "from_min_x", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "from_min[0]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "From Minimum X", "Bottom range of X axis source motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "from_min_y", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "from_min[1]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "From Minimum Y", "Bottom range of Y axis source motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "from_min_z", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "from_min[2]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "From Minimum Z", "Bottom range of Z axis source motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "from_max_x", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "from_max[0]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "From Maximum X", "Top range of X axis source motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "from_max_y", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "from_max[1]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "From Maximum Y", "Top range of Y axis source motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "from_max_z", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "from_max[2]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "From Maximum Z", "Top range of Z axis source motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "to_min_x", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "to_min[0]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "To Minimum X", "Bottom range of X axis destination motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "to_min_y", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "to_min[1]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "To Minimum Y", "Bottom range of Y axis destination motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "to_min_z", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "to_min[2]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "To Minimum Z", "Bottom range of Z axis destination motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "to_max_x", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "to_max[0]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "To Maximum X", "Top range of X axis destination motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "to_max_y", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "to_max[1]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "To Maximum Y", "Top range of Y axis destination motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "to_max_z", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "to_max[2]");
	RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "To Maximum Z", "Top range of Z axis destination motion");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_location_limit(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LimitLocationConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Limit Location Constraint", "Limit the location of the constrained object");
	RNA_def_struct_sdna_from(srna, "bLocLimitConstraint", "data");

	prop = RNA_def_property(srna, "use_min_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_XMIN);
	RNA_def_property_ui_text(prop, "Minimum X", "Use the minimum X value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_min_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_YMIN);
	RNA_def_property_ui_text(prop, "Minimum Y", "Use the minimum Y value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_min_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_ZMIN);
	RNA_def_property_ui_text(prop, "Minimum Z", "Use the minimum Z value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_max_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_XMAX);
	RNA_def_property_ui_text(prop, "Maximum X", "Use the maximum X value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_max_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_YMAX);
	RNA_def_property_ui_text(prop, "Maximum Y", "Use the maximum Y value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_max_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_ZMAX);
	RNA_def_property_ui_text(prop, "Maximum Z", "Use the maximum Z value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "min_x", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "xmin");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Minimum X", "Lowest X value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "min_y", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "ymin");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Minimum Y", "Lowest Y value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "min_z", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "zmin");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Minimum Z", "Lowest Z value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "max_x", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "xmax");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Maximum X", "Highest X value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "max_y", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "ymax");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Maximum Y", "Highest Y value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "max_z", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "zmax");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Maximum Z", "Highest Z value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_transform_limit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag2", LIMIT_TRANSFORM);
	RNA_def_property_ui_text(prop, "For Transform", "Transforms are affected by this constraint as well");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_rotation_limit(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LimitRotationConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Limit Rotation Constraint", "Limit the rotation of the constrained object");
	RNA_def_struct_sdna_from(srna, "bRotLimitConstraint", "data");

	prop = RNA_def_property(srna, "use_limit_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_XROT);
	RNA_def_property_ui_text(prop, "Limit X", "Use the minimum X value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_limit_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_YROT);
	RNA_def_property_ui_text(prop, "Limit Y", "Use the minimum Y value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_limit_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_ZROT);
	RNA_def_property_ui_text(prop, "Limit Z", "Use the minimum Z value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "min_x", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "xmin");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Minimum X", "Lowest X value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "min_y", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "ymin");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Minimum Y", "Lowest Y value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "min_z", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "zmin");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Minimum Z", "Lowest Z value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "max_x", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "xmax");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Maximum X", "Highest X value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "max_y", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "ymax");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Maximum Y", "Highest Y value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "max_z", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "zmax");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Maximum Z", "Highest Z value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_transform_limit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag2", LIMIT_TRANSFORM);
	RNA_def_property_ui_text(prop, "For Transform", "Transforms are affected by this constraint as well");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_size_limit(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LimitScaleConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Limit Size Constraint", "Limit the scaling of the constrained object");
	RNA_def_struct_sdna_from(srna, "bSizeLimitConstraint", "data");

	prop = RNA_def_property(srna, "use_min_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_XMIN);
	RNA_def_property_ui_text(prop, "Minimum X", "Use the minimum X value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_min_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_YMIN);
	RNA_def_property_ui_text(prop, "Minimum Y", "Use the minimum Y value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_min_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_ZMIN);
	RNA_def_property_ui_text(prop, "Minimum Z", "Use the minimum Z value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_max_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_XMAX);
	RNA_def_property_ui_text(prop, "Maximum X", "Use the maximum X value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_max_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_YMAX);
	RNA_def_property_ui_text(prop, "Maximum Y", "Use the maximum Y value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_max_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMIT_ZMAX);
	RNA_def_property_ui_text(prop, "Maximum Z", "Use the maximum Z value");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "min_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xmin");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Minimum X", "Lowest X value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "min_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ymin");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Minimum Y", "Lowest Y value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "min_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "zmin");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Minimum Z", "Lowest Z value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "max_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xmax");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Maximum X", "Highest X value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "max_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ymax");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Maximum Y", "Highest Y value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "max_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "zmax");
	RNA_def_property_range(prop, -1000.0, 1000.f);
	RNA_def_property_ui_text(prop, "Maximum Z", "Highest Z value to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "use_transform_limit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag2", LIMIT_TRANSFORM);
	RNA_def_property_ui_text(prop, "For Transform", "Transforms are affected by this constraint as well");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_distance_limit(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LimitDistanceConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Limit Distance Constraint", "Limit the distance from target object");
	
	prop = RNA_def_property(srna, "head_tail", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, "bConstraint", "headtail");
	RNA_def_property_ui_text(prop, "Head/Tail", "Target along length of bone: Head=0, Tail=1");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	RNA_def_struct_sdna_from(srna, "bDistLimitConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_range(prop, 0.0, 100.f);
	RNA_def_property_ui_text(prop, "Distance", "Radius of limiting sphere");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	prop = RNA_def_property(srna, "limit_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, constraint_distance_items);
	RNA_def_property_ui_text(prop, "Limit Mode", "Distances in relation to sphere of influence to allow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_transform_limit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIMITDIST_TRANSFORM);
	RNA_def_property_ui_text(prop, "For Transform", "Transforms are affected by this constraint as well");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_shrinkwrap(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem type_items[] = {
		{MOD_SHRINKWRAP_NEAREST_SURFACE, "NEAREST_SURFACE", 0, "Nearest Surface Point",
		                                 "Shrink the location to the nearest target surface"},
		{MOD_SHRINKWRAP_PROJECT, "PROJECT", 0, "Project",
		                         "Shrink the location to the nearest target surface along a given axis"},
		{MOD_SHRINKWRAP_NEAREST_VERTEX, "NEAREST_VERTEX", 0, "Nearest Vertex",
		                                "Shrink the location to the nearest target vertex"},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "ShrinkwrapConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Shrinkwrap Constraint", "Create constraint-based shrinkwrap relationship");
	RNA_def_struct_sdna_from(srna, "bShrinkwrapConstraint", "data");
	
	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "target"); /* TODO, mesh type */
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Mesh_object_poll");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
	
	prop = RNA_def_property(srna, "shrinkwrap_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "shrinkType");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Shrinkwrap Type", "Select type of shrinkwrap algorithm for target position");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_range(prop, 0.0, 100.f);
	RNA_def_property_ui_text(prop, "Distance", "Distance to Target");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS);
	RNA_def_property_ui_text(prop, "Axis X", "Projection over X Axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS);
	RNA_def_property_ui_text(prop, "Axis Y", "Projection over Y Axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS);
	RNA_def_property_ui_text(prop, "Axis Z", "Projection over Z Axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_damped_track(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem damptrack_items[] = {
		{TRACK_X, "TRACK_X", 0, "X", ""},
		{TRACK_Y, "TRACK_Y", 0, "Y", ""},
		{TRACK_Z, "TRACK_Z", 0, "Z", ""},
		{TRACK_nX, "TRACK_NEGATIVE_X", 0, "-X", ""},
		{TRACK_nY, "TRACK_NEGATIVE_Y", 0, "-Y", ""},
		{TRACK_nZ, "TRACK_NEGATIVE_Z", 0, "-Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "DampedTrackConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Damped Track Constraint",
	                       "Point toward target by taking the shortest rotation path");
	
	prop = RNA_def_property(srna, "head_tail", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, "bConstraint", "headtail");
	RNA_def_property_ui_text(prop, "Head/Tail", "Target along length of bone: Head=0, Tail=1");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	RNA_def_struct_sdna_from(srna, "bDampTrackConstraint", "data");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	prop = RNA_def_property(srna, "track_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "trackflag");
	RNA_def_property_enum_items(prop, damptrack_items);
	RNA_def_property_ui_text(prop, "Track Axis", "Axis that points to the target object");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_spline_ik(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem splineik_xz_scale_mode[] = {
		{CONSTRAINT_SPLINEIK_XZS_NONE, "NONE", 0, "None", "Don't scale the X and Z axes (Default)"},
		{CONSTRAINT_SPLINEIK_XZS_ORIGINAL, "BONE_ORIGINAL", 0, "Bone Original",
		                                   "Use the original scaling of the bones"},
		{CONSTRAINT_SPLINEIK_XZS_VOLUMETRIC, "VOLUME_PRESERVE", 0, "Volume Preservation",
		                                     "Scale of the X and Z axes is the inverse of the Y-Scale"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "SplineIKConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Spline IK Constraint", "Align 'n' bones along a curve");
	RNA_def_struct_sdna_from(srna, "bSplineIKConstraint", "data");
	
	/* target chain */
	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Curve that controls this relationship");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
	
	prop = RNA_def_property(srna, "chain_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "chainlen");
	/* TODO: this should really check the max length of the chain the constraint is attached to */
	RNA_def_property_range(prop, 1, 255);
	RNA_def_property_ui_text(prop, "Chain Length", "How many bones are included in the chain");
	/* XXX: this update goes wrong... needs extra flush? */
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
	
	/* direct access to bindings */
	/* NOTE: only to be used by experienced users */
	prop = RNA_def_property(srna, "joint_bindings", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_array(prop, 32); /* XXX this is the maximum value allowed - why?  */
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_SplineIKConstraint_joint_bindings_get_length");
	RNA_def_property_float_funcs(prop, "rna_SplineIKConstraint_joint_bindings_get",
	                             "rna_SplineIKConstraint_joint_bindings_set", NULL);
	RNA_def_property_ui_text(prop, "Joint Bindings",
	                         "(EXPERIENCED USERS ONLY) The relative positions of the joints along the chain, "
	                         "as percentages");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	/* settings */
	prop = RNA_def_property(srna, "use_chain_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_SPLINEIK_NO_ROOT);
	RNA_def_property_ui_text(prop, "Chain Offset", "Offset the entire chain relative to the root joint");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_even_divisions", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_SPLINEIK_EVENSPLITS);
	RNA_def_property_ui_text(prop, "Even Divisions",
	                         "Ignore the relative lengths of the bones when fitting to the curve");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_y_stretch", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CONSTRAINT_SPLINEIK_SCALE_LIMITED);
	RNA_def_property_ui_text(prop, "Y Stretch", "Stretch the Y axis of the bones to fit the curve");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "use_curve_radius", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CONSTRAINT_SPLINEIK_NO_CURVERAD);
	RNA_def_property_ui_text(prop, "Use Curve Radius",
	                         "Average radius of the endpoints is used to tweak the X and Z Scaling of the bones, "
	                         "on top of XZ Scale mode");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "xz_scale_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "xzScaleMode");
	RNA_def_property_enum_items(prop, splineik_xz_scale_mode);
	RNA_def_property_ui_text(prop, "XZ Scale Mode",
	                         "Method used for determining the scaling of the X and Z axes of the bones");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_pivot(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem pivot_rotAxis_items[] = {
		{PIVOTCON_AXIS_NONE, "ALWAYS_ACTIVE", 0, "Always", "Use the pivot point in every rotation"},
		{PIVOTCON_AXIS_X_NEG, "NX", 0, "-X Rot",
		                      "Use the pivot point in the negative rotation range around the X-axis"},
		{PIVOTCON_AXIS_Y_NEG, "NY", 0, "-Y Rot",
		                      "Use the pivot point in the negative rotation range around the Y-axis"},
		{PIVOTCON_AXIS_Z_NEG, "NZ", 0, "-Z Rot",
		                      "Use the pivot point in the negative rotation range around the Z-axis"},
		{PIVOTCON_AXIS_X, "X", 0, "X Rot", "Use the pivot point in the positive rotation range around the X-axis"},
		{PIVOTCON_AXIS_Y, "Y", 0, "Y Rot", "Use the pivot point in the positive rotation range around the Y-axis"},
		{PIVOTCON_AXIS_Z, "Z", 0, "Z Rot", "Use the pivot point in the positive rotation range around the Z-axis"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "PivotConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Pivot Constraint", "Rotate around a different point");
	
	prop = RNA_def_property(srna, "head_tail", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, "bConstraint", "headtail");
	RNA_def_property_ui_text(prop, "Head/Tail", "Target along length of bone: Head=0, Tail=1");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	RNA_def_struct_sdna_from(srna, "bPivotConstraint", "data");
	
	/* target-defined pivot */
	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tar");
	RNA_def_property_ui_text(prop, "Target", "Target Object, defining the position of the pivot when defined");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
	
	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
	
	/* pivot offset */
	prop = RNA_def_property(srna, "use_relative_location", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", PIVOTCON_FLAG_OFFSET_ABS);
	RNA_def_property_ui_text(prop, "Use Relative Offset",
	                         "Offset will be an absolute point in space instead of relative to the target");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_ui_text(prop, "Offset",
	                         "Offset of pivot from target (when set), or from owner's location "
	                         "(when Fixed Position is off), or the absolute pivot point");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
	
	/* rotation-based activation */
	prop = RNA_def_property(srna, "rotation_range", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rotAxis");
	RNA_def_property_enum_items(prop, pivot_rotAxis_items);
	RNA_def_property_ui_text(prop, "Enabled Rotation Range", "Rotation range on which pivoting should occur");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_follow_track(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "FollowTrackConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Follow Track Constraint", "Lock motion to the target motion track");
	RNA_def_struct_sdna_from(srna, "bFollowTrackConstraint", "data");

	/* movie clip */
	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "clip");
	RNA_def_property_ui_text(prop, "Movie Clip", "Movie Clip to get tracking data from");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	/* track */
	prop = RNA_def_property(srna, "track", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "track");
	RNA_def_property_ui_text(prop, "Track", "Movie tracking track to follow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	/* use default clip */
	prop = RNA_def_property(srna, "use_active_clip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FOLLOWTRACK_ACTIVECLIP);
	RNA_def_property_ui_text(prop, "Active Clip", "Use active clip defined in scene");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	/* use 3d position */
	prop = RNA_def_property(srna, "use_3d_position", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FOLLOWTRACK_USE_3D_POSITION);
	RNA_def_property_ui_text(prop, "3D Position", "Use 3D position of track to parent to");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	/* object */
	prop = RNA_def_property(srna, "object", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "object");
	RNA_def_property_ui_text(prop, "Object", "Movie tracking object to follow (if empty, camera object is used)");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	/* camera */
	prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "camera");
	RNA_def_property_ui_text(prop, "Camera",
	                         "Camera to which motion is parented (if empty active scene camera is used)");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Constraint_followTrack_camera_set", NULL,
	                               "rna_Constraint_cameraObject_poll");

	/* depth object */
	prop = RNA_def_property(srna, "depth_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "depth_ob");
	RNA_def_property_ui_text(prop, "Depth Object",
	                         "Object used to define depth in camera space by projecting onto surface of this object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Constraint_followTrack_depthObject_set", NULL,
	                               "rna_Constraint_followTrack_depthObject_poll");
}

static void rna_def_constraint_camera_solver(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "CameraSolverConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Camera Solver Constraint", "Lock motion to the reconstructed camera movement");
	RNA_def_struct_sdna_from(srna, "bCameraSolverConstraint", "data");

	/* movie clip */
	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "clip");
	RNA_def_property_ui_text(prop, "Movie Clip", "Movie Clip to get tracking data from");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	/* use default clip */
	prop = RNA_def_property(srna, "use_active_clip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAMERASOLVER_ACTIVECLIP);
	RNA_def_property_ui_text(prop, "Active Clip", "Use active clip defined in scene");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
}

static void rna_def_constraint_object_solver(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ObjectSolverConstraint", "Constraint");
	RNA_def_struct_ui_text(srna, "Object Solver Constraint", "Lock motion to the reconstructed object movement");
	RNA_def_struct_sdna_from(srna, "bObjectSolverConstraint", "data");

	/* movie clip */
	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "clip");
	RNA_def_property_ui_text(prop, "Movie Clip", "Movie Clip to get tracking data from");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	/* use default clip */
	prop = RNA_def_property(srna, "use_active_clip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAMERASOLVER_ACTIVECLIP);
	RNA_def_property_ui_text(prop, "Active Clip", "Use active clip defined in scene");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

	/* object */
	prop = RNA_def_property(srna, "object", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "object");
	RNA_def_property_ui_text(prop, "Object", "Movie tracking object to follow");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

	/* camera */
	prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "camera");
	RNA_def_property_ui_text(prop, "Camera",
	                         "Camera to which motion is parented (if empty active scene camera is used)");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Constraint_objectSolver_camera_set", NULL,
	                               "rna_Constraint_cameraObject_poll");
}

/* base struct for constraints */
void RNA_def_constraint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	/* data */
	srna = RNA_def_struct(brna, "Constraint", NULL);
	RNA_def_struct_ui_text(srna, "Constraint", "Constraint modifying the transformation of objects and bones");
	RNA_def_struct_refine_func(srna, "rna_ConstraintType_refine");
	RNA_def_struct_path_func(srna, "rna_Constraint_path");
	RNA_def_struct_sdna(srna, "bConstraint");
	
	/* strings */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Constraint_name_set");
	RNA_def_property_ui_text(prop, "Name", "Constraint name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT | NA_RENAME, NULL);
	
	/* enums */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, constraint_type_items);
	RNA_def_property_ui_text(prop, "Type", "");

	prop = RNA_def_property(srna, "owner_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ownspace");
	RNA_def_property_enum_items(prop, owner_space_pchan_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Constraint_owner_space_itemf");
	RNA_def_property_ui_text(prop, "Owner Space", "Space that owner is evaluated in");

	prop = RNA_def_property(srna, "target_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "tarspace");
	RNA_def_property_enum_items(prop, target_space_pchan_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Constraint_target_space_itemf");
	RNA_def_property_ui_text(prop, "Target Space", "Space that target is evaluated in");

	/* flags */
	prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_OFF);
	RNA_def_property_ui_text(prop, "Disable", "Enable/Disable Constraint");
	
	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_EXPAND);
	RNA_def_property_ui_text(prop, "Expanded", "Constraint's panel is expanded in UI");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);

	/* XXX this is really an internal flag, but it may be useful for some tools to be able to access this... */
	prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CONSTRAINT_DISABLE);
	RNA_def_property_ui_text(prop, "Valid", "Constraint has valid settings and can be evaluated");
	
	/* TODO: setting this to true must ensure that all others in stack are turned off too... */
	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_ACTIVE);
	RNA_def_property_ui_text(prop, "Active", "Constraint is the one being edited ");
	
	prop = RNA_def_property(srna, "is_proxy_local", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_PROXY_LOCAL);
	RNA_def_property_ui_text(prop, "Proxy Local",
	                         "Constraint was added in this proxy instance (i.e. did not belong to source Armature)");
	
	/* values */
	prop = RNA_def_property(srna, "influence", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "enforce");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Influence", "Amount of influence constraint will have on the final solution");
	RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_influence_update");

	/* readonly values */
	prop = RNA_def_property(srna, "error_location", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lin_error");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Lin error",
	                         "Amount of residual error in Blender space unit for constraints that work on position");

	prop = RNA_def_property(srna, "error_rotation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rot_error");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Rot error",
	                         "Amount of residual error in radiant for constraints that work on orientation");

	/* pointers */
	rna_def_constrainttarget(brna);

	rna_def_constraint_childof(brna);
	rna_def_constraint_python(brna);
	rna_def_constraint_stretch_to(brna);
	rna_def_constraint_follow_path(brna);
	rna_def_constraint_locked_track(brna);
	rna_def_constraint_action(brna);
	rna_def_constraint_size_like(brna);
	rna_def_constraint_same_volume(brna);
	rna_def_constraint_locate_like(brna);
	rna_def_constraint_rotate_like(brna);
	rna_def_constraint_transform_like(brna);
	rna_def_constraint_minmax(brna);
	rna_def_constraint_track_to(brna);
	rna_def_constraint_kinematic(brna);
	rna_def_constraint_rigid_body_joint(brna);
	rna_def_constraint_clamp_to(brna);
	rna_def_constraint_distance_limit(brna);
	rna_def_constraint_size_limit(brna);
	rna_def_constraint_rotation_limit(brna);
	rna_def_constraint_location_limit(brna);
	rna_def_constraint_transform(brna);
	rna_def_constraint_shrinkwrap(brna);
	rna_def_constraint_damped_track(brna);
	rna_def_constraint_spline_ik(brna);
	rna_def_constraint_pivot(brna);
	rna_def_constraint_follow_track(brna);
	rna_def_constraint_camera_solver(brna);
	rna_def_constraint_object_solver(brna);
}

#endif
