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
#include "btConvexTriangleMeshShape.h"
#include "BulletCollision/CollisionShapes/btCollisionMargin.h"

#include "LinearMath/btQuaternion.h"
#include "BulletCollision/CollisionShapes/btStridingMeshInterface.h"


btConvexTriangleMeshShape ::btConvexTriangleMeshShape (btStridingMeshInterface* meshInterface)
:m_stridingMesh(meshInterface)
{
	recalcLocalAabb();
}




///It's not nice to have all this virtual function overhead, so perhaps we can also gather the points once
///but then we are duplicating
class LocalSupportVertexCallback: public btInternalTriangleIndexCallback
{

	btVector3 m_supportVertexLocal;
public:

	btScalar m_maxDot;
	btVector3 m_supportVecLocal;

	LocalSupportVertexCallback(const btVector3& supportVecLocal)
		: m_supportVertexLocal(btScalar(0.),btScalar(0.),btScalar(0.)),
		m_maxDot(btScalar(-1e30)),
                m_supportVecLocal(supportVecLocal)
	{
	}

	virtual void internalProcessTriangleIndex(btVector3* triangle,int partId,int  triangleIndex)
	{
		(void)triangleIndex;
		(void)partId;

		for (int i=0;i<3;i++)
		{
			btScalar dot = m_supportVecLocal.dot(triangle[i]);
			if (dot > m_maxDot)
			{
				m_maxDot = dot;
				m_supportVertexLocal = triangle[i];
			}
		}
	}
	
	btVector3	GetSupportVertexLocal()
	{
		return m_supportVertexLocal;
	}

};





btVector3	btConvexTriangleMeshShape::localGetSupportingVertexWithoutMargin(const btVector3& vec0)const
{
	btVector3 supVec(btScalar(0.),btScalar(0.),btScalar(0.));

	btVector3 vec = vec0;
	btScalar lenSqr = vec.length2();
	if (lenSqr < btScalar(0.0001))
	{
		vec.setValue(1,0,0);
	} else
	{
		btScalar rlen = btScalar(1.) / btSqrt(lenSqr );
		vec *= rlen;
	}

	LocalSupportVertexCallback	supportCallback(vec);
	btVector3 aabbMax(btScalar(1e30),btScalar(1e30),btScalar(1e30));
	m_stridingMesh->InternalProcessAllTriangles(&supportCallback,-aabbMax,aabbMax);
	supVec = supportCallback.GetSupportVertexLocal();

	return supVec;
}

void	btConvexTriangleMeshShape::batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const
{
	//use 'w' component of supportVerticesOut?
	{
		for (int i=0;i<numVectors;i++)
		{
			supportVerticesOut[i][3] = btScalar(-1e30);
		}
	}
	
	//todo: could do the batch inside the callback!


	for (int j=0;j<numVectors;j++)
	{
		const btVector3& vec = vectors[j];
		LocalSupportVertexCallback	supportCallback(vec);
		btVector3 aabbMax(btScalar(1e30),btScalar(1e30),btScalar(1e30));
		m_stridingMesh->InternalProcessAllTriangles(&supportCallback,-aabbMax,aabbMax);
		supportVerticesOut[j] = supportCallback.GetSupportVertexLocal();
	}
	
}
	


btVector3	btConvexTriangleMeshShape::localGetSupportingVertex(const btVector3& vec)const
{
	btVector3 supVertex = localGetSupportingVertexWithoutMargin(vec);

	if ( getMargin()!=btScalar(0.) )
	{
		btVector3 vecnorm = vec;
		if (vecnorm .length2() < (SIMD_EPSILON*SIMD_EPSILON))
		{
			vecnorm.setValue(btScalar(-1.),btScalar(-1.),btScalar(-1.));
		} 
		vecnorm.normalize();
		supVertex+= getMargin() * vecnorm;
	}
	return supVertex;
}









//currently just for debugging (drawing), perhaps future support for algebraic continuous collision detection
//Please note that you can debug-draw btConvexTriangleMeshShape with the Raytracer Demo
int	btConvexTriangleMeshShape::getNumVertices() const
{
	//cache this?
	return 0;
	
}

int btConvexTriangleMeshShape::getNumEdges() const
{
	return 0;
}

void btConvexTriangleMeshShape::getEdge(int ,btPoint3& ,btPoint3& ) const
{
	btAssert(0);	
}

void btConvexTriangleMeshShape::getVertex(int ,btPoint3& ) const
{
	btAssert(0);
}

int	btConvexTriangleMeshShape::getNumPlanes() const
{
	return 0;
}

void btConvexTriangleMeshShape::getPlane(btVector3& ,btPoint3& ,int  ) const
{
	btAssert(0);
}

//not yet
bool btConvexTriangleMeshShape::isInside(const btPoint3& ,btScalar ) const
{
	btAssert(0);
	return false;
}



void	btConvexTriangleMeshShape::setLocalScaling(const btVector3& scaling)
{
	m_stridingMesh->setScaling(scaling);
	
	recalcLocalAabb();
	
}


const btVector3& btConvexTriangleMeshShape::getLocalScaling() const
{
	return m_stridingMesh->getScaling();
}
