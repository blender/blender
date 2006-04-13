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

#include "CollisionShape.h"

#include "SimdVector3.h"
#include "SimdTransform.h"
#include "SimdMatrix3x3.h"
#include <vector>
#include "CollisionShapes/CollisionMargin.h"

//todo: get rid of this ConvexCastResult thing!
struct ConvexCastResult;


/// ConvexShape is an abstract shape interface.
/// The explicit part provides plane-equations, the implicit part provides GetClosestPoint interface.
/// used in combination with GJK or ConvexCast
class ConvexShape : public CollisionShape
{
public:
	ConvexShape();

	virtual ~ConvexShape();

	virtual SimdVector3	LocalGetSupportingVertex(const SimdVector3& vec)const;
	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec) const= 0;
	
	//notice that the vectors should be unit length
	virtual void	BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const= 0;

	// testing for hullnode code

	///GetAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
	void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const
	{
		GetAabbSlow(t,aabbMin,aabbMax);
	}


	
	virtual void GetAabbSlow(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const;


	virtual void	setLocalScaling(const SimdVector3& scaling);
	virtual const SimdVector3& getLocalScaling() const 
	{
		return m_localScaling;
	}


	virtual void	SetMargin(float margin)
	{
		m_collisionMargin = margin;
	}
	virtual float	GetMargin() const
	{
		return m_collisionMargin;
	}
private:
	SimdScalar	m_collisionMargin;
	//local scaling. collisionMargin is not scaled !
protected:
	SimdVector3	m_localScaling;

};



#endif //CONVEX_SHAPE_INTERFACE1
