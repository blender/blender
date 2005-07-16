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

	MAX_BROADPHASE_COLLISION_TYPES
};


///BroadphaseProxy
struct BroadphaseProxy
{
	BroadphaseProxy() :m_clientObject(0),m_clientObjectType(-1){}
	BroadphaseProxy(void* object,int type)
		:m_clientObject(object),
		m_clientObjectType(type)
	{
	}

	void        *m_clientObject;

	int GetClientObjectType ( ) const { return m_clientObjectType;}

	
	void	SetClientObjectType( int type ) { 
		m_clientObjectType = type; 
	}

	bool IsConvexShape()
	{
		return (GetClientObjectType () < TRIANGLE_MESH_SHAPE_PROXYTYPE);
	}
	bool IsConcaveShape()
	{
		return (GetClientObjectType() > CONCAVE_SHAPES_START_HERE);
	}

protected:
	int			 m_clientObjectType;
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

