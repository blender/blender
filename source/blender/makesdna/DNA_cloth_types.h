/**
* $Id: DNA_cloth_types.h,v 1.1 2007/08/01 02:28:34 daniel Exp $
*
* ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version. The Blender
* Foundation also sells licenses for use in proprietary software under
* the Blender License.  See http://www.blender.org/BL/ for information
* about this.
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
* Contributor(s): Daniel (Genscher), Todd Koeckeritz  (zaz)
*
* ***** END GPL/BL DUAL LICENSE BLOCK *****
*/
#ifndef DNA_CLOTH_TYPES_H
#define DNA_CLOTH_TYPES_H

typedef struct SimulationSettings {
	short	vgroup_mass;	/* optional vertexgroup name for assigning weight.	*/
	short	pad;
	float 	mingoal; 	/* see SB */
	int	preroll;	/* How many frames of simulation to do before we start.	*/
	float	Cdis;		/* Mechanical damping of springs.		*/
	float	Cvi;		/* Viscous/fluid damping.			*/
	int 	stepsPerFrame;	/* Number of time steps per frame.			*/
	float	gravity [3];	/* Gravity/external force vector.			*/
	float	ufluid [3];	/* Velocity vector of the fluid.		*/
	float	dt;		/* This is the duration of our time step, computed.		*/
	float	mass;		/* The mass of the entire cloth.		*/
	float	structural;	/* Structural spring stiffness.			*/
	float	shear;		/* Shear spring stiffness.			*/
	float	bending;	/* Flexion spring stiffness.			*/
	float	sim_time;
	int	flags;		/* flags, see CSIMSETT_FLAGS enum above.	*/
	short	solver_type; 	/* which solver should be used?				*/
	short	pad2;
	float	maxgoal; 	/* see SB */
	float	eff_force_scale;/* Scaling of effector forces (see softbody_calc_forces).*/
	float	eff_wind_scale;	/* Scaling of effector wind (see softbody_calc_forces).	*/
	float 	sim_time_old;
	float	defgoal;
	float	goalfrict;
	float	goalspring;
	int	maxspringlen; 	/* in percent!; if tearing enabled, a spring will get cut */
	int 	lastframe; 	/* frame on which simulation stops */
	int	firstframe;	/* frame on which simulation starts */
} SimulationSettings;

typedef struct CollisionSettings {
	float	epsilon;		/* The radius of a particle in the cloth.		*/
	float	self_friction;		/* Fiction/damping with self contact.		 	*/
	float	friction;		/* Friction/damping applied on contact with other object.*/
	short	collision_type;		/* which collision system is used.			*/
	short	loop_count;		/* How many iterations for the collision loop.		*/
	int	flags;			/* collision flags defined in BKE_cloth.h */
	float	selfepsilon;
} CollisionSettings;


#endif
