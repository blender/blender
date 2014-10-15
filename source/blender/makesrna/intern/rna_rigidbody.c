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
 * Contributor(s): Blender Foundation 2013, Joshua Leung, Sergej Reich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file rna_rigidbody.c
 *  \ingroup rna
 *  \brief RNA property definitions for Rigid Body datatypes
 */

#include <stdlib.h>
#include <string.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "WM_types.h"

/* roles of objects in RigidBody Sims */
EnumPropertyItem rigidbody_object_type_items[] = {
	{RBO_TYPE_ACTIVE, "ACTIVE", 0, "Active", "Object is directly controlled by simulation results"},
	{RBO_TYPE_PASSIVE, "PASSIVE", 0, "Passive", "Object is directly controlled by animation system"},
	{0, NULL, 0, NULL, NULL}};

/* collision shapes of objects in rigid body sim */
EnumPropertyItem rigidbody_object_shape_items[] = {
	{RB_SHAPE_BOX, "BOX", ICON_MESH_CUBE, "Box", "Box-like shapes (i.e. cubes), including planes (i.e. ground planes)"},
	{RB_SHAPE_SPHERE, "SPHERE", ICON_MESH_UVSPHERE, "Sphere", ""},
	{RB_SHAPE_CAPSULE, "CAPSULE", ICON_OUTLINER_OB_META, "Capsule", ""},
	{RB_SHAPE_CYLINDER, "CYLINDER", ICON_MESH_CYLINDER, "Cylinder", ""},
	{RB_SHAPE_CONE, "CONE", ICON_MESH_CONE, "Cone", ""},
	{RB_SHAPE_CONVEXH, "CONVEX_HULL", ICON_MESH_ICOSPHERE, "Convex Hull",
	                   "A mesh-like surface encompassing (i.e. shrinkwrap over) all vertices (best results with "
	                   "fewer vertices)"},
	{RB_SHAPE_TRIMESH, "MESH", ICON_MESH_MONKEY, "Mesh",
	                   "Mesh consisting of triangles only, allowing for more detailed interactions than convex hulls"},
	{0, NULL, 0, NULL, NULL}};

/* collision shapes of constraints in rigid body sim */
EnumPropertyItem rigidbody_constraint_type_items[] = {
	{RBC_TYPE_FIXED, "FIXED", ICON_NONE, "Fixed", "Glue rigid bodies together"},
	{RBC_TYPE_POINT, "POINT", ICON_NONE, "Point", "Constrain rigid bodies to move around common pivot point"},
	{RBC_TYPE_HINGE, "HINGE", ICON_NONE, "Hinge", "Restrict rigid body rotation to one axis"},
	{RBC_TYPE_SLIDER, "SLIDER", ICON_NONE, "Slider", "Restrict rigid body translation to one axis"},
	{RBC_TYPE_PISTON, "PISTON", ICON_NONE, "Piston", "Restrict rigid body translation and rotation to one axis"},
	{RBC_TYPE_6DOF, "GENERIC", ICON_NONE, "Generic", "Restrict translation and rotation to specified axes"},
	{RBC_TYPE_6DOF_SPRING, "GENERIC_SPRING", ICON_NONE, "Generic Spring",
	                       "Restrict translation and rotation to specified axes with springs"},
	{RBC_TYPE_MOTOR, "MOTOR", ICON_NONE, "Motor", "Drive rigid body around or along an axis"},
	{0, NULL, 0, NULL, NULL}};

#ifndef RNA_RUNTIME
/* mesh source for collision shape creation */
static EnumPropertyItem rigidbody_mesh_source_items[] = {
	{RBO_MESH_BASE, "BASE", 0, "Base", "Base mesh"},
	{RBO_MESH_DEFORM, "DEFORM", 0, "Deform", "Deformations (shape keys, deform modifiers)"},
	{RBO_MESH_FINAL, "FINAL", 0, "Final", "All modifiers"},
	{0, NULL, 0, NULL, NULL}};
#endif

#ifdef RNA_RUNTIME

#ifdef WITH_BULLET
#  include "RBI_api.h"
#endif

#include "BKE_depsgraph.h"
#include "BKE_rigidbody.h"

#include "WM_api.h"

#define RB_FLAG_SET(dest, value, flag) { \
	if (value) \
		dest |= flag; \
	else \
		dest &= ~flag; \
}


/* ******************************** */

static void rna_RigidBodyWorld_reset(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	RigidBodyWorld *rbw = (RigidBodyWorld *)ptr->data;
	
	BKE_rigidbody_cache_reset(rbw);
}

static char *rna_RigidBodyWorld_path(PointerRNA *UNUSED(ptr))
{	
	return BLI_sprintfN("rigidbody_world");
}

static void rna_RigidBodyWorld_num_solver_iterations_set(PointerRNA *ptr, int value)
{
	RigidBodyWorld *rbw = (RigidBodyWorld *)ptr->data;
	
	rbw->num_solver_iterations = value;

#ifdef WITH_BULLET
	if (rbw->physics_world) {
		RB_dworld_set_solver_iterations(rbw->physics_world, value);
	}
#endif
}

static void rna_RigidBodyWorld_split_impulse_set(PointerRNA *ptr, int value)
{
	RigidBodyWorld *rbw = (RigidBodyWorld *)ptr->data;
	
	RB_FLAG_SET(rbw->flag, value, RBW_FLAG_USE_SPLIT_IMPULSE);

#ifdef WITH_BULLET
	if (rbw->physics_world) {
		RB_dworld_set_split_impulse(rbw->physics_world, value);
	}
#endif
}

/* ******************************** */

static void rna_RigidBodyOb_reset(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	
	BKE_rigidbody_cache_reset(rbw);
}

static void rna_RigidBodyOb_shape_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob = ptr->id.data;
	
	rna_RigidBodyOb_reset(bmain, scene, ptr);
	
	WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

static void rna_RigidBodyOb_shape_reset(Main *UNUSED(bmain), Scene *scene, PointerRNA *ptr)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	BKE_rigidbody_cache_reset(rbw);
	if (rbo->physics_shape)
		rbo->flag |= RBO_FLAG_NEEDS_RESHAPE;
}

static char *rna_RigidBodyOb_path(PointerRNA *UNUSED(ptr))
{
	/* NOTE: this hardcoded path should work as long as only Objects have this */
	return BLI_sprintfN("rigid_body");
}

static void rna_RigidBodyOb_type_set(PointerRNA *ptr, int value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->type = value;
	rbo->flag |= RBO_FLAG_NEEDS_VALIDATE;
}

static void rna_RigidBodyOb_shape_set(PointerRNA *ptr, int value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;

	rbo->shape = value;
	rbo->flag |= RBO_FLAG_NEEDS_VALIDATE;
}

static void rna_RigidBodyOb_disabled_set(PointerRNA *ptr, int value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	RB_FLAG_SET(rbo->flag, !value, RBO_FLAG_DISABLED);

#ifdef WITH_BULLET
	/* update kinematic state if necessary - only needed for active bodies */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE)) {
		RB_body_set_mass(rbo->physics_object, RBO_GET_MASS(rbo));
		RB_body_set_kinematic_state(rbo->physics_object, !value);
		rbo->flag |= RBO_FLAG_NEEDS_VALIDATE;
	}
#endif
}

static void rna_RigidBodyOb_mass_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->mass = value;

#ifdef WITH_BULLET
	/* only active bodies need mass update */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE)) {
		RB_body_set_mass(rbo->physics_object, RBO_GET_MASS(rbo));
	}
#endif
}

static void rna_RigidBodyOb_friction_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->friction = value;

#ifdef WITH_BULLET
	if (rbo->physics_object) {
		RB_body_set_friction(rbo->physics_object, value);
	}
#endif
}

static void rna_RigidBodyOb_restitution_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->restitution = value;
#ifdef WITH_BULLET
	if (rbo->physics_object) {
		RB_body_set_restitution(rbo->physics_object, value);
	}
#endif
}

static void rna_RigidBodyOb_collision_margin_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->margin = value;

#ifdef WITH_BULLET
	if (rbo->physics_shape) {
		RB_shape_set_margin(rbo->physics_shape, RBO_GET_MARGIN(rbo));
	}
#endif
}

static void rna_RigidBodyOb_collision_groups_set(PointerRNA *ptr, const int *values)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	int i;

	for (i = 0; i < 20; i++) {
		if (values[i])
			rbo->col_groups |= (1 << i);
		else
			rbo->col_groups &= ~(1 << i);
	}
	rbo->flag |= RBO_FLAG_NEEDS_VALIDATE;
}

static void rna_RigidBodyOb_kinematic_state_set(PointerRNA *ptr, int value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	RB_FLAG_SET(rbo->flag, value, RBO_FLAG_KINEMATIC);

#ifdef WITH_BULLET
	/* update kinematic state if necessary */
	if (rbo->physics_object) {
		RB_body_set_mass(rbo->physics_object, RBO_GET_MASS(rbo));
		RB_body_set_kinematic_state(rbo->physics_object, value);
		rbo->flag |= RBO_FLAG_NEEDS_VALIDATE;
	}
#endif
}

static void rna_RigidBodyOb_activation_state_set(PointerRNA *ptr, int value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	RB_FLAG_SET(rbo->flag, value, RBO_FLAG_USE_DEACTIVATION);

#ifdef WITH_BULLET
	/* update activation state if necessary - only active bodies can be deactivated */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE)) {
		RB_body_set_activation_state(rbo->physics_object, value);
	}
#endif
}

static void rna_RigidBodyOb_linear_sleepThresh_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->lin_sleep_thresh = value;

#ifdef WITH_BULLET
	/* only active bodies need sleep threshold update */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE)) {
		RB_body_set_linear_sleep_thresh(rbo->physics_object, value);
	}
#endif
}

static void rna_RigidBodyOb_angular_sleepThresh_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->ang_sleep_thresh = value;

#ifdef WITH_BULLET
	/* only active bodies need sleep threshold update */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE)) {
		RB_body_set_angular_sleep_thresh(rbo->physics_object, value);
	}
#endif
}

static void rna_RigidBodyOb_linear_damping_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->lin_damping = value;

#ifdef WITH_BULLET
	/* only active bodies need damping update */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE)) {
		RB_body_set_linear_damping(rbo->physics_object, value);
	}
#endif
}

static void rna_RigidBodyOb_angular_damping_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->ang_damping = value;

#ifdef WITH_BULLET
	/* only active bodies need damping update */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE)) {
		RB_body_set_angular_damping(rbo->physics_object, value);
	}
#endif
}

static char *rna_RigidBodyCon_path(PointerRNA *UNUSED(ptr))
{
	/* NOTE: this hardcoded path should work as long as only Objects have this */
	return BLI_sprintfN("rigid_body_constraint");
}

static void rna_RigidBodyCon_type_set(PointerRNA *ptr, int value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->type = value;
	rbc->flag |= RBC_FLAG_NEEDS_VALIDATE;
}

static void rna_RigidBodyCon_enabled_set(PointerRNA *ptr, int value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	RB_FLAG_SET(rbc->flag, value, RBC_FLAG_ENABLED);

#ifdef WITH_BULLET
	if (rbc->physics_constraint) {
		RB_constraint_set_enabled(rbc->physics_constraint, value);
	}
#endif
}

static void rna_RigidBodyCon_disable_collisions_set(PointerRNA *ptr, int value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	RB_FLAG_SET(rbc->flag, value, RBC_FLAG_DISABLE_COLLISIONS);

	rbc->flag |= RBC_FLAG_NEEDS_VALIDATE;
}

static void rna_RigidBodyCon_use_breaking_set(PointerRNA *ptr, int value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	if (value) {
		rbc->flag |= RBC_FLAG_USE_BREAKING;
#ifdef WITH_BULLET
		if (rbc->physics_constraint) {
			RB_constraint_set_breaking_threshold(rbc->physics_constraint, rbc->breaking_threshold);
		}
#endif
	}
	else {
		rbc->flag &= ~RBC_FLAG_USE_BREAKING;
#ifdef WITH_BULLET
		if (rbc->physics_constraint) {
			RB_constraint_set_breaking_threshold(rbc->physics_constraint, FLT_MAX);
		}
#endif
	}
}

static void rna_RigidBodyCon_breaking_threshold_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->breaking_threshold = value;

#ifdef WITH_BULLET
	if (rbc->physics_constraint && (rbc->flag & RBC_FLAG_USE_BREAKING)) {
		RB_constraint_set_breaking_threshold(rbc->physics_constraint, value);
	}
#endif
}

static void rna_RigidBodyCon_override_solver_iterations_set(PointerRNA *ptr, int value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	if (value) {
		rbc->flag |= RBC_FLAG_OVERRIDE_SOLVER_ITERATIONS;
#ifdef WITH_BULLET
		if (rbc->physics_constraint) {
			RB_constraint_set_solver_iterations(rbc->physics_constraint, rbc->num_solver_iterations);
		}
#endif
	}
	else {
		rbc->flag &= ~RBC_FLAG_OVERRIDE_SOLVER_ITERATIONS;
#ifdef WITH_BULLET
		if (rbc->physics_constraint) {
			RB_constraint_set_solver_iterations(rbc->physics_constraint, -1);
		}
#endif
	}
}

static void rna_RigidBodyCon_num_solver_iterations_set(PointerRNA *ptr, int value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->num_solver_iterations = value;

#ifdef WITH_BULLET
	if (rbc->physics_constraint && (rbc->flag & RBC_FLAG_OVERRIDE_SOLVER_ITERATIONS)) {
		RB_constraint_set_solver_iterations(rbc->physics_constraint, value);
	}
#endif
}

static void rna_RigidBodyCon_spring_stiffness_x_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->spring_stiffness_x = value;

#ifdef WITH_BULLET
	if (rbc->physics_constraint && rbc->type == RBC_TYPE_6DOF_SPRING && (rbc->flag & RBC_FLAG_USE_SPRING_X)) {
		RB_constraint_set_stiffness_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_X, value);
	}
#endif
}

static void rna_RigidBodyCon_spring_stiffness_y_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->spring_stiffness_y = value;

#ifdef WITH_BULLET
	if (rbc->physics_constraint && rbc->type == RBC_TYPE_6DOF_SPRING && (rbc->flag & RBC_FLAG_USE_SPRING_Y)) {
		RB_constraint_set_stiffness_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_Y, value);
	}
#endif
}

static void rna_RigidBodyCon_spring_stiffness_z_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->spring_stiffness_z = value;

#ifdef WITH_BULLET
	if (rbc->physics_constraint && rbc->type == RBC_TYPE_6DOF_SPRING && (rbc->flag & RBC_FLAG_USE_SPRING_Z)) {
		RB_constraint_set_stiffness_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_Z, value);
	}
#endif
}

static void rna_RigidBodyCon_spring_damping_x_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->spring_damping_x = value;

#ifdef WITH_BULLET
	if (rbc->physics_constraint && rbc->type == RBC_TYPE_6DOF_SPRING && (rbc->flag & RBC_FLAG_USE_SPRING_X)) {
		RB_constraint_set_damping_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_X, value);
	}
#endif
}

static void rna_RigidBodyCon_spring_damping_y_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->spring_damping_y = value;
#ifdef WITH_BULLET
	if (rbc->physics_constraint && rbc->type == RBC_TYPE_6DOF_SPRING && (rbc->flag & RBC_FLAG_USE_SPRING_Y)) {
		RB_constraint_set_damping_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_Y, value);
	}
#endif
}

static void rna_RigidBodyCon_spring_damping_z_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->spring_damping_z = value;
#ifdef WITH_BULLET
	if (rbc->physics_constraint && rbc->type == RBC_TYPE_6DOF_SPRING && (rbc->flag & RBC_FLAG_USE_SPRING_Z)) {
		RB_constraint_set_damping_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_Z, value);
	}
#endif
}

static void rna_RigidBodyCon_motor_lin_max_impulse_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->motor_lin_max_impulse = value;

#ifdef WITH_BULLET
	if (rbc->physics_constraint && rbc->type == RBC_TYPE_MOTOR) {
		RB_constraint_set_max_impulse_motor(rbc->physics_constraint, value, rbc->motor_ang_max_impulse);
	}
#endif
}

static void rna_RigidBodyCon_use_motor_lin_set(PointerRNA *ptr, int value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	RB_FLAG_SET(rbc->flag, value, RBC_FLAG_USE_MOTOR_LIN);

#ifdef WITH_BULLET
	if (rbc->physics_constraint) {
		RB_constraint_set_enable_motor(rbc->physics_constraint, rbc->flag & RBC_FLAG_USE_MOTOR_LIN, rbc->flag & RBC_FLAG_USE_MOTOR_ANG);
	}
#endif
}

static void rna_RigidBodyCon_use_motor_ang_set(PointerRNA *ptr, int value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	RB_FLAG_SET(rbc->flag, value, RBC_FLAG_USE_MOTOR_ANG);

#ifdef WITH_BULLET
	if (rbc->physics_constraint) {
		RB_constraint_set_enable_motor(rbc->physics_constraint, rbc->flag & RBC_FLAG_USE_MOTOR_LIN, rbc->flag & RBC_FLAG_USE_MOTOR_ANG);
	}
#endif
}

static void rna_RigidBodyCon_motor_lin_target_velocity_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->motor_lin_target_velocity = value;

#ifdef WITH_BULLET
	if (rbc->physics_constraint && rbc->type == RBC_TYPE_MOTOR) {
		RB_constraint_set_target_velocity_motor(rbc->physics_constraint, value, rbc->motor_ang_target_velocity);
	}
#endif
}

static void rna_RigidBodyCon_motor_ang_max_impulse_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->motor_ang_max_impulse = value;

#ifdef WITH_BULLET
	if (rbc->physics_constraint && rbc->type == RBC_TYPE_MOTOR) {
		RB_constraint_set_max_impulse_motor(rbc->physics_constraint, rbc->motor_lin_max_impulse, value);
	}
#endif
}

static void rna_RigidBodyCon_motor_ang_target_velocity_set(PointerRNA *ptr, float value)
{
	RigidBodyCon *rbc = (RigidBodyCon *)ptr->data;

	rbc->motor_ang_target_velocity = value;

#ifdef WITH_BULLET
	if (rbc->physics_constraint && rbc->type == RBC_TYPE_MOTOR) {
		RB_constraint_set_target_velocity_motor(rbc->physics_constraint, rbc->motor_lin_target_velocity, value);
	}
#endif
}

/* Sweep test */
static void rna_RigidBodyWorld_convex_sweep_test(
        RigidBodyWorld *rbw, ReportList *reports,
        Object *object, float ray_start[3], float ray_end[3],
        float r_location[3], float r_hitpoint[3], float r_normal[3], int *r_hit)
{
#ifdef WITH_BULLET
	RigidBodyOb *rob = object->rigidbody_object;

	if (rbw->physics_world != NULL && rob->physics_object != NULL) {
		RB_world_convex_sweep_test(rbw->physics_world, rob->physics_object, ray_start, ray_end,
		                           r_location, r_hitpoint, r_normal, r_hit);
		if (*r_hit == -2) {
			BKE_report(reports, RPT_ERROR,
			           "A non convex collision shape was passed to the function, use only convex collision shapes");
		}
	}
	else {
		*r_hit = -1;
		BKE_report(reports, RPT_ERROR, "Rigidbody world was not properly initialized, need to step the simulation first");
	}
#else
	(void)rbw, (void)reports, (void)object, (void)ray_start, (void)ray_end;
	(void)r_location, (void)r_hitpoint, (void)r_normal, (void)r_hit;
#endif
}

#else

static void rna_def_rigidbody_world(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;

	srna = RNA_def_struct(brna, "RigidBodyWorld", NULL);
	RNA_def_struct_sdna(srna, "RigidBodyWorld");
	RNA_def_struct_ui_text(srna, "Rigid Body World", "Self-contained rigid body simulation environment and settings");
	RNA_def_struct_path_func(srna, "rna_RigidBodyWorld_path");
	
	/* groups */
	prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_ui_text(prop, "Group", "Group containing objects participating in this simulation");
	RNA_def_property_update(prop, NC_SCENE, "rna_RigidBodyWorld_reset");

	prop = RNA_def_property(srna, "constraints", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_ui_text(prop, "Constraints", "Group containing rigid body constraint objects");
	RNA_def_property_update(prop, NC_SCENE, "rna_RigidBodyWorld_reset");
	
	/* booleans */
	prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", RBW_FLAG_MUTED);
	RNA_def_property_ui_text(prop, "Enabled", "Simulation will be evaluated");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	/* time scale */
	prop = RNA_def_property(srna, "time_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "time_scale");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Time Scale", "Change the speed of the simulation");
	RNA_def_property_update(prop, NC_SCENE, "rna_RigidBodyWorld_reset");
	
	/* timestep */
	prop = RNA_def_property(srna, "steps_per_second", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "steps_per_second");
	RNA_def_property_range(prop, 1, SHRT_MAX);
	RNA_def_property_ui_range(prop, 60, 1000, 1, -1);
	RNA_def_property_int_default(prop, 60);
	RNA_def_property_ui_text(prop, "Steps Per Second",
	                         "Number of simulation steps taken per second (higher values are more accurate "
	                         "but slower)");
	RNA_def_property_update(prop, NC_SCENE, "rna_RigidBodyWorld_reset");
	
	/* constraint solver iterations */
	prop = RNA_def_property(srna, "solver_iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "num_solver_iterations");
	RNA_def_property_range(prop, 1, 1000);
	RNA_def_property_ui_range(prop, 10, 100, 1, -1);
	RNA_def_property_int_default(prop, 10);
	RNA_def_property_int_funcs(prop, NULL, "rna_RigidBodyWorld_num_solver_iterations_set", NULL);
	RNA_def_property_ui_text(prop, "Solver Iterations",
	                         "Number of constraint solver iterations made per simulation step (higher values are more "
	                         "accurate but slower)");
	RNA_def_property_update(prop, NC_SCENE, "rna_RigidBodyWorld_reset");
	
	/* split impulse */
	prop = RNA_def_property(srna, "use_split_impulse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBW_FLAG_USE_SPLIT_IMPULSE);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyWorld_split_impulse_set");
	RNA_def_property_ui_text(prop, "Split Impulse",
	                         "Reduce extra velocity that can build up when objects collide (lowers simulation "
	                         "stability a little so use only when necessary)");
	RNA_def_property_update(prop, NC_SCENE, "rna_RigidBodyWorld_reset");

	/* cache */
	prop = RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "pointcache");
	RNA_def_property_ui_text(prop, "Point Cache", "");

	/* effector weights */
	prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EffectorWeights");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Weights", "");

	/* Sweep test */
	func = RNA_def_function(srna, "convex_sweep_test", "rna_RigidBodyWorld_convex_sweep_test");
	RNA_def_function_ui_description(func, "Sweep test convex rigidbody against the current rigidbody world");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	prop = RNA_def_pointer(func, "object", "Object", "", "Rigidbody object with a convex collision shape");
	RNA_def_property_flag(prop, PROP_REQUIRED | PROP_NEVER_NULL);
	RNA_def_property_clear_flag(prop, PROP_THICK_WRAP);

	/* ray start and end */
	prop = RNA_def_float_vector(func, "start", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_float_vector(func, "end", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_property_flag(prop, PROP_REQUIRED);

	prop = RNA_def_float_vector(func, "object_location", 3, NULL, -FLT_MAX, FLT_MAX, "Location",
	                            "The hit location of this sweep test", -1e4, 1e4);
	RNA_def_property_flag(prop, PROP_THICK_WRAP);
	RNA_def_function_output(func, prop);

	prop = RNA_def_float_vector(func, "hitpoint", 3, NULL, -FLT_MAX, FLT_MAX, "Hitpoint",
	                            "The hit location of this sweep test", -1e4, 1e4);
	RNA_def_property_flag(prop, PROP_THICK_WRAP);
	RNA_def_function_output(func, prop);

	prop = RNA_def_float_vector(func, "normal", 3, NULL, -FLT_MAX, FLT_MAX, "Normal",
	                            "The face normal at the sweep test hit location", -1e4, 1e4);
	RNA_def_property_flag(prop, PROP_THICK_WRAP);
	RNA_def_function_output(func, prop);

	prop = RNA_def_int(func, "has_hit", 0, 0, 0, "", "If the function has found collision point, value is 1, otherwise 0", 0, 0);
	RNA_def_function_output(func, prop);
}

static void rna_def_rigidbody_object(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	
	srna = RNA_def_struct(brna, "RigidBodyObject", NULL);
	RNA_def_struct_sdna(srna, "RigidBodyOb");
	RNA_def_struct_ui_text(srna, "Rigid Body Object", "Settings for object participating in Rigid Body Simulation");
	RNA_def_struct_path_func(srna, "rna_RigidBodyOb_path");
	
	/* Enums */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, rigidbody_object_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_RigidBodyOb_type_set", NULL);
	RNA_def_property_ui_text(prop, "Type", "Role of object in Rigid Body Simulations");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "mesh_source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mesh_source");
	RNA_def_property_enum_items(prop, rigidbody_mesh_source_items);
	RNA_def_property_ui_text(prop, "Mesh Source", "Source of the mesh used to create collision shape");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	/* booleans */
	prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", RBO_FLAG_DISABLED);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyOb_disabled_set");
	RNA_def_property_ui_text(prop, "Enabled", "Rigid Body actively participates to the simulation");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "collision_shape", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "shape");
	RNA_def_property_enum_items(prop, rigidbody_object_shape_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_RigidBodyOb_shape_set", NULL);
	RNA_def_property_ui_text(prop, "Collision Shape", "Collision Shape of object in Rigid Body Simulations");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_shape_update");
	
	prop = RNA_def_property(srna, "kinematic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBO_FLAG_KINEMATIC);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyOb_kinematic_state_set");
	RNA_def_property_ui_text(prop, "Kinematic", "Allow rigid body to be controlled by the animation system");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "use_deform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBO_FLAG_USE_DEFORM);
	RNA_def_property_ui_text(prop, "Deforming", "Rigid body deforms during simulation");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	/* Physics Parameters */
	prop = RNA_def_property(srna, "mass", PROP_FLOAT, PROP_UNIT_MASS);
	RNA_def_property_float_sdna(prop, NULL, "mass");
	RNA_def_property_range(prop, 0.001f, FLT_MAX); // range must always be positive (and non-zero)
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_mass_set", NULL);
	RNA_def_property_ui_text(prop, "Mass", "How much the object 'weighs' irrespective of gravity");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	/* Dynamics Parameters - Activation */
	// TODO: define and figure out how to implement these
	
	/* Dynamics Parameters - Deactivation */
	prop = RNA_def_property(srna, "use_deactivation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBO_FLAG_USE_DEACTIVATION);
	RNA_def_property_boolean_default(prop, true);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyOb_activation_state_set");
	RNA_def_property_ui_text(prop, "Enable Deactivation",
	                         "Enable deactivation of resting rigid bodies (increases performance and stability "
	                         "but can cause glitches)");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "use_start_deactivated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBO_FLAG_START_DEACTIVATED);
	RNA_def_property_ui_text(prop, "Start Deactivated", "Deactivate rigid body at the start of the simulation");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "deactivate_linear_velocity", PROP_FLOAT, PROP_UNIT_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "lin_sleep_thresh");
	RNA_def_property_range(prop, FLT_MIN, FLT_MAX); // range must always be positive (and non-zero)
	RNA_def_property_float_default(prop, 0.4f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_linear_sleepThresh_set", NULL);
	RNA_def_property_ui_text(prop, "Linear Velocity Deactivation Threshold",
	                         "Linear Velocity below which simulation stops simulating object");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "deactivate_angular_velocity", PROP_FLOAT, PROP_UNIT_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "ang_sleep_thresh");
	RNA_def_property_range(prop, FLT_MIN, FLT_MAX); // range must always be positive (and non-zero)
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_angular_sleepThresh_set", NULL);
	RNA_def_property_ui_text(prop, "Angular Velocity Deactivation Threshold",
	                         "Angular Velocity below which simulation stops simulating object");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	/* Dynamics Parameters - Damping Parameters */
	prop = RNA_def_property(srna, "linear_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "lin_damping");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_default(prop, 0.04f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_linear_damping_set", NULL);
	RNA_def_property_ui_text(prop, "Linear Damping", "Amount of linear velocity that is lost over time");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "angular_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "ang_damping");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_default(prop, 0.1f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_angular_damping_set", NULL);
	RNA_def_property_ui_text(prop, "Angular Damping", "Amount of angular velocity that is lost over time");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	/* Collision Parameters - Surface Parameters */
	prop = RNA_def_property(srna, "friction", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "friction");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_friction_set", NULL);
	RNA_def_property_ui_text(prop, "Friction", "Resistance of object to movement");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "restitution", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "restitution");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_restitution_set", NULL);
	RNA_def_property_ui_text(prop, "Restitution",
	                         "Tendency of object to bounce after colliding with another "
	                         "(0 = stays still, 1 = perfectly elastic)");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	/* Collision Parameters - Sensitivity */
	prop = RNA_def_property(srna, "use_margin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBO_FLAG_USE_MARGIN);
	RNA_def_property_boolean_default(prop, false);
	RNA_def_property_ui_text(prop, "Collision Margin",
	                         "Use custom collision margin (some shapes will have a visible gap around them)");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_shape_reset");
	
	prop = RNA_def_property(srna, "collision_margin", PROP_FLOAT, PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "margin");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 3);
	RNA_def_property_float_default(prop, 0.04f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_collision_margin_set", NULL);
	RNA_def_property_ui_text(prop, "Collision Margin",
	                         "Threshold of distance near surface where collisions are still considered "
	                         "(best results when non-zero)");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_shape_reset");
	
	prop = RNA_def_property(srna, "collision_groups", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "col_groups", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyOb_collision_groups_set");
	RNA_def_property_ui_text(prop, "Collision Groups", "Collision Groups Rigid Body belongs to");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
}

static void rna_def_rigidbody_constraint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "RigidBodyConstraint", NULL);
	RNA_def_struct_sdna(srna, "RigidBodyCon");
	RNA_def_struct_ui_text(srna, "Rigid Body Constraint",
	                       "Constraint influencing Objects inside Rigid Body Simulation");
	RNA_def_struct_path_func(srna, "rna_RigidBodyCon_path");

	/* Enums */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, rigidbody_constraint_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_RigidBodyCon_type_set", NULL);
	RNA_def_property_ui_text(prop, "Type", "Type of Rigid Body Constraint");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_ENABLED);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyCon_enabled_set");
	RNA_def_property_ui_text(prop, "Enabled", "Enable this constraint");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "disable_collisions", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_DISABLE_COLLISIONS);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyCon_disable_collisions_set");
	RNA_def_property_ui_text(prop, "Disable Collisions", "Disable collisions between constrained rigid bodies");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "object1", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob1");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object 1", "First Rigid Body Object to be constrained");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "object2", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob2");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object 2", "Second Rigid Body Object to be constrained");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");

	/* Breaking Threshold */
	prop = RNA_def_property(srna, "use_breaking", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_BREAKING);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyCon_use_breaking_set");
	RNA_def_property_ui_text(prop, "Breakable",
	                         "Constraint can be broken if it receives an impulse above the threshold");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "breaking_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "breaking_threshold");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1000.0f, 100.0, 2);
	RNA_def_property_float_default(prop, 10.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_breaking_threshold_set", NULL);
	RNA_def_property_ui_text(prop, "Breaking Threshold",
	                         "Impulse threshold that must be reached for the constraint to break");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");

	/* Solver Iterations */
	prop = RNA_def_property(srna, "use_override_solver_iterations", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_OVERRIDE_SOLVER_ITERATIONS);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyCon_override_solver_iterations_set");
	RNA_def_property_ui_text(prop, "Override Solver Iterations",
	                         "Override the number of solver iterations for this constraint");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "solver_iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "num_solver_iterations");
	RNA_def_property_range(prop, 1, 1000);
	RNA_def_property_ui_range(prop, 1, 100, 1, -1);
	RNA_def_property_int_default(prop, 10);
	RNA_def_property_int_funcs(prop, NULL, "rna_RigidBodyCon_num_solver_iterations_set", NULL);
	RNA_def_property_ui_text(prop, "Solver Iterations",
	                         "Number of constraint solver iterations made per simulation step (higher values are more "
	                         "accurate but slower)");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	/* Limits */
	prop = RNA_def_property(srna, "use_limit_lin_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_LIMIT_LIN_X);
	RNA_def_property_ui_text(prop, "X Axis", "Limit translation on X axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "use_limit_lin_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_LIMIT_LIN_Y);
	RNA_def_property_ui_text(prop, "Y Axis", "Limit translation on Y axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "use_limit_lin_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_LIMIT_LIN_Z);
	RNA_def_property_ui_text(prop, "Z Axis", "Limit translation on Z axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "use_limit_ang_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_LIMIT_ANG_X);
	RNA_def_property_ui_text(prop, "X Angle", "Limit rotation around X axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "use_limit_ang_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_LIMIT_ANG_Y);
	RNA_def_property_ui_text(prop, "Y Angle", "Limit rotation around Y axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "use_limit_ang_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_LIMIT_ANG_Z);
	RNA_def_property_ui_text(prop, "Z Angle", "Limit rotation around Z axis");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "use_spring_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_SPRING_X);
	RNA_def_property_ui_text(prop, "X Spring", "Enable spring on X axis");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "use_spring_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_SPRING_Y);
	RNA_def_property_ui_text(prop, "Y Spring", "Enable spring on Y axis");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "use_spring_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_SPRING_Z);
	RNA_def_property_ui_text(prop, "Z Spring", "Enable spring on Z axis");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "use_motor_lin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_MOTOR_LIN);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyCon_use_motor_lin_set");
	RNA_def_property_ui_text(prop, "Linear Motor", "Enable linear motor");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "use_motor_ang", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBC_FLAG_USE_MOTOR_ANG);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyCon_use_motor_ang_set");
	RNA_def_property_ui_text(prop, "Angular Motor", "Enable angular motor");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_lin_x_lower", PROP_FLOAT, PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "limit_lin_x_lower");
	RNA_def_property_float_default(prop, -1.0f);
	RNA_def_property_ui_text(prop, "Lower X Limit", "Lower limit of X axis translation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_lin_x_upper", PROP_FLOAT, PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "limit_lin_x_upper");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Upper X Limit", "Upper limit of X axis translation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_lin_y_lower", PROP_FLOAT, PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "limit_lin_y_lower");
	RNA_def_property_float_default(prop, -1.0f);
	RNA_def_property_ui_text(prop, "Lower Y Limit", "Lower limit of Y axis translation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_lin_y_upper", PROP_FLOAT, PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "limit_lin_y_upper");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Upper Y Limit", "Upper limit of Y axis translation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_lin_z_lower", PROP_FLOAT, PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "limit_lin_z_lower");
	RNA_def_property_float_default(prop, -1.0f);
	RNA_def_property_ui_text(prop, "Lower Z Limit", "Lower limit of Z axis translation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_lin_z_upper", PROP_FLOAT, PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "limit_lin_z_upper");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Upper Z Limit", "Upper limit of Z axis translation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_ang_x_lower", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limit_ang_x_lower");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_float_default(prop, -M_PI_4);
	RNA_def_property_ui_text(prop, "Lower X Angle Limit", "Lower limit of X axis rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_ang_x_upper", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limit_ang_x_upper");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_float_default(prop, M_PI_4);
	RNA_def_property_ui_text(prop, "Upper X Angle Limit", "Upper limit of X axis rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_ang_y_lower", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limit_ang_y_lower");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_float_default(prop, -M_PI_4);
	RNA_def_property_ui_text(prop, "Lower Y Angle Limit", "Lower limit of Y axis rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_ang_y_upper", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limit_ang_y_upper");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_float_default(prop, M_PI_4);
	RNA_def_property_ui_text(prop, "Upper Y Angle Limit", "Upper limit of Y axis rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_ang_z_lower", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limit_ang_z_lower");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_float_default(prop, -M_PI_4);
	RNA_def_property_ui_text(prop, "Lower Z Angle Limit", "Lower limit of Z axis rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "limit_ang_z_upper", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "limit_ang_z_upper");
	RNA_def_property_range(prop, -M_PI * 2, M_PI * 2);
	RNA_def_property_float_default(prop, M_PI_4);
	RNA_def_property_ui_text(prop, "Upper Z Angle Limit", "Upper limit of Z axis rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "spring_stiffness_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spring_stiffness_x");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
	RNA_def_property_float_default(prop, 10.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_spring_stiffness_x_set", NULL);
	RNA_def_property_ui_text(prop, "X Axis Stiffness", "Stiffness on the X axis");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "spring_stiffness_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spring_stiffness_y");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
	RNA_def_property_float_default(prop, 10.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_spring_stiffness_y_set", NULL);
	RNA_def_property_ui_text(prop, "Y Axis Stiffness", "Stiffness on the Y axis");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "spring_stiffness_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spring_stiffness_z");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
	RNA_def_property_float_default(prop, 10.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_spring_stiffness_z_set", NULL);
	RNA_def_property_ui_text(prop, "Z Axis Stiffness", "Stiffness on the Z axis");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "spring_damping_x", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "spring_damping_x");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_spring_damping_x_set", NULL);
	RNA_def_property_ui_text(prop, "Damping X", "Damping on the X axis");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "spring_damping_y", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "spring_damping_y");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_spring_damping_y_set", NULL);
	RNA_def_property_ui_text(prop, "Damping Y", "Damping on the Y axis");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "spring_damping_z", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "spring_damping_z");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_spring_damping_z_set", NULL);
	RNA_def_property_ui_text(prop, "Damping Z", "Damping on the Z axis");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "motor_lin_target_velocity", PROP_FLOAT, PROP_UNIT_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "motor_lin_target_velocity");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100.0f, 100.0f, 1, 3);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_motor_lin_target_velocity_set", NULL);
	RNA_def_property_ui_text(prop, "Target Velocity", "Target linear motor velocity");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "motor_lin_max_impulse", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "motor_lin_max_impulse");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_motor_lin_max_impulse_set", NULL);
	RNA_def_property_ui_text(prop, "Max Impulse", "Maximum linear motor impulse");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "motor_ang_target_velocity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "motor_ang_target_velocity");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100.0f, 100.0f, 1, 3);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_motor_ang_target_velocity_set", NULL);
	RNA_def_property_ui_text(prop, "Target Velocity", "Target angular motor velocity");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");

	prop = RNA_def_property(srna, "motor_ang_max_impulse", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "motor_ang_max_impulse");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyCon_motor_ang_max_impulse_set", NULL);
	RNA_def_property_ui_text(prop, "Max Impulse", "Maximum angular motor impulse");
	RNA_def_property_update(prop, NC_OBJECT, "rna_RigidBodyOb_reset");
}

void RNA_def_rigidbody(BlenderRNA *brna)
{
	rna_def_rigidbody_world(brna);
	rna_def_rigidbody_object(brna);
	rna_def_rigidbody_constraint(brna);
}


#endif
