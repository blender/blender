/*
Copyright (c) 2003-2006 Gino van den Bergen / Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/



#include "btGeometryUtil.h"

bool	btGeometryUtil::isPointInsidePlanes(const btAlignedObjectArray<btVector3>& planeEquations, const btVector3& point, float	margin)
{
	int numbrushes = planeEquations.size();
	for (int i=0;i<numbrushes;i++)
	{
		const btVector3& N1 = planeEquations[i];
		float dist = float(N1.dot(point))+float(N1[3])-margin;
		if (dist>0.f)
		{
			return false;
		}
	}
	return true;
		
}


bool	btGeometryUtil::areVerticesBehindPlane(const btVector3& planeNormal, const btAlignedObjectArray<btVector3>& vertices, float	margin)
{
	int numvertices = vertices.size();
	for (int i=0;i<numvertices;i++)
	{
		const btVector3& N1 = vertices[i];
		float dist = float(planeNormal.dot(N1))+float(planeNormal[3])-margin;
		if (dist>0.f)
		{
			return false;
		}
	}
	return true;
}

bool notExist(const btVector3& planeEquation,const btAlignedObjectArray<btVector3>& planeEquations)
{
	int numbrushes = planeEquations.size();
	for (int i=0;i<numbrushes;i++)
	{
		const btVector3& N1 = planeEquations[i];
		if (planeEquation.dot(N1) > 0.999f)
		{
			return false;
		} 
	}
	return true;
}

void	btGeometryUtil::getPlaneEquationsFromVertices(btAlignedObjectArray<btVector3>& vertices, btAlignedObjectArray<btVector3>& planeEquationsOut )
{
		const int numvertices = vertices.size();
	// brute force:
	for (int i=0;i<numvertices;i++)
	{
		const btVector3& N1 = vertices[i];
		

		for (int j=i+1;j<numvertices;j++)
		{
			const btVector3& N2 = vertices[j];
				
			for (int k=j+1;k<numvertices;k++)
			{

				const btVector3& N3 = vertices[k];

				btVector3 planeEquation,edge0,edge1;
				edge0 = N2-N1;
				edge1 = N3-N1;
				float normalSign = 1.f;
				for (int ww=0;ww<2;ww++)
				{
					planeEquation = normalSign * edge0.cross(edge1);
					if (planeEquation.length2() > 0.0001f)
					{
						planeEquation.normalize();
						if (notExist(planeEquation,planeEquationsOut))
						{
							planeEquation[3] = -planeEquation.dot(N1);
							
								//check if inside, and replace supportingVertexOut if needed
								if (areVerticesBehindPlane(planeEquation,vertices,0.01f))
								{
									planeEquationsOut.push_back(planeEquation);
								}
						}
					}
					normalSign = -1.f;
				}
			
			}
		}
	}

}

void	btGeometryUtil::getVerticesFromPlaneEquations(const btAlignedObjectArray<btVector3>& planeEquations , btAlignedObjectArray<btVector3>& verticesOut )
{
	const int numbrushes = planeEquations.size();
	// brute force:
	for (int i=0;i<numbrushes;i++)
	{
		const btVector3& N1 = planeEquations[i];
		

		for (int j=i+1;j<numbrushes;j++)
		{
			const btVector3& N2 = planeEquations[j];
				
			for (int k=j+1;k<numbrushes;k++)
			{

				const btVector3& N3 = planeEquations[k];

				btVector3 n2n3; n2n3 = N2.cross(N3);
				btVector3 n3n1; n3n1 = N3.cross(N1);
				btVector3 n1n2; n1n2 = N1.cross(N2);
				
				if ( ( n2n3.length2() > 0.0001f ) &&
					 ( n3n1.length2() > 0.0001f ) &&
					 ( n1n2.length2() > 0.0001f ) )
				{
					//point P out of 3 plane equations:

					//	d1 ( N2 * N3 ) + d2 ( N3 * N1 ) + d3 ( N1 * N2 )  
					//P =  -------------------------------------------------------------------------  
					//   N1 . ( N2 * N3 )  


					float quotient = (N1.dot(n2n3));
					if (btFabs(quotient) > 0.000001f)
					{
						quotient = -1.f / quotient;
						n2n3 *= N1[3];
						n3n1 *= N2[3];
						n1n2 *= N3[3];
						btVector3 potentialVertex = n2n3;
						potentialVertex += n3n1;
						potentialVertex += n1n2;
						potentialVertex *= quotient;

						//check if inside, and replace supportingVertexOut if needed
						if (isPointInsidePlanes(planeEquations,potentialVertex,0.01f))
						{
							verticesOut.push_back(potentialVertex);
						}
					}
				}
			}
		}
	}
}

