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

#ifndef VERTEX_POLY_H
#define VERTEX_POLY_H


class BU_Screwing;
#include <SimdTransform.h>
#include <SimdPoint3.h>
#include <SimdScalar.h>

///BU_VertexPoly implements algebraic time of impact calculation between vertex and a plane.
class BU_VertexPoly
{
public:
	BU_VertexPoly();
	bool GetTimeOfImpact(
		const BU_Screwing& screwAB,
		const SimdPoint3& vtx,
		const SimdVector4& planeEq,
		SimdScalar &minTime,
		bool swapAB);

private:

	//cached data (frame coherency etc.) here

};
#endif //VERTEX_POLY_H
