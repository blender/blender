
#ifndef BT_CONVEX_INTERNAL_SHAPE_H
#define BT_CONVEX_INTERNAL_SHAPE_H

#include "btConvexShape.h"

///btConvexInternalShape carries some additional data, shared by most implementations
class btConvexInternalShape : public btConvexShape
{

	protected:

	//local scaling. collisionMargin is not scaled !
	btVector3	m_localScaling;

	btVector3	m_implicitShapeDimensions;
	
	btScalar	m_collisionMargin;

	btScalar	m_padding[2];




public:

	btConvexInternalShape();

	virtual ~btConvexInternalShape()
	{

	}


	virtual btVector3	localGetSupportingVertex(const btVector3& vec)const;
#ifndef __SPU__
	virtual btVector3	localGetSupportingVertexWithoutMargin(const btVector3& vec) const= 0;
	
	//notice that the vectors should be unit length
	virtual void	batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const= 0;
#endif //#ifndef __SPU__

	const btVector3& getImplicitShapeDimensions() const
	{
		return m_implicitShapeDimensions;
	}

	///getAabb's default implementation is brute force, expected derived classes to implement a fast dedicated version
	void getAabb(const btTransform& t,btVector3& aabbMin,btVector3& aabbMax) const
	{
		getAabbSlow(t,aabbMin,aabbMax);
	}


	
	virtual void getAabbSlow(const btTransform& t,btVector3& aabbMin,btVector3& aabbMax) const;


	virtual void	setLocalScaling(const btVector3& scaling);
	virtual const btVector3& getLocalScaling() const 
	{
		return m_localScaling;
	}

	const btVector3& getLocalScalingNV() const 
	{
		return m_localScaling;
	}

	virtual void	setMargin(btScalar margin)
	{
		m_collisionMargin = margin;
	}
	virtual btScalar	getMargin() const
	{
		return m_collisionMargin;
	}

	btScalar	getMarginNV() const
	{
		return m_collisionMargin;
	}

	virtual int		getNumPreferredPenetrationDirections() const
	{
		return 0;
	}
	
	virtual void	getPreferredPenetrationDirection(int index, btVector3& penetrationVector) const
	{
		(void)penetrationVector;
		(void)index;
		btAssert(0);
	}

};


#endif //BT_CONVEX_INTERNAL_SHAPE_H
