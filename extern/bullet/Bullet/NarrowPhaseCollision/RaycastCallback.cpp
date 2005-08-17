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

#include "RaycastCallback.h"

RaycastCallback::RaycastCallback(const SimdVector3& from,const SimdVector3& to)
	:
	m_from(from),
	m_to(to),
	m_hitFraction(1.f),
	m_hitProxy(0),
	m_hitFound(false)
{

}



void RaycastCallback::ProcessTriangle(SimdVector3* triangle)
{
	const SimdVector3 &vert0=triangle[0];
	const SimdVector3 &vert1=triangle[1];
	const SimdVector3 &vert2=triangle[2];

	SimdVector3 v10; v10 = vert1 - vert0 ;
	SimdVector3 v20; v20 = vert2 - vert0 ;

	SimdVector3 triangleNormal; triangleNormal = v10.cross( v20 );
	
	const float dist = vert0.dot(triangleNormal);
	float dist_a = triangleNormal.dot(m_from) ;
	dist_a-= dist;
	float dist_b = triangleNormal.dot(m_to);
	dist_b -= dist;

	if ( dist_a * dist_b >= 0.0f)
	{
		return ; // same sign
	}
	
	const float proj_length=dist_a-dist_b;
	const float distance = (dist_a)/(proj_length);
	// Now we have the intersection point on the plane, we'll see if it's inside the triangle
	// Add an epsilon as a tolerance for the raycast,
	// in case the ray hits exacly on the edge of the triangle.
	// It must be scaled for the triangle size.
	
	if(distance < m_hitFraction)
	{
		

		float edge_tolerance =triangleNormal.length2();		
		edge_tolerance *= -0.0001f;
		SimdVector3 point; point.setInterpolate3( m_from, m_to, distance);
		{
			SimdVector3 v0p; v0p = vert0 - point;
			SimdVector3 v1p; v1p = vert1 - point;
			SimdVector3 cp0; cp0 = v0p.cross( v1p );

			if ( (float)(cp0.dot(triangleNormal)) >=edge_tolerance) 
			{
						

				SimdVector3 v2p; v2p = vert2 -  point;
				SimdVector3 cp1;
				cp1 = v1p.cross( v2p);
				if ( (float)(cp1.dot(triangleNormal)) >=edge_tolerance) 
				{
					SimdVector3 cp2;
					cp2 = v2p.cross(v0p);
					
					if ( (float)(cp2.dot(triangleNormal)) >=edge_tolerance) 
					{
						m_hitFraction = distance;
						if ( dist_a > 0 )
						{
							m_hitNormalLocal = triangleNormal;
						}
						else
						{
							m_hitNormalLocal = -triangleNormal;
						}
						
						m_hitFound= true;
					}
				}
			}
		}
	}
}
