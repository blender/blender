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


#include "PersistentManifold.h"
#include "SimdTransform.h"
#include <assert.h>

float						gContactBreakingTreshold = 0.02f;
ContactDestroyedCallback	gContactDestroyedCallback = 0;



PersistentManifold::PersistentManifold()
:m_body0(0),
m_body1(0),
m_cachedPoints (0),
m_index1(0)
{
}


void	PersistentManifold::ClearManifold()
{
	int i;
	for (i=0;i<m_cachedPoints;i++)
	{
		ClearUserCache(m_pointCache[i]);
	}
	m_cachedPoints = 0;
}

#ifdef DEBUG_PERSISTENCY
#include <stdio.h>
void	PersistentManifold::DebugPersistency()
{
	int i;
	printf("DebugPersistency : numPoints %d\n",m_cachedPoints);
	for (i=0;i<m_cachedPoints;i++)
	{
		printf("m_pointCache[%d].m_userPersistentData = %x\n",i,m_pointCache[i].m_userPersistentData);
	}
}
#endif //DEBUG_PERSISTENCY

void PersistentManifold::ClearUserCache(ManifoldPoint& pt)
{

	void* oldPtr = pt.m_userPersistentData;
	if (oldPtr)
	{
#ifdef DEBUG_PERSISTENCY
		int i;
		int occurance = 0;
		for (i=0;i<m_cachedPoints;i++)
		{
			if (m_pointCache[i].m_userPersistentData == oldPtr)
			{
				occurance++;
				if (occurance>1)
					printf("error in ClearUserCache\n");
			}
		}
		assert(occurance<=0);
#endif //DEBUG_PERSISTENCY

		if (pt.m_userPersistentData && gContactDestroyedCallback)
		{
			(*gContactDestroyedCallback)(pt.m_userPersistentData);
			pt.m_userPersistentData = 0;
		}
		
#ifdef DEBUG_PERSISTENCY
		DebugPersistency();
#endif
	}

	
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
	SimdScalar shortestDist =  GetContactBreakingTreshold() * GetContactBreakingTreshold();
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
#if MANIFOLD_CACHE_SIZE >= 4
		//sort cache so best points come first, based on area
		insertIndex = SortCachedPoints(newPoint);
#else
		insertIndex = 0;
#endif

		
	} else
	{
		m_cachedPoints++;

		
	}
	ReplaceContactPoint(newPoint,insertIndex);
}

float	PersistentManifold::GetContactBreakingTreshold() const
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
			if (distance2d  > GetContactBreakingTreshold()*GetContactBreakingTreshold() )
			{
				RemoveContactPoint(i);
			}
		}
	}
#ifdef DEBUG_PERSISTENCY
	DebugPersistency();
#endif //
}





