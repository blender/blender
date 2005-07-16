/*
 * Copyright (c) 2005 Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#ifndef CONTACT_CONSTRAINT_H
#define CONTACT_CONSTRAINT_H

//todo: make into a proper class working with the iterative constraint solver

class RigidBody;
#include "SimdVector3.h"
#include "SimdScalar.h"
struct ContactSolverInfo;

///bilateral constraint between two dynamic objects
void resolveSingleBilateral(RigidBody& body1, const SimdVector3& pos1,
                      RigidBody& body2, const SimdVector3& pos2,
                      SimdScalar depth, const SimdVector3& normal,SimdScalar& impulse ,float timeStep);


//contact constraint resolution:
//calculate and apply impulse to satisfy non-penetration and non-negative relative velocity constraint
float resolveSingleCollision(RigidBody& body1, const SimdVector3& pos1,
                      RigidBody& body2, const SimdVector3& pos2,
                      SimdScalar depth, const SimdVector3& normal, 
					  const ContactSolverInfo& info);


/// apply friction force to simulate friction in a contact point related to the normal impulse
void	applyFrictionInContactPointOld(RigidBody& body1, const SimdVector3& pos1,
                      RigidBody& body2, const SimdVector3& pos2,
                      const SimdVector3& normal,float normalImpulse,
					  const ContactSolverInfo& info);

//contact constraint resolution:
//calculate and apply impulse to satisfy non-penetration and non-negative relative velocity constraint
float resolveSingleCollisionWithFriction(RigidBody& body1, const SimdVector3& pos1,
                      RigidBody& body2, const SimdVector3& pos2,
                      SimdScalar depth, const SimdVector3& normal, 
					  const ContactSolverInfo& info);

#endif //CONTACT_CONSTRAINT_H
