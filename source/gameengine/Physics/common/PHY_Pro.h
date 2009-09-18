/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef PHY_PROPSH
#define PHY_PROPSH

#include <MT_Scalar.h>

// Properties of dynamic objects
struct PHY_ShapeProps {
	MT_Scalar  m_mass;                  // Total mass
	MT_Scalar  m_inertia;               // Inertia, should be a tensor some time 
	MT_Scalar  m_lin_drag;              // Linear drag (air, water) 0 = concrete, 1 = vacuum, inverted and called dampening in blenders UI
	MT_Scalar  m_ang_drag;              // Angular drag, inverted and called dampening in blenders UI
	MT_Scalar  m_friction_scaling[3];   // Scaling for anisotropic friction. Component in range [0, 1]   
	MT_Scalar  m_clamp_vel_min;			// Clamp the minimum velocity, this ensures an object moves at a minimum speed unless its stationary
	MT_Scalar  m_clamp_vel_max;			// Clamp max velocity
	bool       m_do_anisotropic;        // Should I do anisotropic friction? 
	bool       m_do_fh;                 // Should the object have a linear Fh spring?
	bool       m_do_rot_fh;             // Should the object have an angular Fh spring?
};


// Properties of collidable objects (non-ghost objects)
struct PHY_MaterialProps {
	MT_Scalar m_restitution;           // restitution of energie after a collision 0 = inelastic, 1 = elastic
	MT_Scalar m_friction;              // Coulomb friction (= ratio between the normal en maximum friction force)
	MT_Scalar m_fh_spring;             // Spring constant (both linear and angular)
	MT_Scalar m_fh_damping;            // Damping factor (linear and angular) in range [0, 1]
	MT_Scalar m_fh_distance;           // The range above the surface where Fh is active.    
	bool      m_fh_normal;             // Should the object slide off slopes?
};

#endif //PHY_PROPSH

