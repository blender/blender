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

#include "BoxShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types
#include "SimdVector3.h"

/// implements cylinder shape interface
class CylinderShape : public BoxShape

{

public:
	CylinderShape (const SimdVector3& halfExtents);
	
	///GetAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
	void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const
	{
		GetAabbSlow(t,aabbMin,aabbMax);
	}

	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;

	virtual void	BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const;

	virtual SimdVector3	LocalGetSupportingVertex(const SimdVector3& vec) const
	{

		SimdVector3 supVertex;
		supVertex = LocalGetSupportingVertexWithoutMargin(vec);
		
		if ( GetMargin()!=0.f )
		{
			SimdVector3 vecnorm = vec;
			if (vecnorm .length2() < (SIMD_EPSILON*SIMD_EPSILON))
			{
				vecnorm.setValue(-1.f,-1.f,-1.f);
			} 
			vecnorm.normalize();
			supVertex+= GetMargin() * vecnorm;
		}
		return supVertex;
	}


	//use box inertia
	//	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);

	virtual int	GetShapeType() const
	{
		return CYLINDER_SHAPE_PROXYTYPE;
	}
	
	virtual int	GetUpAxis() const
	{
		return 1;
	}

	//debugging
	virtual char*	GetName()const
	{
		return "CylinderY";
	}



};

class CylinderShapeX : public CylinderShape
{
public:
	CylinderShapeX (const SimdVector3& halfExtents);

	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;
	virtual void	BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const;
	virtual int	GetUpAxis() const
	{
		return 0;
	}
		//debugging
	virtual char*	GetName()const
	{
		return "CylinderX";
	}

};

class CylinderShapeZ : public CylinderShape
{
public:
	CylinderShapeZ (const SimdVector3& halfExtents);

	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;
	virtual void	BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const;

	virtual int	GetUpAxis() const
	{
		return 2;
	}
		//debugging
	virtual char*	GetName()const
	{
		return "CylinderZ";
	}

};


#endif //CYLINDER_MINKOWSKI_H

