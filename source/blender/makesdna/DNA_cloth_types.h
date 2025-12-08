/* SPDX-FileCopyrightText: 2006 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_math_constants.h"

#include "DNA_defs.h"

/* SIMULATION FLAGS: goal flags, etc. */
/* These are the bits used in SimSettings.flags. */
enum CLOTH_SIMSETTINGS_FLAGS {
  /** Object is only collision object, no cloth simulation is done. */
  CLOTH_SIMSETTINGS_FLAG_COLLOBJ = (1 << 2),
  /** DEPRECATED, for versioning only. */
  CLOTH_SIMSETTINGS_FLAG_GOAL = (1 << 3),
  /** True if tearing is enabled. */
  CLOTH_SIMSETTINGS_FLAG_TEARING = (1 << 4),
  /** True if pressure sim is enabled. */
  CLOTH_SIMSETTINGS_FLAG_PRESSURE = (1 << 5),
  /** Use the user defined target volume. */
  CLOTH_SIMSETTINGS_FLAG_PRESSURE_VOL = (1 << 6),
  /** True if internal spring generation is enabled. */
  CLOTH_SIMSETTINGS_FLAG_INTERNAL_SPRINGS = (1 << 7),
  /** DEPRECATED, for versioning only. */
  CLOTH_SIMSETTINGS_FLAG_SCALING = (1 << 8),
  /** Require internal springs to be created between points with opposite normals. */
  CLOTH_SIMSETTINGS_FLAG_INTERNAL_SPRINGS_NORMAL = (1 << 9),
  /** Edit cache in edit-mode. */
  /* CLOTH_SIMSETTINGS_FLAG_CCACHE_EDIT = (1 << 12), */ /* UNUSED */
  /** Don't allow spring compression. */
  CLOTH_SIMSETTINGS_FLAG_RESIST_SPRING_COMPRESS = (1 << 13),
  /** Pull ends of loose edges together. */
  CLOTH_SIMSETTINGS_FLAG_SEW = (1 << 14),
  /** Make simulation respect deformations in the base object. */
  CLOTH_SIMSETTINGS_FLAG_DYNAMIC_BASEMESH = (1 << 15),
};

/* ClothSimSettings.bending_model. */
enum CLOTH_BENDING_MODEL {
  CLOTH_BENDING_LINEAR = 0,
  CLOTH_BENDING_ANGULAR = 1,
};

/* COLLISION FLAGS */
enum CLOTH_COLLISIONSETTINGS_FLAGS {
  CLOTH_COLLSETTINGS_FLAG_ENABLED = (1 << 1), /* enables cloth - object collisions */
  CLOTH_COLLSETTINGS_FLAG_SELF = (1 << 2),    /* enables selfcollisions */
};

/**
 * This struct contains all the global data required to run a simulation.
 * At the time of this writing, this structure contains data appropriate
 * to run a simulation as described in Deformation Constraints in a
 * Mass-Spring Model to Describe Rigid Cloth Behavior by Xavier Provot.
 *
 * I've tried to keep similar, if not exact names for the variables as
 * are presented in the paper. Where I've changed the concept slightly,
 * as in `stepsPerFrame` compared to the time step in the paper, I've used
 * variables with different names to minimize confusion.
 */
struct ClothSimSettings {
  DNA_DEFINE_CXX_METHODS(ClothSimSettings)

  /** UNUSED. */
  struct LinkNode *cache = nullptr;
  /** See SB. */
  float mingoal = 0.0f;
  /** Mechanical damping of springs. */
  DNA_DEPRECATED float Cdis = {};
  /** Viscous/fluid damping. */
  float Cvi = 1.0f;
  /** Gravity/external force vector. */
  float gravity[3] = {0.0f, 0.0f, -9.81f};
  /** This is the duration of our time step, computed. */
  float dt = 0.0f;
  /** The mass of the entire cloth. */
  float mass = 0.3f;
  /** Structural spring stiffness. */
  DNA_DEPRECATED float structural = 0;
  /** Shear spring stiffness. */
  float shear = 5.0f;
  /** Flexion spring stiffness. */
  float bending = 0.5f;
  /** Max bending scaling value, min is "bending". */
  float max_bend = 0.5f;
  /** Max structural scaling value, min is "structural". */
  DNA_DEPRECATED float max_struct = 0;
  /** Max shear scaling value. */
  float max_shear = 5.0f;
  /** Max sewing force. */
  float max_sewing = 0.0f;
  /** Used for normalized springs. */
  float avg_spring_len = 0.0f;
  /** Parameter how fast cloth runs. */
  float timescale = 1.0f;
  /** Multiplies cloth speed. */
  float time_scale = 1.0f;
  /** See SB. */
  float maxgoal = 1.0f;
  /** Scaling of effector forces (see #softbody_calc_forces). */
  float eff_force_scale = 1000.0f;
  /** Scaling of effector wind (see #softbody_calc_forces). */
  float eff_wind_scale = 250.0f;
  float sim_time_old = 0.0f;
  float defgoal = 0.0f;
  float goalspring = 1.0f;
  float goalfrict = 0.0f;
  /** Smoothing of velocities for hair. */
  float velocity_smooth = 0.0f;
  /** Minimum density for hair. */
  float density_target = 0.0f;
  /** Influence of hair density. */
  float density_strength = 0.0f;
  /** Friction with colliders. */
  float collider_friction = 0.0f;
  /** Damp the velocity to speed up getting to the resting position. */
  DNA_DEPRECATED float vel_damping = 0;
  /** Min amount to shrink cloth by 0.0f (no shrink), 1.0f (shrink to nothing), -1.0f (double the
   * edge length). */
  float shrink_min = 0.0f;
  /** Max amount to shrink cloth by 0.0f (no shrink), 1.0f (shrink to nothing), -1.0f (double the
   * edge length). */
  float shrink_max = 0.0f;

  /* Air pressure */
  /** The uniform pressure that is constantly applied to the mesh. Can be negative. */
  float uniform_pressure_force = 0.0f;
  /** User set volume. This is the volume the mesh wants to expand to (the equilibrium volume). */
  float target_volume = 0.0f;
  /**
   * The scaling factor to apply to the actual pressure.
   * `pressure = ((current_volume/target_volume) - 1 + uniform_pressure_force) * pressure_factor`
   */
  float pressure_factor = 1.0f;
  /**
   * Density of the fluid inside or outside the object
   * for use in the hydro-static pressure gradient.
   */
  float fluid_density = 0.0f;
  short vgroup_pressure = 0;
  char _pad7[6] = {};

  /* XXX various hair stuff
   * should really be separate, this struct is a horrible mess already
   */
  /** Damping of bending springs. */
  float bending_damping = 0.5f;
  /** Size of voxel grid cells for continuum dynamics. */
  float voxel_cell_size = 0.1f;

  /** Number of time steps per frame. */
  int stepsPerFrame = 5;
  /** Flags, see CSIMSETT_FLAGS enum above. */
  int flags = CLOTH_SIMSETTINGS_FLAG_INTERNAL_SPRINGS_NORMAL;
  /** How many frames of simulation to do before we start. */
  DNA_DEPRECATED int preroll = 0;
  /** In percent!; if tearing enabled, a spring will get cut. */
  int maxspringlen = 10;
  /** Which solver should be used? txold. */
  short solver_type = 0;
  /** Vertex group for scaling bending stiffness. */
  short vgroup_bend = 0;
  /** Optional vertexgroup name for assigning weight. */
  short vgroup_mass = 0;
  /** Vertex group for scaling structural stiffness. */
  short vgroup_struct = 0;
  /** Vertex group for shrinking cloth. */
  short vgroup_shrink = 0;
  /** Vertex group for scaling structural stiffness. */
  short shapekey_rest = 0;
  /** Used for presets on GUI. */
  short presets = 2;
  short reset = 0;

  struct EffectorWeights *effector_weights = nullptr;

  short bending_model = CLOTH_BENDING_ANGULAR;
  /** Vertex group for scaling structural stiffness. */
  short vgroup_shear = 0;
  float tension = 15.0f;
  float compression = 15.0f;
  float max_tension = 15.0f;
  float max_compression = 15.0f;
  /** Mechanical damping of tension springs. */
  float tension_damp = 5.0f;
  /** Mechanical damping of compression springs. */
  float compression_damp = 5.0f;
  /** Mechanical damping of shear springs. */
  float shear_damp = 5.0f;

  /** The maximum length an internal spring can have during creation. */
  float internal_spring_max_length = 0.0f;
  /** How much the internal spring can diverge from the vertex normal during creation. */
  float internal_spring_max_diversion = M_PI_4;
  /** Vertex group for scaling structural stiffness. */
  short vgroup_intern = 0;
  char _pad1[2] = {};
  float internal_tension = 15.0f;
  float internal_compression = 15.0f;
  float max_internal_tension = 15.0f;
  float max_internal_compression = 15.0f;
  char _pad0[4] = {};
};

struct ClothCollSettings {
  DNA_DEFINE_CXX_METHODS(ClothCollSettings)

  /** E.g. pointer to temp memory for collisions. */
  struct LinkNode *collision_list = nullptr;
  /** Min distance for collisions. */
  float epsilon = 0.015f;
  /** Fiction/damping with self contact. */
  float self_friction = 5.0f;
  /** Friction/damping applied on contact with other object. */
  float friction = 5.0f;
  /** Collision restitution on contact with other object. */
  float damping = 0.0f;
  /** For selfcollision. */
  float selfepsilon = 0.015f;
  DNA_DEPRECATED float repel_force = 0;
  DNA_DEPRECATED float distance_repel = 0;
  /** Collision flags defined in BKE_cloth.hh. */
  int flags = CLOTH_COLLSETTINGS_FLAG_ENABLED;
  /** How many iterations for the selfcollision loop. */
  DNA_DEPRECATED short self_loop_count = 0;
  /** How many iterations for the collision loop. */
  short loop_count = 2;
  char _pad[4] = {};
  /** Only use colliders from this group of objects. */
  struct Collection *group = nullptr;
  /** Vgroup to paint which vertices are not used for self collisions. */
  short vgroup_selfcol = 0;
  /** Vgroup to paint which vertices are not used for object collisions. */
  short vgroup_objcol = 0;
  char _pad2[4] = {};
  /** Impulse clamp for object collisions. */
  float clamp = 0.0f;
  /** Impulse clamp for self collisions. */
  float self_clamp = 0.0f;
};
