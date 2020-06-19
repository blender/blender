/*
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
 * The Original Code is Copyright (C) 2013 Blender Foundation,
 * All rights reserved.
 */

/** \file
 * \ingroup RigidBody
 * \brief Rigid Body API for interfacing with external Physics Engines
 */

#ifndef __RB_API_H__
#define __RB_API_H__

#ifdef __cplusplus
extern "C" {
#endif

/* API Notes:
 * Currently, this API is optimised for Bullet RigidBodies, and doesn't
 * take into account other Physics Engines. Some tweaking may be necessary
 * to allow other systems to be used, in particular there may be references
 * to datatypes that aren't used here...
 *
 * -- Joshua Leung (22 June 2010)
 */

/* ********************************** */
/* Partial Type Defines - Aliases for the type of data we store */

// ----------

/* Dynamics World */
typedef struct rbDynamicsWorld rbDynamicsWorld;

/* Rigid Body */
typedef struct rbRigidBody rbRigidBody;

/* Collision Shape */
typedef struct rbCollisionShape rbCollisionShape;

/* Mesh Data (for Collision Shapes of Meshes) */
typedef struct rbMeshData rbMeshData;

/* Constraint */
typedef struct rbConstraint rbConstraint;

/* ********************************** */
/* Dynamics World Methods */

/* Setup ---------------------------- */

/* Create a new dynamics world instance */
// TODO: add args to set the type of constraint solvers, etc.
rbDynamicsWorld *RB_dworld_new(const float gravity[3]);

/* Delete the given dynamics world, and free any extra data it may require */
void RB_dworld_delete(rbDynamicsWorld *world);

/* Settings ------------------------- */

/* Gravity */
void RB_dworld_get_gravity(rbDynamicsWorld *world, float g_out[3]);
void RB_dworld_set_gravity(rbDynamicsWorld *world, const float g_in[3]);

/* Constraint Solver */
void RB_dworld_set_solver_iterations(rbDynamicsWorld *world, int num_solver_iterations);
/* Split Impulse */
void RB_dworld_set_split_impulse(rbDynamicsWorld *world, int split_impulse);

/* Simulation ----------------------- */

/* Step the simulation by the desired amount (in seconds) with extra controls on substep sizes and
 * maximum substeps */
void RB_dworld_step_simulation(rbDynamicsWorld *world,
                               float timeStep,
                               int maxSubSteps,
                               float timeSubStep);

/* Export -------------------------- */

/* Exports the dynamics world to physics simulator's serialisation format */
void RB_dworld_export(rbDynamicsWorld *world, const char *filename);

/* ********************************** */
/* Rigid Body Methods */

/* Setup ---------------------------- */

/* Add RigidBody to dynamics world */
void RB_dworld_add_body(rbDynamicsWorld *world, rbRigidBody *body, int col_groups);

/* Remove RigidBody from dynamics world */
void RB_dworld_remove_body(rbDynamicsWorld *world, rbRigidBody *body);

/* Collision detection */

void RB_world_convex_sweep_test(rbDynamicsWorld *world,
                                rbRigidBody *object,
                                const float loc_start[3],
                                const float loc_end[3],
                                float v_location[3],
                                float v_hitpoint[3],
                                float v_normal[3],
                                int *r_hit);

/* ............ */

/* Create new RigidBody instance */
rbRigidBody *RB_body_new(rbCollisionShape *shape, const float loc[3], const float rot[4]);

/* Delete the given RigidBody instance */
void RB_body_delete(rbRigidBody *body);

/* Settings ------------------------- */

/* 'Type' */
void RB_body_set_type(rbRigidBody *body, int type, float mass);

/* ............ */

/* Collision Shape */
void RB_body_set_collision_shape(rbRigidBody *body, rbCollisionShape *shape);

/* ............ */

/* Mass */
float RB_body_get_mass(rbRigidBody *body);
void RB_body_set_mass(rbRigidBody *body, float value);

/* Friction */
float RB_body_get_friction(rbRigidBody *body);
void RB_body_set_friction(rbRigidBody *body, float value);

/* Restitution */
float RB_body_get_restitution(rbRigidBody *body);
void RB_body_set_restitution(rbRigidBody *body, float value);

/* Damping */
float RB_body_get_linear_damping(rbRigidBody *body);
void RB_body_set_linear_damping(rbRigidBody *body, float value);

float RB_body_get_angular_damping(rbRigidBody *body);
void RB_body_set_angular_damping(rbRigidBody *body, float value);

void RB_body_set_damping(rbRigidBody *object, float linear, float angular);

/* Sleeping Thresholds */
float RB_body_get_linear_sleep_thresh(rbRigidBody *body);
void RB_body_set_linear_sleep_thresh(rbRigidBody *body, float value);

float RB_body_get_angular_sleep_thresh(rbRigidBody *body);
void RB_body_set_angular_sleep_thresh(rbRigidBody *body, float value);

void RB_body_set_sleep_thresh(rbRigidBody *body, float linear, float angular);

/* Linear Velocity */
void RB_body_get_linear_velocity(rbRigidBody *body, float v_out[3]);
void RB_body_set_linear_velocity(rbRigidBody *body, const float v_in[3]);

/* Angular Velocity */
void RB_body_get_angular_velocity(rbRigidBody *body, float v_out[3]);
void RB_body_set_angular_velocity(rbRigidBody *body, const float v_in[3]);

/* Linear/Angular Factor, used to lock translation/rotation axes */
void RB_body_set_linear_factor(rbRigidBody *object, float x, float y, float z);
void RB_body_set_angular_factor(rbRigidBody *object, float x, float y, float z);

/* Kinematic State */
void RB_body_set_kinematic_state(rbRigidBody *body, int kinematic);

/* RigidBody Interface - Rigid Body Activation States */
int RB_body_get_activation_state(rbRigidBody *body);
void RB_body_set_activation_state(rbRigidBody *body, int use_deactivation);
void RB_body_activate(rbRigidBody *body);
void RB_body_deactivate(rbRigidBody *body);

/* Simulation ----------------------- */

/* Get current transform matrix of RigidBody to use in Blender (OpenGL format) */
void RB_body_get_transform_matrix(rbRigidBody *body, float m_out[4][4]);

/* Set RigidBody's location and rotation */
void RB_body_set_loc_rot(rbRigidBody *body, const float loc[3], const float rot[4]);
/* Set RigidBody's local scaling */
void RB_body_set_scale(rbRigidBody *body, const float scale[3]);

/* ............ */

/* Get RigidBody's position as vector */
void RB_body_get_position(rbRigidBody *body, float v_out[3]);
/* Get RigidBody's orientation as quaternion */
void RB_body_get_orientation(rbRigidBody *body, float v_out[4]);

/* ............ */

void RB_body_apply_central_force(rbRigidBody *body, const float v_in[3]);

/* ********************************** */
/* Collision Shape Methods */

/* Setup (Standard Shapes) ----------- */

rbCollisionShape *RB_shape_new_box(float x, float y, float z);
rbCollisionShape *RB_shape_new_sphere(float radius);
rbCollisionShape *RB_shape_new_capsule(float radius, float height);
rbCollisionShape *RB_shape_new_cone(float radius, float height);
rbCollisionShape *RB_shape_new_cylinder(float radius, float height);

/* Setup (Convex Hull) ------------ */

rbCollisionShape *RB_shape_new_convex_hull(
    float *verts, int stride, int count, float margin, bool *can_embed);

/* Setup (Triangle Mesh) ---------- */

/* 1 */
rbMeshData *RB_trimesh_data_new(int num_tris, int num_verts);
void RB_trimesh_add_vertices(rbMeshData *mesh, float *vertices, int num_verts, int vert_stride);
void RB_trimesh_add_triangle_indices(
    rbMeshData *mesh, int num, int index0, int index1, int index2);
void RB_trimesh_finish(rbMeshData *mesh);
/* 2a - Triangle Meshes */
rbCollisionShape *RB_shape_new_trimesh(rbMeshData *mesh);
/* 2b - GImpact Meshes */
rbCollisionShape *RB_shape_new_gimpact_mesh(rbMeshData *mesh);

/* Cleanup --------------------------- */

void RB_shape_delete(rbCollisionShape *shape);

/* Settings --------------------------- */

/* Collision Margin */
float RB_shape_get_margin(rbCollisionShape *shape);
void RB_shape_set_margin(rbCollisionShape *shape, float value);

void RB_shape_trimesh_update(rbCollisionShape *shape,
                             float *vertices,
                             int num_verts,
                             int vert_stride,
                             float min[3],
                             float max[3]);

/* ********************************** */
/* Constraints */

/* Setup ----------------------------- */

/* Add Rigid Body Constraint to simulation world */
void RB_dworld_add_constraint(rbDynamicsWorld *world, rbConstraint *con, int disable_collisions);

/* Remove Rigid Body Constraint from simulation world */
void RB_dworld_remove_constraint(rbDynamicsWorld *world, rbConstraint *con);

rbConstraint *RB_constraint_new_point(float pivot[3], rbRigidBody *rb1, rbRigidBody *rb2);
rbConstraint *RB_constraint_new_fixed(float pivot[3],
                                      float orn[4],
                                      rbRigidBody *rb1,
                                      rbRigidBody *rb2);
rbConstraint *RB_constraint_new_hinge(float pivot[3],
                                      float orn[4],
                                      rbRigidBody *rb1,
                                      rbRigidBody *rb2);
rbConstraint *RB_constraint_new_slider(float pivot[3],
                                       float orn[4],
                                       rbRigidBody *rb1,
                                       rbRigidBody *rb2);
rbConstraint *RB_constraint_new_piston(float pivot[3],
                                       float orn[4],
                                       rbRigidBody *rb1,
                                       rbRigidBody *rb2);
rbConstraint *RB_constraint_new_6dof(float pivot[3],
                                     float orn[4],
                                     rbRigidBody *rb1,
                                     rbRigidBody *rb2);
rbConstraint *RB_constraint_new_6dof_spring(float pivot[3],
                                            float orn[4],
                                            rbRigidBody *rb1,
                                            rbRigidBody *rb2);
rbConstraint *RB_constraint_new_6dof_spring2(float pivot[3],
                                             float orn[4],
                                             rbRigidBody *rb1,
                                             rbRigidBody *rb2);
rbConstraint *RB_constraint_new_motor(float pivot[3],
                                      float orn[4],
                                      rbRigidBody *rb1,
                                      rbRigidBody *rb2);

/* ............ */

/* Cleanup --------------------------- */

void RB_constraint_delete(rbConstraint *con);

/* Settings --------------------------- */

/* Enable or disable constraint */
void RB_constraint_set_enabled(rbConstraint *con, int enabled);

/* Limits */
#define RB_LIMIT_LIN_X 0
#define RB_LIMIT_LIN_Y 1
#define RB_LIMIT_LIN_Z 2
#define RB_LIMIT_ANG_X 3
#define RB_LIMIT_ANG_Y 4
#define RB_LIMIT_ANG_Z 5
/* Bullet uses the following convention:
 * - lower limit == upper limit -> axis is locked
 * - lower limit > upper limit -> axis is free
 * - lower limit < upper limit -> axis is limited in given range
 */
void RB_constraint_set_limits_hinge(rbConstraint *con, float lower, float upper);
void RB_constraint_set_limits_slider(rbConstraint *con, float lower, float upper);
void RB_constraint_set_limits_piston(
    rbConstraint *con, float lin_lower, float lin_upper, float ang_lower, float ang_upper);
void RB_constraint_set_limits_6dof(rbConstraint *con, int axis, float lower, float upper);

/* 6dof spring specific */
void RB_constraint_set_stiffness_6dof_spring(rbConstraint *con, int axis, float stiffness);
void RB_constraint_set_damping_6dof_spring(rbConstraint *con, int axis, float damping);
void RB_constraint_set_spring_6dof_spring(rbConstraint *con, int axis, int enable);
void RB_constraint_set_equilibrium_6dof_spring(rbConstraint *con);

/* 6dof spring 2 specific */
void RB_constraint_set_limits_6dof_spring2(rbConstraint *con, int axis, float lower, float upper);
void RB_constraint_set_stiffness_6dof_spring2(rbConstraint *con, int axis, float stiffness);
void RB_constraint_set_damping_6dof_spring2(rbConstraint *con, int axis, float damping);
void RB_constraint_set_spring_6dof_spring2(rbConstraint *con, int axis, int enable);
void RB_constraint_set_equilibrium_6dof_spring2(rbConstraint *con);

/* motors */
void RB_constraint_set_enable_motor(rbConstraint *con, int enable_lin, int enable_ang);
void RB_constraint_set_max_impulse_motor(rbConstraint *con,
                                         float max_impulse_lin,
                                         float max_impulse_ang);
void RB_constraint_set_target_velocity_motor(rbConstraint *con,
                                             float velocity_lin,
                                             float velocity_ang);

/* Set number of constraint solver iterations made per step, this overrided world setting
 * To use default set it to -1 */
void RB_constraint_set_solver_iterations(rbConstraint *con, int num_solver_iterations);

/* Set breaking impulse threshold, if constraint shouldn't break it can be set to FLT_MAX */
void RB_constraint_set_breaking_threshold(rbConstraint *con, float threshold);

/* ********************************** */

#ifdef __cplusplus
}
#endif

#endif /* __RB_API_H__ */
