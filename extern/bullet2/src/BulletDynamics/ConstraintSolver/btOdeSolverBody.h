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

#ifndef ODE_SOLVER_BODY_H
#define ODE_SOLVER_BODY_H

class	btRigidBody;
#include "LinearMath/btVector3.h"
typedef btScalar dMatrix3[4*3];

///ODE's quickstep needs just a subset of the rigidbody data in its own layout, so make a temp copy
struct	btOdeSolverBody 
{
	btRigidBody*	m_originalBody;

	btVector3		m_centerOfMassPosition;
	/// for ode solver-binding
	dMatrix3		m_R;//temp
	dMatrix3		m_I;
	dMatrix3		m_invI;

	int				m_odeTag;
	float			m_invMass;
	float			m_friction;

	btVector3		m_tacc;//temp
	btVector3		m_facc;

	btVector3		m_linearVelocity;
	btVector3		m_angularVelocity;
		
};


#endif //#ifndef ODE_SOLVER_BODY_H

