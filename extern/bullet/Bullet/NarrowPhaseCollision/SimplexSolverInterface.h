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


#ifndef SIMPLEX_SOLVER_INTERFACE_H
#define SIMPLEX_SOLVER_INTERFACE_H

#include "SimdVector3.h"
#include "SimdPoint3.h"

#define NO_VIRTUAL_INTERFACE
#ifdef NO_VIRTUAL_INTERFACE
#include "VoronoiSimplexSolver.h"
#define SimplexSolverInterface VoronoiSimplexSolver
#else
/// for simplices from 1 to 4 vertices
/// for example Johnson-algorithm or alternative approaches based on
/// voronoi regions or barycentric coordinates
class SimplexSolverInterface
{
	public:
		virtual ~SimplexSolverInterface() {};

	virtual void reset() = 0;

	virtual void addVertex(const SimdVector3& w, const SimdPoint3& p, const SimdPoint3& q) = 0;
	
	virtual bool closest(SimdVector3& v) = 0;

	virtual SimdScalar maxVertex() = 0;

	virtual bool fullSimplex() const = 0;

	virtual int getSimplex(SimdPoint3 *pBuf, SimdPoint3 *qBuf, SimdVector3 *yBuf) const = 0;

	virtual bool inSimplex(const SimdVector3& w) = 0;
	
	virtual void backup_closest(SimdVector3& v) = 0;

	virtual bool emptySimplex() const = 0;

	virtual void compute_points(SimdPoint3& p1, SimdPoint3& p2) = 0;

	virtual int numVertices() const =0;


};
#endif
#endif //SIMPLEX_SOLVER_INTERFACE_H

