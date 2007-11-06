//Bullet Continuous Collision Detection and Physics Library
//Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

//
// btAxisSweep3.h
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

#ifndef AXIS_SWEEP_3_H
#define AXIS_SWEEP_3_H

#include "LinearMath/btPoint3.h"
#include "LinearMath/btVector3.h"
#include "btOverlappingPairCache.h"
#include "btBroadphaseInterface.h"
#include "btBroadphaseProxy.h"
#include "btOverlappingPairCallback.h"

//#define DEBUG_BROADPHASE 1

/// btAxisSweep3Internal is an internal template class that implements sweep and prune.
/// Dont use this class directly, use btAxisSweep3 or bt32BitAxisSweep3 instead.
template <typename BP_FP_INT_TYPE>
class btAxisSweep3Internal : public btBroadphaseInterface
{
protected:

	BP_FP_INT_TYPE	m_bpHandleMask;
	BP_FP_INT_TYPE	m_handleSentinel;

public:
	

	class Edge
	{
	public:
		BP_FP_INT_TYPE m_pos;			// low bit is min/max
		BP_FP_INT_TYPE m_handle;

		BP_FP_INT_TYPE IsMax() const {return m_pos & 1;}
	};

public:
	ATTRIBUTE_ALIGNED16(class) Handle : public btBroadphaseProxy
	{
	public:
	BT_DECLARE_ALIGNED_ALLOCATOR();
	
		// indexes into the edge arrays
		BP_FP_INT_TYPE m_minEdges[3], m_maxEdges[3];		// 6 * 2 = 12
//		BP_FP_INT_TYPE m_uniqueId;
		BP_FP_INT_TYPE m_pad;
		
		//void* m_pOwner; this is now in btBroadphaseProxy.m_clientObject
	
		SIMD_FORCE_INLINE void SetNextFree(BP_FP_INT_TYPE next) {m_minEdges[0] = next;}
		SIMD_FORCE_INLINE BP_FP_INT_TYPE GetNextFree() const {return m_minEdges[0];}
	};		// 24 bytes + 24 for Edge structures = 44 bytes total per entry

	
protected:
	btPoint3 m_worldAabbMin;						// overall system bounds
	btPoint3 m_worldAabbMax;						// overall system bounds

	btVector3 m_quantize;						// scaling factor for quantization

	BP_FP_INT_TYPE m_numHandles;						// number of active handles
	BP_FP_INT_TYPE m_maxHandles;						// max number of handles
	Handle* m_pHandles;						// handles pool
	BP_FP_INT_TYPE m_firstFreeHandle;		// free handles list

	Edge* m_pEdges[3];						// edge arrays for the 3 axes (each array has m_maxHandles * 2 + 2 sentinel entries)

	btOverlappingPairCache* m_pairCache;

	///btOverlappingPairCallback is an additional optional user callback for adding/removing overlapping pairs, similar interface to btOverlappingPairCache.
	btOverlappingPairCallback* m_userPairCallback;
	
	bool	m_ownsPairCache;

	int	m_invalidPair;

	// allocation/deallocation
	BP_FP_INT_TYPE allocHandle();
	void freeHandle(BP_FP_INT_TYPE handle);
	

	bool testOverlap(int ignoreAxis,const Handle* pHandleA, const Handle* pHandleB);

#ifdef DEBUG_BROADPHASE
	void debugPrintAxis(int axis,bool checkCardinality=true);
#endif //DEBUG_BROADPHASE

	//Overlap* AddOverlap(BP_FP_INT_TYPE handleA, BP_FP_INT_TYPE handleB);
	//void RemoveOverlap(BP_FP_INT_TYPE handleA, BP_FP_INT_TYPE handleB);

	void quantize(BP_FP_INT_TYPE* out, const btPoint3& point, int isMax) const;

	void sortMinDown(int axis, BP_FP_INT_TYPE edge, btDispatcher* dispatcher, bool updateOverlaps );
	void sortMinUp(int axis, BP_FP_INT_TYPE edge, btDispatcher* dispatcher, bool updateOverlaps );
	void sortMaxDown(int axis, BP_FP_INT_TYPE edge, btDispatcher* dispatcher, bool updateOverlaps );
	void sortMaxUp(int axis, BP_FP_INT_TYPE edge, btDispatcher* dispatcher, bool updateOverlaps );

public:

	btAxisSweep3Internal(const btPoint3& worldAabbMin,const btPoint3& worldAabbMax, BP_FP_INT_TYPE handleMask, BP_FP_INT_TYPE handleSentinel, BP_FP_INT_TYPE maxHandles = 16384, btOverlappingPairCache* pairCache=0);

	virtual	~btAxisSweep3Internal();


	virtual void	calculateOverlappingPairs(btDispatcher* dispatcher);
	
	BP_FP_INT_TYPE addHandle(const btPoint3& aabbMin,const btPoint3& aabbMax, void* pOwner,short int collisionFilterGroup,short int collisionFilterMask,btDispatcher* dispatcher);
	void removeHandle(BP_FP_INT_TYPE handle,btDispatcher* dispatcher);
	void updateHandle(BP_FP_INT_TYPE handle, const btPoint3& aabbMin,const btPoint3& aabbMax,btDispatcher* dispatcher);
	SIMD_FORCE_INLINE Handle* getHandle(BP_FP_INT_TYPE index) const {return m_pHandles + index;}

	void	processAllOverlappingPairs(btOverlapCallback* callback);

	//Broadphase Interface
	virtual btBroadphaseProxy*	createProxy(  const btVector3& aabbMin,  const btVector3& aabbMax,int shapeType,void* userPtr ,short int collisionFilterGroup,short int collisionFilterMask,btDispatcher* dispatcher);
	virtual void	destroyProxy(btBroadphaseProxy* proxy,btDispatcher* dispatcher);
	virtual void	setAabb(btBroadphaseProxy* proxy,const btVector3& aabbMin,const btVector3& aabbMax,btDispatcher* dispatcher);
	
	bool	testAabbOverlap(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1);

	btOverlappingPairCache*	getOverlappingPairCache()
	{
		return m_pairCache;
	}
	const btOverlappingPairCache*	getOverlappingPairCache() const
	{
		return m_pairCache;
	}

	void	setOverlappingPairUserCallback(btOverlappingPairCallback* pairCallback)
	{
		m_userPairCallback = pairCallback;
	}
	const btOverlappingPairCallback*	getOverlappingPairUserCallback() const
	{
		return m_userPairCallback;
	}
};

////////////////////////////////////////////////////////////////////




#ifdef DEBUG_BROADPHASE
#include <stdio.h>

template <typename BP_FP_INT_TYPE>
void btAxisSweep3<BP_FP_INT_TYPE>::debugPrintAxis(int axis, bool checkCardinality)
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

template <typename BP_FP_INT_TYPE>
btBroadphaseProxy*	btAxisSweep3Internal<BP_FP_INT_TYPE>::createProxy(  const btVector3& aabbMin,  const btVector3& aabbMax,int shapeType,void* userPtr,short int collisionFilterGroup,short int collisionFilterMask,btDispatcher* dispatcher)
{
		(void)shapeType;
		BP_FP_INT_TYPE handleId = addHandle(aabbMin,aabbMax, userPtr,collisionFilterGroup,collisionFilterMask,dispatcher);
		
		Handle* handle = getHandle(handleId);
				
		return handle;
}



template <typename BP_FP_INT_TYPE>
void	btAxisSweep3Internal<BP_FP_INT_TYPE>::destroyProxy(btBroadphaseProxy* proxy,btDispatcher* dispatcher)
{
	Handle* handle = static_cast<Handle*>(proxy);
	removeHandle(handle->m_uniqueId,dispatcher);
}

template <typename BP_FP_INT_TYPE>
void	btAxisSweep3Internal<BP_FP_INT_TYPE>::setAabb(btBroadphaseProxy* proxy,const btVector3& aabbMin,const btVector3& aabbMax,btDispatcher* dispatcher)
{
	Handle* handle = static_cast<Handle*>(proxy);
	updateHandle(handle->m_uniqueId,aabbMin,aabbMax,dispatcher);

}





template <typename BP_FP_INT_TYPE>
btAxisSweep3Internal<BP_FP_INT_TYPE>::btAxisSweep3Internal(const btPoint3& worldAabbMin,const btPoint3& worldAabbMax, BP_FP_INT_TYPE handleMask, BP_FP_INT_TYPE handleSentinel,BP_FP_INT_TYPE maxHandles, btOverlappingPairCache* pairCache )
:m_bpHandleMask(handleMask),
m_handleSentinel(handleSentinel),
m_pairCache(pairCache),
m_userPairCallback(0),
m_ownsPairCache(false),
m_invalidPair(0)
{
	if (!m_pairCache)
	{
		void* ptr = btAlignedAlloc(sizeof(btOverlappingPairCache),16);
		m_pairCache = new(ptr) btOverlappingPairCache();
		m_ownsPairCache = true;
	}

	//assert(bounds.HasVolume());

	// init bounds
	m_worldAabbMin = worldAabbMin;
	m_worldAabbMax = worldAabbMax;

	btVector3 aabbSize = m_worldAabbMax - m_worldAabbMin;

	BP_FP_INT_TYPE	maxInt = m_handleSentinel;

	m_quantize = btVector3(btScalar(maxInt),btScalar(maxInt),btScalar(maxInt)) / aabbSize;

	// allocate handles buffer and put all handles on free list
	void* ptr = btAlignedAlloc(sizeof(Handle)*maxHandles,16);
	m_pHandles = new(ptr) Handle[maxHandles];
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
		{
			void* ptr = btAlignedAlloc(sizeof(Edge)*maxHandles*2,16);
			m_pEdges[i] = new(ptr) Edge[maxHandles * 2];
		}
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
		m_pEdges[axis][1].m_pos = m_handleSentinel;
		m_pEdges[axis][1].m_handle = 0;
#ifdef DEBUG_BROADPHASE
		debugPrintAxis(axis);
#endif //DEBUG_BROADPHASE

	}

}

template <typename BP_FP_INT_TYPE>
btAxisSweep3Internal<BP_FP_INT_TYPE>::~btAxisSweep3Internal()
{
	
	for (int i = 2; i >= 0; i--)
	{
		btAlignedFree(m_pEdges[i]);
	}
	btAlignedFree(m_pHandles);

	if (m_ownsPairCache)
	{
		m_pairCache->~btOverlappingPairCache();
		btAlignedFree(m_pairCache);
	}
}

template <typename BP_FP_INT_TYPE>
void btAxisSweep3Internal<BP_FP_INT_TYPE>::quantize(BP_FP_INT_TYPE* out, const btPoint3& point, int isMax) const
{
	btPoint3 clampedPoint(point);
	


	clampedPoint.setMax(m_worldAabbMin);
	clampedPoint.setMin(m_worldAabbMax);

	btVector3 v = (clampedPoint - m_worldAabbMin) * m_quantize;
	out[0] = (BP_FP_INT_TYPE)(((BP_FP_INT_TYPE)v.getX() & m_bpHandleMask) | isMax);
	out[1] = (BP_FP_INT_TYPE)(((BP_FP_INT_TYPE)v.getY() & m_bpHandleMask) | isMax);
	out[2] = (BP_FP_INT_TYPE)(((BP_FP_INT_TYPE)v.getZ() & m_bpHandleMask) | isMax);
	
}


template <typename BP_FP_INT_TYPE>
BP_FP_INT_TYPE btAxisSweep3Internal<BP_FP_INT_TYPE>::allocHandle()
{
	assert(m_firstFreeHandle);

	BP_FP_INT_TYPE handle = m_firstFreeHandle;
	m_firstFreeHandle = getHandle(handle)->GetNextFree();
	m_numHandles++;

	return handle;
}

template <typename BP_FP_INT_TYPE>
void btAxisSweep3Internal<BP_FP_INT_TYPE>::freeHandle(BP_FP_INT_TYPE handle)
{
	assert(handle > 0 && handle < m_maxHandles);

	getHandle(handle)->SetNextFree(m_firstFreeHandle);
	m_firstFreeHandle = handle;

	m_numHandles--;
}


template <typename BP_FP_INT_TYPE>
BP_FP_INT_TYPE btAxisSweep3Internal<BP_FP_INT_TYPE>::addHandle(const btPoint3& aabbMin,const btPoint3& aabbMax, void* pOwner,short int collisionFilterGroup,short int collisionFilterMask,btDispatcher* dispatcher)
{
	// quantize the bounds
	BP_FP_INT_TYPE min[3], max[3];
	quantize(min, aabbMin, 0);
	quantize(max, aabbMax, 1);

	// allocate a handle
	BP_FP_INT_TYPE handle = allocHandle();
	

	Handle* pHandle = getHandle(handle);
	
	pHandle->m_uniqueId = handle;
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
	sortMinDown(0, pHandle->m_minEdges[0], dispatcher,false);
	sortMaxDown(0, pHandle->m_maxEdges[0], dispatcher,false);
	sortMinDown(1, pHandle->m_minEdges[1], dispatcher,false);
	sortMaxDown(1, pHandle->m_maxEdges[1], dispatcher,false);
	sortMinDown(2, pHandle->m_minEdges[2], dispatcher,true);
	sortMaxDown(2, pHandle->m_maxEdges[2], dispatcher,true);


	return handle;
}


template <typename BP_FP_INT_TYPE>
void btAxisSweep3Internal<BP_FP_INT_TYPE>::removeHandle(BP_FP_INT_TYPE handle,btDispatcher* dispatcher)
{

	Handle* pHandle = getHandle(handle);

	//explicitly remove the pairs containing the proxy
	//we could do it also in the sortMinUp (passing true)
	//todo: compare performance
	m_pairCache->removeOverlappingPairsContainingProxy(pHandle,dispatcher);


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
		pEdges[max].m_pos = m_handleSentinel;

		sortMaxUp(axis,max,dispatcher,false);


		BP_FP_INT_TYPE i = pHandle->m_minEdges[axis];
		pEdges[i].m_pos = m_handleSentinel;


		sortMinUp(axis,i,dispatcher,false);

		pEdges[limit-1].m_handle = 0;
		pEdges[limit-1].m_pos = m_handleSentinel;
		
#ifdef DEBUG_BROADPHASE
			debugPrintAxis(axis,false);
#endif //DEBUG_BROADPHASE


	}


	// free the handle
	freeHandle(handle);

	
}

extern int gOverlappingPairs;
#include <stdio.h>

template <typename BP_FP_INT_TYPE>
void	btAxisSweep3Internal<BP_FP_INT_TYPE>::calculateOverlappingPairs(btDispatcher* dispatcher)
{
#ifdef USE_LAZY_REMOVAL

	if (m_ownsPairCache)
	{
	
		btBroadphasePairArray&	overlappingPairArray = m_pairCache->getOverlappingPairArray();

		//perform a sort, to find duplicates and to sort 'invalid' pairs to the end
		overlappingPairArray.heapSort(btBroadphasePairSortPredicate());

		overlappingPairArray.resize(overlappingPairArray.size() - m_invalidPair);
		m_invalidPair = 0;

		
		int i;

		btBroadphasePair previousPair;
		previousPair.m_pProxy0 = 0;
		previousPair.m_pProxy1 = 0;
		previousPair.m_algorithm = 0;
		
		
		for (i=0;i<overlappingPairArray.size();i++)
		{
		
			btBroadphasePair& pair = overlappingPairArray[i];

			bool isDuplicate = (pair == previousPair);

			previousPair = pair;

			bool needsRemoval = false;

			if (!isDuplicate)
			{
				bool hasOverlap = testAabbOverlap(pair.m_pProxy0,pair.m_pProxy1);

				if (hasOverlap)
				{
					needsRemoval = false;//callback->processOverlap(pair);
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
				m_pairCache->cleanOverlappingPair(pair,dispatcher);

		//		m_overlappingPairArray.swap(i,m_overlappingPairArray.size()-1);
		//		m_overlappingPairArray.pop_back();
				pair.m_pProxy0 = 0;
				pair.m_pProxy1 = 0;
				m_invalidPair++;
				gOverlappingPairs--;
			} 
			
		}

	///if you don't like to skip the invalid pairs in the array, execute following code:
	#define CLEAN_INVALID_PAIRS 1
	#ifdef CLEAN_INVALID_PAIRS

		//perform a sort, to sort 'invalid' pairs to the end
		overlappingPairArray.heapSort(btBroadphasePairSortPredicate());

		overlappingPairArray.resize(overlappingPairArray.size() - m_invalidPair);
		m_invalidPair = 0;
	#endif//CLEAN_INVALID_PAIRS
		
		//printf("overlappingPairArray.size()=%d\n",overlappingPairArray.size());
	}
#endif //USE_LAZY_REMOVAL


	

}


template <typename BP_FP_INT_TYPE>
bool btAxisSweep3Internal<BP_FP_INT_TYPE>::testAabbOverlap(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1)
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

template <typename BP_FP_INT_TYPE>
bool btAxisSweep3Internal<BP_FP_INT_TYPE>::testOverlap(int ignoreAxis,const Handle* pHandleA, const Handle* pHandleB)
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

template <typename BP_FP_INT_TYPE>
void btAxisSweep3Internal<BP_FP_INT_TYPE>::updateHandle(BP_FP_INT_TYPE handle, const btPoint3& aabbMin,const btPoint3& aabbMax,btDispatcher* dispatcher)
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
			sortMinDown(axis, emin,dispatcher,true);

		if (dmax > 0)
			sortMaxUp(axis, emax,dispatcher,true);

		// shrink (only removes overlaps)
		if (dmin > 0)
			sortMinUp(axis, emin,dispatcher,true);

		if (dmax < 0)
			sortMaxDown(axis, emax,dispatcher,true);

#ifdef DEBUG_BROADPHASE
	debugPrintAxis(axis);
#endif //DEBUG_BROADPHASE
	}

	
}




// sorting a min edge downwards can only ever *add* overlaps
template <typename BP_FP_INT_TYPE>
void btAxisSweep3Internal<BP_FP_INT_TYPE>::sortMinDown(int axis, BP_FP_INT_TYPE edge, btDispatcher* dispatcher, bool updateOverlaps)
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
				m_pairCache->addOverlappingPair(pHandleEdge,pHandlePrev);
				if (m_userPairCallback)
					m_userPairCallback->addOverlappingPair(pHandleEdge,pHandlePrev);

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
template <typename BP_FP_INT_TYPE>
void btAxisSweep3Internal<BP_FP_INT_TYPE>::sortMinUp(int axis, BP_FP_INT_TYPE edge, btDispatcher* dispatcher, bool updateOverlaps)
{
	Edge* pEdge = m_pEdges[axis] + edge;
	Edge* pNext = pEdge + 1;
	Handle* pHandleEdge = getHandle(pEdge->m_handle);

	while (pNext->m_handle && (pEdge->m_pos >= pNext->m_pos))
	{
		Handle* pHandleNext = getHandle(pNext->m_handle);

		if (pNext->IsMax())
		{
#ifndef USE_LAZY_REMOVAL
			// if next edge is maximum remove any overlap between the two handles
			if (updateOverlaps)
			{
				Handle* handle0 = getHandle(pEdge->m_handle);
				Handle* handle1 = getHandle(pNext->m_handle);

				m_pairCache->removeOverlappingPair(handle0,handle1,dispatcher);	
				if (m_userPairCallback)
					m_userPairCallback->removeOverlappingPair(handle0,handle1);
				
			}
#endif //USE_LAZY_REMOVAL

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
template <typename BP_FP_INT_TYPE>
void btAxisSweep3Internal<BP_FP_INT_TYPE>::sortMaxDown(int axis, BP_FP_INT_TYPE edge, btDispatcher* dispatcher, bool updateOverlaps)
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
#ifndef USE_LAZY_REMOVAL
				Handle* handle0 = getHandle(pEdge->m_handle);
				Handle* handle1 = getHandle(pPrev->m_handle);
				m_pairCache->removeOverlappingPair(handle0,handle1,dispatcher);
				if (m_userPairCallback)
					m_userPairCallback->removeOverlappingPair(handle0,handle1);
			
#endif //USE_LAZY_REMOVAL		

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
template <typename BP_FP_INT_TYPE>
void btAxisSweep3Internal<BP_FP_INT_TYPE>::sortMaxUp(int axis, BP_FP_INT_TYPE edge, btDispatcher* dispatcher, bool updateOverlaps)
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
				m_pairCache->addOverlappingPair(handle0,handle1);
				if (m_userPairCallback)
					m_userPairCallback->addOverlappingPair(handle0,handle1);
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



////////////////////////////////////////////////////////////////////


/// btAxisSweep3 is an efficient implementation of the 3d axis sweep and prune broadphase.
/// It uses arrays rather then lists for storage of the 3 axis. Also it operates using 16 bit integer coordinates instead of floats.
/// For large worlds and many objects, use bt32BitAxisSweep3 instead. bt32BitAxisSweep3 has higher precision and allows more then 16384 objects at the cost of more memory and bit of performance.
class btAxisSweep3 : public btAxisSweep3Internal<unsigned short int>
{
public:

	btAxisSweep3(const btPoint3& worldAabbMin,const btPoint3& worldAabbMax, unsigned short int maxHandles = 16384, btOverlappingPairCache* pairCache = 0);

};

/// bt32BitAxisSweep3 allows higher precision quantization and more objects compared to the btAxisSweep3 sweep and prune.
/// This comes at the cost of more memory per handle, and a bit slower performance.
/// It uses arrays rather then lists for storage of the 3 axis.
class bt32BitAxisSweep3 : public btAxisSweep3Internal<unsigned int>
{
public:

	bt32BitAxisSweep3(const btPoint3& worldAabbMin,const btPoint3& worldAabbMax, unsigned int maxHandles = 1500000, btOverlappingPairCache* pairCache = 0);

};

#endif

