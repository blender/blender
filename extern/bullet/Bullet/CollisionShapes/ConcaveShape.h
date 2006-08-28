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

#ifndef CONCAVE_SHAPE_H
#define CONCAVE_SHAPE_H

#include "CollisionShapes/CollisionShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types

#include "TriangleCallback.h"


///Concave shape proves an interface concave shapes that can produce triangles that overlapping a given AABB.
///Static triangle mesh, infinite plane, height field/landscapes are example that implement this interface.
class ConcaveShape : public CollisionShape
{
protected:
	float m_collisionMargin;

public:
	ConcaveShape();

	virtual ~ConcaveShape();

	virtual void	ProcessAllTriangles(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const = 0;

	virtual float GetMargin() const {
		return m_collisionMargin;
	}
	virtual void SetMargin(float collisionMargin)
	{
		m_collisionMargin = collisionMargin;
	}



};

#endif //CONCAVE_SHAPE_H
