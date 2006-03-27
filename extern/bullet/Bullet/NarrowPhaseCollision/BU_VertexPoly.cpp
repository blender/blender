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



#include "BU_VertexPoly.h"
#include "BU_Screwing.h"
#include <SimdTransform.h>
#include <SimdPoint3.h>
#include <SimdVector3.h>

#define USE_ALGEBRAIC
#ifdef USE_ALGEBRAIC
	#include "BU_AlgebraicPolynomialSolver.h"
	#define BU_Polynomial BU_AlgebraicPolynomialSolver
#else
	#include "BU_IntervalArithmeticPolynomialSolver.h"
	#define BU_Polynomial BU_IntervalArithmeticPolynomialSolver
#endif

inline bool      TestFuzzyZero(SimdScalar x) { return SimdFabs(x) < 0.0001f; }


BU_VertexPoly::BU_VertexPoly()
{

}
//return true if a collision will occur between [0..1]
//false otherwise. If true, minTime contains the time of impact
bool BU_VertexPoly::GetTimeOfImpact(
		const BU_Screwing& screwAB,
		const SimdPoint3& a,
		const SimdVector4& planeEq,
		SimdScalar &minTime,bool swapAB)
{

	bool hit = false;

	// precondition: s=0 and w= 0 is catched by caller!
	if (TestFuzzyZero(screwAB.GetS()) &&
		TestFuzzyZero(screwAB.GetW()))
	{
		return false;
	}


	//case w<>0 and s<> 0
	const SimdScalar w=screwAB.GetW();
	const SimdScalar s=screwAB.GetS();

	SimdScalar coefs[4];
	const SimdScalar p=planeEq[0];
	const SimdScalar q=planeEq[1];
	const SimdScalar r=planeEq[2];
	const SimdScalar d=planeEq[3];

	const SimdVector3 norm(p,q,r);
	BU_Polynomial polynomialSolver;
	int numroots = 0;

	//SimdScalar eps=1e-80f;
	//SimdScalar eps2=1e-100f;
	
	if (TestFuzzyZero(screwAB.GetS()) )	
	{
		//S = 0 , W <> 0
		
		//ax^3+bx^2+cx+d=0
		coefs[0]=0.;
		coefs[1]=(-p*a.x()-q*a.y()+r*a.z()-d);
		coefs[2]=-2*p*a.y()+2*q*a.x();
		coefs[3]=p*a.x()+q*a.y()+r*a.z()-d;

//		numroots = polynomialSolver.Solve3Cubic(coefs[0],coefs[1],coefs[2],coefs[3]);
		numroots = polynomialSolver.Solve2QuadraticFull(coefs[1],coefs[2],coefs[3]);

	} else
	{
		if (TestFuzzyZero(screwAB.GetW()))
		{
			// W = 0 , S <> 0
			//pax+qay+r(az+st)=d
			
			SimdScalar dist = (d - a.dot(norm));

			if (TestFuzzyZero(r))
			{
				if (TestFuzzyZero(dist))
				{
					// no hit
				} else
				{
					// todo a a' might hit sides of polygon T
					//printf("unhandled case, w=0,s<>0,r<>0, a a' might hit sides of polygon T \n");
				}
				
			} else
			{
				SimdScalar etoi = (dist)/(r*screwAB.GetS());
				if (swapAB)
					etoi *= -1;

				if (etoi >= 0. && etoi <= minTime)
				{
					minTime = etoi;
					hit = true;
				}
			}

		} else
		{
				//ax^3+bx^2+cx+d=0

				//degenerate coefficients mess things up :(
				SimdScalar ietsje = (r*s)/SimdTan(w/2.f);
				if (ietsje*ietsje < 0.01f)
					ietsje = 0.f;

				coefs[0]=ietsje;//(r*s)/tan(w/2.);
				coefs[1]=(-p*a.x()-q*a.y()+r*a.z()-d);
				coefs[2]=-2.f*p*a.y()+2.f*q*a.x()+ietsje;//((r*s)/(tan(w/2.)));
				coefs[3]=p*a.x()+q*a.y()+r*a.z()-d;

				numroots = polynomialSolver.Solve3Cubic(coefs[0],coefs[1],coefs[2],coefs[3]);
		}
	}


	for (int i=0;i<numroots;i++)
	{
		SimdScalar tau = polynomialSolver.GetRoot(i);

		SimdScalar t = 2.f*SimdAtan(tau)/w;
		//tau = tan (wt/2) so 2*atan (tau)/w
		if (swapAB)
		{
			t *= -1.;
		}
		if (t>=0 && t<minTime)
		{
			minTime = t;
			hit = true;
		}
	}
	
	return hit;
}
