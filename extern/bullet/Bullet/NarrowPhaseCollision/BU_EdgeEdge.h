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

#ifndef BU_EDGEEDGE
#define BU_EDGEEDGE

class BU_Screwing;
#include <SimdTransform.h>
#include <SimdPoint3.h>
#include <SimdVector3.h>

//class BUM_Point2;

#include <SimdScalar.h>

///BU_EdgeEdge implements algebraic time of impact calculation between two (angular + linear) moving edges.
class BU_EdgeEdge
{
public:
	

	BU_EdgeEdge();
	bool GetTimeOfImpact(
		const BU_Screwing& screwAB,
		const SimdPoint3& a,//edge in object A
		const SimdVector3& u,
		const SimdPoint3& c,//edge in object B
		const SimdVector3& v,
		SimdScalar &minTime,
		SimdScalar &lamda,
		SimdScalar& mu
		);
private:

	bool Calc2DRotationPointPoint(const SimdPoint3& rotPt, SimdScalar rotRadius, SimdScalar rotW,const SimdPoint3& intersectPt,SimdScalar& minTime);
	bool GetTimeOfImpactGeneralCase(
		const BU_Screwing& screwAB,
		const SimdPoint3& a,//edge in object A
		const SimdVector3& u,
		const SimdPoint3& c,//edge in object B
		const SimdVector3& v,
		SimdScalar &minTime,
		SimdScalar &lamda,
		SimdScalar& mu

		);

	
	bool GetTimeOfImpactVertexEdge(
		const BU_Screwing& screwAB,
		const SimdPoint3& a,//edge in object A
		const SimdVector3& u,
		const SimdPoint3& c,//edge in object B
		const SimdVector3& v,
		SimdScalar &minTime,
		SimdScalar &lamda,
		SimdScalar& mu

		);

};

#endif //BU_EDGEEDGE
