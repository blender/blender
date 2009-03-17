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

#ifndef CYLINDER_MINKOWSKI_H
#define CYLINDER_MINKOWSKI_H

#include "btBoxShape.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h" // for the types
#include "LinearMath/btVector3.h"

/// The btCylinderShape class implements a cylinder shape primitive, centered around the origin. Its central axis aligned with the Y axis. btCylinderShapeX is aligned with the X axis and btCylinderShapeZ around the Z axis.
class btCylinderShape : public btBoxShape

{

protected:

	int	m_upAxis;

public:
	btCylinderShape (const btVector3& halfExtents);
	
	///getAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
	void getAabb(const btTransform& t,btVector3& aabbMin,btVector3& aabbMax) const;

	virtual btVector3	localGetSupportingVertexWithoutMargin(const btVector3& vec)const;

	virtual void	batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const;

	virtual btVector3	localGetSupportingVertex(const btVector3& vec) const
	{

		btVector3 supVertex;
		supVertex = localGetSupportingVertexWithoutMargin(vec);
		
		if ( getMargin()!=btScalar(0.) )
		{
			btVector3 vecnorm = vec;
			if (vecnorm .length2() < (SIMD_EPSILON*SIMD_EPSILON))
			{
				vecnorm.setValue(btScalar(-1.),btScalar(-1.),btScalar(-1.));
			} 
			vecnorm.normalize();
			supVertex+= getMargin() * vecnorm;
		}
		return supVertex;
	}


	//use box inertia
	//	virtual void	calculateLocalInertia(btScalar mass,btVector3& inertia) const;


	int	getUpAxis() const
	{
		return m_upAxis;
	}

	virtual btScalar getRadius() const
	{
		return getHalfExtentsWithMargin().getX();
	}

	//debugging
	virtual const char*	getName()const
	{
		return "CylinderY";
	}



};

class btCylinderShapeX : public btCylinderShape
{
public:
	btCylinderShapeX (const btVector3& halfExtents);

	virtual btVector3	localGetSupportingVertexWithoutMargin(const btVector3& vec)const;
	virtual void	batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const;
	
		//debugging
	virtual const char*	getName()const
	{
		return "CylinderX";
	}

	virtual btScalar getRadius() const
	{
		return getHalfExtentsWithMargin().getY();
	}

};

class btCylinderShapeZ : public btCylinderShape
{
public:
	btCylinderShapeZ (const btVector3& halfExtents);

	virtual btVector3	localGetSupportingVertexWithoutMargin(const btVector3& vec)const;
	virtual void	batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const;

	virtual int	getUpAxis() const
	{
		return 2;
	}
		//debugging
	virtual const char*	getName()const
	{
		return "CylinderZ";
	}

	virtual btScalar getRadius() const
	{
		return getHalfExtentsWithMargin().getX();
	}

};


#endif //CYLINDER_MINKOWSKI_H

