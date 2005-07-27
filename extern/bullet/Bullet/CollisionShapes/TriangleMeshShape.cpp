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
#include "TriangleMeshShape.h"
#include "SimdVector3.h"
#include "SimdQuaternion.h"
#include "StridingMeshInterface.h"
#include "AabbUtil2.h"



TriangleMeshShape::TriangleMeshShape(StridingMeshInterface* meshInterface)
: m_meshInterface(meshInterface),
m_collisionMargin(0.1f)
{
}

TriangleMeshShape::~TriangleMeshShape()
{
		
}




void TriangleMeshShape::GetAabb(const SimdTransform& trans,SimdVector3& aabbMin,SimdVector3& aabbMax) const
{

	for (int i=0;i<3;i++)
	{
		SimdVector3 vec(0.f,0.f,0.f);
		vec[i] = 1.f;
		SimdVector3 tmp = trans(LocalGetSupportingVertex(vec*trans.getBasis()));
		aabbMax[i] = tmp[i]+m_collisionMargin;
		vec[i] = -1.f;
		tmp = trans(LocalGetSupportingVertex(vec*trans.getBasis()));
		aabbMin[i] = tmp[i]-m_collisionMargin;
	}
}


TriangleCallback::~TriangleCallback()
{

}

class SupportVertexCallback : public TriangleCallback
{

	SimdVector3 m_supportVertexLocal;
public:

	SimdTransform	m_worldTrans;
	SimdScalar m_maxDot;
	SimdVector3 m_supportVecLocal;

	SupportVertexCallback(const SimdVector3& supportVecWorld,const SimdTransform& trans)
		: m_supportVertexLocal(0.f,0.f,0.f), m_worldTrans(trans) ,m_maxDot(-1e30f)
		
	{
		m_supportVecLocal = supportVecWorld * m_worldTrans.getBasis();
	}

	virtual void ProcessTriangle( SimdVector3* triangle)
	{
		for (int i=0;i<3;i++)
		{
			SimdScalar dot = m_supportVecLocal.dot(triangle[i]);
			if (dot > m_maxDot)
			{
				m_maxDot = dot;
				m_supportVertexLocal = triangle[i];
			}
		}
	}

	SimdVector3 GetSupportVertexWorldSpace()
	{
		return m_worldTrans(m_supportVertexLocal);
	}

	SimdVector3	GetSupportVertexLocal()
	{
		return m_supportVertexLocal;
	}

};

	
void TriangleMeshShape::setLocalScaling(const SimdVector3& scaling)
{
	m_meshInterface->setScaling(scaling);
}

void	TriangleMeshShape::ProcessAllTriangles(TriangleCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
{

	SimdVector3 meshScaling = m_meshInterface->getScaling();
	int numtotalphysicsverts = 0;
	int part,graphicssubparts = m_meshInterface->getNumSubParts();
	for (part=0;part<graphicssubparts ;part++)
	{
		unsigned char * vertexbase;
		unsigned char * indexbase;
		int indexstride;
		PHY_ScalarType type;
		PHY_ScalarType gfxindextype;
		int stride,numverts,numtriangles;
		m_meshInterface->getLockedVertexIndexBase(&vertexbase,numverts,type,stride,&indexbase,indexstride,numtriangles,gfxindextype,part);
		numtotalphysicsverts+=numtriangles*3; //upper bound

	
		int gfxindex;
		SimdVector3 triangle[3];

		for (gfxindex=0;gfxindex<numtriangles;gfxindex++)
		{
		
			int	graphicsindex=0;

			for (int j=2;j>=0;j--)
			{
				ASSERT(gfxindextype == PHY_INTEGER);
				int* gfxbase = (int*)(indexbase+gfxindex*indexstride);
				graphicsindex = gfxbase[j];
				float* graphicsbase = (float*)(vertexbase+graphicsindex*stride);

				triangle[j] = SimdVector3(
					graphicsbase[0]*meshScaling.getX(),
					graphicsbase[1]*meshScaling.getY(),
					graphicsbase[2]*meshScaling.getZ());
			}

			if (TestTriangleAgainstAabb2(&triangle[0],aabbMin,aabbMax))
			{
				//check aabb in triangle-space, before doing this
				callback->ProcessTriangle(triangle);
			}
			
		}
		
		m_meshInterface->unLockVertexBase(part);
	}


}





void	TriangleMeshShape::CalculateLocalInertia(SimdScalar mass,SimdVector3& inertia)
{
	//moving concave objects not supported
	assert(0);
	inertia.setValue(0.f,0.f,0.f);
}


SimdVector3 TriangleMeshShape::LocalGetSupportingVertex(const SimdVector3& vec) const
{
	SimdVector3 supportVertex;

	SimdTransform ident;
	ident.setIdentity();

	SupportVertexCallback supportCallback(vec,ident);

	SimdVector3 aabbMax(1e30f,1e30f,1e30f);
	
	ProcessAllTriangles(&supportCallback,-aabbMax,aabbMax);
		
	supportVertex = supportCallback.GetSupportVertexLocal();

	return supportVertex;
}
