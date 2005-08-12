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


#include "PersistentManifold.h"
#include "SimdTransform.h"
#include <assert.h>

float gContactBreakingTreshold = 0.02f;

PersistentManifold::PersistentManifold()
:m_body0(0),
m_body1(0),
m_cachedPoints (0),
m_index1(0)
{
}


void	PersistentManifold::ClearManifold()
{
	m_cachedPoints = 0;
}





int PersistentManifold::SortCachedPoints(const ManifoldPoint& pt) 
{

		//calculate 4 possible cases areas, and take biggest area

		SimdScalar res0,res1,res2,res3;

		{
			SimdVector3 a0 = pt.m_localPointA-m_pointCache[1].m_localPointA;
			SimdVector3 b0 = m_pointCache[3].m_localPointA-m_pointCache[2].m_localPointA;
			SimdVector3 cross = a0.cross(b0);
			res0 = cross.length2();
		}
		{
			SimdVector3 a1 = pt.m_localPointA-m_pointCache[0].m_localPointA;
			SimdVector3 b1 = m_pointCache[3].m_localPointA-m_pointCache[2].m_localPointA;
			SimdVector3 cross = a1.cross(b1);
			res1 = cross.length2();
		}
		{
			SimdVector3 a2 = pt.m_localPointA-m_pointCache[0].m_localPointA;
			SimdVector3 b2 = m_pointCache[3].m_localPointA-m_pointCache[1].m_localPointA;
			SimdVector3 cross = a2.cross(b2);
			res2 = cross.length2();
		}
		{
			SimdVector3 a3 = pt.m_localPointA-m_pointCache[0].m_localPointA;
			SimdVector3 b3 = m_pointCache[2].m_localPointA-m_pointCache[1].m_localPointA;
			SimdVector3 cross = a3.cross(b3);
			res3 = cross.length2();
		}

		SimdVector4 maxvec(res0,res1,res2,res3);
		int biggestarea = maxvec.closestAxis4();

		return biggestarea;


}



int PersistentManifold::GetCacheEntry(const ManifoldPoint& newPoint) const
{
	SimdScalar shortestDist =  GetManifoldMargin() * GetManifoldMargin();
	int size = GetNumContacts();
	int nearestPoint = -1;
	for( int i = 0; i < size; i++ )
	{
		const ManifoldPoint &mp = m_pointCache[i];

		SimdVector3 diffA =  mp.m_localPointA- newPoint.m_localPointA;
		const SimdScalar distToManiPoint = diffA.dot(diffA);
		if( distToManiPoint < shortestDist )
		{
			shortestDist = distToManiPoint;
			nearestPoint = i;
		}
	}
	return nearestPoint;
}

void PersistentManifold::AddManifoldPoint(const ManifoldPoint& newPoint)
{
	assert(ValidContactDistance(newPoint));

	int insertIndex = GetNumContacts();
	if (insertIndex == MANIFOLD_CACHE_SIZE)
	{
		//sort cache so best points come first
		insertIndex = SortCachedPoints(newPoint);
	} else
	{
		m_cachedPoints++;
	}
	ReplaceContactPoint(newPoint,insertIndex);
}

float	PersistentManifold::GetManifoldMargin() const
{
	return gContactBreakingTreshold;
}

void PersistentManifold::RefreshContactPoints(const SimdTransform& trA,const SimdTransform& trB)
{
	int i;

	/// first refresh worldspace positions and distance
	for (i=GetNumContacts()-1;i>=0;i--)
	{
		ManifoldPoint &manifoldPoint = m_pointCache[i];
		manifoldPoint.m_positionWorldOnA = trA( manifoldPoint.m_localPointA );
		manifoldPoint.m_positionWorldOnB = trB( manifoldPoint.m_localPointB );
		manifoldPoint.m_distance1 = (manifoldPoint.m_positionWorldOnA -  manifoldPoint.m_positionWorldOnB).dot(manifoldPoint.m_normalWorldOnB);
		manifoldPoint.m_lifeTime++;
	}

	/// then 
	SimdScalar distance2d;
	SimdVector3 projectedDifference,projectedPoint;
	for (i=GetNumContacts()-1;i>=0;i--)
	{
		
		ManifoldPoint &manifoldPoint = m_pointCache[i];
		//contact becomes invalid when signed distance exceeds margin (projected on contactnormal direction)
		if (!ValidContactDistance(manifoldPoint))
		{
			RemoveContactPoint(i);
		} else
		{
			//contact also becomes invalid when relative movement orthogonal to normal exceeds margin
			projectedPoint = manifoldPoint.m_positionWorldOnA - manifoldPoint.m_normalWorldOnB * manifoldPoint.m_distance1;
			projectedDifference = manifoldPoint.m_positionWorldOnB - projectedPoint;
			distance2d = projectedDifference.dot(projectedDifference);
			if (distance2d  > GetManifoldMargin()*GetManifoldMargin() )
			{
				RemoveContactPoint(i);
			}
		}
	}
}


//todo: remove this treshold
float gPenetrationDistanceCheck = -0.05f;

float	PersistentManifold::GetCollisionImpulse() const
{
	float averageImpulse = 0.f;
	if (GetNumContacts() > 0)
	{
		float totalImpulse = 0.f;

		//return the sum of the applied impulses on the box
		for (int i=0;i<GetNumContacts();i++)
		{
			const ManifoldPoint& cp = GetContactPoint(i);
			//avoid conflic noice
			if ( cp.GetDistance() <gPenetrationDistanceCheck)
				return 0.f;

			totalImpulse += cp.m_appliedImpulse;

		}
		averageImpulse = totalImpulse / ((float)GetNumContacts());

	}
	return averageImpulse;

}


