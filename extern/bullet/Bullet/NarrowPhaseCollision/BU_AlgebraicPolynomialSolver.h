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
