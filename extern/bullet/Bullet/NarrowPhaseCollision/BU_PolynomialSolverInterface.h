
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
