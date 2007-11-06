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

#ifndef COLLISION_SHAPE_H
#define COLLISION_SHAPE_H

#include "LinearMath/btTransform.h"
#include "LinearMath/btVector3.h"
#include "LinearMath/btMatrix3x3.h"
#include "LinearMath/btPoint3.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h" //for the shape types

///btCollisionShape provides interface for collision shapes that can be shared among btCollisionObjects.
class btCollisionShape
{
public:

	btCollisionShape() 
	{
	}
	virtual ~btCollisionShape()
	{
	}

	///getAabb returns the axis aligned bounding box in the coordinate frame of the given transform t.
	virtual void getAabb(const btTransform& t,btVector3& aabbMin,btVector3& aabbMax) const =0;

	virtual void	getBoundingSphere(btVector3& center,btScalar& radius) const;

	///getAngularMotionDisc returns the maximus radius needed for Conservative Advancement to handle time-of-impact with rotations.
	virtual btScalar	getAngularMotionDisc() const;


	///calculateTemporalAabb calculates the enclosing aabb for the moving object over interval [0..timeStep)
	///result is conservative
	void calculateTemporalAabb(const btTransform& curTrans,const btVector3& linvel,const btVector3& angvel,btScalar timeStep, btVector3& temporalAabbMin,btVector3& temporalAabbMax);

#ifndef __SPU__

	SIMD_FORCE_INLINE bool	isPolyhedral() const
	{
		return btBroadphaseProxy::isPolyhedral(getShapeType());
	}

	SIMD_FORCE_INLINE bool	isConvex() const
	{
		return btBroadphaseProxy::isConvex(getShapeType());
	}
	SIMD_FORCE_INLINE bool	isConcave() const
	{
		return btBroadphaseProxy::isConcave(getShapeType());
	}
	SIMD_FORCE_INLINE bool	isCompound() const
	{
		return btBroadphaseProxy::isCompound(getShapeType());
	}

	///isInfinite is used to catch simulation error (aabb check)
	SIMD_FORCE_INLINE bool isInfinite() const
	{
		return btBroadphaseProxy::isInfinite(getShapeType());
	}

	virtual int		getShapeType() const=0;
	virtual void	setLocalScaling(const btVector3& scaling) =0;
	virtual const btVector3& getLocalScaling() const =0;
	virtual void	calculateLocalInertia(btScalar mass,btVector3& inertia) const = 0;


//debugging support
	virtual const char*	getName()const =0 ;
#endif //__SPU__

	

	virtual void	setMargin(btScalar margin) = 0;
	virtual btScalar	getMargin() const = 0;

};	

#endif //COLLISION_SHAPE_H

