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
#ifndef BUM_POLYNOMIAL_SOLVER_INTERFACE
#define BUM_POLYNOMIAL_SOLVER_INTERFACE

#include <SimdScalar.h>
//
//BUM_PolynomialSolverInterface is interface class for polynomial root finding.
//The number of roots is returned as a result, query GetRoot to get the actual solution.
//
class BUM_PolynomialSolverInterface
{
public:
	virtual ~BUM_PolynomialSolverInterface() {};

	
//	virtual int Solve2QuadraticFull(SimdScalar a,SimdScalar b, SimdScalar c) = 0;
	
	virtual int	Solve3Cubic(SimdScalar lead, SimdScalar a, SimdScalar b, SimdScalar c) = 0;
	
	virtual int Solve4Quartic(SimdScalar lead, SimdScalar a, SimdScalar b, SimdScalar c, SimdScalar d) = 0;

	virtual SimdScalar GetRoot(int i) const = 0;

};

#endif //BUM_POLYNOMIAL_SOLVER_INTERFACE
