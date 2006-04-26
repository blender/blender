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

#ifndef BROADPHASE_PROXY_H
#define BROADPHASE_PROXY_H



/// Dispatcher uses these types
/// IMPORTANT NOTE:The types are ordered polyhedral, implicit convex and concave
/// to facilitate type checking
enum BroadphaseNativeTypes
{
// polyhedral convex shapes
	BOX_SHAPE_PROXYTYPE,
	TRIANGLE_SHAPE_PROXYTYPE,
	TETRAHEDRAL_SHAPE_PROXYTYPE,
	CONVEX_HULL_SHAPE_PROXYTYPE,
//implicit convex shapes
IMPLICIT_CONVEX_SHAPES_START_HERE,
	SPHERE_SHAPE_PROXYTYPE,
	MULTI_SPHERE_SHAPE_PROXYTYPE,
	CONE_SHAPE_PROXYTYPE,
	CONVEX_SHAPE_PROXYTYPE,
	CYLINDER_SHAPE_PROXYTYPE,
	MINKOWSKI_SUM_SHAPE_PROXYTYPE,
	MINKOWSKI_DIFFERENCE_SHAPE_PROXYTYPE,
//concave shapes
CONCAVE_SHAPES_START_HERE,
	//keep all the convex shapetype below here, for the check IsConvexShape in broadphase proxy!
	TRIANGLE_MESH_SHAPE_PROXYTYPE,
	EMPTY_SHAPE_PROXYTYPE,

	MAX_BROADPHASE_COLLISION_TYPES
};


///BroadphaseProxy
struct BroadphaseProxy
{
	
	//Usually the client CollisionObject or Rigidbody class
	void*	m_clientObject;


	BroadphaseProxy() :m_clientObject(0){}
	BroadphaseProxy(int shapeType,void* userPtr)
		:m_clientObject(userPtr)
		//m_clientObjectType(shapeType)
	{
	}
	
};

class CollisionAlgorithm;

struct BroadphaseProxy;

#define SIMPLE_MAX_ALGORITHMS 2

/// contains a pair of aabb-overlapping objects
struct BroadphasePair
{
	BroadphasePair ()
		:
	m_pProxy0(0),
		m_pProxy1(0)
	{
		for (int i=0;i<SIMPLE_MAX_ALGORITHMS;i++)
		{
			m_algorithms[i] = 0;
		}
	}

	BroadphasePair(const BroadphasePair& other)
		:		m_pProxy0(other.m_pProxy0),
				m_pProxy1(other.m_pProxy1)
	{
		for (int i=0;i<SIMPLE_MAX_ALGORITHMS;i++)
		{
			m_algorithms[i] = other.m_algorithms[i];
		}
	}
	BroadphasePair(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1)
		:
		m_pProxy0(&proxy0),
		m_pProxy1(&proxy1)
	{
		for (int i=0;i<SIMPLE_MAX_ALGORITHMS;i++)
	{
			m_algorithms[i] = 0;
		}

	}

	
	BroadphaseProxy* m_pProxy0;
	BroadphaseProxy* m_pProxy1;
	
	mutable CollisionAlgorithm* m_algorithms[SIMPLE_MAX_ALGORITHMS];
};

#endif //BROADPHASE_PROXY_H

