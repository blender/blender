#include "MT_Optimize.h"

GEN_INLINE MT_Quaternion& MT_Quaternion::operator*=(const MT_Quaternion& q) {
    setValue(m_co[3] * q[0] + m_co[0] * q[3] + m_co[1] * q[2] - m_co[2] * q[1],
             m_co[3] * q[1] + m_co[1] * q[3] + m_co[2] * q[0] - m_co[0] * q[2],
             m_co[3] * q[2] + m_co[2] * q[3] + m_co[0] * q[1] - m_co[1] * q[0],
             m_co[3] * q[3] - m_co[0] * q[0] - m_co[1] * q[1] - m_co[2] * q[2]);
    return *this;
}

GEN_INLINE void MT_Quaternion::conjugate() {
    m_co[0] = -m_co[0]; m_co[1] = -m_co[1]; m_co[2] = -m_co[2];
}

GEN_INLINE MT_Quaternion MT_Quaternion::conjugate() const {
    return MT_Quaternion(-m_co[0], -m_co[1], -m_co[2], m_co[3]);
}
  
GEN_INLINE void MT_Quaternion::invert() {
    conjugate();
    *this /= length2();
}

GEN_INLINE MT_Quaternion MT_Quaternion::inverse() const {
    return conjugate() / length2();
}

// From: "Uniform Random Rotations", Ken Shoemake, Graphics Gems III, 
//       pg. 124-132
GEN_INLINE MT_Quaternion MT_Quaternion::random() {
    MT_Scalar x0 = MT_random();
    MT_Scalar r1 = sqrt(MT_Scalar(1.0) - x0), r2 = sqrt(x0);
    MT_Scalar t1 = MT_2_PI * MT_random(), t2 = MT_2_PI * MT_random();
    MT_Scalar c1 = cos(t1), s1 = sin(t1);
    MT_Scalar c2 = cos(t2), s2 = sin(t2);
    return MT_Quaternion(s1 * r1, c1 * r1, s2 * r2, c2 * r2);
}

GEN_INLINE MT_Quaternion operator*(const MT_Quaternion& q1, 
                                   const MT_Quaternion& q2) {
    return MT_Quaternion(q1[3] * q2[0] + q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1],
                         q1[3] * q2[1] + q1[1] * q2[3] + q1[2] * q2[0] - q1[0] * q2[2],
                         q1[3] * q2[2] + q1[2] * q2[3] + q1[0] * q2[1] - q1[1] * q2[0],
                         q1[3] * q2[3] - q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2]); 
}

GEN_INLINE MT_Quaternion operator*(const MT_Quaternion& q, const MT_Vector3& w)
{
    return MT_Quaternion( q[3] * w[0] + q[1] * w[2] - q[2] * w[1],
                          q[3] * w[1] + q[2] * w[0] - q[0] * w[2],
                          q[3] * w[2] + q[0] * w[1] - q[1] * w[0],
                         -q[0] * w[0] - q[1] * w[1] - q[2] * w[2]); 
}

GEN_INLINE MT_Quaternion operator*(const MT_Vector3& w, const MT_Quaternion& q)
{
    return MT_Quaternion( w[0] * q[3] + w[1] * q[2] - w[2] * q[1],
                          w[1] * q[3] + w[2] * q[0] - w[0] * q[2],
                          w[2] * q[3] + w[0] * q[1] - w[1] * q[0],
                         -w[0] * q[0] - w[1] * q[1] - w[2] * q[2]); 
}

GEN_INLINE MT_Scalar MT_Quaternion::angle(const MT_Quaternion& q) const 
{
	MT_Scalar s = sqrt(length2() * q.length2());
	assert(s != MT_Scalar(0.0));
	
	s = dot(q) / s;
	
	s = MT_clamp(s, -1.0, 1.0);
	
	return acos(s);
}

GEN_INLINE MT_Quaternion MT_Quaternion::slerp(const MT_Quaternion& q, const MT_Scalar& t) const
{
	MT_Scalar d, s0, s1;
	MT_Scalar s = dot(q);
	bool neg = (s < 0.0);

	if (neg)
		s = -s;
	if ((1.0 - s) > 0.0001) 
	{
		MT_Scalar theta = acos(s);
		d = MT_Scalar(1.0) / sin(theta);
		s0 = sin((MT_Scalar(1.0) - t) * theta);
		s1 = sin(t * theta);
	}
	else
	{
		d = MT_Scalar(1.0);
		s0 = MT_Scalar(1.0) - t;
		s1 = t;
	}
	if (neg)
		s1 = -s1;
	return d*(*this * s0 + q * s1);
}

