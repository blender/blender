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

#include "../../LinearMath/btPoint3.h"
#include "../../LinearMath/btVector3.h"
#include "btOverlappingPairCache.h"
#include "btBroadphaseProxy.h"


//Enable BP_USE_FIXEDPOINT_INT_32 if you need more then 32767 objects
//#define BP_USE_FIXEDPOINT_INT_32 1

#ifdef BP_USE_FIXEDPOINT_INT_32
	#define BP_FP_INT_TYPE unsigned int
	#define BP_MAX_HANDLES 1500000 //arbitrary maximum number of handles
	#define BP_HANDLE_SENTINEL 0x7fffffff
	#define BP_HANDLE_MASK	0xfffffffe
#else
	#define BP_FP_INT_TYPE unsigned short int
	#define BP_MAX_HANDLES 32767
	#define BP_HANDLE_SENTINEL 0xffff
	#define BP_HANDLE_MASK	0xfffe
#endif //BP_USE_FIXEDPOINT_INT_32

//#define DEBUG_BROADPHASE 1

/// btAxisSweep3 is an efficient implementation of the 3d axis sweep and prune broadphase.
/// It uses arrays rather then lists for storage of the 3 axis. Also it operates using integer coordinates instead of floats.
/// The testOverlap check is optimized to check the array index, rather then the actual AABB coordinates/pos
class btAxisSweep3 : public btOverlappingPairCache
{

public:
	

	class Edge
	{
	public:
		BP_FP_INT_TYPE m_pos;			// low bit is min/max
		BP_FP_INT_TYPE m_handle;

		BP_FP_INT_TYPE IsMax() const {return m_pos & 1;}
	};

public:
	class Handle : public btBroadphaseProxy
	{
	public:
		
		// indexes into the edge arrays
		BP_FP_INT_TYPE m_minEdges[3], m_maxEdges[3];		// 6 * 2 = 12
		BP_FP_INT_TYPE m_handleId;
		BP_FP_INT_TYPE m_pad;
		
		//void* m_pOwner; this is now in btBroadphaseProxy.m_clientObject
	
		inline void SetNextFree(BP_FP_INT_TYPE next) {m_minEdges[0] = next;}
		inline BP_FP_INT_TYPE GetNextFree() const {return m_minEdges[0];}
	};		// 24 bytes + 24 for Edge structures = 44 bytes total per entry

	
private:
	btPoint3 m_worldAabbMin;						// overall system bounds
	btPoint3 m_worldAabbMax;						// overall system bounds

	btVector3 m_quantize;						// scaling factor for quantization

	BP_FP_INT_TYPE m_numHandles;						// number of active handles
	int m_maxHandles;						// max number of handles
	Handle* m_pHandles;						// handles pool
	BP_FP_INT_TYPE m_firstFreeHandle;		// free handles list

	Edge* m_pEdges[3];						// edge arrays for the 3 axes (each array has m_maxHandles * 2 + 2 sentinel entries)

	int m_invalidPair;

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

	void sortMinDown(int axis, BP_FP_INT_TYPE edge, bool updateOverlaps = true);
	void sortMinUp(int axis, BP_FP_INT_TYPE edge, bool updateOverlaps = true);
	void sortMaxDown(int axis, BP_FP_INT_TYPE edge, bool updateOverlaps = true);
	void sortMaxUp(int axis, BP_FP_INT_TYPE edge, bool updateOverlaps = true);

public:
	btAxisSweep3(const btPoint3& worldAabbMin,const btPoint3& worldAabbMax, int maxHandles = 16384);
	virtual	~btAxisSweep3();

	virtual void	refreshOverlappingPairs();
	
	BP_FP_INT_TYPE addHandle(const btPoint3& aabbMin,const btPoint3& aabbMax, void* pOwner,short int collisionFilterGroup,short int collisionFilterMask);
	void removeHandle(BP_FP_INT_TYPE handle);
	void updateHandle(BP_FP_INT_TYPE handle, const btPoint3& aabbMin,const btPoint3& aabbMax);
	inline Handle* getHandle(BP_FP_INT_TYPE index) const {return m_pHandles + index;}

	void	processAllOverlappingPairs(btOverlapCallback* callback);

	//Broadphase Interface
	virtual btBroadphaseProxy*	createProxy(  const btVector3& min,  const btVector3& max,int shapeType,void* userPtr ,short int collisionFilterGroup,short int collisionFilterMask);
	virtual void	destroyProxy(btBroadphaseProxy* proxy);
	virtual void	setAabb(btBroadphaseProxy* proxy,const btVector3& aabbMin,const btVector3& aabbMax);
	bool testOverlap(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1);

};

#endif

