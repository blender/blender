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

#ifndef CONTACT_CONSTRAINT_H
#define CONTACT_CONSTRAINT_H

//todo: make into a proper class working with the iterative constraint solver

class RigidBody;
#include "SimdVector3.h"
#include "SimdScalar.h"
struct ContactSolverInfo;
class ManifoldPoint;

///bilateral constraint between two dynamic objects
///positive distance = separation, negative distance = penetration
void resolveSingleBilateral(RigidBody& body1, const SimdVector3& pos1,
                      RigidBody& body2, const SimdVector3& pos2,
                      SimdScalar distance, const SimdVector3& normal,SimdScalar& impulse ,float timeStep);


///contact constraint resolution:
///calculate and apply impulse to satisfy non-penetration and non-negative relative velocity constraint
///positive distance = separation, negative distance = penetration
float resolveSingleCollision(
	RigidBody& body1,
	RigidBody& body2,
		ManifoldPoint& contactPoint,
		 const ContactSolverInfo& info);

float resolveSingleFriction(
	RigidBody& body1,
	RigidBody& body2,
	ManifoldPoint& contactPoint,
	const ContactSolverInfo& solverInfo
		);

#endif //CONTACT_CONSTRAINT_H
