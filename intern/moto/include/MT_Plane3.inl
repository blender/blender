#include "MT_Optimize.h"


GEN_INLINE
MT_Plane3::
MT_Plane3(
	const MT_Vector3 &a,
	const MT_Vector3 &b,
	const MT_Vector3 &c
){
	MT_Vector3 l1 = b-a;
	MT_Vector3 l2 = c-b;

	MT_Vector3 n = l1.cross(l2);
	n = n.safe_normalized();
	MT_Scalar d = n.dot(a); 

	m_co[0] = n.x();
	m_co[1] = n.y();
	m_co[2] = n.z();
	m_co[3] = -d;
}

/**
 * Construction from vector and a point.
 */
GEN_INLINE
MT_Plane3::
MT_Plane3(
	const MT_Vector3 &n,
	const MT_Vector3 &p
){
	
	MT_Vector3 mn = n.safe_normalized();
	MT_Scalar md = mn.dot(p); 

	m_co[0] = mn.x();
	m_co[1] = mn.y();
	m_co[2] = mn.z();
	m_co[3] = -md;
}


/**
 * Default constructor
 */
GEN_INLINE
MT_Plane3::
MT_Plane3(
):
	MT_Tuple4()
{
	m_co[0] = MT_Scalar(1);
	m_co[1] = MT_Scalar(0);
	m_co[2] = MT_Scalar(0);
	m_co[3] = MT_Scalar(0);
}

/**
 * Return plane normal
 */

GEN_INLINE
	MT_Vector3
MT_Plane3::
Normal(
) const {
	return MT_Vector3(m_co[0],m_co[1],m_co[2]);
}

/**
 * Return plane scalar i.e the d from n.x + d = 0
 */

GEN_INLINE
	MT_Scalar
MT_Plane3::
Scalar(
) const {
	return m_co[3];
}

GEN_INLINE
	void
MT_Plane3::
Invert(
) {
	m_co[0] = -m_co[0];
	m_co[1] = -m_co[1];
	m_co[2] = -m_co[2];
	m_co[3] = -m_co[3];
}


/**
 * Assignment operator
 */

GEN_INLINE
	MT_Plane3 &
MT_Plane3::
operator = (
	const MT_Plane3 & rhs
) {
	m_co[0] = rhs.m_co[0];
	m_co[1] = rhs.m_co[1];
	m_co[2] = rhs.m_co[2];
	m_co[3] = rhs.m_co[3];
	return *this;
}

/**
 * Return the distance from a point to the plane
 */

GEN_INLINE
	MT_Scalar
MT_Plane3::
signedDistance(
	const MT_Vector3 &v
) const {
	return Normal().dot(v) + m_co[3];
}





