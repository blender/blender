/*
 * Copyright (c) 2005 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#include "StridingMeshInterface.h"

StridingMeshInterface::~StridingMeshInterface()
{

}


void	StridingMeshInterface::InternalProcessAllTriangles(InternalTriangleIndexCallback* callback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
{

	SimdVector3 meshScaling = getScaling();

	int numtotalphysicsverts = 0;
	int part,graphicssubparts = getNumSubParts();
	for (part=0;part<graphicssubparts ;part++)
	{
		const unsigned char * vertexbase;
		const unsigned char * indexbase;
		int indexstride;
		PHY_ScalarType type;
		PHY_ScalarType gfxindextype;
		int stride,numverts,numtriangles;
		getLockedReadOnlyVertexIndexBase(&vertexbase,numverts,type,stride,&indexbase,indexstride,numtriangles,gfxindextype,part);

		numtotalphysicsverts+=numtriangles*3; //upper bound

	
		int gfxindex;
		SimdVector3 triangle[3];

		for (gfxindex=0;gfxindex<numtriangles;gfxindex++)
		{
		
			int	graphicsindex=0;

#ifdef DEBUG_TRIANGLE_MESH
			printf("triangle indices:\n");
#endif //DEBUG_TRIANGLE_MESH
			ASSERT(gfxindextype == PHY_INTEGER);
			int* gfxbase = (int*)(indexbase+gfxindex*indexstride);

			for (int j=2;j>=0;j--)
			{
				
				graphicsindex = gfxbase[j];
#ifdef DEBUG_TRIANGLE_MESH
				printf("%d ,",graphicsindex);
#endif //DEBUG_TRIANGLE_MESH
				float* graphicsbase = (float*)(vertexbase+graphicsindex*stride);

				triangle[j] = SimdVector3(
					graphicsbase[0]*meshScaling.getX(),
					graphicsbase[1]*meshScaling.getY(),
					graphicsbase[2]*meshScaling.getZ());
#ifdef DEBUG_TRIANGLE_MESH
				printf("triangle vertices:%f,%f,%f\n",triangle[j].x(),triangle[j].y(),triangle[j].z());
#endif //DEBUG_TRIANGLE_MESH
			}

			
			//check aabb in triangle-space, before doing this
			callback->InternalProcessTriangleIndex(triangle,part,gfxindex);
			
		}
		
		unLockReadOnlyVertexBase(part);
	}
}

