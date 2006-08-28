#ifndef CONVEX_TRIANGLEMESH_SHAPE_H
#define CONVEX_TRIANGLEMESH_SHAPE_H


#include "PolyhedralConvexShape.h"
#include "BroadphaseCollision/BroadphaseProxy.h" // for the types

#include <vector>

/// ConvexTriangleMeshShape is a convex hull of a triangle mesh. If you just have a point cloud, you can use ConvexHullShape instead.
/// It uses the StridingMeshInterface instead of a point cloud. This can avoid the duplication of the triangle mesh data.
class ConvexTriangleMeshShape : public PolyhedralConvexShape
{

	class StridingMeshInterface*	m_stridingMesh;

public:
	ConvexTriangleMeshShape(StridingMeshInterface* meshInterface);

	class StridingMeshInterface*	GetStridingMesh()
	{
		return m_stridingMesh;
	}
	
	virtual SimdVector3	LocalGetSupportingVertex(const SimdVector3& vec)const;
	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const;
	virtual void	BatchedUnitVectorGetSupportingVertexWithoutMargin(const SimdVector3* vectors,SimdVector3* supportVerticesOut,int numVectors) const;
	
	virtual int	GetShapeType()const { return CONVEX_TRIANGLEMESH_SHAPE_PROXYTYPE; }

	//debugging
	virtual char*	GetName()const {return "ConvexTrimesh";}
	
	virtual int	GetNumVertices() const;
	virtual int GetNumEdges() const;
	virtual void GetEdge(int i,SimdPoint3& pa,SimdPoint3& pb) const;
	virtual void GetVertex(int i,SimdPoint3& vtx) const;
	virtual int	GetNumPlanes() const;
	virtual void GetPlane(SimdVector3& planeNormal,SimdPoint3& planeSupport,int i ) const;
	virtual	bool IsInside(const SimdPoint3& pt,SimdScalar tolerance) const;

	
	void	setLocalScaling(const SimdVector3& scaling);

};



#endif //CONVEX_TRIANGLEMESH_SHAPE_H