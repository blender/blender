#ifndef MATH_UTILS_H
#define MATH_UTILS_H

/* needed for fabs, sqrt() below */
#include <cmath>

#ifdef _WIN32
#define hypot _hypot
#endif

namespace TNT
{
/**
	@returns hypotenuse of real (non-complex) scalars a and b by 
	avoiding underflow/overflow
	using (a * sqrt( 1 + (b/a) * (b/a))), rather than
	sqrt(a*a + b*b).
*/
template <class Real>
Real hypot(const Real &a, const Real &b)
{
	
	if (a== 0)
		return fabs(b);
	else
	{
		Real c = b/a;
		return fabs(a) * sqrt(1 + c*c);
	}
}
} /* TNT namespace */



#endif
/* MATH_UTILS_H */
