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

#ifndef BU_ALGEBRAIC_POLYNOMIAL_SOLVER_H
#define BU_ALGEBRAIC_POLYNOMIAL_SOLVER_H

#include "BU_PolynomialSolverInterface.h"

/// BU_AlgebraicPolynomialSolver implements polynomial root finding by analytically solving algebraic equations.
/// Polynomials up to 4rd degree are supported, Cardano's formula is used for 3rd degree
class BU_AlgebraicPolynomialSolver : public BUM_PolynomialSolverInterface
{
public:
	BU_AlgebraicPolynomialSolver() {};

	int Solve2Quadratic(SimdScalar p, SimdScalar q);
	int Solve2QuadraticFull(SimdScalar a,SimdScalar b, SimdScalar c);
	int	Solve3Cubic(SimdScalar lead, SimdScalar a, SimdScalar b, SimdScalar c);
	int Solve4Quartic(SimdScalar lead, SimdScalar a, SimdScalar b, SimdScalar c, SimdScalar d);
	

	SimdScalar GetRoot(int i) const 
	{
		return m_roots[i];
	}

private:
	SimdScalar	m_roots[4];

};

#endif //BU_ALGEBRAIC_POLYNOMIAL_SOLVER_H
