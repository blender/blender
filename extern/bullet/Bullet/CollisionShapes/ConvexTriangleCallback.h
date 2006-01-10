#ifndef CONVEX_TRIANGLE_CALLBACK_H
#define CONVEX_TRIANGLE_CALLBACK_H

#include "TriangleCallback.h"
class ConvexShape;
class PersistentManifold;
#include "SimdTransform.h"
///ConvexTriangleCallback processes the narrowphase convex-triangle collision detection
class ConvexTriangleCallback: public TriangleCallback
{
	SimdVector3	m_aabbMin;
	SimdVector3	m_aabbMax ;

	SimdTransform	m_triangleMeshTransform;
	SimdTransform	m_convexTransform;

//	bool m_useContinuous;
	float m_collisionMarginTriangle;
	
public:
int	m_triangleCount;
	
	ConvexShape*	m_convexShape;

	PersistentManifold*	m_manifoldPtr;

	ConvexTriangleCallback(PersistentManifold* manifold,ConvexShape* convexShape,const SimdTransform&convexTransform,const SimdTransform& triangleMeshTransform);

	void	Update(float collisionMarginTriangle);

	virtual ~ConvexTriangleCallback();

	virtual void ProcessTriangle(SimdVector3* triangle);
	
	void ClearCache();

	inline const SimdVector3& GetAabbMin() const
	{
		return m_aabbMin;
	}
	inline const SimdVector3& GetAabbMax() const
	{
		return m_aabbMax;
	}

};


#endif //CONVEX_TRIANGLE_CALLBACK_H