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

#include "rna_internal.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "WM_types.h"

/* roles of objects in RigidBody Sims */
EnumPropertyItem rigidbody_ob_type_items[] = {
	{RBO_TYPE_ACTIVE, "ACTIVE", 0, "Active", "Object is directly controlled by simulation results"},
	{RBO_TYPE_PASSIVE, "PASSIVE", 0, "Passive", "Object is directly controlled by animation system"},
	{0, NULL, 0, NULL, NULL}};

/* collision shapes of objects in rigid body sim */
EnumPropertyItem rigidbody_ob_shape_items[] = {
	{RB_SHAPE_BOX, "BOX", ICON_MESH_CUBE, "Box", "Box-like shapes (i.e. cubes), including planes (i.e. ground planes)"},
	{RB_SHAPE_SPHERE, "SPHERE", ICON_MESH_UVSPHERE, "Sphere", ""},
	{RB_SHAPE_CAPSULE, "CAPSULE", ICON_OUTLINER_OB_META, "Capsule", ""},
	{RB_SHAPE_CYLINDER, "CYLINDER", ICON_MESH_CYLINDER, "Cylinder", ""},
	{RB_SHAPE_CONE, "CONE", ICON_MESH_CONE, "Cone", ""},
	{RB_SHAPE_CONVEXH, "CONVEX_HULL", ICON_MESH_ICOSPHERE, "Convex Hull", "A mesh-like surface encompassing (i.e. shrinkwrap over) all verts. Best results with fewer vertices"},
	{RB_SHAPE_TRIMESH, "MESH", ICON_MESH_MONKEY, "Mesh", "Mesh consisting of triangles only, allowing for more detailed interactions than convex hulls"},
	{0, NULL, 0, NULL, NULL}};


#ifdef RNA_RUNTIME

#include "RBI_api.h"

#include "BKE_depsgraph.h"
#include "BKE_rigidbody.h"

#define RB_FLAG_SET(dest, value, flag) { \
	if (value) \
		dest |= flag; \
	else \
		dest &= ~flag; \
}


/* ******************************** */

static void rna_RigidBodyWorld_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	RigidBodyWorld *rbw = (RigidBodyWorld *)ptr->data;
	
	BKE_rigidbody_cache_reset(rbw);
}

static char *rna_RigidBodyWorld_path(PointerRNA *ptr)
{	
	return BLI_sprintfN("rigidbody_world");
}

static void rna_RigidBodyWorld_num_solver_iterations_set(PointerRNA *ptr, int value)
{
	RigidBodyWorld *rbw = (RigidBodyWorld *)ptr->data;
	
	rbw->num_solver_iterations = value;
	
	if (rbw->physics_world)
		RB_dworld_set_solver_iterations(rbw->physics_world, value);
}

static void rna_RigidBodyWorld_split_impulse_set(PointerRNA *ptr, int value)
{
	RigidBodyWorld *rbw = (RigidBodyWorld *)ptr->data;
	
	RB_FLAG_SET(rbw->flag, value, RBW_FLAG_USE_SPLIT_IMPULSE);
	
	if (rbw->physics_world)
		RB_dworld_set_split_impulse(rbw->physics_world, value);
}

/* ******************************** */

static void rna_RigidBodyOb_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	
	BKE_rigidbody_cache_reset(rbw);
}

static void rna_RigidBodyOb_shape_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	BKE_rigidbody_cache_reset(rbw);
	if (rbo->physics_shape)
		rbo->flag |= RBO_FLAG_NEEDS_RESHAPE;
}

static char *rna_RigidBodyOb_path(PointerRNA *ptr)
{
	/* NOTE: this hardcoded path should work as long as only Objects have this */
	return BLI_sprintfN("rigid_body");
}

static void rna_RigidBodyOb_type_set(PointerRNA *ptr, int value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->type = value;
	rbo->flag |= RBO_FLAG_NEEDS_VALIDATE;
	
	/* do physics sim updates */
	if (rbo->physics_object)
		RB_body_set_mass(rbo->physics_object, RBO_GET_MASS(rbo));
}

static void rna_RigidBodyOb_disabled_set(PointerRNA *ptr, int value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	RB_FLAG_SET(rbo->flag, !value, RBO_FLAG_DISABLED);
	
	/* update kinematic state if necessary - only needed for active bodies */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE)) {
		RB_body_set_mass(rbo->physics_object, RBO_GET_MASS(rbo));
		RB_body_set_kinematic_state(rbo->physics_object, !value);
		rbo->flag |= RBO_FLAG_NEEDS_VALIDATE;
	}
}

static void rna_RigidBodyOb_shape_set(PointerRNA *ptr, int value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	Object *ob = (Object *)ptr->id.data;
	
	rbo->shape = value;
	
	/* force creation of new collision shape reflecting this */
	BKE_rigidbody_validate_sim_shape(ob, TRUE);
	
	/* now tell RB sim about it */
	if (rbo->physics_object && rbo->physics_shape)
		RB_body_set_collision_shape(rbo->physics_object, rbo->physics_shape);
}


static void rna_RigidBodyOb_mass_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->mass = value;
	
	/* only active bodies need mass update */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE))
		RB_body_set_mass(rbo->physics_object, RBO_GET_MASS(rbo));
}

static void rna_RigidBodyOb_friction_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->friction = value;
	
	if (rbo->physics_object)
		RB_body_set_friction(rbo->physics_object, value);
}

static void rna_RigidBodyOb_restitution_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->restitution = value;
	
	if (rbo->physics_object)
		RB_body_set_restitution(rbo->physics_object, value);
}

static void rna_RigidBodyOb_collision_margin_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->margin = value;
	
	if (rbo->physics_shape)
		RB_shape_set_margin(rbo->physics_shape, RBO_GET_MARGIN(rbo));
}

static void rna_RigidBodyOb_kinematic_state_set(PointerRNA *ptr, int value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	RB_FLAG_SET(rbo->flag, value, RBO_FLAG_KINEMATIC);
	
	/* update kinematic state if necessary */
	if (rbo->physics_object) {
		RB_body_set_mass(rbo->physics_object, RBO_GET_MASS(rbo));
		RB_body_set_kinematic_state(rbo->physics_object, value);
		rbo->flag |= RBO_FLAG_NEEDS_VALIDATE;
	}
}

static void rna_RigidBodyOb_activation_state_set(PointerRNA *ptr, int value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	RB_FLAG_SET(rbo->flag, value, RBO_FLAG_USE_DEACTIVATION);
	
	/* update activation state if necessary - only active bodies can be deactivated */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE))
		RB_body_set_activation_state(rbo->physics_object, value);
}

static void rna_RigidBodyOb_linear_sleepThresh_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->lin_sleep_thresh = value;
	
	/* only active bodies need sleep threshold update */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE))
		RB_body_set_linear_sleep_thresh(rbo->physics_object, value);
}

static void rna_RigidBodyOb_angular_sleepThresh_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->ang_sleep_thresh = value;
	
	/* only active bodies need sleep threshold update */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE))
		RB_body_set_angular_sleep_thresh(rbo->physics_object, value);
}

static void rna_RigidBodyOb_linear_damping_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->lin_damping = value;
	
	/* only active bodies need damping update */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE))
		RB_body_set_linear_damping(rbo->physics_object, value);
}

static void rna_RigidBodyOb_angular_damping_set(PointerRNA *ptr, float value)
{
	RigidBodyOb *rbo = (RigidBodyOb *)ptr->data;
	
	rbo->ang_damping = value;
	
	/* only active bodies need damping update */
	if ((rbo->physics_object) && (rbo->type == RBO_TYPE_ACTIVE))
		RB_body_set_angular_damping(rbo->physics_object, value);
}

#else

static void rna_def_rigidbody_world(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "RigidBodyWorld", NULL);
	RNA_def_struct_sdna(srna, "RigidBodyWorld");
	RNA_def_struct_ui_text(srna, "Rigid Body World", "Self-contained rigid body simulation environment and settings");
	RNA_def_struct_path_func(srna, "rna_RigidBodyWorld_path");
	
	/* groups */
	prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_ID_SELF_CHECK);
	RNA_def_property_ui_text(prop, "Group", "Group containing objects participating in this simulation");
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
	RNA_def_property_ui_text(prop, "Time Scale", "Changes the speed of the simulation");
	RNA_def_property_update(prop, NC_SCENE, "rna_RigidBodyWorld_reset");
	
	/* timestep */
	prop = RNA_def_property(srna, "steps_per_second", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "steps_per_second");
	RNA_def_property_range(prop, 1, SHRT_MAX);
	RNA_def_property_ui_range(prop, 60, 1000, 1, 0);
	RNA_def_property_int_default(prop, 60);
	RNA_def_property_ui_text(prop, "Steps Per Second", "Number of simulation steps taken per second (higher values are more accurate but slower)");
	RNA_def_property_update(prop, NC_SCENE, "rna_RigidBodyWorld_reset");
	
	/* constraint solver iterations */
	prop = RNA_def_property(srna, "num_solver_iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "num_solver_iterations");
	RNA_def_property_range(prop, 1, 1000);
	RNA_def_property_ui_range(prop, 10, 100, 1, 0);
	RNA_def_property_int_default(prop, 10);
	RNA_def_property_int_funcs(prop, NULL, "rna_RigidBodyWorld_num_solver_iterations_set", NULL);
	RNA_def_property_ui_text(prop, "Solver Iterations", "Number of constraint solver iterations made per simulation step (higher values are more accurate but slower)");
	RNA_def_property_update(prop, NC_SCENE, "rna_RigidBodyWorld_reset");
	
	/* split impulse */
	prop = RNA_def_property(srna, "use_split_impulse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBW_FLAG_USE_SPLIT_IMPULSE);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyWorld_split_impulse_set");
	RNA_def_property_ui_text(prop, "Split Impulse", "Reduces extra velocity that can build up when objects collide (lowers simulation stabilty a litte so use only when necessary)");
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
	RNA_def_property_enum_items(prop, rigidbody_ob_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_RigidBodyOb_type_set", NULL);
	RNA_def_property_ui_text(prop, "Type", "Role of object in Rigid Body Simulations");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	/* booleans */
	prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", RBO_FLAG_DISABLED);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyOb_disabled_set");
	RNA_def_property_ui_text(prop, "Enabled", "Rigid Body actively participated in the simulation");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "collision_shape", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "shape");
	RNA_def_property_enum_items(prop, rigidbody_ob_shape_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_RigidBodyOb_shape_set", NULL);
	RNA_def_property_ui_text(prop, "Collision Shape", "Collision Shape of object in Rigid Body Simulations");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "kinematic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBO_FLAG_KINEMATIC);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyOb_kinematic_state_set");
	RNA_def_property_ui_text(prop, "Kinematic", "Allows rigid body to be controlled by the animation system");
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
	RNA_def_property_boolean_default(prop, TRUE);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_RigidBodyOb_activation_state_set");
	RNA_def_property_ui_text(prop, "Enable Deactivation", "Enables deactivation of resting rigid bodies (increases performance and stability but can cause glitches)");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "start_deactivated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBO_FLAG_START_DEACTIVATED);
	RNA_def_property_ui_text(prop, "Start Deactivated", "Deactivates rigid body at the start of the simulation");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "deactivate_linear_velocity", PROP_FLOAT, PROP_UNIT_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "lin_sleep_thresh");
	RNA_def_property_range(prop, FLT_MIN, FLT_MAX); // range must always be positive (and non-zero)
	RNA_def_property_float_default(prop, 0.4f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_linear_sleepThresh_set", NULL);
	RNA_def_property_ui_text(prop, "Linear Velocity Deactivation Threshold", "Linear Velocity below which simulation stops simulating object");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	prop = RNA_def_property(srna, "deactivate_angular_velocity", PROP_FLOAT, PROP_UNIT_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "ang_sleep_thresh");
	RNA_def_property_range(prop, FLT_MIN, FLT_MAX); // range must always be positive (and non-zero)
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_angular_sleepThresh_set", NULL);
	RNA_def_property_ui_text(prop, "Angular Velocity Deactivation Threshold", "Angular Velocity below which simulation stops simulating object");
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
	RNA_def_property_ui_text(prop, "Restitution", "Tendency of object to bounce after colliding with another (0 = stays still, 1 = perfectly elastic)");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	
	/* Collision Parameters - Sensitivity */
	prop = RNA_def_property(srna, "use_margin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RBO_FLAG_USE_MARGIN);
	RNA_def_property_boolean_default(prop, FALSE);
	RNA_def_property_ui_text(prop, "Collision Margin", "Use custom collision margin (some shapes will have a visible gap around them)");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_shape_reset");
	
	prop = RNA_def_property(srna, "collision_margin", PROP_FLOAT, PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "margin");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 3);
	RNA_def_property_float_default(prop, 0.04f);
	RNA_def_property_float_funcs(prop, NULL, "rna_RigidBodyOb_collision_margin_set", NULL);
	RNA_def_property_ui_text(prop, "Collision Margin", "Threshold of distance near surface where collisions are still considered (best results when non-zero)");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_shape_reset");
	
	prop = RNA_def_property(srna, "collision_groups", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "col_groups", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_ui_text(prop, "Collison Groups", "Collision Groups Rigid Body belongs to");
	RNA_def_property_update(prop, NC_OBJECT | ND_POINTCACHE, "rna_RigidBodyOb_reset");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
}

void RNA_def_rigidbody(BlenderRNA *brna)
{
	rna_def_rigidbody_world(brna);
	rna_def_rigidbody_object(brna);
}


#endif
