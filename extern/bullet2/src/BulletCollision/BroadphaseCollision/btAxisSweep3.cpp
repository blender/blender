
//Bullet Continuous Collision Detection and Physics Library
//Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/


//
// btAxisSweep3
//
// Copyright (c) 2006 Simon Hobbs
//
// This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
#include "btAxisSweep3.h"

#include <assert.h>

#ifdef DEBUG_BROADPHASE
#include <stdio.h>
void btAxisSweep3::debugPrintAxis(int axis, bool checkCardinality)
{
	int numEdges = m_pHandles[0].m_maxEdges[axis];
	printf("SAP Axis %d, numEdges=%d\n",axis,numEdges);

	int i;
	for (i=0;i<numEdges+1;i++)
	{
		Edge* pEdge = m_pEdges[axis] + i;
		Handle* pHandlePrev = getHandle(pEdge->m_handle);
		int handleIndex = pEdge->IsMax()? pHandlePrev->m_maxEdges[axis] : pHandlePrev->m_minEdges[axis];
		char beginOrEnd;
		beginOrEnd=pEdge->IsMax()?'E':'B';
		printf("	[%c,h=%d,p=%x,i=%d]\n",beginOrEnd,pEdge->m_handle,pEdge->m_pos,handleIndex);
	}

	if (checkCardinality)
		assert(numEdges == m_numHandles*2+1);
}
#endif //DEBUG_BROADPHASE


btBroadphaseProxy*	btAxisSweep3::createProxy(  const btVector3& min,  const btVector3& max,int shapeType,void* userPtr,short int collisionFilterGroup,short int collisionFilterMask)
{
		(void)shapeType;
		BP_FP_INT_TYPE handleId = addHandle(min,max, userPtr,collisionFilterGroup,collisionFilterMask);
		
		Handle* handle = getHandle(handleId);
				
		return handle;
}

void	btAxisSweep3::destroyProxy(btBroadphaseProxy* proxy)
{
	Handle* handle = static_cast<Handle*>(proxy);
	removeHandle(handle->m_handleId);
}

void	btAxisSweep3::setAabb(btBroadphaseProxy* proxy,const btVector3& aabbMin,const btVector3& aabbMax)
{
	Handle* handle = static_cast<Handle*>(proxy);
	updateHandle(handle->m_handleId,aabbMin,aabbMax);

}






btAxisSweep3::btAxisSweep3(const btPoint3& worldAabbMin,const btPoint3& worldAabbMax, int maxHandles)
:btOverlappingPairCache()
{
	m_invalidPair = 0;
	//assert(bounds.HasVolume());

	// 1 handle is reserved as sentinel
	btAssert(maxHandles > 1 && maxHandles < BP_MAX_HANDLES);

	// init bounds
	m_worldAabbMin = worldAabbMin;
	m_worldAabbMax = worldAabbMax;

	btVector3 aabbSize = m_worldAabbMax - m_worldAabbMin;

	BP_FP_INT_TYPE	maxInt = BP_HANDLE_SENTINEL;

	m_quantize = btVector3(btScalar(maxInt),btScalar(maxInt),btScalar(maxInt)) / aabbSize;

	// allocate handles buffer and put all handles on free list
	m_pHandles = new Handle[maxHandles];
	m_maxHandles = maxHandles;
	m_numHandles = 0;

	// handle 0 is reserved as the null index, and is also used as the sentinel
	m_firstFreeHandle = 1;
	{
		for (BP_FP_INT_TYPE i = m_firstFreeHandle; i < maxHandles; i++)
			m_pHandles[i].SetNextFree(i + 1);
		m_pHandles[maxHandles - 1].SetNextFree(0);
	}

	{
	// allocate edge buffers
	for (int i = 0; i < 3; i++)
		m_pEdges[i] = new Edge[maxHandles * 2];
	}
	//removed overlap management

	// make boundary sentinels
	
	m_pHandles[0].m_clientObject = 0;

	for (int axis = 0; axis < 3; axis++)
	{
		m_pHandles[0].m_minEdges[axis] = 0;
		m_pHandles[0].m_maxEdges[axis] = 1;

		m_pEdges[axis][0].m_pos = 0;
		m_pEdges[axis][0].m_handle = 0;
		m_pEdges[axis][1].m_pos = BP_HANDLE_SENTINEL;
		m_pEdges[axis][1].m_handle = 0;
#ifdef DEBUG_BROADPHASE
		debugPrintAxis(axis);
#endif //DEBUG_BROADPHASE

	}

}

btAxisSweep3::~btAxisSweep3()
{
	
	for (int i = 2; i >= 0; i--)
		delete[] m_pEdges[i];
	delete[] m_pHandles;
}

void btAxisSweep3::quantize(BP_FP_INT_TYPE* out, const btPoint3& point, int isMax) const
{
	btPoint3 clampedPoint(point);
	


	clampedPoint.setMax(m_worldAabbMin);
	clampedPoint.setMin(m_worldAabbMax);

	btVector3 v = (clampedPoint - m_worldAabbMin) * m_quantize;
	out[0] = (BP_FP_INT_TYPE)(((BP_FP_INT_TYPE)v.getX() & BP_HANDLE_MASK) | isMax);
	out[1] = (BP_FP_INT_TYPE)(((BP_FP_INT_TYPE)v.getY() & BP_HANDLE_MASK) | isMax);
	out[2] = (BP_FP_INT_TYPE)(((BP_FP_INT_TYPE)v.getZ() & BP_HANDLE_MASK) | isMax);
	
}



BP_FP_INT_TYPE btAxisSweep3::allocHandle()
{
	assert(m_firstFreeHandle);

	BP_FP_INT_TYPE handle = m_firstFreeHandle;
	m_firstFreeHandle = getHandle(handle)->GetNextFree();
	m_numHandles++;

	return handle;
}

void btAxisSweep3::freeHandle(BP_FP_INT_TYPE handle)
{
	assert(handle > 0 && handle < m_maxHandles);

	getHandle(handle)->SetNextFree(m_firstFreeHandle);
	m_firstFreeHandle = handle;

	m_numHandles--;
}



BP_FP_INT_TYPE btAxisSweep3::addHandle(const btPoint3& aabbMin,const btPoint3& aabbMax, void* pOwner,short int collisionFilterGroup,short int collisionFilterMask)
{
	// quantize the bounds
	BP_FP_INT_TYPE min[3], max[3];
	quantize(min, aabbMin, 0);
	quantize(max, aabbMax, 1);

	// allocate a handle
	BP_FP_INT_TYPE handle = allocHandle();
	assert(handle!= 0xcdcd);

	Handle* pHandle = getHandle(handle);
	
	pHandle->m_handleId = handle;
	//pHandle->m_pOverlaps = 0;
	pHandle->m_clientObject = pOwner;
	pHandle->m_collisionFilterGroup = collisionFilterGroup;
	pHandle->m_collisionFilterMask = collisionFilterMask;

	// compute current limit of edge arrays
	BP_FP_INT_TYPE limit = m_numHandles * 2;

	
	// insert new edges just inside the max boundary edge
	for (BP_FP_INT_TYPE axis = 0; axis < 3; axis++)
	{

		m_pHandles[0].m_maxEdges[axis] += 2;

		m_pEdges[axis][limit + 1] = m_pEdges[axis][limit - 1];

		m_pEdges[axis][limit - 1].m_pos = min[axis];
		m_pEdges[axis][limit - 1].m_handle = handle;

		m_pEdges[axis][limit].m_pos = max[axis];
		m_pEdges[axis][limit].m_handle = handle;

		pHandle->m_minEdges[axis] = limit - 1;
		pHandle->m_maxEdges[axis] = limit;
	}

	// now sort the new edges to their correct position
	sortMinDown(0, pHandle->m_minEdges[0], false);
	sortMaxDown(0, pHandle->m_maxEdges[0], false);
	sortMinDown(1, pHandle->m_minEdges[1], false);
	sortMaxDown(1, pHandle->m_maxEdges[1], false);
	sortMinDown(2, pHandle->m_minEdges[2], true);
	sortMaxDown(2, pHandle->m_maxEdges[2], true);


	return handle;
}


void btAxisSweep3::removeHandle(BP_FP_INT_TYPE handle)
{
	
	Handle* pHandle = getHandle(handle);

	//explicitly remove the pairs containing the proxy
	//we could do it also in the sortMinUp (passing true)
	//todo: compare performance
	removeOverlappingPairsContainingProxy(pHandle);


	// compute current limit of edge arrays
	int limit = m_numHandles * 2;
	
	int axis;

	for (axis = 0;axis<3;axis++)
	{
		m_pHandles[0].m_maxEdges[axis] -= 2;
	}

	// remove the edges by sorting them up to the end of the list
	for ( axis = 0; axis < 3; axis++)
	{
		Edge* pEdges = m_pEdges[axis];
		BP_FP_INT_TYPE max = pHandle->m_maxEdges[axis];
		pEdges[max].m_pos = BP_HANDLE_SENTINEL;

		sortMaxUp(axis,max,false);


		BP_FP_INT_TYPE i = pHandle->m_minEdges[axis];
		pEdges[i].m_pos = BP_HANDLE_SENTINEL;


		sortMinUp(axis,i,false);

		pEdges[limit-1].m_handle = 0;
		pEdges[limit-1].m_pos = BP_HANDLE_SENTINEL;
		
#ifdef DEBUG_BROADPHASE
			debugPrintAxis(axis,false);
#endif //DEBUG_BROADPHASE


	}


	// free the handle
	freeHandle(handle);

	
}

extern int gOverlappingPairs;


void	btAxisSweep3::refreshOverlappingPairs()
{

}
void	btAxisSweep3::processAllOverlappingPairs(btOverlapCallback* callback)
{

	//perform a sort, to find duplicates and to sort 'invalid' pairs to the end
	m_overlappingPairArray.heapSort(btBroadphasePairSortPredicate());

	//remove the 'invalid' ones
#ifdef USE_POPBACK_REMOVAL
	while (m_invalidPair>0)
	{
		m_invalidPair--;
		m_overlappingPairArray.pop_back();
	}
#else	
	m_overlappingPairArray.resize(m_overlappingPairArray.size() - m_invalidPair);
	m_invalidPair = 0;
#endif

	
	int i;

	btBroadphasePair previousPair;
	previousPair.m_pProxy0 = 0;
	previousPair.m_pProxy1 = 0;
	previousPair.m_algorithm = 0;
	
	
	for (i=0;i<m_overlappingPairArray.size();i++)
	{
	
		btBroadphasePair& pair = m_overlappingPairArray[i];

		bool isDuplicate = (pair == previousPair);

		previousPair = pair;

		bool needsRemoval = false;

		if (!isDuplicate)
		{
			bool hasOverlap = testOverlap(pair.m_pProxy0,pair.m_pProxy1);

			if (hasOverlap)
			{
				needsRemoval = callback->processOverlap(pair);
			} else
			{
				needsRemoval = true;
			}
		} else
		{
			//remove duplicate
			needsRemoval = true;
			//should have no algorithm
			btAssert(!pair.m_algorithm);
		}
		
		if (needsRemoval)
		{
			cleanOverlappingPair(pair);

	//		m_overlappingPairArray.swap(i,m_overlappingPairArray.size()-1);
	//		m_overlappingPairArray.pop_back();
			pair.m_pProxy0 = 0;
			pair.m_pProxy1 = 0;
			m_invalidPair++;
			gOverlappingPairs--;
		} 
		
	}
}


bool btAxisSweep3::testOverlap(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1)
{
	const Handle* pHandleA = static_cast<Handle*>(proxy0);
	const Handle* pHandleB = static_cast<Handle*>(proxy1);
	
	//optimization 1: check the array index (memory address), instead of the m_pos

	for (int axis = 0; axis < 3; axis++)
	{ 
		if (pHandleA->m_maxEdges[axis] < pHandleB->m_minEdges[axis] || 
			pHandleB->m_maxEdges[axis] < pHandleA->m_minEdges[axis]) 
		{ 
			return false; 
		} 
	} 
	return true;
}

bool btAxisSweep3::testOverlap(int ignoreAxis,const Handle* pHandleA, const Handle* pHandleB)
{
	//optimization 1: check the array index (memory address), instead of the m_pos

	for (int axis = 0; axis < 3; axis++)
	{ 
		if (axis != ignoreAxis)
		{
			if (pHandleA->m_maxEdges[axis] < pHandleB->m_minEdges[axis] || 
				pHandleB->m_maxEdges[axis] < pHandleA->m_minEdges[axis]) 
			{ 
				return false; 
			} 
		}
	} 

	//optimization 2: only 2 axis need to be tested (conflicts with 'delayed removal' optimization)

	/*for (int axis = 0; axis < 3; axis++)
	{
		if (m_pEdges[axis][pHandleA->m_maxEdges[axis]].m_pos < m_pEdges[axis][pHandleB->m_minEdges[axis]].m_pos ||
			m_pEdges[axis][pHandleB->m_maxEdges[axis]].m_pos < m_pEdges[axis][pHandleA->m_minEdges[axis]].m_pos)
		{
			return false;
		}
	}
	*/

	return true;
}

void btAxisSweep3::updateHandle(BP_FP_INT_TYPE handle, const btPoint3& aabbMin,const btPoint3& aabbMax)
{
//	assert(bounds.IsFinite());
	//assert(bounds.HasVolume());

	Handle* pHandle = getHandle(handle);

	// quantize the new bounds
	BP_FP_INT_TYPE min[3], max[3];
	quantize(min, aabbMin, 0);
	quantize(max, aabbMax, 1);

	// update changed edges
	for (int axis = 0; axis < 3; axis++)
	{
		BP_FP_INT_TYPE emin = pHandle->m_minEdges[axis];
		BP_FP_INT_TYPE emax = pHandle->m_maxEdges[axis];

		int dmin = (int)min[axis] - (int)m_pEdges[axis][emin].m_pos;
		int dmax = (int)max[axis] - (int)m_pEdges[axis][emax].m_pos;

		m_pEdges[axis][emin].m_pos = min[axis];
		m_pEdges[axis][emax].m_pos = max[axis];

		// expand (only adds overlaps)
		if (dmin < 0)
			sortMinDown(axis, emin);

		if (dmax > 0)
			sortMaxUp(axis, emax);

		// shrink (only removes overlaps)
		if (dmin > 0)
			sortMinUp(axis, emin);

		if (dmax < 0)
			sortMaxDown(axis, emax);

#ifdef DEBUG_BROADPHASE
	debugPrintAxis(axis);
#endif //DEBUG_BROADPHASE
	}

	
}




// sorting a min edge downwards can only ever *add* overlaps
void btAxisSweep3::sortMinDown(int axis, BP_FP_INT_TYPE edge, bool updateOverlaps)
{

	Edge* pEdge = m_pEdges[axis] + edge;
	Edge* pPrev = pEdge - 1;
	Handle* pHandleEdge = getHandle(pEdge->m_handle);

	while (pEdge->m_pos < pPrev->m_pos)
	{
		Handle* pHandlePrev = getHandle(pPrev->m_handle);

		if (pPrev->IsMax())
		{
			// if previous edge is a maximum check the bounds and add an overlap if necessary
			if (updateOverlaps && testOverlap(axis,pHandleEdge, pHandlePrev))
			{
				addOverlappingPair(pHandleEdge,pHandlePrev);

				//AddOverlap(pEdge->m_handle, pPrev->m_handle);

			}

			// update edge reference in other handle
			pHandlePrev->m_maxEdges[axis]++;
		}
		else
			pHandlePrev->m_minEdges[axis]++;

		pHandleEdge->m_minEdges[axis]--;

		// swap the edges
		Edge swap = *pEdge;
		*pEdge = *pPrev;
		*pPrev = swap;

		// decrement
		pEdge--;
		pPrev--;
	}

#ifdef DEBUG_BROADPHASE
	debugPrintAxis(axis);
#endif //DEBUG_BROADPHASE

}

// sorting a min edge upwards can only ever *remove* overlaps
void btAxisSweep3::sortMinUp(int axis, BP_FP_INT_TYPE edge, bool updateOverlaps)
{
	Edge* pEdge = m_pEdges[axis] + edge;
	Edge* pNext = pEdge + 1;
	Handle* pHandleEdge = getHandle(pEdge->m_handle);

	while (pNext->m_handle && (pEdge->m_pos >= pNext->m_pos))
	{
		Handle* pHandleNext = getHandle(pNext->m_handle);

		if (pNext->IsMax())
		{
			// if next edge is maximum remove any overlap between the two handles
			if (updateOverlaps)
			{
				/*
				Handle* handle0 = getHandle(pEdge->m_handle);
				Handle* handle1 = getHandle(pNext->m_handle);
				btBroadphasePair tmpPair(*handle0,*handle1);
				removeOverlappingPair(tmpPair);
				*/

			}

			// update edge reference in other handle
			pHandleNext->m_maxEdges[axis]--;
		}
		else
			pHandleNext->m_minEdges[axis]--;

		pHandleEdge->m_minEdges[axis]++;

		// swap the edges
		Edge swap = *pEdge;
		*pEdge = *pNext;
		*pNext = swap;

		// increment
		pEdge++;
		pNext++;
	}


}

// sorting a max edge downwards can only ever *remove* overlaps
void btAxisSweep3::sortMaxDown(int axis, BP_FP_INT_TYPE edge, bool updateOverlaps)
{

	Edge* pEdge = m_pEdges[axis] + edge;
	Edge* pPrev = pEdge - 1;
	Handle* pHandleEdge = getHandle(pEdge->m_handle);

	while (pEdge->m_pos < pPrev->m_pos)
	{
		Handle* pHandlePrev = getHandle(pPrev->m_handle);

		if (!pPrev->IsMax())
		{
			// if previous edge was a minimum remove any overlap between the two handles
			if (updateOverlaps)
			{
				//this is done during the overlappingpairarray iteration/narrowphase collision
				/*
				Handle* handle0 = getHandle(pEdge->m_handle);
				Handle* handle1 = getHandle(pPrev->m_handle);
				btBroadphasePair* pair = findPair(handle0,handle1);
				//assert(pair);

				if (pair)
				{
					removeOverlappingPair(*pair);
				}
				*/

			}

			// update edge reference in other handle
			pHandlePrev->m_minEdges[axis]++;;
		}
		else
			pHandlePrev->m_maxEdges[axis]++;

		pHandleEdge->m_maxEdges[axis]--;

		// swap the edges
		Edge swap = *pEdge;
		*pEdge = *pPrev;
		*pPrev = swap;

		// decrement
		pEdge--;
		pPrev--;
	}

	
#ifdef DEBUG_BROADPHASE
	debugPrintAxis(axis);
#endif //DEBUG_BROADPHASE

}

// sorting a max edge upwards can only ever *add* overlaps
void btAxisSweep3::sortMaxUp(int axis, BP_FP_INT_TYPE edge, bool updateOverlaps)
{
	Edge* pEdge = m_pEdges[axis] + edge;
	Edge* pNext = pEdge + 1;
	Handle* pHandleEdge = getHandle(pEdge->m_handle);

	while (pNext->m_handle && (pEdge->m_pos >= pNext->m_pos))
	{
		Handle* pHandleNext = getHandle(pNext->m_handle);

		if (!pNext->IsMax())
		{
			// if next edge is a minimum check the bounds and add an overlap if necessary
			if (updateOverlaps && testOverlap(axis, pHandleEdge, pHandleNext))
			{
				Handle* handle0 = getHandle(pEdge->m_handle);
				Handle* handle1 = getHandle(pNext->m_handle);
				addOverlappingPair(handle0,handle1);
			}

			// update edge reference in other handle
			pHandleNext->m_minEdges[axis]--;
		}
		else
			pHandleNext->m_maxEdges[axis]--;

		pHandleEdge->m_maxEdges[axis]++;

		// swap the edges
		Edge swap = *pEdge;
		*pEdge = *pNext;
		*pNext = swap;

		// increment
		pEdge++;
		pNext++;
	}
	
}
