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

#ifndef OBB_BOX_MINKOWSKI_H
#define OBB_BOX_MINKOWSKI_H

#include "PolyhedralConvexShape.h"
#include "NarrowPhaseCollision/CollisionMargin.h"
#include "BroadphaseCollision/BroadphaseProxy.h"
#include "SimdPoint3.h"
#include "SimdMinMax.h"

///BoxShape implements both a feature based (vertex/edge/plane) and implicit (getSupportingVertex) Box
class BoxShape: public PolyhedralConvexShape
{

	SimdVector3	m_boxHalfExtents1;


public:

	virtual ~BoxShape()
	{

	}

	SimdVector3 GetHalfExtents() const;
	//{ return m_boxHalfExtents1 * m_localScaling;}
 	//const SimdVector3& GetHalfExtents() const{ return m_boxHalfExtents1;}


	
	virtual int	GetShapeType() const { return BOX_SHAPE_PROXYTYPE;}

	virtual SimdVector3	LocalGetSupportingVertex(const SimdVector3& vec) const
	{
		
		SimdVector3 halfExtents = GetHalfExtents();
		SimdVector3 margin(GetMargin(),GetMargin(),GetMargin());
		halfExtents -= margin;
		
		SimdVector3 supVertex;
		supVertex = SimdPoint3(vec.x() < SimdScalar(0.0f) ? -halfExtents.x() : halfExtents.x(),
                     vec.y() < SimdScalar(0.0f) ? -halfExtents.y() : halfExtents.y(),
                     vec.z() < SimdScalar(0.0f) ? -halfExtents.z() : halfExtents.z()); 
  
		if ( GetMargin()!=0.f )
		{
			SimdVector3 vecnorm = vec;
			if (vecnorm .length2() == 0.f)
			{
				vecnorm.setValue(-1.f,-1.f,-1.f);
			} 
			vecnorm.normalize();
			supVertex+= GetMargin() * vecnorm;
		}
		return supVertex;
	}


	virtual SimdVector3	LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec)const
	{
		SimdVector3 halfExtents = GetHalfExtents();
		SimdVector3 margin(GetMargin(),GetMargin(),GetMargin());
		halfExtents -= margin;

		return SimdVector3(vec.x() < SimdScalar(0.0f) ? -halfExtents.x() : halfExtents.x(),
                    vec.y() < SimdScalar(0.0f) ? -halfExtents.y() : halfExtents.y(),
                    vec.z() < SimdScalar(0.0f) ? -halfExtents.z() : halfExtents.z()); 
	}


	BoxShape( const SimdVector3& boxHalfExtents) :  m_boxHalfExtents1(boxHalfExtents){};
	
	virtual void GetAabb(const SimdTransform& t,SimdVector3& aabbMin,SimdVector3& aabbMax) const;

	

	virtual void	CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia);

	virtual void GetPlane(SimdVector3& planeNormal,SimdPoint3& planeSupport,int i ) const
	{
		//this plane might not be aligned...
		SimdVector4 plane ;
		GetPlaneEquation(plane,i);
		planeNormal = SimdVector3(plane.getX(),plane.getY(),plane.getZ());
		planeSupport = LocalGetSupportingVertex(-planeNormal);
	}

	
	virtual int GetNumPlanes() const
	{
		return 6;
	}	
	
	virtual int	GetNumVertices() const 
	{
		return 8;
	}

	virtual int GetNumEdges() const
	{
		return 12;
	}


	virtual void GetVertex(int i,SimdVector3& vtx) const
	{
		SimdVector3 halfExtents = GetHalfExtents();

		vtx = SimdVector3(
				halfExtents.x() * (1-(i&1)) - halfExtents.x() * (i&1),
				halfExtents.y() * (1-((i&2)>>1)) - halfExtents.y() * ((i&2)>>1),
				halfExtents.z() * (1-((i&4)>>2)) - halfExtents.z() * ((i&4)>>2));
	}
	

	virtual void	GetPlaneEquation(SimdVector4& plane,int i) const
	{
		SimdVector3 halfExtents = GetHalfExtents();

		switch (i)
		{
		case 0:
			plane.setValue(1.f,0.f,0.f);
			plane[3] = -halfExtents.x();
			break;
		case 1:
			plane.setValue(-1.f,0.f,0.f);
			plane[3] = -halfExtents.x();
			break;
		case 2:
			plane.setValue(0.f,1.f,0.f);
			plane[3] = -halfExtents.y();
			break;
		case 3:
			plane.setValue(0.f,-1.f,0.f);
			plane[3] = -halfExtents.y();
			break;
		case 4:
			plane.setValue(0.f,0.f,1.f);
			plane[3] = -halfExtents.z();
			break;
		case 5:
			plane.setValue(0.f,0.f,-1.f);
			plane[3] = -halfExtents.z();
			break;
		default:
			assert(0);
		}
	}

	
	virtual void GetEdge(int i,SimdPoint3& pa,SimdPoint3& pb) const
	//virtual void GetEdge(int i,Edge& edge) const
	{
		int edgeVert0 = 0;
		int edgeVert1 = 0;

		switch (i)
		{
		case 0:
				edgeVert0 = 0;
				edgeVert1 = 1;
			break;
		case 1:
				edgeVert0 = 0;
				edgeVert1 = 2;
			break;
		case 2:
			edgeVert0 = 1;
			edgeVert1 = 3;

			break;
		case 3:
			edgeVert0 = 2;
			edgeVert1 = 3;
			break;
		case 4:
			edgeVert0 = 0;
			edgeVert1 = 4;
			break;
		case 5:
			edgeVert0 = 1;
			edgeVert1 = 5;

			break;
		case 6:
			edgeVert0 = 2;
			edgeVert1 = 6;
			break;
		case 7:
			edgeVert0 = 3;
			edgeVert1 = 7;
			break;
		case 8:
			edgeVert0 = 4;
			edgeVert1 = 5;
			break;
		case 9:
			edgeVert0 = 4;
			edgeVert1 = 6;
			break;
		case 10:
			edgeVert0 = 5;
			edgeVert1 = 7;
			break;
		case 11:
			edgeVert0 = 6;
			edgeVert1 = 7;
			break;
		default:
			ASSERT(0);

		}

		GetVertex(edgeVert0,pa );
		GetVertex(edgeVert1,pb );
	}




	
	virtual	bool IsInside(const SimdPoint3& pt,SimdScalar tolerance) const
	{
		SimdVector3 halfExtents = GetHalfExtents();

		//SimdScalar minDist = 2*tolerance;
		
		bool result =	(pt.x() <= (halfExtents.x()+tolerance)) &&
						(pt.x() >= (-halfExtents.x()-tolerance)) &&
						(pt.y() <= (halfExtents.y()+tolerance)) &&
						(pt.y() >= (-halfExtents.y()-tolerance)) &&
						(pt.z() <= (halfExtents.z()+tolerance)) &&
						(pt.z() >= (-halfExtents.z()-tolerance));
		
		return result;
	}


	//debugging
	virtual char*	GetName()const
	{
		return "Box";
	}


};

#endif //OBB_BOX_MINKOWSKI_H
