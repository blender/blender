
//Bullet Continuous Collision Detection and Physics Library
//Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/


//
// AxisSweep3
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
#include "AxisSweep3.h"

#include <assert.h>

BroadphaseProxy*	AxisSweep3::CreateProxy(  const SimdVector3& min,  const SimdVector3& max,int shapeType,void* userPtr,short int collisionFilterGroup,short int collisionFilterMask)
{
		unsigned short handleId = AddHandle(min,max, userPtr,collisionFilterGroup,collisionFilterMask);
		
		Handle* handle = GetHandle(handleId);
				
		return handle;
}

void	AxisSweep3::DestroyProxy(BroadphaseProxy* proxy)
{
	Handle* handle = static_cast<Handle*>(proxy);
	RemoveHandle(handle->m_handleId);
}

void	AxisSweep3::SetAabb(BroadphaseProxy* proxy,const SimdVector3& aabbMin,const SimdVector3& aabbMax)
{
	Handle* handle = static_cast<Handle*>(proxy);
	UpdateHandle(handle->m_handleId,aabbMin,aabbMax);
}






AxisSweep3::AxisSweep3(const SimdPoint3& worldAabbMin,const SimdPoint3& worldAabbMax, int maxHandles, int maxOverlaps)
:OverlappingPairCache(maxOverlaps)
{
	//assert(bounds.HasVolume());

	// 1 handle is reserved as sentinel
	assert(maxHandles > 1 && maxHandles < 32767);

	// doesn't need this limit right now, but I may want to use unsigned short indexes into overlaps array
	assert(maxOverlaps > 0 && maxOverlaps < 65536);

	// init bounds
	m_worldAabbMin = worldAabbMin;
	m_worldAabbMax = worldAabbMax;

	SimdVector3 aabbSize = m_worldAabbMax - m_worldAabbMin;

	m_quantize = SimdVector3(65535.0f,65535.0f,65535.0f) / aabbSize;

	// allocate handles buffer and put all handles on free list
	m_pHandles = new Handle[maxHandles];
	m_maxHandles = maxHandles;
	m_numHandles = 0;

	// handle 0 is reserved as the null index, and is also used as the sentinel
	m_firstFreeHandle = 1;
	{
		for (int i = m_firstFreeHandle; i < maxHandles; i++)
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
		m_pEdges[axis][1].m_pos = 0xffff;
		m_pEdges[axis][1].m_handle = 0;
	}
}

AxisSweep3::~AxisSweep3()
{
	
	for (int i = 2; i >= 0; i--)
		delete[] m_pEdges[i];
	delete[] m_pHandles;
}

void AxisSweep3::Quantize(unsigned short* out, const SimdPoint3& point, int isMax) const
{
	SimdPoint3 clampedPoint(point);
	/*
	if (isMax)
		clampedPoint += SimdVector3(10,10,10);
	else
	{
		clampedPoint -= SimdVector3(10,10,10);
	}
	*/


	clampedPoint.setMax(m_worldAabbMin);
	clampedPoint.setMin(m_worldAabbMax);

	SimdVector3 v = (clampedPoint - m_worldAabbMin) * m_quantize;
	out[0] = (unsigned short)(((int)v.getX() & 0xfffc) | isMax);
	out[1] = (unsigned short)(((int)v.getY() & 0xfffc) | isMax);
	out[2] = (unsigned short)(((int)v.getZ() & 0xfffc) | isMax);
	
}



unsigned short AxisSweep3::AllocHandle()
{
	assert(m_firstFreeHandle);

	unsigned short handle = m_firstFreeHandle;
	m_firstFreeHandle = GetHandle(handle)->GetNextFree();
	m_numHandles++;

	return handle;
}

void AxisSweep3::FreeHandle(unsigned short handle)
{
	assert(handle > 0 && handle < m_maxHandles);

	GetHandle(handle)->SetNextFree(m_firstFreeHandle);
	m_firstFreeHandle = handle;

	m_numHandles--;
}



unsigned short AxisSweep3::AddHandle(const SimdPoint3& aabbMin,const SimdPoint3& aabbMax, void* pOwner,short int collisionFilterGroup,short int collisionFilterMask)
{
	// quantize the bounds
	unsigned short min[3], max[3];
	Quantize(min, aabbMin, 0);
	Quantize(max, aabbMax, 1);

	// allocate a handle
	unsigned short handle = AllocHandle();
	assert(handle!= 0xcdcd);

	Handle* pHandle = GetHandle(handle);
	
	pHandle->m_handleId = handle;
	//pHandle->m_pOverlaps = 0;
	pHandle->m_clientObject = pOwner;
	pHandle->m_collisionFilterGroup = collisionFilterGroup;
	pHandle->m_collisionFilterMask = collisionFilterMask;

	// compute current limit of edge arrays
	int limit = m_numHandles * 2;

	// insert new edges just inside the max boundary edge
	for (int axis = 0; axis < 3; axis++)
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
	SortMinDown(0, pHandle->m_minEdges[0], false);
	SortMaxDown(0, pHandle->m_maxEdges[0], false);
	SortMinDown(1, pHandle->m_minEdges[1], false);
	SortMaxDown(1, pHandle->m_maxEdges[1], false);
	SortMinDown(2, pHandle->m_minEdges[2], true);
	SortMaxDown(2, pHandle->m_maxEdges[2], true);

	//PrintAxis(1);

	return handle;
}


void AxisSweep3::RemoveHandle(unsigned short handle)
{
	Handle* pHandle = GetHandle(handle);

	RemoveOverlappingPairsContainingProxy(pHandle);


	// compute current limit of edge arrays
	int limit = m_numHandles * 2;
	int axis;

	for (axis = 0;axis<3;axis++)
	{
		Edge* pEdges = m_pEdges[axis];
		int maxEdge= pHandle->m_maxEdges[axis];
		pEdges[maxEdge].m_pos = 0xffff;
		int minEdge = pHandle->m_minEdges[axis];
		pEdges[minEdge].m_pos = 0xffff;
	}

	// remove the edges by sorting them up to the end of the list
	for ( axis = 0; axis < 3; axis++)
	{
		Edge* pEdges = m_pEdges[axis];
		int max = pHandle->m_maxEdges[axis];
		pEdges[max].m_pos = 0xffff;

		SortMaxUp(axis,max,false);
		
		int i = pHandle->m_minEdges[axis];
		pEdges[i].m_pos = 0xffff;

		SortMinUp(axis,i,false);

		pEdges[limit-1].m_handle = 0;
		pEdges[limit-1].m_pos = 0xffff;

	}

	// free the handle
	FreeHandle(handle);

	
}

bool AxisSweep3::TestOverlap(int ignoreAxis,const Handle* pHandleA, const Handle* pHandleB)
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

	//optimization 2: only 2 axis need to be tested

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

void AxisSweep3::UpdateHandle(unsigned short handle, const SimdPoint3& aabbMin,const SimdPoint3& aabbMax)
{
//	assert(bounds.IsFinite());
	//assert(bounds.HasVolume());

	Handle* pHandle = GetHandle(handle);

	// quantize the new bounds
	unsigned short min[3], max[3];
	Quantize(min, aabbMin, 0);
	Quantize(max, aabbMax, 1);

	// update changed edges
	for (int axis = 0; axis < 3; axis++)
	{
		unsigned short emin = pHandle->m_minEdges[axis];
		unsigned short emax = pHandle->m_maxEdges[axis];

		int dmin = (int)min[axis] - (int)m_pEdges[axis][emin].m_pos;
		int dmax = (int)max[axis] - (int)m_pEdges[axis][emax].m_pos;

		m_pEdges[axis][emin].m_pos = min[axis];
		m_pEdges[axis][emax].m_pos = max[axis];

		// expand (only adds overlaps)
		if (dmin < 0)
			SortMinDown(axis, emin);

		if (dmax > 0)
			SortMaxUp(axis, emax);

		// shrink (only removes overlaps)
		if (dmin > 0)
			SortMinUp(axis, emin);

		if (dmax < 0)
			SortMaxDown(axis, emax);
	}

	//PrintAxis(1);
}

// sorting a min edge downwards can only ever *add* overlaps
void AxisSweep3::SortMinDown(int axis, unsigned short edge, bool updateOverlaps)
{
	Edge* pEdge = m_pEdges[axis] + edge;
	Edge* pPrev = pEdge - 1;
	Handle* pHandleEdge = GetHandle(pEdge->m_handle);

	while (pEdge->m_pos < pPrev->m_pos)
	{
		Handle* pHandlePrev = GetHandle(pPrev->m_handle);

		if (pPrev->IsMax())
		{
			// if previous edge is a maximum check the bounds and add an overlap if necessary
			if (updateOverlaps && TestOverlap(axis,pHandleEdge, pHandlePrev))
			{
				AddOverlappingPair(pHandleEdge,pHandlePrev);

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
}

// sorting a min edge upwards can only ever *remove* overlaps
void AxisSweep3::SortMinUp(int axis, unsigned short edge, bool updateOverlaps)
{
	Edge* pEdge = m_pEdges[axis] + edge;
	Edge* pNext = pEdge + 1;
	Handle* pHandleEdge = GetHandle(pEdge->m_handle);

	while (pEdge->m_pos > pNext->m_pos)
	{
		Handle* pHandleNext = GetHandle(pNext->m_handle);

		if (pNext->IsMax())
		{
			// if next edge is maximum remove any overlap between the two handles
			if (updateOverlaps)
			{
				Handle* handle0 = GetHandle(pEdge->m_handle);
				Handle* handle1 = GetHandle(pNext->m_handle);
				BroadphasePair* pair = FindPair(handle0,handle1);
				//assert(pair);
				if (pair)
				{
					RemoveOverlappingPair(*pair);
				}
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
void AxisSweep3::SortMaxDown(int axis, unsigned short edge, bool updateOverlaps)
{
	Edge* pEdge = m_pEdges[axis] + edge;
	Edge* pPrev = pEdge - 1;
	Handle* pHandleEdge = GetHandle(pEdge->m_handle);

	while (pEdge->m_pos < pPrev->m_pos)
	{
		Handle* pHandlePrev = GetHandle(pPrev->m_handle);

		if (!pPrev->IsMax())
		{
			// if previous edge was a minimum remove any overlap between the two handles
			if (updateOverlaps)
			{
				Handle* handle0 = GetHandle(pEdge->m_handle);
				Handle* handle1 = GetHandle(pPrev->m_handle);
				BroadphasePair* pair = FindPair(handle0,handle1);
				//assert(pair);

				if (pair)
				{
					RemoveOverlappingPair(*pair);
				}
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
}

// sorting a max edge upwards can only ever *add* overlaps
void AxisSweep3::SortMaxUp(int axis, unsigned short edge, bool updateOverlaps)
{
	Edge* pEdge = m_pEdges[axis] + edge;
	Edge* pNext = pEdge + 1;
	Handle* pHandleEdge = GetHandle(pEdge->m_handle);

	while (pEdge->m_pos > pNext->m_pos)
	{
		Handle* pHandleNext = GetHandle(pNext->m_handle);

		if (!pNext->IsMax())
		{
			// if next edge is a minimum check the bounds and add an overlap if necessary
			if (updateOverlaps && TestOverlap(axis, pHandleEdge, pHandleNext))
			{
				Handle* handle0 = GetHandle(pEdge->m_handle);
				Handle* handle1 = GetHandle(pNext->m_handle);
				AddOverlappingPair(handle0,handle1);
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
