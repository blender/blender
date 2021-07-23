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
 * The Original Code is Copyright (C) 2013 Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 * \brief Types and defines for representing Rigid Body entities
 */

#pragma once

#include "DNA_listBase.h"
#include "DNA_object_force_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Collection;

struct EffectorWeights;

/* ******************************** */
/* RigidBody World */

/* Container for data shared by original and evaluated copies of RigidBodyWorld */
typedef struct RigidBodyWorld_Shared {
  /* cache */
  struct PointCache *pointcache;
  struct ListBase ptcaches;

  /* References to Physics Sim objects. Exist at runtime only ---------------------- */
  /** Physics sim world (i.e. #btDiscreteDynamicsWorld). */
  void *physics_world;
} RigidBodyWorld_Shared;

/* RigidBodyWorld (rbw)
 *
 * Represents a "simulation scene" existing within the parent scene.
 */
typedef struct RigidBodyWorld {
  /* Sim World Settings ------------------------------------------------------------- */
  /** Effectors info. */
  struct EffectorWeights *effector_weights;

  /** Group containing objects to use for Rigid Bodies. */
  struct Collection *group;
  /** Array to access group objects by index, only used at runtime. */
  struct Object **objects;

  /** Group containing objects to use for Rigid Body Constraints. */
  struct Collection *constraints;

  char _pad[4];
  /** Last frame world was evaluated for (internal). */
  float ltime;

  /** This pointer is shared between all evaluated copies. */
  struct RigidBodyWorld_Shared *shared;
  /** Moved to `shared->pointcache`. */
  struct PointCache *pointcache DNA_DEPRECATED;
  /** Moved to `shared->ptcaches`. */
  struct ListBase ptcaches DNA_DEPRECATED;
  /** Number of objects in rigid body group. */
  int numbodies;

  /** Number of simulation sub-steps steps taken per frame. */
  short substeps_per_frame;
  /** Number of constraint solver iterations made per simulation step. */
  short num_solver_iterations;

  /** (#eRigidBodyWorld_Flag) settings for this RigidBodyWorld. */
  int flag;
  /** Used to speed up or slow down the simulation. */
  float time_scale;
} RigidBodyWorld;

/* Flags for RigidBodyWorld */
typedef enum eRigidBodyWorld_Flag {
  /* should sim world be skipped when evaluating (user setting) */
  RBW_FLAG_MUTED = (1 << 0),
  /* sim data needs to be rebuilt */
  /* RBW_FLAG_NEEDS_REBUILD = (1 << 1), */ /* UNUSED */
  /** Use split impulse when stepping the simulation. */
  RBW_FLAG_USE_SPLIT_IMPULSE = (1 << 2),
} eRigidBodyWorld_Flag;

/* ******************************** */
/* RigidBody Object */

/* Container for data that is shared among CoW copies.
 *
 * This is placed in a separate struct so that, for example, the physics_shape
 * pointer can be replaced without having to update all CoW copies. */
#
#
typedef struct RigidBodyOb_Shared {
  /* References to Physics Sim objects. Exist at runtime only */
  /** Physics object representation (i.e. btRigidBody). */
  void *physics_object;
  /** Collision shape used by physics sim (i.e. btCollisionShape). */
  void *physics_shape;
} RigidBodyOb_Shared;

/* RigidBodyObject (rbo)
 *
 * Represents an object participating in a RigidBody sim.
 * This is attached to each object that is currently
 * participating in a sim.
 */
typedef struct RigidBodyOb {
  /* General Settings for this RigidBodyOb */
  /** (eRigidBodyOb_Type) role of RigidBody in sim. */
  short type;
  /** (eRigidBody_Shape) collision shape to use. */
  short shape;

  /** (eRigidBodyOb_Flag). */
  int flag;
  /** Collision groups that determines which rigid bodies can collide with each other. */
  int col_groups;
  /** (eRigidBody_MeshSource) mesh source for mesh based collision shapes. */
  short mesh_source;
  char _pad[2];

  /* Physics Parameters */
  /** How much object 'weighs' (i.e. absolute 'amount of stuff' it holds). */
  float mass;

  /** Resistance of object to movement. */
  float friction;
  /** How 'bouncy' object is when it collides. */
  float restitution;

  /** Tolerance for detecting collisions. */
  float margin;

  /** Damping for linear velocities. */
  float lin_damping;
  /** Damping for angular velocities. */
  float ang_damping;

  /** Deactivation threshold for linear velocities. */
  float lin_sleep_thresh;
  /** Deactivation threshold for angular velocities. */
  float ang_sleep_thresh;

  /** Rigid body orientation. */
  float orn[4];
  /** Rigid body position. */
  float pos[3];
  char _pad1[4];

  /** This pointer is shared between all evaluated copies. */
  struct RigidBodyOb_Shared *shared;
} RigidBodyOb;

/* Participation types for RigidBodyOb */
typedef enum eRigidBodyOb_Type {
  /* active geometry participant in simulation. is directly controlled by sim */
  RBO_TYPE_ACTIVE = 0,
  /* passive geometry participant in simulation. is directly controlled by animsys */
  RBO_TYPE_PASSIVE = 1,
} eRigidBodyOb_Type;

/* Flags for RigidBodyOb */
typedef enum eRigidBodyOb_Flag {
  /* rigidbody is kinematic (controlled by the animation system) */
  RBO_FLAG_KINEMATIC = (1 << 0),
  /* rigidbody needs to be validated (usually set after duplicating and not hooked up yet) */
  RBO_FLAG_NEEDS_VALIDATE = (1 << 1),
  /* rigidbody shape needs refreshing (usually after exiting editmode) */
  RBO_FLAG_NEEDS_RESHAPE = (1 << 2),
  /* rigidbody can be deactivated */
  RBO_FLAG_USE_DEACTIVATION = (1 << 3),
  /* rigidbody is deactivated at the beginning of simulation */
  RBO_FLAG_START_DEACTIVATED = (1 << 4),
  /* rigidbody is not dynamically simulated */
  RBO_FLAG_DISABLED = (1 << 5),
  /* collision margin is not embedded (only used by convex hull shapes for now) */
  RBO_FLAG_USE_MARGIN = (1 << 6),
  /* collision shape deforms during simulation (only for passive triangle mesh shapes) */
  RBO_FLAG_USE_DEFORM = (1 << 7),
} eRigidBodyOb_Flag;

/* RigidBody Collision Shape */
typedef enum eRigidBody_Shape {
  /** Simple box (i.e. bounding box). */
  RB_SHAPE_BOX = 0,
  /** Sphere. */
  RB_SHAPE_SPHERE = 1,
  /** Rounded "pill" shape (i.e. calcium tablets). */
  RB_SHAPE_CAPSULE = 2,
  /** Cylinder (i.e. tin of beans). */
  RB_SHAPE_CYLINDER = 3,
  /** Cone (i.e. party hat). */
  RB_SHAPE_CONE = 4,

  /** Convex hull (minimal shrink-wrap encompassing all verts). */
  RB_SHAPE_CONVEXH = 5,
  /** Triangulated mesh. */
  RB_SHAPE_TRIMESH = 6,

  /* concave mesh approximated using primitives */
  RB_SHAPE_COMPOUND = 7,
} eRigidBody_Shape;

typedef enum eRigidBody_MeshSource {
  /* base mesh */
  RBO_MESH_BASE = 0,
  /* only deformations */
  RBO_MESH_DEFORM = 1,
  /* final derived mesh */
  RBO_MESH_FINAL = 2,
} eRigidBody_MeshSource;

/* ******************************** */
/* RigidBody Constraint */

/* RigidBodyConstraint (rbc)
 *
 * Represents an constraint connecting two rigid bodies.
 */
typedef struct RigidBodyCon {
  /** First object influenced by the constraint. */
  struct Object *ob1;
  /** Second object influenced by the constraint. */
  struct Object *ob2;

  /* General Settings for this RigidBodyCon */
  /** (eRigidBodyCon_Type) role of RigidBody in sim. */
  short type;
  /** Number of constraint solver iterations made per simulation step. */
  short num_solver_iterations;

  /** (eRigidBodyCon_Flag). */
  int flag;

  /** Breaking impulse threshold. */
  float breaking_threshold;
  /** Spring implementation to use. */
  char spring_type;
  char _pad[3];

  /* limits */
  /* translation limits */
  float limit_lin_x_lower;
  float limit_lin_x_upper;
  float limit_lin_y_lower;
  float limit_lin_y_upper;
  float limit_lin_z_lower;
  float limit_lin_z_upper;
  /* rotation limits */
  float limit_ang_x_lower;
  float limit_ang_x_upper;
  float limit_ang_y_lower;
  float limit_ang_y_upper;
  float limit_ang_z_lower;
  float limit_ang_z_upper;

  /* spring settings */
  /* resistance to deformation */
  float spring_stiffness_x;
  float spring_stiffness_y;
  float spring_stiffness_z;
  float spring_stiffness_ang_x;
  float spring_stiffness_ang_y;
  float spring_stiffness_ang_z;
  /* amount of velocity lost over time */
  float spring_damping_x;
  float spring_damping_y;
  float spring_damping_z;
  float spring_damping_ang_x;
  float spring_damping_ang_y;
  float spring_damping_ang_z;

  /* motor settings */
  /** Linear velocity the motor tries to hold. */
  float motor_lin_target_velocity;
  /** Angular velocity the motor tries to hold. */
  float motor_ang_target_velocity;
  /** Maximum force used to reach linear target velocity. */
  float motor_lin_max_impulse;
  /** Maximum force used to reach angular target velocity. */
  float motor_ang_max_impulse;

  /* References to Physics Sim object. Exist at runtime only */
  /** Physics object representation (i.e. btTypedConstraint). */
  void *physics_constraint;
} RigidBodyCon;

/* Participation types for RigidBodyOb */
typedef enum eRigidBodyCon_Type {
  /** lets bodies rotate around a specified point */
  RBC_TYPE_POINT = 0,
  /** lets bodies rotate around a specified axis */
  RBC_TYPE_HINGE = 1,
  /** simulates wheel suspension */
  /* RBC_TYPE_HINGE2 = 2, */ /* UNUSED */
  /** Restricts moment to a specified axis. */
  RBC_TYPE_SLIDER = 3,
  /** lets object rotate within a specified cone */
  /* RBC_TYPE_CONE_TWIST = 4, */ /* UNUSED */
  /** allows user to specify constraint axes */
  RBC_TYPE_6DOF = 5,
  /** like 6DOF but has springs */
  RBC_TYPE_6DOF_SPRING = 6,
  /** simulates a universal joint */
  /* RBC_TYPE_UNIVERSAL = 7, */ /* UNUSED */
  /** glues two bodies together */
  RBC_TYPE_FIXED = 8,
  /** similar to slider but also allows rotation around slider axis */
  RBC_TYPE_PISTON = 9,
  /** Simplified spring constraint with only once axis that's
   * automatically placed between the connected bodies */
  /* RBC_TYPE_SPRING = 10, */ /* UNUSED */
  /** Drives bodies by applying linear and angular forces. */
  RBC_TYPE_MOTOR = 11,
} eRigidBodyCon_Type;

/* Spring implementation type for RigidBodyOb */
typedef enum eRigidBodyCon_SpringType {
  RBC_SPRING_TYPE1 = 0, /* btGeneric6DofSpringConstraint */
  RBC_SPRING_TYPE2 = 1, /* btGeneric6DofSpring2Constraint */
} eRigidBodyCon_SpringType;

/* Flags for RigidBodyCon */
typedef enum eRigidBodyCon_Flag {
  /* constraint influences rigid body motion */
  RBC_FLAG_ENABLED = (1 << 0),
  /* constraint needs to be validated */
  RBC_FLAG_NEEDS_VALIDATE = (1 << 1),
  /* allow constrained bodies to collide */
  RBC_FLAG_DISABLE_COLLISIONS = (1 << 2),
  /* constraint can break */
  RBC_FLAG_USE_BREAKING = (1 << 3),
  /* constraint use custom number of constraint solver iterations */
  RBC_FLAG_OVERRIDE_SOLVER_ITERATIONS = (1 << 4),
  /* limits */
  RBC_FLAG_USE_LIMIT_LIN_X = (1 << 5),
  RBC_FLAG_USE_LIMIT_LIN_Y = (1 << 6),
  RBC_FLAG_USE_LIMIT_LIN_Z = (1 << 7),
  RBC_FLAG_USE_LIMIT_ANG_X = (1 << 8),
  RBC_FLAG_USE_LIMIT_ANG_Y = (1 << 9),
  RBC_FLAG_USE_LIMIT_ANG_Z = (1 << 10),
  /* springs */
  RBC_FLAG_USE_SPRING_X = (1 << 11),
  RBC_FLAG_USE_SPRING_Y = (1 << 12),
  RBC_FLAG_USE_SPRING_Z = (1 << 13),
  /* motors */
  RBC_FLAG_USE_MOTOR_LIN = (1 << 14),
  RBC_FLAG_USE_MOTOR_ANG = (1 << 15),
  /* angular springs */
  RBC_FLAG_USE_SPRING_ANG_X = (1 << 16),
  RBC_FLAG_USE_SPRING_ANG_Y = (1 << 17),
  RBC_FLAG_USE_SPRING_ANG_Z = (1 << 18),
} eRigidBodyCon_Flag;

/* ******************************** */

#ifdef __cplusplus
}
#endif
