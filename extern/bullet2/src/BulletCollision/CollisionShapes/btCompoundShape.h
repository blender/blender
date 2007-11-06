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

#include "btCollisionShape.h"

#include "LinearMath/btVector3.h"
#include "LinearMath/btTransform.h"
#include "LinearMath/btMatrix3x3.h"
#include "btCollisionMargin.h"
#include "LinearMath/btAlignedObjectArray.h"

class btOptimizedBvh;

ATTRIBUTE_ALIGNED16(struct) btCompoundShapeChild
{
	BT_DECLARE_ALIGNED_ALLOCATOR();

	btTransform			m_transform;
	btCollisionShape*	m_childShape;
	int					m_childShapeType;
	btScalar			m_childMargin;
};
	
/// btCompoundShape allows to store multiple other btCollisionShapes
/// This allows for concave collision objects. This is more general then the Static Concave btTriangleMeshShape.
ATTRIBUTE_ALIGNED16(class) btCompoundShape	: public btCollisionShape
{
	//btAlignedObjectArray<btTransform>		m_childTransforms;
	//btAlignedObjectArray<btCollisionShape*>	m_childShapes;
	btAlignedObjectArray<btCompoundShapeChild> m_children;
	btVector3						m_localAabbMin;
	btVector3						m_localAabbMax;

	btOptimizedBvh*					m_aabbTree;

public:
	BT_DECLARE_ALIGNED_ALLOCATOR();

	btCompoundShape();

	virtual ~btCompoundShape();

	void	addChildShape(const btTransform& localTransform,btCollisionShape* shape);

	int		getNumChildShapes() const
	{
		return int (m_children.size());
	}

	btCollisionShape* getChildShape(int index)
	{
		return m_children[index].m_childShape;
	}
	const btCollisionShape* getChildShape(int index) const
	{
		return m_children[index].m_childShape;
	}

	btTransform	getChildTransform(int index)
	{
		return m_children[index].m_transform;
	}
	const btTransform	getChildTransform(int index) const
	{
		return m_children[index].m_transform;
	}


	btCompoundShapeChild* getChildList()
	{
		return &m_children[0];
	}

	///getAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
	void getAabb(const btTransform& t,btVector3& aabbMin,btVector3& aabbMax) const;


	virtual void	setLocalScaling(const btVector3& scaling)
	{
		m_localScaling = scaling;
	}
	virtual const btVector3& getLocalScaling() const 
	{
		return m_localScaling;
	}

	virtual void	calculateLocalInertia(btScalar mass,btVector3& inertia) const;
	
	virtual int	getShapeType() const { return COMPOUND_SHAPE_PROXYTYPE;}

	virtual void	setMargin(btScalar margin)
	{
		m_collisionMargin = margin;
	}
	virtual btScalar	getMargin() const
	{
		return m_collisionMargin;
	}
	virtual const char*	getName()const
	{
		return "Compound";
	}

	//this is optional, but should make collision queries faster, by culling non-overlapping nodes
	void	createAabbTreeFromChildren();

	const btOptimizedBvh*					getAabbTree() const
	{
		return m_aabbTree;
	}

private:
	btScalar	m_collisionMargin;
protected:
	btVector3	m_localScaling;

};



#endif //COMPOUND_SHAPE_H
