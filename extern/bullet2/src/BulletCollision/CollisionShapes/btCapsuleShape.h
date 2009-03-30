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

#ifndef BT_CAPSULE_SHAPE_H
#define BT_CAPSULE_SHAPE_H

#include "btConvexInternalShape.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h" // for the types


///The btCapsuleShape represents a capsule around the Y axis, there is also the btCapsuleShapeX aligned around the X axis and btCapsuleShapeZ around the Z axis.
///The total height is height+2*radius, so the height is just the height between the center of each 'sphere' of the capsule caps.
///The btCapsuleShape is a convex hull of two spheres. The btMultiSphereShape is a more general collision shape that takes the convex hull of multiple sphere, so it can also represent a capsule when just using two spheres.
class btCapsuleShape : public btConvexInternalShape
{
protected:
	int	m_upAxis;

protected:
	///only used for btCapsuleShapeZ and btCapsuleShapeX subclasses.
	btCapsuleShape() : btConvexInternalShape() {m_shapeType = CAPSULE_SHAPE_PROXYTYPE;};

public:
	btCapsuleShape(btScalar radius,btScalar height);

	///CollisionShape Interface
	virtual void	calculateLocalInertia(btScalar mass,btVector3& inertia) const;

	/// btConvexShape Interface
	virtual btVector3	localGetSupportingVertexWithoutMargin(const btVector3& vec)const;

	virtual void	batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const;
	
	virtual void getAabb (const btTransform& t, btVector3& aabbMin, btVector3& aabbMax) const
	{
			btVector3 halfExtents(getRadius(),getRadius(),getRadius());
			halfExtents[m_upAxis] = getRadius() + getHalfHeight();
			halfExtents += btVector3(getMargin(),getMargin(),getMargin());
			btMatrix3x3 abs_b = t.getBasis().absolute();  
			btVector3 center = t.getOrigin();
			btVector3 extent = btVector3(abs_b[0].dot(halfExtents),abs_b[1].dot(halfExtents),abs_b[2].dot(halfExtents));		  
			
			aabbMin = center - extent;
			aabbMax = center + extent;
	}

	virtual const char*	getName()const 
	{
		return "CapsuleShape";
	}

	int	getUpAxis() const
	{
		return m_upAxis;
	}

	btScalar	getRadius() const
	{
		int radiusAxis = (m_upAxis+2)%3;
		return m_implicitShapeDimensions[radiusAxis];
	}

	btScalar	getHalfHeight() const
	{
		return m_implicitShapeDimensions[m_upAxis];
	}

};

///btCapsuleShapeX represents a capsule around the Z axis
///the total height is height+2*radius, so the height is just the height between the center of each 'sphere' of the capsule caps.
class btCapsuleShapeX : public btCapsuleShape
{
public:

	btCapsuleShapeX(btScalar radius,btScalar height);
		
	//debugging
	virtual const char*	getName()const
	{
		return "CapsuleX";
	}

	

};

///btCapsuleShapeZ represents a capsule around the Z axis
///the total height is height+2*radius, so the height is just the height between the center of each 'sphere' of the capsule caps.
class btCapsuleShapeZ : public btCapsuleShape
{
public:
	btCapsuleShapeZ(btScalar radius,btScalar height);

		//debugging
	virtual const char*	getName()const
	{
		return "CapsuleZ";
	}

	
};



#endif //BT_CAPSULE_SHAPE_H
