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
		pt.x()*cosf(m_w*t)-pt.y()*sinf(m_w*t),
		pt.x()*sinf(m_w*t)+pt.y()*cosf(m_w*t),
		pt.z()+m_s*CalculateF(t));
	}

	inline SimdVector3	InBetweenVector(const SimdVector3& vec,SimdScalar t) const
	{
		return SimdVector3(
		vec.x()*cosf(m_w*t)-vec.y()*sinf(m_w*t),
		vec.x()*sinf(m_w*t)+vec.y()*cosf(m_w*t),
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
