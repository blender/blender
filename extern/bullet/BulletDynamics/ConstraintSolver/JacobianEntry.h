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
#ifndef JACOBIAN_ENTRY_H
#define JACOBIAN_ENTRY_H

#include "SimdVector3.h"
#include "Dynamics/RigidBody.h"


//notes:
// Another memory optimization would be to store m_MbJ in the remaining 3 w components
// which makes the JacobianEntry memory layout 16 bytes
// if you only are interested in angular part, just feed massInvA and massInvB zero

/// Jacobian entry is an abstraction that allows to describe constraints
/// it can be used in combination with a constraint solver
/// Can be used to relate the effect of an impulse to the constraint error
class JacobianEntry
{
public:
	JacobianEntry() {};
	//constraint between two different rigidbodies
	JacobianEntry(
		const SimdMatrix3x3& world2A,
		const SimdMatrix3x3& world2B,
		const SimdVector3& rel_pos1,const SimdVector3& rel_pos2,
		const SimdVector3& normal,
		const SimdVector3& inertiaInvA, 
		const SimdScalar massInvA,
		const SimdVector3& inertiaInvB,
		const SimdScalar massInvB)
		:m_normalAxis(normal)
	{
		m_aJ = world2A*(rel_pos1.cross(normal));
		m_bJ = world2B*(rel_pos2.cross(normal));
		m_MaJ	= inertiaInvA * m_aJ;
		m_MbJ = inertiaInvB * m_bJ;
		m_jacDiagAB = massInvA + m_MaJ.dot(m_aJ) + massInvB + m_MbJ.dot(m_bJ);
	}

		//angular constraint between two different rigidbodies
	JacobianEntry(const SimdVector3& normal,
		const SimdMatrix3x3& world2A,
		const SimdMatrix3x3& world2B,
		const SimdVector3& inertiaInvA,
		const SimdVector3& inertiaInvB)
		:m_normalAxis(normal)
	{
		m_aJ= world2A*normal;
		m_bJ = world2B*-normal;
		m_MaJ	= inertiaInvA * m_aJ;
		m_MbJ = inertiaInvB * m_bJ;
		m_jacDiagAB =  m_MaJ.dot(m_aJ) + m_MbJ.dot(m_bJ);
	}

	//constraint on one rigidbody
	JacobianEntry(
		const SimdMatrix3x3& world2A,
		const SimdVector3& rel_pos1,const SimdVector3& rel_pos2,
		const SimdVector3& normal,
		const SimdVector3& inertiaInvA, 
		const SimdScalar massInvA)
		:m_normalAxis(normal)
	{
		m_aJ= world2A*(rel_pos1.cross(normal));
		m_bJ = world2A*(rel_pos2.cross(normal));
		m_MaJ	= inertiaInvA * m_aJ;
		m_MbJ = SimdVector3(0.f,0.f,0.f);
		m_jacDiagAB = massInvA + m_MaJ.dot(m_aJ);
	}

	SimdScalar	getDiagonal() const { return m_jacDiagAB; }

	// for two constraints on the same rigidbody (for example vehicle friction)
	SimdScalar	getNonDiagonal(const JacobianEntry& jacB, const SimdScalar massInvA) const
	{
		const JacobianEntry& jacA = *this;
		SimdScalar lin = massInvA * jacA.m_normalAxis.dot(jacB.m_normalAxis);
		SimdScalar ang = jacA.m_MaJ.dot(jacB.m_aJ);
		return lin + ang;
	}

	

	// for two constraints on sharing two same rigidbodies (for example two contact points between two rigidbodies)
	SimdScalar	getNonDiagonal(const JacobianEntry& jacB,const SimdScalar massInvA,const SimdScalar massInvB) const
	{
		const JacobianEntry& jacA = *this;
		SimdVector3 lin = jacA.m_normalAxis * jacB.m_normalAxis;
		SimdVector3 ang0 = jacA.m_MaJ * jacB.m_aJ;
		SimdVector3 ang1 = jacA.m_MbJ * jacB.m_bJ;
		SimdVector3 lin0 = massInvA * lin ;
		SimdVector3 lin1 = massInvB * lin;
		SimdVector3 sum = ang0+ang1+lin0+lin1;
		return sum[0]+sum[1]+sum[2];
	}

	SimdScalar getRelativeVelocity(const SimdVector3& linvelA,const SimdVector3& angvelA,const SimdVector3& linvelB,const SimdVector3& angvelB)
	{
		SimdVector3 linrel = linvelA - linvelB;
		SimdVector3 angvela  = angvelA * m_aJ;
		SimdVector3 angvelb  = angvelB * m_bJ;
		linrel *= m_normalAxis;
		angvela += angvelb;
		angvela += linrel;
		SimdScalar rel_vel2 = angvela[0]+angvela[1]+angvela[2];
		return rel_vel2 + SIMD_EPSILON;
	}
//private:

	SimdVector3	m_normalAxis;
	SimdVector3	m_aJ;
	SimdVector3	m_bJ;
	SimdVector3	m_MaJ;
	SimdVector3	m_MbJ;
	//Optimization: can be stored in the w/last component of one of the vectors
	SimdScalar	m_jacDiagAB;

};

#endif //JACOBIAN_ENTRY_H
