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


#ifndef BU_COLLISIONPAIR
#define BU_COLLISIONPAIR

#include <NarrowPhaseCollision/BU_Screwing.h>
#include <NarrowPhaseCollision/ConvexCast.h>


#include <SimdQuaternion.h>

class PolyhedralConvexShape;


///BU_CollisionPair implements collision algorithm for algebraic time of impact calculation of feature based shapes.
class BU_CollisionPair : public ConvexCast
{
	
public:
	BU_CollisionPair(const PolyhedralConvexShape* convexA,const PolyhedralConvexShape* convexB,SimdScalar tolerance=0.2f);
	//toi

	virtual bool	calcTimeOfImpact(
					const SimdTransform& fromA,
					const SimdTransform& toA,
					const SimdTransform& fromB,
					const SimdTransform& toB,
					CastResult& result);

	
	

private:
	const PolyhedralConvexShape*	m_convexA;
	const PolyhedralConvexShape*	m_convexB;
	BU_Screwing	m_screwing;
	SimdScalar	m_tolerance;
	
};
#endif //BU_COLLISIONPAIR
