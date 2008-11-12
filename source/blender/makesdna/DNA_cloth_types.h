/**
* $Id: DNA_cloth_types.h,v 1.1 2007/08/01 02:28:34 daniel Exp $
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
* The Original Code is Copyright (C) 2006 by NaN Holding BV.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): Daniel (Genscher)
*
* ***** END GPL LICENSE BLOCK *****
*/
#ifndef DNA_CLOTH_TYPES_H
#define DNA_CLOTH_TYPES_H

/**
* This struct contains all the global data required to run a simulation.
* At the time of this writing, this structure contains data appropriate
* to run a simulation as described in Deformation Constraints in a
* Mass-Spring Model to Describe Rigid Cloth Behavior by Xavier Provot.
*
* I've tried to keep similar, if not exact names for the variables as
* are presented in the paper.  Where I've changed the concept slightly,
* as in stepsPerFrame comapred to the time step in the paper, I've used
* variables with different names to minimize confusion.
**/

typedef struct ClothSimSettings
{
	struct	LinkNode *cache; /* UNUSED atm */	
	float 	mingoal; 	/* see SB */
	float	Cdis;		/* Mechanical damping of springs.		*/
	float	Cvi;		/* Viscous/fluid damping.			*/
	float	gravity [3];	/* Gravity/external force vector.		*/
	float	dt;		/* This is the duration of our time step, computed.	*/
	float	mass;		/* The mass of the entire cloth.		*/
	float	structural;	/* Structural spring stiffness.			*/
	float	shear;		/* Shear spring stiffness.			*/
	float	bending;	/* Flexion spring stiffness.			*/
	float	max_bend; 	/* max bending scaling value, min is "bending" */
	float	max_struct; 	/* max structural scaling value, min is "structural" */
	float	max_shear; 	/* max shear scaling value, UNUSED */
	float 	avg_spring_len; /* used for normalized springs */
	float 	timescale; /* parameter how fast cloth runs */
	float	maxgoal; 	/* see SB */
	float	eff_force_scale;/* Scaling of effector forces (see softbody_calc_forces).*/
	float	eff_wind_scale;	/* Scaling of effector wind (see softbody_calc_forces).	*/
	float 	sim_time_old;
	float	defgoal;
	float	goalspring;
	float	goalfrict;
	int 	stepsPerFrame;	/* Number of time steps per frame.		*/
	int	flags;		/* flags, see CSIMSETT_FLAGS enum above.	*/
	int	preroll;	/* How many frames of simulation to do before we start.	*/
	int	maxspringlen; 	/* in percent!; if tearing enabled, a spring will get cut */
	short	solver_type; 	/* which solver should be used?		txold	*/
	short	vgroup_bend;	/* vertex group for scaling bending stiffness */
	short	vgroup_mass;	/* optional vertexgroup name for assigning weight.*/
	short	vgroup_struct;  /* vertex group for scaling structural stiffness */
	short	presets; /* used for presets on GUI */
	short 	pad;
	int 	pad2;
} ClothSimSettings;


typedef struct ClothCollSettings
{
	struct	LinkNode *collision_list; /* e.g. pointer to temp memory for collisions */
	float	epsilon;		/* min distance for collisions.		*/
	float	self_friction;		/* Fiction/damping with self contact.		 	*/
	float	friction;		/* Friction/damping applied on contact with other object.*/
	float 	selfepsilon; 		/* for selfcollision */
	int	flags;			/* collision flags defined in BKE_cloth.h */
	short	self_loop_count;	/* How many iterations for the selfcollision loop	*/
	short	loop_count;		/* How many iterations for the collision loop.		*/
} ClothCollSettings;


#endif
