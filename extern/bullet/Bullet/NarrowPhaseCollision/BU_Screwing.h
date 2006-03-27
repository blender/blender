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


#ifndef B_SCREWING_H
#define B_SCREWING_H


#include <SimdVector3.h>
#include <SimdPoint3.h>
#include <SimdTransform.h>


#define SCREWEPSILON 0.00001f

///BU_Screwing implements screwing motion interpolation.
class BU_Screwing
{
public:

	
	BU_Screwing(const SimdVector3& relLinVel,const SimdVector3& relAngVel);

	~BU_Screwing() {
	};
	
	SimdScalar CalculateF(SimdScalar t) const;
	//gives interpolated position for time in [0..1] in screwing frame

	inline SimdPoint3	InBetweenPosition(const SimdPoint3& pt,SimdScalar t) const
	{
		return SimdPoint3(
		pt.x()*SimdCos(m_w*t)-pt.y()*SimdSin(m_w*t),
		pt.x()*SimdSin(m_w*t)+pt.y()*SimdCos(m_w*t),
		pt.z()+m_s*CalculateF(t));
	}

	inline SimdVector3	InBetweenVector(const SimdVector3& vec,SimdScalar t) const
	{
		return SimdVector3(
		vec.x()*SimdCos(m_w*t)-vec.y()*SimdSin(m_w*t),
		vec.x()*SimdSin(m_w*t)+vec.y()*SimdCos(m_w*t),
		vec.z());
	}

	//gives interpolated transform for time in [0..1] in screwing frame
	SimdTransform	InBetweenTransform(const SimdTransform& tr,SimdScalar t) const;

	
	//gives matrix from global frame into screwing frame
	void	LocalMatrix(SimdTransform &t) const;

	inline const SimdVector3& GetU() const {	return m_u;}
	inline const SimdVector3& GetO() const {return m_o;}
	inline const SimdScalar GetS() const{ return m_s;}
	inline const SimdScalar GetW() const { return m_w;}
	
private:
	float		m_w;
	float		m_s;
	SimdVector3 m_u;
	SimdVector3	m_o;
};

#endif //B_SCREWING_H
