/*
 * Copyright (c) 2001-2005 Erwin Coumans <phy@erwincoumans.com>
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Erwin Coumans makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifndef PHY_PROPSH
#define PHY_PROPSH


class CollisionShape;

// Properties of dynamic objects
struct PHY_ShapeProps {
	float  m_mass;                  // Total mass
	float  m_inertia;               // Inertia, should be a tensor some time 
	float  m_lin_drag;              // Linear drag (air, water) 0 = concrete, 1 = vacuum 
	float  m_ang_drag;              // Angular drag
	float  m_friction_scaling[3];   // Scaling for anisotropic friction. Component in range [0, 1]   
	bool       m_do_anisotropic;        // Should I do anisotropic friction? 
	bool       m_do_fh;                 // Should the object have a linear Fh spring?
	bool       m_do_rot_fh;             // Should the object have an angular Fh spring?
	CollisionShape*	m_shape;
};


// Properties of collidable objects (non-ghost objects)
struct PHY_MaterialProps {
	float m_restitution;           // restitution of energie after a collision 0 = inelastic, 1 = elastic
	float m_friction;              // Coulomb friction (= ratio between the normal en maximum friction force)
	float m_fh_spring;             // Spring constant (both linear and angular)
	float m_fh_damping;            // Damping factor (linear and angular) in range [0, 1]
	float m_fh_distance;           // The range above the surface where Fh is active.    
	bool      m_fh_normal;             // Should the object slide off slopes?
};

#endif //PHY_PROPSH

