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
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel (Genscher)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_cloth_types.h
 *  \ingroup DNA
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
	struct	LinkNode *cache; /* UNUSED atm */
	float 	mingoal; 	/* see SB */
	float	Cdis;		/* Mechanical damping of springs.		*/
	float	Cvi;		/* Viscous/fluid damping.			*/
	float	gravity[3];	/* Gravity/external force vector.		*/
	float	dt;		/* This is the duration of our time step, computed.	*/
	float	mass;		/* The mass of the entire cloth.		*/
	float	structural;	/* Structural spring stiffness.			*/
	float	shear;		/* Shear spring stiffness.			*/
	float	bending;	/* Flexion spring stiffness.			*/
	float	max_bend; 	/* max bending scaling value, min is "bending" */
	float	max_struct; 	/* max structural scaling value, min is "structural" */
	float	max_shear; 	/* max shear scaling value, UNUSED */
	float	max_sewing; 	/* max sewing force */
	float 	avg_spring_len; /* used for normalized springs */
	float 	timescale; /* parameter how fast cloth runs */
	float	time_scale; /* multiplies cloth speed */
	float	maxgoal; 	/* see SB */
	float	eff_force_scale;/* Scaling of effector forces (see softbody_calc_forces).*/
	float	eff_wind_scale;	/* Scaling of effector wind (see softbody_calc_forces).	*/
	float 	sim_time_old;
	float	defgoal;
	float	goalspring;
	float	goalfrict;
	float	velocity_smooth; /* smoothing of velocities for hair */
	float	density_target;		/* minimum density for hair */
	float	density_strength;	/* influence of hair density */
	float	collider_friction; /* friction with colliders */
	float	vel_damping; /* damp the velocity to speed up getting to the resting position */
	float	shrink_min;  /* min amount to shrink cloth by 0.0f (no shrink) - 1.0f (shrink to nothing) */
	float	shrink_max;  /* max amount to shrink cloth by 0.0f (no shrink) - 1.0f (shrink to nothing) */
	
	/* XXX various hair stuff
	 * should really be separate, this struct is a horrible mess already
	 */
	float	bending_damping;	/* damping of bending springs */
	float	voxel_cell_size;    /* size of voxel grid cells for continuum dynamics */
	int		pad;

	int 	stepsPerFrame;	/* Number of time steps per frame.		*/
	int	flags;		/* flags, see CSIMSETT_FLAGS enum above.	*/
	int	preroll  DNA_DEPRECATED;	/* How many frames of simulation to do before we start.	*/
	int	maxspringlen; 	/* in percent!; if tearing enabled, a spring will get cut */
	short	solver_type; 	/* which solver should be used?		txold	*/
	short	vgroup_bend;	/* vertex group for scaling bending stiffness */
	short	vgroup_mass;	/* optional vertexgroup name for assigning weight.*/
	short	vgroup_struct;  /* vertex group for scaling structural stiffness */
	short	vgroup_shrink;  /* vertex group for shrinking cloth */
	short	shapekey_rest;  /* vertex group for scaling structural stiffness */
	short	presets; /* used for presets on GUI */
	short 	reset;

	char pad0[4];
	struct EffectorWeights *effector_weights;
} ClothSimSettings;


typedef struct ClothCollSettings {
	struct	LinkNode *collision_list; /* e.g. pointer to temp memory for collisions */
	float	epsilon;		/* min distance for collisions.		*/
	float	self_friction;		/* Fiction/damping with self contact. */
	float	friction;		/* Friction/damping applied on contact with other object.*/
	float	damping;	/* Collision restitution on contact with other object.*/
	float 	selfepsilon; 		/* for selfcollision */
	float repel_force, distance_repel;
	int	flags;			/* collision flags defined in BKE_cloth.h */
	short	self_loop_count;	/* How many iterations for the selfcollision loop	*/
	short	loop_count;		/* How many iterations for the collision loop.		*/
	int pad;
	struct Group *group;	/* Only use colliders from this group of objects */
	short	vgroup_selfcol; /* vgroup to paint which vertices are used for self collisions */
	short pad2[3];
} ClothCollSettings;


#endif
