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
#ifndef RAYCAST_TRI_CALLBACK_H
#define RAYCAST_TRI_CALLBACK_H

#include "CollisionShapes/TriangleCallback.h"
struct BroadphaseProxy;


class  RaycastCallback: public TriangleCallback
{
public:

		//input
	SimdVector3 m_from;
	SimdVector3 m_to;
	//input / output
	SimdScalar			m_hitFraction;
	BroadphaseProxy*	m_hitProxy;

	//output
	SimdVector3			m_hitNormalWorld;



	RaycastCallback(const SimdVector3& from,const SimdVector3& to);

	
	virtual void ProcessTriangle(SimdVector3* triangle);
};

#endif //RAYCAST_TRI_CALLBACK_H