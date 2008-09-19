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

#ifndef CONVEX_SHAPE_INTERFACE1
#define CONVEX_SHAPE_INTERFACE1

#include "btCollisionShape.h"

#include "../../LinearMath/btVector3.h"
#include "../../LinearMath/btTransform.h"
#include "../../LinearMath/btMatrix3x3.h"
#include "btCollisionMargin.h"

//todo: get rid of this btConvexCastResult thing!
struct btConvexCastResult;
#define MAX_PREFERRED_PENETRATION_DIRECTIONS 10

/// btConvexShape is an abstract shape interface.
/// The explicit part provides plane-equations, the implicit part provides GetClosestPoint interface.
/// used in combination with GJK or btConvexCast
ATTRIBUTE_ALIGNED16(class) btConvexShape : public btCollisionShape
{

protected:

	//local scaling. collisionMargin is not scaled !
	btVector3	m_localScaling;

	btVector3	m_implicitShapeDimensions;
	
	btScalar	m_collisionMargin;

	btScalar	m_padding[2];




public:
	btConvexShape();

	virtual ~btConvexShape()
	{

	}


	virtual btVector3	localGetSupportingVertex(const btVector3& vec)const;
#ifndef __SPU__
	virtual btVector3	localGetSupportingVertexWithoutMargin(const btVector3& vec) const= 0;
	
	//notice that the vectors should be unit length
	virtual void	batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const= 0;
#endif //#ifndef __SPU__

	const btVector3& getImplicitShapeDimensions() const
	{
		return m_implicitShapeDimensions;
	}

	///getAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
	void getAabb(const btTransform& t,btVector3& aabbMin,btVector3& aabbMax) const
	{
		getAabbSlow(t,aabbMin,aabbMax);
	}


	
	virtual void getAabbSlow(const btTransform& t,btVector3& aabbMin,btVector3& aabbMax) const;


	virtual void	setLocalScaling(const btVector3& scaling);
	virtual const btVector3& getLocalScaling() const 
	{
		return m_localScaling;
	}

	const btVector3& getLocalScalingNV() const 
	{
		return m_localScaling;
	}

	virtual void	setMargin(btScalar margin)
	{
		m_collisionMargin = margin;
	}
	virtual btScalar	getMargin() const
	{
		return m_collisionMargin;
	}

	btScalar	getMarginNV() const
	{
		return m_collisionMargin;
	}

	virtual int		getNumPreferredPenetrationDirections() const
	{
		return 0;
	}
	
	virtual void	getPreferredPenetrationDirection(int index, btVector3& penetrationVector) const
	{
		(void)penetrationVector;
		(void)index;
		btAssert(0);
	}



}
;



#endif //CONVEX_SHAPE_INTERFACE1
