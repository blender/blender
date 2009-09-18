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


#include "btPoint2PointConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include <new>



btPoint2PointConstraint::btPoint2PointConstraint()
:btTypedConstraint(POINT2POINT_CONSTRAINT_TYPE),
m_useSolveConstraintObsolete(false)
{
}

btPoint2PointConstraint::btPoint2PointConstraint(btRigidBody& rbA,btRigidBody& rbB, const btVector3& pivotInA,const btVector3& pivotInB)
:btTypedConstraint(POINT2POINT_CONSTRAINT_TYPE,rbA,rbB),m_pivotInA(pivotInA),m_pivotInB(pivotInB),
m_useSolveConstraintObsolete(false)
{

}


btPoint2PointConstraint::btPoint2PointConstraint(btRigidBody& rbA,const btVector3& pivotInA)
:btTypedConstraint(POINT2POINT_CONSTRAINT_TYPE,rbA),m_pivotInA(pivotInA),m_pivotInB(rbA.getCenterOfMassTransform()(pivotInA)),
m_useSolveConstraintObsolete(false)
{
	
}

void	btPoint2PointConstraint::buildJacobian()
{
	///we need it for both methods
	{
		m_appliedImpulse = btScalar(0.);

		btVector3	normal(0,0,0);

		for (int i=0;i<3;i++)
		{
			normal[i] = 1;
			new (&m_jac[i]) btJacobianEntry(
			m_rbA.getCenterOfMassTransform().getBasis().transpose(),
			m_rbB.getCenterOfMassTransform().getBasis().transpose(),
			m_rbA.getCenterOfMassTransform()*m_pivotInA - m_rbA.getCenterOfMassPosition(),
			m_rbB.getCenterOfMassTransform()*m_pivotInB - m_rbB.getCenterOfMassPosition(),
			normal,
			m_rbA.getInvInertiaDiagLocal(),
			m_rbA.getInvMass(),
			m_rbB.getInvInertiaDiagLocal(),
			m_rbB.getInvMass());
		normal[i] = 0;
		}
	}

}


void btPoint2PointConstraint::getInfo1 (btConstraintInfo1* info)
{
	if (m_useSolveConstraintObsolete)
	{
		info->m_numConstraintRows = 0;
		info->nub = 0;
	} else
	{
		info->m_numConstraintRows = 3;
		info->nub = 3;
	}
}

void btPoint2PointConstraint::getInfo2 (btConstraintInfo2* info)
{
	btAssert(!m_useSolveConstraintObsolete);

	 //retrieve matrices
	btTransform body0_trans;
	body0_trans = m_rbA.getCenterOfMassTransform();
    btTransform body1_trans;
	body1_trans = m_rbB.getCenterOfMassTransform();

	// anchor points in global coordinates with respect to body PORs.
   
    // set jacobian
    info->m_J1linearAxis[0] = 1;
    info->m_J1linearAxis[info->rowskip+1] = 1;
    info->m_J1linearAxis[2*info->rowskip+2] = 1;

	btVector3 a1 = body0_trans.getBasis()*getPivotInA();
	{
		btVector3* angular0 = (btVector3*)(info->m_J1angularAxis);
		btVector3* angular1 = (btVector3*)(info->m_J1angularAxis+info->rowskip);
		btVector3* angular2 = (btVector3*)(info->m_J1angularAxis+2*info->rowskip);
		btVector3 a1neg = -a1;
		a1neg.getSkewSymmetricMatrix(angular0,angular1,angular2);
	}
    
	/*info->m_J2linearAxis[0] = -1;
    info->m_J2linearAxis[s+1] = -1;
    info->m_J2linearAxis[2*s+2] = -1;
	*/
	
	btVector3 a2 = body1_trans.getBasis()*getPivotInB();
   
	{
		btVector3 a2n = -a2;
		btVector3* angular0 = (btVector3*)(info->m_J2angularAxis);
		btVector3* angular1 = (btVector3*)(info->m_J2angularAxis+info->rowskip);
		btVector3* angular2 = (btVector3*)(info->m_J2angularAxis+2*info->rowskip);
		a2.getSkewSymmetricMatrix(angular0,angular1,angular2);
	}
    


    // set right hand side
    btScalar k = info->fps * info->erp;
    int j;

	for (j=0; j<3; j++)
    {
        info->m_constraintError[j*info->rowskip] = k * (a2[j] + body1_trans.getOrigin()[j] -                     a1[j] - body0_trans.getOrigin()[j]);
		//printf("info->m_constraintError[%d]=%f\n",j,info->m_constraintError[j]);
    }

	btScalar impulseClamp = m_setting.m_impulseClamp;//
	for (j=0; j<3; j++)
    {
		if (m_setting.m_impulseClamp > 0)
		{
			info->m_lowerLimit[j*info->rowskip] = -impulseClamp;
			info->m_upperLimit[j*info->rowskip] = impulseClamp;
		}
	}
	
}


void	btPoint2PointConstraint::solveConstraintObsolete(btSolverBody& bodyA,btSolverBody& bodyB,btScalar	timeStep)
{
	if (m_useSolveConstraintObsolete)
	{
		btVector3 pivotAInW = m_rbA.getCenterOfMassTransform()*m_pivotInA;
		btVector3 pivotBInW = m_rbB.getCenterOfMassTransform()*m_pivotInB;


		btVector3 normal(0,0,0);
		

	//	btVector3 angvelA = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
	//	btVector3 angvelB = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();

		for (int i=0;i<3;i++)
		{		
			normal[i] = 1;
			btScalar jacDiagABInv = btScalar(1.) / m_jac[i].getDiagonal();

			btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
			btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();
			//this jacobian entry could be re-used for all iterations
			
			btVector3 vel1,vel2;
			bodyA.getVelocityInLocalPointObsolete(rel_pos1,vel1);
			bodyB.getVelocityInLocalPointObsolete(rel_pos2,vel2);
			btVector3 vel = vel1 - vel2;
			
			btScalar rel_vel;
			rel_vel = normal.dot(vel);

		/*
			//velocity error (first order error)
			btScalar rel_vel = m_jac[i].getRelativeVelocity(m_rbA.getLinearVelocity(),angvelA,
															m_rbB.getLinearVelocity(),angvelB);
		*/
		
			//positional error (zeroth order error)
			btScalar depth = -(pivotAInW - pivotBInW).dot(normal); //this is the error projected on the normal
			
			btScalar deltaImpulse = depth*m_setting.m_tau/timeStep  * jacDiagABInv -  m_setting.m_damping * rel_vel * jacDiagABInv;

			btScalar impulseClamp = m_setting.m_impulseClamp;
			
			const btScalar sum = btScalar(m_appliedImpulse) + deltaImpulse;
			if (sum < -impulseClamp)
			{
				deltaImpulse = -impulseClamp-m_appliedImpulse;
				m_appliedImpulse = -impulseClamp;
			}
			else if (sum > impulseClamp) 
			{
				deltaImpulse = impulseClamp-m_appliedImpulse;
				m_appliedImpulse = impulseClamp;
			}
			else
			{
				m_appliedImpulse = sum;
			}

			
			btVector3 impulse_vector = normal * deltaImpulse;
			
			btVector3 ftorqueAxis1 = rel_pos1.cross(normal);
			btVector3 ftorqueAxis2 = rel_pos2.cross(normal);
			bodyA.applyImpulse(normal*m_rbA.getInvMass(), m_rbA.getInvInertiaTensorWorld()*ftorqueAxis1,deltaImpulse);
			bodyB.applyImpulse(normal*m_rbB.getInvMass(), m_rbB.getInvInertiaTensorWorld()*ftorqueAxis2,-deltaImpulse);


			normal[i] = 0;
		}
	}
}

void	btPoint2PointConstraint::updateRHS(btScalar	timeStep)
{
	(void)timeStep;

}

