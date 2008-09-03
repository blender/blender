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


#include "btRaycastCallback.h"

btTriangleRaycastCallback::btTriangleRaycastCallback(const btVector3& from,const btVector3& to,bool faceNormal)
	:
	m_from(from),
	m_to(to),
	m_faceNormal(faceNormal),
	m_hitFraction(btScalar(1.))
{

}



void btTriangleRaycastCallback::processTriangle(btVector3* triangle,int partId, int triangleIndex)
{
	

	const btVector3 &vert0=triangle[0];
	const btVector3 &vert1=triangle[1];
	const btVector3 &vert2=triangle[2];

	btVector3 v10; v10 = vert1 - vert0 ;
	btVector3 v20; v20 = vert2 - vert0 ;

	btVector3 triangleNormal; triangleNormal = v10.cross( v20 );
	
	const btScalar dist = vert0.dot(triangleNormal);
	btScalar dist_a = triangleNormal.dot(m_from) ;
	dist_a-= dist;
	btScalar dist_b = triangleNormal.dot(m_to);
	dist_b -= dist;

	if ( dist_a * dist_b >= btScalar(0.0) )
	{
		return ; // same sign
	}
	
	const btScalar proj_length=dist_a-dist_b;
	const btScalar distance = (dist_a)/(proj_length);
	// Now we have the intersection point on the plane, we'll see if it's inside the triangle
	// Add an epsilon as a tolerance for the raycast,
	// in case the ray hits exacly on the edge of the triangle.
	// It must be scaled for the triangle size.
	
	if(distance < m_hitFraction)
	{
		

		btScalar edge_tolerance =triangleNormal.length2();		
		edge_tolerance *= btScalar(-0.0001);
		btVector3 point; point.setInterpolate3( m_from, m_to, distance);
		{
			btVector3 v0p; v0p = vert0 - point;
			btVector3 v1p; v1p = vert1 - point;
			btVector3 cp0; cp0 = v0p.cross( v1p );

			if ( (btScalar)(cp0.dot(triangleNormal)) >=edge_tolerance) 
			{
						

				btVector3 v2p; v2p = vert2 -  point;
				btVector3 cp1;
				cp1 = v1p.cross( v2p);
				if ( (btScalar)(cp1.dot(triangleNormal)) >=edge_tolerance) 
				{
					btVector3 cp2;
					cp2 = v2p.cross(v0p);
					
					if ( (btScalar)(cp2.dot(triangleNormal)) >=edge_tolerance) 
					{
						if (m_faceNormal || dist_a > 0)
						{
							m_hitFraction = reportHit(triangleNormal,distance,partId,triangleIndex);
						}
						else
						{
							m_hitFraction = reportHit(-triangleNormal,distance,partId,triangleIndex);
						}
					}
				}
			}
		}
	}
}
