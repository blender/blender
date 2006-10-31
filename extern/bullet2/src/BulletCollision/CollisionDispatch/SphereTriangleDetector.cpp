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

#include "SphereTriangleDetector.h"
#include "BulletCollision/CollisionShapes/btTriangleShape.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h"


SphereTriangleDetector::SphereTriangleDetector(btSphereShape* sphere,btTriangleShape* triangle)
:m_sphere(sphere),
m_triangle(triangle)
{

}

void	SphereTriangleDetector::getClosestPoints(const ClosestPointInput& input,Result& output,class btIDebugDraw* debugDraw)
{

	const btTransform& transformA = input.m_transformA;
	const btTransform& transformB = input.m_transformB;

	btVector3 point,normal;
	btScalar timeOfImpact = 1.f;
	btScalar depth = 0.f;
//	output.m_distance = 1e30f;
	//move sphere into triangle space
	btTransform	sphereInTr = transformB.inverseTimes(transformA);

	if (collide(sphereInTr.getOrigin(),point,normal,depth,timeOfImpact))
	{
		output.addContactPoint(transformB.getBasis()*normal,transformB*point,depth);
	}

}

#define MAX_OVERLAP 0.f



// See also geometrictools.com
// Basic idea: D = |p - (lo + t0*lv)| where t0 = lv . (p - lo) / lv . lv
float SegmentSqrDistance(const btVector3& from, const btVector3& to,const btVector3 &p, btVector3 &nearest) {
	btVector3 diff = p - from;
	btVector3 v = to - from;
	float t = v.dot(diff);
	
	if (t > 0) {
		float dotVV = v.dot(v);
		if (t < dotVV) {
			t /= dotVV;
			diff -= t*v;
		} else {
			t = 1;
			diff -= v;
		}
	} else
		t = 0;

	nearest = from + t*v;
	return diff.dot(diff);	
}

bool SphereTriangleDetector::facecontains(const btVector3 &p,const btVector3* vertices,btVector3& normal)  {
	btVector3 lp(p);
	btVector3 lnormal(normal);
	
	return pointInTriangle(vertices, lnormal, &lp);
}

///combined discrete/continuous sphere-triangle
bool SphereTriangleDetector::collide(const btVector3& sphereCenter,btVector3 &point, btVector3& resultNormal, btScalar& depth, float &timeOfImpact)
{

	const btVector3* vertices = &m_triangle->getVertexPtr(0);
	const btVector3& c = sphereCenter;
	btScalar r = m_sphere->getRadius();

	btVector3 delta (0,0,0);

	btVector3 normal = (vertices[1]-vertices[0]).cross(vertices[2]-vertices[0]);
	normal.normalize();
	btVector3 p1ToCentre = c - vertices[0];
	float distanceFromPlane = p1ToCentre.dot(normal);

	if (distanceFromPlane < 0.f)
	{
		//triangle facing the other way
	
		distanceFromPlane *= -1.f;
		normal *= -1.f;
	}

	///todo: move this gContactBreakingThreshold into a proper structure
	extern float gContactBreakingThreshold;

	float contactMargin = gContactBreakingThreshold;
	bool isInsideContactPlane = distanceFromPlane < r + contactMargin;
	bool isInsideShellPlane = distanceFromPlane < r;
	
	float deltaDotNormal = delta.dot(normal);
	if (!isInsideShellPlane && deltaDotNormal >= 0.0f)
		return false;

	// Check for contact / intersection
	bool hasContact = false;
	btVector3 contactPoint;
	if (isInsideContactPlane) {
		if (facecontains(c,vertices,normal)) {
			// Inside the contact wedge - touches a point on the shell plane
			hasContact = true;
			contactPoint = c - normal*distanceFromPlane;
		} else {
			// Could be inside one of the contact capsules
			float contactCapsuleRadiusSqr = (r + contactMargin) * (r + contactMargin);
			btVector3 nearestOnEdge;
			for (int i = 0; i < m_triangle->getNumEdges(); i++) {
				
				btPoint3 pa;
				btPoint3 pb;
				
				m_triangle->getEdge(i,pa,pb);

				float distanceSqr = SegmentSqrDistance(pa,pb,c, nearestOnEdge);
				if (distanceSqr < contactCapsuleRadiusSqr) {
					// Yep, we're inside a capsule
					hasContact = true;
					contactPoint = nearestOnEdge;
				}
				
			}
		}
	}

	if (hasContact) {
		btVector3 contactToCentre = c - contactPoint;
		float distanceSqr = contactToCentre.length2();
		if (distanceSqr < (r - MAX_OVERLAP)*(r - MAX_OVERLAP)) {
			float distance = sqrtf(distanceSqr);
			if (1)
			{
				resultNormal = contactToCentre;
				resultNormal.normalize();
			}
			point = contactPoint;
			depth = -(r-distance);
			return true;
		}

		if (delta.dot(contactToCentre) >= 0.0f) 
			return false;
		
		// Moving towards the contact point -> collision
		point = contactPoint;
		timeOfImpact = 0.0f;
		return true;
	}
	
	return false;
}


bool SphereTriangleDetector::pointInTriangle(const btVector3 vertices[], const btVector3 &normal, btVector3 *p )
{
	const btVector3* p1 = &vertices[0];
	const btVector3* p2 = &vertices[1];
	const btVector3* p3 = &vertices[2];

	btVector3 edge1( *p2 - *p1 );
	btVector3 edge2( *p3 - *p2 );
	btVector3 edge3( *p1 - *p3 );

	btVector3 p1_to_p( *p - *p1 );
	btVector3 p2_to_p( *p - *p2 );
	btVector3 p3_to_p( *p - *p3 );

	btVector3 edge1_normal( edge1.cross(normal));
	btVector3 edge2_normal( edge2.cross(normal));
	btVector3 edge3_normal( edge3.cross(normal));
	
	float r1, r2, r3;
	r1 = edge1_normal.dot( p1_to_p );
	r2 = edge2_normal.dot( p2_to_p );
	r3 = edge3_normal.dot( p3_to_p );
	if ( ( r1 > 0 && r2 > 0 && r3 > 0 ) ||
	     ( r1 <= 0 && r2 <= 0 && r3 <= 0 ) )
		return true;
	return false;

}
