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
