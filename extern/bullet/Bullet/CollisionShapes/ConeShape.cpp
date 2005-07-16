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
#include "ConeShape.h"
#include "SimdPoint3.h"

#ifdef WIN32
static int coneindices[3] = {1,2,0};
#else
static int coneindices[3] = {2,1,0};
#endif

ConeShape::ConeShape (SimdScalar radius,SimdScalar height):
m_radius (radius),
m_height(height)
{
	SimdVector3 halfExtents;
	m_sinAngle = (m_radius / sqrt(m_radius * m_radius + m_height * m_height));
}


SimdVector3 ConeShape::ConeLocalSupport(const SimdVector3& v) const
{
	
	float halfHeight = m_height * 0.5f;

 if (v[coneindices[1]] > v.length() * m_sinAngle)
 {
	SimdVector3 tmp;

	tmp[coneindices[0]] = 0.f;
	tmp[coneindices[1]] = halfHeight;
	tmp[coneindices[2]] = 0.f;
	return tmp;
 }
  else {
    SimdScalar s = sqrtf(v[coneindices[0]] * v[coneindices[0]] + v[coneindices[2]] * v[coneindices[2]]);
    if (s > SIMD_EPSILON) {
      SimdScalar d = m_radius / s;
	  SimdVector3 tmp;
	  tmp[coneindices[0]] = v[coneindices[0]] * d;
	  tmp[coneindices[1]] = -halfHeight;
	  tmp[coneindices[2]] = v[coneindices[2]] * d;
	  return tmp;
    }
    else  {
		SimdVector3 tmp;
		tmp[coneindices[0]] = 0.f;
		tmp[coneindices[1]] = -halfHeight;
		tmp[coneindices[2]] = 0.f;
		return tmp;
	}
  }

}

SimdVector3	ConeShape::LocalGetSupportingVertexWithoutMargin(const SimdVector3& vec) const
{
		return ConeLocalSupport(vec);
}

SimdVector3	ConeShape::LocalGetSupportingVertex(const SimdVector3& vec)  const
{
	SimdVector3 supVertex = ConeLocalSupport(vec);
	if ( GetMargin()!=0.f )
	{
		SimdVector3 vecnorm = vec;
		if (vecnorm .length2() == 0.f)
		{
			vecnorm.setValue(-1.f,-1.f,-1.f);
		} 
		vecnorm.normalize();
		supVertex+= GetMargin() * vecnorm;
	}
	return supVertex;
}


