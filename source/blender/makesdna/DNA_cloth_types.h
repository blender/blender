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

#include "DNA_listBase.h"


/**
* Pin and unpin frames are the frames on which the vertices stop moving.
* They will assume the position they had prior to pinFrame until unpinFrame
* is reached.
*/
typedef struct ClothVertex {
	int	flags;			/* General flags per vertex.			*/
	float	v [3];			/* The velocity of the point.			*/
	float	xconst [3];		/* constrained position		*/
	float	x [3];			/* The current position of this vertice.	*/
	float 	xold [3];		/* The previous position of this vertice.	*/
	float	tx [3];	
	float 	txold [3];
	float 	tv[3];
	float 	mass;			/* mass / weight of the vertex			*/
	float 	goal;			/* goal, from SB				*/
} ClothVertex;


/**
* The definition of a spring.
*/
typedef struct ClothSpring {
	int	ij;		/* Pij from the paper, one end of the spring.	*/
	int	kl;		/* Pkl from the paper, one end of the spring.	*/
	float	restlen;	/* The original length of the spring.	*/
	int	matrix_index; 	/* needed for implicit */
	int	type;
	int	flags; 		/* defined in BKE_cloth.h, e.g. deactivated due to tearing */
} ClothSpring;



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
typedef struct SimulationSettings {
	short	vgroup_mass;	/* optional vertexgroup name for assigning weight.	*/
	short	pad;
	float 	mingoal; /* see SB */
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
	float	maxgoal; /* see SB */
	float	eff_force_scale;/* Scaling of effector forces (see softbody_calc_forces).*/
	float	eff_wind_scale;	/* Scaling of effector wind (see softbody_calc_forces).	*/
	float 	sim_time_old;
	struct	LinkNode *cache;
	float	defgoal;
	int	goalfrict;
	float	goalspring;
	int	maxspringlen; /* in percent!; if tearing enabled, a spring will get cut */
} SimulationSettings;


typedef struct CollisionSettings {
	float	epsilon;		/* The radius of a particle in the cloth.		*/
	float	self_friction;		/* Fiction/damping with self contact.		 	*/
	float	friction;		/* Friction/damping applied on contact with other object.*/
	short	collision_type;		/* which collision system is used.			*/
	short	loop_count;		/* How many iterations for the collision loop.		*/
	void	*temp; /* e.g. pointer to temp memory for collisions */
} CollisionSettings;     


/**
* This structure describes a cloth object against which the
* simulation can run.
*
* The m and n members of this structure represent the assumed
* rectangular ordered grid for which the original paper is written.
* At some point they need to disappear and we need to determine out
* own connectivity of the mesh based on the actual edges in the mesh.
*
**/
typedef struct Cloth {
	struct ClothVertex	*verts;			/* The vertices that represent this cloth.	 */  
	struct ClothSpring	*springs;		/* The springs connecting the mesh.		 */
	unsigned int		numverts;		/* The number of verts == m * n.	 	 */
	unsigned int		numsprings;		/* The count of springs.	 	 	*/
	unsigned char		*facemarks;
	char			old_solver_type;	/* Needed to allow call to free if solver changes.	 */
	char			old_collision_type;	/* Needed to allow call to free if collision changes.*/
	short 			pad7;
	unsigned int		numfaces;
	int			pad2;
	int 			pad4;
	void			*tree;			/* collision tree for this cloth object */
	struct MFace 		*mfaces;
	void			*implicit; 	
} Cloth;

#endif
