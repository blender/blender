/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
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

