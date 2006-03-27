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



#ifndef VoronoiSimplexSolver_H
#define VoronoiSimplexSolver_H

#include "SimplexSolverInterface.h"



#define VORONOI_SIMPLEX_MAX_VERTS 5

struct UsageBitfield{
	UsageBitfield()
	{
		reset();
	}

	void reset()
	{
		usedVertexA = false;
		usedVertexB = false;
		usedVertexC = false;
		usedVertexD = false;
	}
	unsigned short usedVertexA	: 1;
	unsigned short usedVertexB	: 1;
	unsigned short usedVertexC	: 1;
	unsigned short usedVertexD	: 1;
	unsigned short unused1		: 1;
	unsigned short unused2		: 1;
	unsigned short unused3		: 1;
	unsigned short unused4		: 1;
};


struct	SubSimplexClosestResult
{
	SimdPoint3	m_closestPointOnSimplex;
	//MASK for m_usedVertices
	//stores the simplex vertex-usage, using the MASK, 
	// if m_usedVertices & MASK then the related vertex is used
	UsageBitfield	m_usedVertices;
	float	m_barycentricCoords[4];
	bool m_degenerate;

	void	Reset()
	{
		m_degenerate = false;
		SetBarycentricCoordinates();
		m_usedVertices.reset();
	}
	bool	IsValid()
	{
		bool valid = (m_barycentricCoords[0] >= 0.f) &&
			(m_barycentricCoords[1] >= 0.f) &&
			(m_barycentricCoords[2] >= 0.f) &&
			(m_barycentricCoords[3] >= 0.f);


		return valid;
	}
	void	SetBarycentricCoordinates(float a=0.f,float b=0.f,float c=0.f,float d=0.f)
	{
		m_barycentricCoords[0] = a;
		m_barycentricCoords[1] = b;
		m_barycentricCoords[2] = c;
		m_barycentricCoords[3] = d;
	}

};

/// VoronoiSimplexSolver is an implementation of the closest point distance algorithm from a 1-4 points simplex to the origin.
/// Can be used with GJK, as an alternative to Johnson distance algorithm.
#ifdef NO_VIRTUAL_INTERFACE
class VoronoiSimplexSolver
#else
class VoronoiSimplexSolver : public SimplexSolverInterface
#endif
{
public:

	int	m_numVertices;

	SimdVector3	m_simplexVectorW[VORONOI_SIMPLEX_MAX_VERTS];
	SimdPoint3	m_simplexPointsP[VORONOI_SIMPLEX_MAX_VERTS];
	SimdPoint3	m_simplexPointsQ[VORONOI_SIMPLEX_MAX_VERTS];

	

	SimdPoint3	m_cachedP1;
	SimdPoint3	m_cachedP2;
	SimdVector3	m_cachedV;
	SimdVector3	m_lastW;
	bool		m_cachedValidClosest;

	SubSimplexClosestResult m_cachedBC;

	bool	m_needsUpdate;
	
	void	removeVertex(int index);
	void	ReduceVertices (const UsageBitfield& usedVerts);
	bool	UpdateClosestVectorAndPoints();

	bool	ClosestPtPointTetrahedron(const SimdPoint3& p, const SimdPoint3& a, const SimdPoint3& b, const SimdPoint3& c, const SimdPoint3& d, SubSimplexClosestResult& finalResult);
	int		PointOutsideOfPlane(const SimdPoint3& p, const SimdPoint3& a, const SimdPoint3& b, const SimdPoint3& c, const SimdPoint3& d);
	bool	ClosestPtPointTriangle(const SimdPoint3& p, const SimdPoint3& a, const SimdPoint3& b, const SimdPoint3& c,SubSimplexClosestResult& result);

public:

	 void reset();

	 void addVertex(const SimdVector3& w, const SimdPoint3& p, const SimdPoint3& q);


	 bool closest(SimdVector3& v);

	 SimdScalar maxVertex();

	 bool fullSimplex() const
	 {
		 return (m_numVertices == 4);
	 }

	 int getSimplex(SimdPoint3 *pBuf, SimdPoint3 *qBuf, SimdVector3 *yBuf) const;

	 bool inSimplex(const SimdVector3& w);
	
	 void backup_closest(SimdVector3& v) ;

	 bool emptySimplex() const ;

	 void compute_points(SimdPoint3& p1, SimdPoint3& p2) ;

	 int numVertices() const 
	 {
		 return m_numVertices;
	 }


};

#endif //VoronoiSimplexSolver
