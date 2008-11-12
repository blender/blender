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

#include "btStridingMeshInterface.h"

btStridingMeshInterface::~btStridingMeshInterface()
{

}


void	btStridingMeshInterface::InternalProcessAllTriangles(btInternalTriangleIndexCallback* callback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	(void)aabbMin;
	(void)aabbMax;
	int numtotalphysicsverts = 0;
	int part,graphicssubparts = getNumSubParts();
	const unsigned char * vertexbase;
	const unsigned char * indexbase;
	int indexstride;
	PHY_ScalarType type;
	PHY_ScalarType gfxindextype;
	int stride,numverts,numtriangles;
	int gfxindex;
	btVector3 triangle[3];
	btScalar* graphicsbase;

	btVector3 meshScaling = getScaling();

	///if the number of parts is big, the performance might drop due to the innerloop switch on indextype
	for (part=0;part<graphicssubparts ;part++)
	{
		getLockedReadOnlyVertexIndexBase(&vertexbase,numverts,type,stride,&indexbase,indexstride,numtriangles,gfxindextype,part);
		numtotalphysicsverts+=numtriangles*3; //upper bound

		switch (gfxindextype)
		{
		case PHY_INTEGER:
			{
				for (gfxindex=0;gfxindex<numtriangles;gfxindex++)
				{
					unsigned int* tri_indices= (unsigned int*)(indexbase+gfxindex*indexstride);
					graphicsbase = (btScalar*)(vertexbase+tri_indices[0]*stride);
					triangle[0].setValue(graphicsbase[0]*meshScaling.getX(),graphicsbase[1]*meshScaling.getY(),graphicsbase[2]*meshScaling.getZ());
					graphicsbase = (btScalar*)(vertexbase+tri_indices[1]*stride);
					triangle[1].setValue(graphicsbase[0]*meshScaling.getX(),graphicsbase[1]*meshScaling.getY(),	graphicsbase[2]*meshScaling.getZ());
					graphicsbase = (btScalar*)(vertexbase+tri_indices[2]*stride);
					triangle[2].setValue(graphicsbase[0]*meshScaling.getX(),graphicsbase[1]*meshScaling.getY(),	graphicsbase[2]*meshScaling.getZ());
					callback->internalProcessTriangleIndex(triangle,part,gfxindex);
				}
				break;
			}
		case PHY_SHORT:
			{
				for (gfxindex=0;gfxindex<numtriangles;gfxindex++)
				{
					unsigned short int* tri_indices= (unsigned short int*)(indexbase+gfxindex*indexstride);
					graphicsbase = (btScalar*)(vertexbase+tri_indices[0]*stride);
					triangle[0].setValue(graphicsbase[0]*meshScaling.getX(),graphicsbase[1]*meshScaling.getY(),graphicsbase[2]*meshScaling.getZ());
					graphicsbase = (btScalar*)(vertexbase+tri_indices[1]*stride);
					triangle[1].setValue(graphicsbase[0]*meshScaling.getX(),graphicsbase[1]*meshScaling.getY(),	graphicsbase[2]*meshScaling.getZ());
					graphicsbase = (btScalar*)(vertexbase+tri_indices[2]*stride);
					triangle[2].setValue(graphicsbase[0]*meshScaling.getX(),graphicsbase[1]*meshScaling.getY(),	graphicsbase[2]*meshScaling.getZ());
					callback->internalProcessTriangleIndex(triangle,part,gfxindex);
				}
				break;
			}
		default:
			btAssert((gfxindextype == PHY_INTEGER) || (gfxindextype == PHY_SHORT));
		}

		unLockReadOnlyVertexBase(part);
	}
}

void	btStridingMeshInterface::calculateAabbBruteForce(btVector3& aabbMin,btVector3& aabbMax)
{

	struct	AabbCalculationCallback : public btInternalTriangleIndexCallback
	{
		btVector3	m_aabbMin;
		btVector3	m_aabbMax;

		AabbCalculationCallback()
		{
			m_aabbMin.setValue(btScalar(1e30),btScalar(1e30),btScalar(1e30));
			m_aabbMax.setValue(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30));
		}

		virtual void internalProcessTriangleIndex(btVector3* triangle,int partId,int  triangleIndex)
		{
			(void)partId;
			(void)triangleIndex;

			m_aabbMin.setMin(triangle[0]);
			m_aabbMax.setMax(triangle[0]);
			m_aabbMin.setMin(triangle[1]);
			m_aabbMax.setMax(triangle[1]);
			m_aabbMin.setMin(triangle[2]);
			m_aabbMax.setMax(triangle[2]);
		}
	};

		//first calculate the total aabb for all triangles
	AabbCalculationCallback	aabbCallback;
	aabbMin.setValue(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30));
	aabbMax.setValue(btScalar(1e30),btScalar(1e30),btScalar(1e30));
	InternalProcessAllTriangles(&aabbCallback,aabbMin,aabbMax);

	aabbMin = aabbCallback.m_aabbMin;
	aabbMax = aabbCallback.m_aabbMax;
}
