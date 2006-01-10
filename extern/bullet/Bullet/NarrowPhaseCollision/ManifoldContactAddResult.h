/*
 * Copyright (c) 2005 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/

#ifndef MANIFOLD_CONTACT_ADD_RESULT_H
#define MANIFOLD_CONTACT_ADD_RESULT_H

#include "NarrowPhaseCollision/DiscreteCollisionDetectorInterface.h"
class PersistentManifold;

class ManifoldContactAddResult : public DiscreteCollisionDetectorInterface::Result
{
	PersistentManifold* m_manifoldPtr;
	SimdTransform	m_transAInv;
	SimdTransform	m_transBInv;

public:

	ManifoldContactAddResult(SimdTransform transA,SimdTransform transB,PersistentManifold* manifoldPtr);

	virtual void AddContactPoint(const SimdVector3& normalOnBInWorld,const SimdVector3& pointInWorld,float depth);

};

#endif //MANIFOLD_CONTACT_ADD_RESULT_H
