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

#ifndef COMPOUND_SHAPE_H
#define COMPOUND_SHAPE_H

#include "CollisionShape.h"

#include "SimdVector3.h"
#include "SimdTransform.h"
#include "SimdMatrix3x3.h"
#include <vector>
#include "CollisionShapes/CollisionMargin.h"

class OptimizedBvh;

/// CompoundShape allows to store multiple other CollisionShapes
/// This allows for concave collision objects. This is more general then the Static Concave TriangleMeshShape.
class CompoundShape	: public CollisionShape
{
	std::vector<SimdTransform>		m_childTransforms;
	std::vector<CollisionShape*>	m_childShapes;
	SimdVector3						m_localAabbMin;
	SimdVector3						m_localAabbMax;

	OptimizedBvh*					m_aabbTree;

public:
	CompoundShape();

	virtual ~CompoundShape();

	void	AddChildShape(const SimdTransform& localTransform,CollisionShape* shape);

	int		GetNumChildShapes() const
	{
		return m_childShapes.size();
	}

	CollisionShape* GetChildShape(int index)
	{
		return m_childShapes[index];
	}
	const CollisionShape* GetChildShape(int index) const
	{
		return m_childShapes[index];
	}

	SimdTransform	GetChildTransform(int index)
	{
		return m_childTransforms[index];
	}
	const SimdTransform	GetChildTransform(int index) const
	{
		return m_childTransforms[index];
	}

	///GetAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
	void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const;


	virtual void	setLocalScaling(const SimdVector3& scaling)
	{
		m_localScaling = scaling;
	}
	virtual const SimdVector3& getLocalScaling() const 
	{
		return m_localScaling;
	}

	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);
	
	virtual int	GetShapeType() const { return COMPOUND_SHAPE_PROXYTYPE;}

	virtual void	SetMargin(float margin)
	{
		m_collisionMargin = margin;
	}
	virtual float	GetMargin() const
	{
		return m_collisionMargin;
	}
	virtual char*	GetName()const
	{
		return "Compound";
	}

	//this is optional, but should make collision queries faster, by culling non-overlapping nodes
	void	CreateAabbTreeFromChildren();

	const OptimizedBvh*					GetAabbTree() const
	{
		return m_aabbTree;
	}

private:
	SimdScalar	m_collisionMargin;
protected:
	SimdVector3	m_localScaling;

};



#endif //COMPOUND_SHAPE_H
