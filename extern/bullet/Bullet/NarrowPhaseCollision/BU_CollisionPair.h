/*
 * Copyright (c) 2005 Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
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
