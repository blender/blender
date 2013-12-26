/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2013 Erwin Coumans  http://bulletphysics.org

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/


#include "btFixedConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btTransformUtil.h"
#include <new>


btFixedConstraint::btFixedConstraint(btRigidBody& rbA,btRigidBody& rbB, const btTransform& frameInA,const btTransform& frameInB)
:btTypedConstraint(FIXED_CONSTRAINT_TYPE,rbA,rbB)
{
	m_frameInA = frameInA;
	m_frameInB = frameInB;

}

btFixedConstraint::~btFixedConstraint ()
{
}

	
void btFixedConstraint::getInfo1 (btConstraintInfo1* info)
{
	info->m_numConstraintRows = 6;
	info->nub = 0;
}

void btFixedConstraint::getInfo2 (btConstraintInfo2* info)
{
	//fix the 3 linear degrees of freedom

	const btTransform& transA = m_rbA.getCenterOfMassTransform();
	const btTransform& transB = m_rbB.getCenterOfMassTransform();

	const btVector3& worldPosA = m_rbA.getCenterOfMassTransform().getOrigin();
	const btMatrix3x3& worldOrnA = m_rbA.getCenterOfMassTransform().getBasis();
	const btVector3& worldPosB= m_rbB.getCenterOfMassTransform().getOrigin();
	const btMatrix3x3& worldOrnB = m_rbB.getCenterOfMassTransform().getBasis();
	

	info->m_J1linearAxis[0] = 1;
	info->m_J1linearAxis[info->rowskip+1] = 1;
	info->m_J1linearAxis[2*info->rowskip+2] = 1;

	btVector3 a1 = worldOrnA * m_frameInA.getOrigin();
    {
		btVector3* angular0 = (btVector3*)(info->m_J1angularAxis);
		btVector3* angular1 = (btVector3*)(info->m_J1angularAxis+info->rowskip);
		btVector3* angular2 = (btVector3*)(info->m_J1angularAxis+2*info->rowskip);
		btVector3 a1neg = -a1;
		a1neg.getSkewSymmetricMatrix(angular0,angular1,angular2);
	}

	if (info->m_J2linearAxis)
	{
		info->m_J2linearAxis[0] = -1;
		info->m_J2linearAxis[info->rowskip+1] = -1;
		info->m_J2linearAxis[2*info->rowskip+2] = -1;
	}
	
	btVector3 a2 = worldOrnB*m_frameInB.getOrigin();
   {
		btVector3* angular0 = (btVector3*)(info->m_J2angularAxis);
		btVector3* angular1 = (btVector3*)(info->m_J2angularAxis+info->rowskip);
		btVector3* angular2 = (btVector3*)(info->m_J2angularAxis+2*info->rowskip);
		a2.getSkewSymmetricMatrix(angular0,angular1,angular2);
	}

    // set right hand side for the linear dofs
	btScalar k = info->fps * info->erp;
	
	btVector3 linearError = k*(a2+worldPosB-a1-worldPosA);
    int j;
	for (j=0; j<3; j++)
    {
        info->m_constraintError[j*info->rowskip] = linearError[j];
		//printf("info->m_constraintError[%d]=%f\n",j,info->m_constraintError[j]);
    }

	btVector3 ivA = transA.getBasis() * m_frameInA.getBasis().getColumn(0);
	btVector3 jvA = transA.getBasis() * m_frameInA.getBasis().getColumn(1);
	btVector3 kvA = transA.getBasis() * m_frameInA.getBasis().getColumn(2);
	btVector3 ivB = transB.getBasis() * m_frameInB.getBasis().getColumn(0);
	btVector3 target;
	btScalar x = ivB.dot(ivA);
	btScalar y = ivB.dot(jvA);
	btScalar z = ivB.dot(kvA);
	btVector3 swingAxis(0,0,0);
	{ 
		if((!btFuzzyZero(y)) || (!(btFuzzyZero(z))))
		{
			swingAxis = -ivB.cross(ivA);
		}
	}
	btVector3 vTwist(1,0,0);

	// compute rotation of A wrt B (in constraint space)
	btQuaternion qA = transA.getRotation() * m_frameInA.getRotation();
	btQuaternion qB = transB.getRotation() * m_frameInB.getRotation();
	btQuaternion qAB = qB.inverse() * qA;
	// split rotation into cone and twist
	// (all this is done from B's perspective. Maybe I should be averaging axes...)
	btVector3 vConeNoTwist = quatRotate(qAB, vTwist); vConeNoTwist.normalize();
	btQuaternion qABCone  = shortestArcQuat(vTwist, vConeNoTwist); qABCone.normalize();
	btQuaternion qABTwist = qABCone.inverse() * qAB; qABTwist.normalize();

	int row = 3;
    int srow = row * info->rowskip;
	btVector3 ax1;
	// angular limits
	{
		btScalar *J1 = info->m_J1angularAxis;
		btScalar *J2 = info->m_J2angularAxis;
		btTransform trA = transA*m_frameInA;
		btVector3 twistAxis = trA.getBasis().getColumn(0);

		btVector3 p = trA.getBasis().getColumn(1);
		btVector3 q = trA.getBasis().getColumn(2);
		int srow1 = srow + info->rowskip;
		J1[srow+0] = p[0];
		J1[srow+1] = p[1];
		J1[srow+2] = p[2];
		J1[srow1+0] = q[0];
		J1[srow1+1] = q[1];
		J1[srow1+2] = q[2];
		J2[srow+0] = -p[0];
		J2[srow+1] = -p[1];
		J2[srow+2] = -p[2];
		J2[srow1+0] = -q[0];
		J2[srow1+1] = -q[1];
		J2[srow1+2] = -q[2];
		btScalar fact = info->fps;
		info->m_constraintError[srow] =   fact * swingAxis.dot(p);
		info->m_constraintError[srow1] =  fact * swingAxis.dot(q);
		info->m_lowerLimit[srow] = -SIMD_INFINITY;
		info->m_upperLimit[srow] = SIMD_INFINITY;
		info->m_lowerLimit[srow1] = -SIMD_INFINITY;
		info->m_upperLimit[srow1] = SIMD_INFINITY;
		srow = srow1 + info->rowskip;

		{
			btQuaternion qMinTwist = qABTwist;
			btScalar twistAngle = qABTwist.getAngle();

			if (twistAngle > SIMD_PI) // long way around. flip quat and recalculate.
			{
				qMinTwist = -(qABTwist);
				twistAngle = qMinTwist.getAngle();
			}

			if (twistAngle > SIMD_EPSILON)
			{
				twistAxis = btVector3(qMinTwist.x(), qMinTwist.y(), qMinTwist.z());
				twistAxis.normalize();
				twistAxis = quatRotate(qB, -twistAxis);
			}
			ax1 = twistAxis;
			btScalar *J1 = info->m_J1angularAxis;
			btScalar *J2 = info->m_J2angularAxis;
			J1[srow+0] = ax1[0];
			J1[srow+1] = ax1[1];
			J1[srow+2] = ax1[2];
			J2[srow+0] = -ax1[0];
			J2[srow+1] = -ax1[1];
			J2[srow+2] = -ax1[2];
			btScalar k = info->fps;
			info->m_constraintError[srow] = k * twistAngle;
			info->m_lowerLimit[srow] = -SIMD_INFINITY;
			info->m_upperLimit[srow] = SIMD_INFINITY;
		}
	}
}