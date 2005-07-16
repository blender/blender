#ifndef POINT_COLLECTOR_H
#define POINT_COLLECTOR_H

#include "DiscreteCollisionDetectorInterface.h"



struct PointCollector : public DiscreteCollisionDetectorInterface::Result
{
	
	
	SimdVector3 m_normalOnBInWorld;
	SimdVector3 m_pointInWorld;
	SimdScalar	m_distance;//negative means penetration

	bool	m_hasResult;

	PointCollector () 
		: m_hasResult(false),m_distance(1e30f)
	{
	}

	virtual void AddContactPoint(const SimdVector3& normalOnBInWorld,const SimdVector3& pointInWorld,float depth)
	{
		if (depth< m_distance)
		{
			m_hasResult = true;
			m_normalOnBInWorld = normalOnBInWorld;
			m_pointInWorld = pointInWorld;
			//negative means penetration
			m_distance = depth;
		}
	}
};

#endif //POINT_COLLECTOR_H