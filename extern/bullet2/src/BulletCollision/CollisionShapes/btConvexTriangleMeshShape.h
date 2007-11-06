#ifndef CONVEX_TRIANGLEMESH_SHAPE_H
#define CONVEX_TRIANGLEMESH_SHAPE_H


#include "btPolyhedralConvexShape.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h" // for the types


/// btConvexTriangleMeshShape is a convex hull of a triangle mesh. If you just have a point cloud, you can use btConvexHullShape instead.
/// It uses the btStridingMeshInterface instead of a point cloud. This can avoid the duplication of the triangle mesh data.
class btConvexTriangleMeshShape : public btPolyhedralConvexShape
{

	class btStridingMeshInterface*	m_stridingMesh;

public:
	btConvexTriangleMeshShape(btStridingMeshInterface* meshInterface);

	class btStridingMeshInterface*	getMeshInterface()
	{
		return m_stridingMesh;
	}
	const class btStridingMeshInterface* getMeshInterface() const
	{
		return m_stridingMesh;
	}
	
	virtual btVector3	localGetSupportingVertex(const btVector3& vec)const;
	virtual btVector3	localGetSupportingVertexWithoutMargin(const btVector3& vec)const;
	virtual void	batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const;
	
	virtual int	getShapeType()const { return CONVEX_TRIANGLEMESH_SHAPE_PROXYTYPE; }

	//debugging
	virtual const char*	getName()const {return "ConvexTrimesh";}
	
	virtual int	getNumVertices() const;
	virtual int getNumEdges() const;
	virtual void getEdge(int i,btPoint3& pa,btPoint3& pb) const;
	virtual void getVertex(int i,btPoint3& vtx) const;
	virtual int	getNumPlanes() const;
	virtual void getPlane(btVector3& planeNormal,btPoint3& planeSupport,int i ) const;
	virtual	bool isInside(const btPoint3& pt,btScalar tolerance) const;

	
	virtual void	setLocalScaling(const btVector3& scaling);
	virtual const btVector3& getLocalScaling() const;

};



#endif //CONVEX_TRIANGLEMESH_SHAPE_H


