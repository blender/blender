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
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_CLOTH_TYPES_H__
#define __DNA_CLOTH_TYPES_H__

#include "DNA_defs.h"

/**
 * This struct contains all the global data required to run a simulation.
 * At the time of this writing, this structure contains data appropriate
 * to run a simulation as described in Deformation Constraints in a
 * Mass-Spring Model to Describe Rigid Cloth Behavior by Xavier Provot.
 *
 * I've tried to keep similar, if not exact names for the variables as
 * are presented in the paper.  Where I've changed the concept slightly,
 * as in stepsPerFrame compared to the time step in the paper, I've used
 * variables with different names to minimize confusion.
 */

typedef struct ClothSimSettings {
  /** UNUSED atm. */
  struct LinkNode *cache;
  /** See SB. */
  float mingoal;
  /** Mechanical damping of springs. */
  float Cdis DNA_DEPRECATED;
  /** Viscous/fluid damping. */
  float Cvi;
  /** Gravity/external force vector. */
  float gravity[3];
  /** This is the duration of our time step, computed..   */
  float dt;
  /** The mass of the entire cloth. */
  float mass;
  /** Structural spring stiffness. */
  float structural DNA_DEPRECATED;
  /** Shear spring stiffness. */
  float shear;
  /** Flexion spring stiffness. */
  float bending;
  /** Max bending scaling value, min is "bending". */
  float max_bend;
  /** Max structural scaling value, min is "structural". */
  float max_struct DNA_DEPRECATED;
  /** Max shear scaling value. */
  float max_shear;
  /** Max sewing force. */
  float max_sewing;
  /** Used for normalized springs. */
  float avg_spring_len;
  /** Parameter how fast cloth runs. */
  float timescale;
  /** Multiplies cloth speed. */
  float time_scale;
  /** See SB. */
  float maxgoal;
  /** Scaling of effector forces (see softbody_calc_forces)..*/
  float eff_force_scale;
  /** Scaling of effector wind (see softbody_calc_forces)..   */
  float eff_wind_scale;
  float sim_time_old;
  float defgoal;
  float goalspring;
  float goalfrict;
  /** Smoothing of velocities for hair. */
  float velocity_smooth;
  /** Minimum density for hair. */
  float density_target;
  /** Influence of hair density. */
  float density_strength;
  /** Friction with colliders. */
  float collider_friction;
  /** Damp the velocity to speed up getting to the resting position. */
  float vel_damping DNA_DEPRECATED;
  /** Min amount to shrink cloth by 0.0f (no shrink), 1.0f (shrink to nothing), -1.0f (double the
   * edge length). */
  float shrink_min;
  /** Max amount to shrink cloth by 0.0f (no shrink), 1.0f (shrink to nothing), -1.0f (double the
   * edge length). */
  float shrink_max;

  /* Air pressure */
  /* The uniform pressure that is constanty applied to the mesh. Can be negative */
  float uniform_pressure_force;
  /* User set volume. This is the volume the mesh wants to expand to (the equilibrium volume). */
  float target_volume;
  /* The scaling factor to apply to the actual pressure.
     pressure=( (current_volume/target_volume) - 1 + uniform_pressure_force) *
     pressure_factor */
  float pressure_factor;
  /* Density of the fluid inside or outside the object for use in the hydrostatic pressure
   * gradient. */
  float fluid_density;
  short vgroup_pressure;
  char _pad7[6];

  /* XXX various hair stuff
   * should really be separate, this struct is a horrible mess already
   */
  /** Damping of bending springs. */
  float bending_damping;
  /** Size of voxel grid cells for continuum dynamics. */
  float voxel_cell_size;

  /** Number of time steps per frame. */
  int stepsPerFrame;
  /** Flags, see CSIMSETT_FLAGS enum above. */
  int flags;
  /** How many frames of simulation to do before we start. */
  int preroll DNA_DEPRECATED;
  /** In percent!; if tearing enabled, a spring will get cut. */
  int maxspringlen;
  /** Which solver should be used? txold. */
  short solver_type;
  /** Vertex group for scaling bending stiffness. */
  short vgroup_bend;
  /** Optional vertexgroup name for assigning weight..*/
  short vgroup_mass;
  /** Vertex group for scaling structural stiffness. */
  short vgroup_struct;
  /** Vertex group for shrinking cloth. */
  short vgroup_shrink;
  /** Vertex group for scaling structural stiffness. */
  short shapekey_rest;
  /** Used for presets on GUI. */
  short presets;
  short reset;

  struct EffectorWeights *effector_weights;

  short bending_model;
  /** Vertex group for scaling structural stiffness. */
  short vgroup_shear;
  float tension;
  float compression;
  float max_tension;
  float max_compression;
  /** Mechanical damping of tension springs. */
  float tension_damp;
  /** Mechanical damping of compression springs. */
  float compression_damp;
  /** Mechanical damping of shear springs. */
  float shear_damp;

  /** The maximum lenght an internal spring can have during creation. */
  float internal_spring_max_length;
  /** How much the interal spring can diverge from the vertex normal during creation. */
  float internal_spring_max_diversion;
  /** Vertex group for scaling structural stiffness. */
  short vgroup_intern;
  char _pad1[2];
  float internal_tension;
  float internal_compression;
  float max_internal_tension;
  float max_internal_compression;
  char _pad0[4];

} ClothSimSettings;

typedef struct ClothCollSettings {
  /** E.g. pointer to temp memory for collisions. */
  struct LinkNode *collision_list;
  /** Min distance for collisions. */
  float epsilon;
  /** Fiction/damping with self contact. */
  float self_friction;
  /** Friction/damping applied on contact with other object. */
  float friction;
  /** Collision restitution on contact with other object. */
  float damping;
  /** For selfcollision. */
  float selfepsilon;
  float repel_force DNA_DEPRECATED;
  float distance_repel DNA_DEPRECATED;
  /** Collision flags defined in BKE_cloth.h. */
  int flags;
  /** How many iterations for the selfcollision loop. */
  short self_loop_count DNA_DEPRECATED;
  /** How many iterations for the collision loop. */
  short loop_count;
  char _pad[4];
  /** Only use colliders from this group of objects. */
  struct Collection *group;
  /** Vgroup to paint which vertices are used for self collisions. */
  short vgroup_selfcol;
  char _pad2[6];
  /** Impulse clamp for object collisions. */
  float clamp;
  /** Impulse clamp for self collisions. */
  float self_clamp;
} ClothCollSettings;

#endif
