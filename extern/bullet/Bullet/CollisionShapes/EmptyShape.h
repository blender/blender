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

#ifndef EMPTY_SHAPE_H
#define EMPTY_SHAPE_H

#include "ConcaveShape.h"

#include "SimdVector3.h"
#include "SimdTransform.h"
#include "SimdMatrix3x3.h"
#include <vector>
#include "CollisionShapes/CollisionMargin.h"




/// EmptyShape is a collision shape without actual collision detection. 
///It can be replaced by another shape during runtime
class EmptyShape	: public ConcaveShape
{
public:
	EmptyShape();

	virtual ~EmptyShape();


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
	
	virtual int	GetShapeType() const { return EMPTY_SHAPE_PROXYTYPE;}

	
	virtual char*	GetName()const
	{
		return "Empty";
	}


protected:
	SimdVector3	m_localScaling;

};



#endif //EMPTY_SHAPE_H
