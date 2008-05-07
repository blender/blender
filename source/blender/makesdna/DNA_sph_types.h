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
 * Contributor(s): Daniel (Genscher)
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef DNA_SPH_TYPES_H
#define DNA_SPH_TYPES_H

// string scene, constraint, backup: missing
//	string _file;
//	string pov;
//	string rendered;
typedef struct SphSimSettings
{
	int flags; // see pw_extern.h
	float timestep; 
	float viscosity;
	float incompressibility; /* how incompressible is the fluid? */
	float surfacetension;
	float density;
	float gravity[3]; /* gravity on the domain */
	float samplingdistance;
	float smoothinglength;

	float controlviscosity;
	int dumpimageevery;
	int computesurfaceevery;
	int fastmarchingevery;
	int dumppovrayevery;
	
	float totaltime;
	
	float tangentialfriction; /* constraint tangential friction */
	float normalfriction; /* constraint normal friction */

	float rotation_angle;
	float rotation_axis[3];
	float rotation_center[3];
	float scenelowerbound[3];
	float sceneupperbound[3];
	
	int initiallevel;
	
	float alpha;
	float beta;
	float gamma;
	
	/* needed for better direct resolution input for constraints, fluids,... */
	int resolution; /* can also be calculated by (Max-Min) / samplingdistance */
	int pad;
	float *verts;
	float *normals;
	int *tris;
	unsigned int numverts;
	unsigned int numtris;
	float *co; /* particle positions */
	float *r; /* particle radius */
	long numpart;
	int pad2;
}
SphSimSettings;

typedef struct SphCollSettings
{
	float	epsilon;		/* min distance for collisions.		*/
	float	self_friction;		/* Fiction/damping with self contact.		 	*/
	float	friction;		/* Friction/damping applied on contact with other object.*/
	short	self_loop_count;	/* How many iterations for the selfcollision loop	*/
	short	loop_count;		/* How many iterations for the collision loop.		*/
	struct	LinkNode *collision_list; /* e.g. pointer to temp memory for collisions */
	int	flags;			/* collision flags defined in BKE_cloth.h */
	float 	selfepsilon; 		/* for selfcollision */
}
SphCollSettings;

#endif // DNA_SPH_TYPES_H
