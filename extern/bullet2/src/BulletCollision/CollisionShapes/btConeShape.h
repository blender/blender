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

#ifndef CONE_MINKOWSKI_H
#define CONE_MINKOWSKI_H

#include "btConvexShape.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h" // for the types

/// implements cone shape interface
class btConeShape : public btConvexShape

{

	float m_sinAngle;
	float m_radius;
	float m_height;
	
	btVector3 coneLocalSupport(const btVector3& v) const;


public:
	btConeShape (btScalar radius,btScalar height);
	
	virtual btVector3	localGetSupportingVertex(const btVector3& vec) const;
	virtual btVector3	localGetSupportingVertexWithoutMargin(const btVector3& vec) const;
	virtual void	batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const;

	float getRadius() const { return m_radius;}
	float getHeight() const { return m_height;}


	virtual void	calculateLocalInertia(btScalar mass,btVector3& inertia)
	{
		btTransform identity;
		identity.setIdentity();
		btVector3 aabbMin,aabbMax;
		getAabb(identity,aabbMin,aabbMax);

		btVector3 halfExtents = (aabbMax-aabbMin)*0.5f;

		float margin = getMargin();

		btScalar lx=2.f*(halfExtents.x()+margin);
		btScalar ly=2.f*(halfExtents.y()+margin);
		btScalar lz=2.f*(halfExtents.z()+margin);
		const btScalar x2 = lx*lx;
		const btScalar y2 = ly*ly;
		const btScalar z2 = lz*lz;
		const btScalar scaledmass = mass * 0.08333333f;

		inertia = scaledmass * (btVector3(y2+z2,x2+z2,x2+y2));

//		inertia.x() = scaledmass * (y2+z2);
//		inertia.y() = scaledmass * (x2+z2);
//		inertia.z() = scaledmass * (x2+y2);
	}



		virtual int	getShapeType() const { return CONE_SHAPE_PROXYTYPE; }

		virtual char*	getName()const 
		{
			return "Cone";
		}
};


#endif //CONE_MINKOWSKI_H

