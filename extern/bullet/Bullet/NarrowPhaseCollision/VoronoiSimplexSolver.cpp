
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
	
	Elsevier CDROM license agreements grants nonexclusive license to use the software
	for any purpose, commercial or non-commercial as long as the following credit is included
	identifying the original source of the software:

	Parts of the source are "from the book Real-Time Collision Detection by
	Christer Ericson, published by Morgan Kaufmann Publishers,
	(c) 2005 Elsevier Inc."
		
*/


#include "VoronoiSimplexSolver.h"
#include <assert.h>
#include <stdio.h>

#define VERTA  0
#define VERTB  1
#define VERTC  2
#define VERTD  3

#define CATCH_DEGENERATE_TETRAHEDRON 1
void	VoronoiSimplexSolver::removeVertex(int index)
{
	
	assert(m_numVertices>0);
	m_numVertices--;
	m_simplexVectorW[index] = m_simplexVectorW[m_numVertices];
	m_simplexPointsP[index] = m_simplexPointsP[m_numVertices];
	m_simplexPointsQ[index] = m_simplexPointsQ[m_numVertices];
}

void	VoronoiSimplexSolver::ReduceVertices (const UsageBitfield& usedVerts)
{
	if ((numVertices() >= 4) && (!usedVerts.usedVertexD))
		removeVertex(3);

	if ((numVertices() >= 3) && (!usedVerts.usedVertexC))
		removeVertex(2);

	if ((numVertices() >= 2) && (!usedVerts.usedVertexB))
		removeVertex(1);
	
	if ((numVertices() >= 1) && (!usedVerts.usedVertexA))
		removeVertex(0);

}





//clear the simplex, remove all the vertices
void VoronoiSimplexSolver::reset()
{
	m_cachedValidClosest = false;
	m_numVertices = 0;
	m_needsUpdate = true;
	m_lastW = SimdVector3(1e30f,1e30f,1e30f);
	m_cachedBC.Reset();
}



	//add a vertex
void VoronoiSimplexSolver::addVertex(const SimdVector3& w, const SimdPoint3& p, const SimdPoint3& q)
{
	m_lastW = w;
	m_needsUpdate = true;

	m_simplexVectorW[m_numVertices] = w;
	m_simplexPointsP[m_numVertices] = p;
	m_simplexPointsQ[m_numVertices] = q;

	m_numVertices++;
}

bool	VoronoiSimplexSolver::UpdateClosestVectorAndPoints()
{
	
	if (m_needsUpdate)
	{
		m_cachedBC.Reset();

		m_needsUpdate = false;

		switch (numVertices())
		{
		case 0:
				m_cachedValidClosest = false;
				break;
		case 1:
			{
				m_cachedP1 = m_simplexPointsP[0];
				m_cachedP2 = m_simplexPointsQ[0];
				m_cachedV = m_cachedP1-m_cachedP2; //== m_simplexVectorW[0]
				m_cachedBC.Reset();
				m_cachedBC.SetBarycentricCoordinates(1.f,0.f,0.f,0.f);
				m_cachedValidClosest = m_cachedBC.IsValid();
				break;
			};
		case 2:
			{
			//closest point origin from line segment
					const SimdVector3& from = m_simplexVectorW[0];
					const SimdVector3& to = m_simplexVectorW[1];
					SimdVector3 nearest;

					SimdVector3 p (0.f,0.f,0.f);
					SimdVector3 diff = p - from;
					SimdVector3 v = to - from;
					float t = v.dot(diff);
					
					if (t > 0) {
						float dotVV = v.dot(v);
						if (t < dotVV) {
							t /= dotVV;
							diff -= t*v;
							m_cachedBC.m_usedVertices.usedVertexA = true;
							m_cachedBC.m_usedVertices.usedVertexB = true;
						} else {
							t = 1;
							diff -= v;
							//reduce to 1 point
							m_cachedBC.m_usedVertices.usedVertexB = true;
						}
					} else
					{
						t = 0;
						//reduce to 1 point
						m_cachedBC.m_usedVertices.usedVertexA = true;
					}
					m_cachedBC.SetBarycentricCoordinates(1-t,t);
					nearest = from + t*v;

					m_cachedP1 = m_simplexPointsP[0] + t * (m_simplexPointsP[1] - m_simplexPointsP[0]);
					m_cachedP2 = m_simplexPointsQ[0] + t * (m_simplexPointsQ[1] - m_simplexPointsQ[0]);
					m_cachedV = m_cachedP1 - m_cachedP2;
					
					ReduceVertices(m_cachedBC.m_usedVertices);

					m_cachedValidClosest = m_cachedBC.IsValid();
					break;
			}
		case 3:
			{
				//closest point origin from triangle
				SimdVector3 p (0.f,0.f,0.f);
				
				const SimdVector3& a = m_simplexVectorW[0];
				const SimdVector3& b = m_simplexVectorW[1];
				const SimdVector3& c = m_simplexVectorW[2];

				ClosestPtPointTriangle(p,a,b,c,m_cachedBC);
				m_cachedP1 = m_simplexPointsP[0] * m_cachedBC.m_barycentricCoords[0] +
								m_simplexPointsP[1] * m_cachedBC.m_barycentricCoords[1] +
								m_simplexPointsP[2] * m_cachedBC.m_barycentricCoords[2] +
								m_simplexPointsP[3] * m_cachedBC.m_barycentricCoords[3];

				m_cachedP2 = m_simplexPointsQ[0] * m_cachedBC.m_barycentricCoords[0] +
					m_simplexPointsQ[1] * m_cachedBC.m_barycentricCoords[1] +
					m_simplexPointsQ[2] * m_cachedBC.m_barycentricCoords[2] +
					m_simplexPointsQ[3] * m_cachedBC.m_barycentricCoords[3];

				m_cachedV = m_cachedP1-m_cachedP2;

				ReduceVertices (m_cachedBC.m_usedVertices);
				m_cachedValidClosest =  m_cachedBC.IsValid();

				break;
			}
		case 4:
			{

				
				SimdVector3 p (0.f,0.f,0.f);
				
				const SimdVector3& a = m_simplexVectorW[0];
				const SimdVector3& b = m_simplexVectorW[1];
				const SimdVector3& c = m_simplexVectorW[2];
				const SimdVector3& d = m_simplexVectorW[3];

				bool hasSeperation = ClosestPtPointTetrahedron(p,a,b,c,d,m_cachedBC);

				if (hasSeperation)
				{

					m_cachedP1 = m_simplexPointsP[0] * m_cachedBC.m_barycentricCoords[0] +
						m_simplexPointsP[1] * m_cachedBC.m_barycentricCoords[1] +
						m_simplexPointsP[2] * m_cachedBC.m_barycentricCoords[2] +
						m_simplexPointsP[3] * m_cachedBC.m_barycentricCoords[3];

					m_cachedP2 = m_simplexPointsQ[0] * m_cachedBC.m_barycentricCoords[0] +
						m_simplexPointsQ[1] * m_cachedBC.m_barycentricCoords[1] +
						m_simplexPointsQ[2] * m_cachedBC.m_barycentricCoords[2] +
						m_simplexPointsQ[3] * m_cachedBC.m_barycentricCoords[3];

					m_cachedV = m_cachedP1-m_cachedP2;
					ReduceVertices (m_cachedBC.m_usedVertices);
				} else
				{
//					printf("sub distance got penetration\n");

					if (m_cachedBC.m_degenerate)
					{
						m_cachedValidClosest = false;
					} else
					{
						m_cachedValidClosest = true;
						//degenerate case == false, penetration = true + zero
						m_cachedV.setValue(0.f,0.f,0.f);
					}
					break;
				}

				m_cachedValidClosest = m_cachedBC.IsValid();

				//closest point origin from tetrahedron
				break;
			}
		default:
			{
				m_cachedValidClosest = false;
			}
		};
	}

	return m_cachedValidClosest;

}

//return/calculate the closest vertex
bool VoronoiSimplexSolver::closest(SimdVector3& v)
{
	bool succes = UpdateClosestVectorAndPoints();
	v = m_cachedV;
	return succes;
}



SimdScalar VoronoiSimplexSolver::maxVertex()
{
	int i, numverts = numVertices();
	SimdScalar maxV = 0.f;
	for (i=0;i<numverts;i++)
	{
		SimdScalar curLen2 = m_simplexVectorW[i].length2();
		if (maxV < curLen2)
			maxV = curLen2;
	}
	return maxV;
}



	//return the current simplex
int VoronoiSimplexSolver::getSimplex(SimdPoint3 *pBuf, SimdPoint3 *qBuf, SimdVector3 *yBuf) const
{
	int i;
	for (i=0;i<numVertices();i++)
	{
		yBuf[i] = m_simplexVectorW[i];
		pBuf[i] = m_simplexPointsP[i];
		qBuf[i] = m_simplexPointsQ[i];
	}
	return numVertices();
}




bool VoronoiSimplexSolver::inSimplex(const SimdVector3& w)
{
	bool found = false;
	int i, numverts = numVertices();
	//SimdScalar maxV = 0.f;
	
	//w is in the current (reduced) simplex
	for (i=0;i<numverts;i++)
	{
		if (m_simplexVectorW[i] == w)
			found = true;
	}

	//check in case lastW is already removed
	if (w == m_lastW)
		return true;
    	
	return found;
}

void VoronoiSimplexSolver::backup_closest(SimdVector3& v) 
{
	v = m_cachedV;
}


bool VoronoiSimplexSolver::emptySimplex() const 
{
	return (numVertices() == 0);

}

void VoronoiSimplexSolver::compute_points(SimdPoint3& p1, SimdPoint3& p2) 
{
	UpdateClosestVectorAndPoints();
	p1 = m_cachedP1;
	p2 = m_cachedP2;

}




bool	VoronoiSimplexSolver::ClosestPtPointTriangle(const SimdPoint3& p, const SimdPoint3& a, const SimdPoint3& b, const SimdPoint3& c,SubSimplexClosestResult& result)
{
	result.m_usedVertices.reset();

    // Check if P in vertex region outside A
    SimdVector3 ab = b - a;
    SimdVector3 ac = c - a;
    SimdVector3 ap = p - a;
    float d1 = ab.dot(ap);
    float d2 = ac.dot(ap);
    if (d1 <= 0.0f && d2 <= 0.0f) 
	{
		result.m_closestPointOnSimplex = a;
		result.m_usedVertices.usedVertexA = true;
		result.SetBarycentricCoordinates(1,0,0);
		return true;// a; // barycentric coordinates (1,0,0)
	}

    // Check if P in vertex region outside B
    SimdVector3 bp = p - b;
    float d3 = ab.dot(bp);
    float d4 = ac.dot(bp);
    if (d3 >= 0.0f && d4 <= d3) 
	{
		result.m_closestPointOnSimplex = b;
		result.m_usedVertices.usedVertexB = true;
		result.SetBarycentricCoordinates(0,1,0);

		return true; // b; // barycentric coordinates (0,1,0)
	}
    // Check if P in edge region of AB, if so return projection of P onto AB
    float vc = d1*d4 - d3*d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
		result.m_closestPointOnSimplex = a + v * ab;
		result.m_usedVertices.usedVertexA = true;
		result.m_usedVertices.usedVertexB = true;
		result.SetBarycentricCoordinates(1-v,v,0);
		return true;
        //return a + v * ab; // barycentric coordinates (1-v,v,0)
    }

    // Check if P in vertex region outside C
    SimdVector3 cp = p - c;
    float d5 = ab.dot(cp);
    float d6 = ac.dot(cp);
    if (d6 >= 0.0f && d5 <= d6) 
	{
		result.m_closestPointOnSimplex = c;
		result.m_usedVertices.usedVertexC = true;
		result.SetBarycentricCoordinates(0,0,1);
		return true;//c; // barycentric coordinates (0,0,1)
	}

    // Check if P in edge region of AC, if so return projection of P onto AC
    float vb = d5*d2 - d1*d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
		result.m_closestPointOnSimplex = a + w * ac;
		result.m_usedVertices.usedVertexA = true;
		result.m_usedVertices.usedVertexC = true;
		result.SetBarycentricCoordinates(1-w,0,w);
		return true;
        //return a + w * ac; // barycentric coordinates (1-w,0,w)
    }

    // Check if P in edge region of BC, if so return projection of P onto BC
    float va = d3*d6 - d5*d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		
		result.m_closestPointOnSimplex = b + w * (c - b);
		result.m_usedVertices.usedVertexB = true;
		result.m_usedVertices.usedVertexC = true;
		result.SetBarycentricCoordinates(0,1-w,w);
		return true;		
       // return b + w * (c - b); // barycentric coordinates (0,1-w,w)
    }

    // P inside face region. Compute Q through its barycentric coordinates (u,v,w)
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    
	result.m_closestPointOnSimplex = a + ab * v + ac * w;
	result.m_usedVertices.usedVertexA = true;
	result.m_usedVertices.usedVertexB = true;
	result.m_usedVertices.usedVertexC = true;
	result.SetBarycentricCoordinates(1-v-w,v,w);
	
	return true;
//	return a + ab * v + ac * w; // = u*a + v*b + w*c, u = va * denom = 1.0f - v - w

}





/// Test if point p and d lie on opposite sides of plane through abc
int VoronoiSimplexSolver::PointOutsideOfPlane(const SimdPoint3& p, const SimdPoint3& a, const SimdPoint3& b, const SimdPoint3& c, const SimdPoint3& d)
{
	SimdVector3 normal = (b-a).cross(c-a);

    float signp = (p - a).dot(normal); // [AP AB AC]
    float signd = (d - a).dot( normal); // [AD AB AC]

#ifdef CATCH_DEGENERATE_TETRAHEDRON
	if (signd * signd < (1e-4f * 1e-4f))
	{
//		printf("affine dependent/degenerate\n");//
		return -1;
	}
#endif
	// Points on opposite sides if expression signs are opposite
    return signp * signd < 0.f;
}


bool	VoronoiSimplexSolver::ClosestPtPointTetrahedron(const SimdPoint3& p, const SimdPoint3& a, const SimdPoint3& b, const SimdPoint3& c, const SimdPoint3& d, SubSimplexClosestResult& finalResult)
{
	SubSimplexClosestResult tempResult;

    // Start out assuming point inside all halfspaces, so closest to itself
	finalResult.m_closestPointOnSimplex = p;
	finalResult.m_usedVertices.reset();
    finalResult.m_usedVertices.usedVertexA = true;
	finalResult.m_usedVertices.usedVertexB = true;
	finalResult.m_usedVertices.usedVertexC = true;
	finalResult.m_usedVertices.usedVertexD = true;

    int pointOutsideABC = PointOutsideOfPlane(p, a, b, c, d);
	int pointOutsideACD = PointOutsideOfPlane(p, a, c, d, b);
  	int	pointOutsideADB = PointOutsideOfPlane(p, a, d, b, c);
	int	pointOutsideBDC = PointOutsideOfPlane(p, b, d, c, a);

   if (pointOutsideABC < 0 || pointOutsideACD < 0 || pointOutsideADB < 0 || pointOutsideBDC < 0)
   {
	   finalResult.m_degenerate = true;
	   return false;
   }

   if (!pointOutsideABC  && !pointOutsideACD && !pointOutsideADB && !pointOutsideBDC)
	 {
		 return false;
	 }


    float bestSqDist = FLT_MAX;
    // If point outside face abc then compute closest point on abc
	if (pointOutsideABC) 
	{
        ClosestPtPointTriangle(p, a, b, c,tempResult);
		SimdPoint3 q = tempResult.m_closestPointOnSimplex;
		
        float sqDist = (q - p).dot( q - p);
        // Update best closest point if (squared) distance is less than current best
        if (sqDist < bestSqDist) {
			bestSqDist = sqDist;
			finalResult.m_closestPointOnSimplex = q;
			//convert result bitmask!
			finalResult.m_usedVertices.reset();
			finalResult.m_usedVertices.usedVertexA = tempResult.m_usedVertices.usedVertexA;
			finalResult.m_usedVertices.usedVertexB = tempResult.m_usedVertices.usedVertexB;
			finalResult.m_usedVertices.usedVertexC = tempResult.m_usedVertices.usedVertexC;
			finalResult.SetBarycentricCoordinates(
					tempResult.m_barycentricCoords[VERTA],
					tempResult.m_barycentricCoords[VERTB],
					tempResult.m_barycentricCoords[VERTC],
					0
			);

		}
    }
  

	// Repeat test for face acd
	if (pointOutsideACD) 
	{
        ClosestPtPointTriangle(p, a, c, d,tempResult);
		SimdPoint3 q = tempResult.m_closestPointOnSimplex;
		//convert result bitmask!

        float sqDist = (q - p).dot( q - p);
        if (sqDist < bestSqDist) 
		{
			bestSqDist = sqDist;
			finalResult.m_closestPointOnSimplex = q;
			finalResult.m_usedVertices.reset();
			finalResult.m_usedVertices.usedVertexA = tempResult.m_usedVertices.usedVertexA;
			finalResult.m_usedVertices.usedVertexC = tempResult.m_usedVertices.usedVertexB;
			finalResult.m_usedVertices.usedVertexD = tempResult.m_usedVertices.usedVertexC;
			finalResult.SetBarycentricCoordinates(
					tempResult.m_barycentricCoords[VERTA],
					0,
					tempResult.m_barycentricCoords[VERTB],
					tempResult.m_barycentricCoords[VERTC]
			);

		}
    }
    // Repeat test for face adb

	
	if (pointOutsideADB)
	{
		ClosestPtPointTriangle(p, a, d, b,tempResult);
		SimdPoint3 q = tempResult.m_closestPointOnSimplex;
		//convert result bitmask!

        float sqDist = (q - p).dot( q - p);
        if (sqDist < bestSqDist) 
		{
			bestSqDist = sqDist;
			finalResult.m_closestPointOnSimplex = q;
			finalResult.m_usedVertices.reset();
			finalResult.m_usedVertices.usedVertexA = tempResult.m_usedVertices.usedVertexA;
			finalResult.m_usedVertices.usedVertexD = tempResult.m_usedVertices.usedVertexB;
			finalResult.m_usedVertices.usedVertexB = tempResult.m_usedVertices.usedVertexC;
			finalResult.SetBarycentricCoordinates(
					tempResult.m_barycentricCoords[VERTA],
					tempResult.m_barycentricCoords[VERTC],
					0,
					tempResult.m_barycentricCoords[VERTB]
			);

		}
    }
    // Repeat test for face bdc
    

	if (pointOutsideBDC)
	{
        ClosestPtPointTriangle(p, b, d, c,tempResult);
		SimdPoint3 q = tempResult.m_closestPointOnSimplex;
		//convert result bitmask!
        float sqDist = (q - p).dot( q - p);
        if (sqDist < bestSqDist) 
		{
			bestSqDist = sqDist;
			finalResult.m_closestPointOnSimplex = q;
			finalResult.m_usedVertices.reset();
			finalResult.m_usedVertices.usedVertexB = tempResult.m_usedVertices.usedVertexA;
			finalResult.m_usedVertices.usedVertexD = tempResult.m_usedVertices.usedVertexB;
			finalResult.m_usedVertices.usedVertexC = tempResult.m_usedVertices.usedVertexC;

			finalResult.SetBarycentricCoordinates(
					0,
					tempResult.m_barycentricCoords[VERTA],
					tempResult.m_barycentricCoords[VERTC],
					tempResult.m_barycentricCoords[VERTB]
			);

		}
    }

	//help! we ended up full !
	
	if (finalResult.m_usedVertices.usedVertexA &&
		finalResult.m_usedVertices.usedVertexB &&
		finalResult.m_usedVertices.usedVertexC &&
		finalResult.m_usedVertices.usedVertexD) 
	{
		return true;
	}

    return true;
}

