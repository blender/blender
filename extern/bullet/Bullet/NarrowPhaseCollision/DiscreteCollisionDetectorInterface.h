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

#ifndef DISCRETE_COLLISION_DETECTOR_INTERFACE_H
#define DISCRETE_COLLISION_DETECTOR_INTERFACE_H
#include "SimdTransform.h"
#include "SimdVector3.h"


/// This interface is made to be used by an iterative approach to do TimeOfImpact calculations
/// This interface allows to query for closest points and penetration depth between two (convex) objects
/// the closest point is on the second object (B), and the normal points from the surface on B towards A.
/// distance is between closest points on B and closest point on A. So you can calculate closest point on A
/// by taking closestPointInA = closestPointInB + m_distance * m_normalOnSurfaceB
struct DiscreteCollisionDetectorInterface
{
	void operator delete(void* ptr) {};

	struct Result
	{
		void operator delete(void* ptr) {};
		
		virtual void AddContactPoint(const SimdVector3& normalOnBInWorld,const SimdVector3& pointInWorld,float depth)=0;
	};

	struct ClosestPointInput
	{
		ClosestPointInput()
			:m_maximumDistanceSquared(1e30f)
		{
		}

		SimdTransform m_transformA;
		SimdTransform m_transformB;
		SimdScalar	m_maximumDistanceSquared;
	};

	virtual ~DiscreteCollisionDetectorInterface() {};

	//
	// give either closest points (distance > 0) or penetration (distance)
	// the normal always points from B towards A
	//
	virtual void	GetClosestPoints(const ClosestPointInput& input,Result& output) = 0;

	SimdScalar	getCollisionMargin() { return 0.2f;}
};

struct StorageResult : public DiscreteCollisionDetectorInterface::Result
{
		SimdVector3	m_normalOnSurfaceB;
		SimdVector3	m_closestPointInB;
		SimdScalar	m_distance; //negative means penetration !

		StorageResult() : m_distance(1e30f)
		{

		}

		virtual void AddContactPoint(const SimdVector3& normalOnBInWorld,const SimdVector3& pointInWorld,float depth)
		{
			if (depth < m_distance)
			{
				m_normalOnSurfaceB = normalOnBInWorld;
				m_closestPointInB = pointInWorld;
				m_distance = depth;
			}
		}
};

#endif //DISCRETE_COLLISION_DETECTOR_INTERFACE_H
