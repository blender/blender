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
/*
2007-09-09
Refactored by Francisco León
email: projectileman@yahoo.com
http://gimpact.sf.net
*/


#include "btGeneric6DofConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btTransformUtil.h"
#include <new>


static const btScalar kSign[] = { btScalar(1.0), btScalar(-1.0), btScalar(1.0) };
static const int kAxisA[] = { 1, 0, 0 };
static const int kAxisB[] = { 2, 2, 1 };
#define GENERIC_D6_DISABLE_WARMSTARTING 1

btScalar btGetMatrixElem(const btMatrix3x3& mat, int index)
{
	int i = index%3;
	int j = index/3;
	return mat[i][j];
}

///MatrixToEulerXYZ from http://www.geometrictools.com/LibFoundation/Mathematics/Wm4Matrix3.inl.html
bool	matrixToEulerXYZ(const btMatrix3x3& mat,btVector3& xyz)
{
//	// rot =  cy*cz          -cy*sz           sy
//	//        cz*sx*sy+cx*sz  cx*cz-sx*sy*sz -cy*sx
//	//       -cx*cz*sy+sx*sz  cz*sx+cx*sy*sz  cx*cy
//

		if (btGetMatrixElem(mat,2) < btScalar(1.0))
		{
			if (btGetMatrixElem(mat,2) > btScalar(-1.0))
			{
				xyz[0] = btAtan2(-btGetMatrixElem(mat,5),btGetMatrixElem(mat,8));
				xyz[1] = btAsin(btGetMatrixElem(mat,2));
				xyz[2] = btAtan2(-btGetMatrixElem(mat,1),btGetMatrixElem(mat,0));
				return true;
			}
			else
			{
				// WARNING.  Not unique.  XA - ZA = -atan2(r10,r11)
				xyz[0] = -btAtan2(btGetMatrixElem(mat,3),btGetMatrixElem(mat,4));
				xyz[1] = -SIMD_HALF_PI;
				xyz[2] = btScalar(0.0);
				return false;
			}
		}
		else
		{
			// WARNING.  Not unique.  XAngle + ZAngle = atan2(r10,r11)
			xyz[0] = btAtan2(btGetMatrixElem(mat,3),btGetMatrixElem(mat,4));
			xyz[1] = SIMD_HALF_PI;
			xyz[2] = 0.0;

		}


	return false;
}



//////////////////////////// btRotationalLimitMotor ////////////////////////////////////


int btRotationalLimitMotor::testLimitValue(btScalar test_value)
{
	if(m_loLimit>m_hiLimit)
	{
		m_currentLimit = 0;//Free from violation
		return 0;
	}

	if (test_value < m_loLimit)
	{
		m_currentLimit = 1;//low limit violation
		m_currentLimitError =  test_value - m_loLimit;
		return 1;
	}
	else if (test_value> m_hiLimit)
	{
		m_currentLimit = 2;//High limit violation
		m_currentLimitError = test_value - m_hiLimit;
		return 2;
	}
	else
	{
		m_currentLimit = 0;//Free from violation
		return 0;
	}
	return 0;
}


btScalar btRotationalLimitMotor::solveAngularLimits(
		btScalar timeStep,btVector3& axis,btScalar jacDiagABInv,
	 	btRigidBody * body0, btRigidBody * body1)
{
    if (needApplyTorques()==false) return 0.0f;

    btScalar target_velocity = m_targetVelocity;
    btScalar maxMotorForce = m_maxMotorForce;

	//current error correction
    if (m_currentLimit!=0)
    {
        target_velocity = -m_ERP*m_currentLimitError/(timeStep);
        maxMotorForce = m_maxLimitForce;
    }

    maxMotorForce *= timeStep;

    // current velocity difference
    btVector3 vel_diff = body0->getAngularVelocity();
    if (body1)
    {
        vel_diff -= body1->getAngularVelocity();
    }



    btScalar rel_vel = axis.dot(vel_diff);

	// correction velocity
    btScalar motor_relvel = m_limitSoftness*(target_velocity  - m_damping*rel_vel);


    if ( motor_relvel < SIMD_EPSILON && motor_relvel > -SIMD_EPSILON  )
    {
        return 0.0f;//no need for applying force
    }


	// correction impulse
    btScalar unclippedMotorImpulse = (1+m_bounce)*motor_relvel*jacDiagABInv;

	// clip correction impulse
    btScalar clippedMotorImpulse;

    //todo: should clip against accumulated impulse
    if (unclippedMotorImpulse>0.0f)
    {
        clippedMotorImpulse =  unclippedMotorImpulse > maxMotorForce? maxMotorForce: unclippedMotorImpulse;
    }
    else
    {
        clippedMotorImpulse =  unclippedMotorImpulse < -maxMotorForce ? -maxMotorForce: unclippedMotorImpulse;
    }


	// sort with accumulated impulses
    btScalar	lo = btScalar(-1e30);
    btScalar	hi = btScalar(1e30);

    btScalar oldaccumImpulse = m_accumulatedImpulse;
    btScalar sum = oldaccumImpulse + clippedMotorImpulse;
    m_accumulatedImpulse = sum > hi ? btScalar(0.) : sum < lo ? btScalar(0.) : sum;

    clippedMotorImpulse = m_accumulatedImpulse - oldaccumImpulse;



    btVector3 motorImp = clippedMotorImpulse * axis;


    body0->applyTorqueImpulse(motorImp);
    if (body1) body1->applyTorqueImpulse(-motorImp);

    return clippedMotorImpulse;


}

//////////////////////////// End btRotationalLimitMotor ////////////////////////////////////

//////////////////////////// btTranslationalLimitMotor ////////////////////////////////////
btScalar btTranslationalLimitMotor::solveLinearAxis(
		btScalar timeStep,
        btScalar jacDiagABInv,
        btRigidBody& body1,const btVector3 &pointInA,
        btRigidBody& body2,const btVector3 &pointInB,
        int limit_index,
        const btVector3 & axis_normal_on_a)
{

///find relative velocity
    btVector3 rel_pos1 = pointInA - body1.getCenterOfMassPosition();
    btVector3 rel_pos2 = pointInB - body2.getCenterOfMassPosition();

    btVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
    btVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
    btVector3 vel = vel1 - vel2;

    btScalar rel_vel = axis_normal_on_a.dot(vel);



/// apply displacement correction

//positional error (zeroth order error)
    btScalar depth = -(pointInA - pointInB).dot(axis_normal_on_a);
    btScalar	lo = btScalar(-1e30);
    btScalar	hi = btScalar(1e30);

    btScalar minLimit = m_lowerLimit[limit_index];
    btScalar maxLimit = m_upperLimit[limit_index];

    //handle the limits
    if (minLimit < maxLimit)
    {
        {
            if (depth > maxLimit)
            {
                depth -= maxLimit;
                lo = btScalar(0.);

            }
            else
            {
                if (depth < minLimit)
                {
                    depth -= minLimit;
                    hi = btScalar(0.);
                }
                else
                {
                    return 0.0f;
                }
            }
        }
    }

    btScalar normalImpulse= m_limitSoftness*(m_restitution*depth/timeStep - m_damping*rel_vel) * jacDiagABInv;




    btScalar oldNormalImpulse = m_accumulatedImpulse[limit_index];
    btScalar sum = oldNormalImpulse + normalImpulse;
    m_accumulatedImpulse[limit_index] = sum > hi ? btScalar(0.) : sum < lo ? btScalar(0.) : sum;
    normalImpulse = m_accumulatedImpulse[limit_index] - oldNormalImpulse;

    btVector3 impulse_vector = axis_normal_on_a * normalImpulse;
    body1.applyImpulse( impulse_vector, rel_pos1);
    body2.applyImpulse(-impulse_vector, rel_pos2);
    return normalImpulse;
}

//////////////////////////// btTranslationalLimitMotor ////////////////////////////////////


btGeneric6DofConstraint::btGeneric6DofConstraint()
        :btTypedConstraint(D6_CONSTRAINT_TYPE),
		m_useLinearReferenceFrameA(true)
{
}

btGeneric6DofConstraint::btGeneric6DofConstraint(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB, bool useLinearReferenceFrameA)
        : btTypedConstraint(D6_CONSTRAINT_TYPE, rbA, rbB)
        , m_frameInA(frameInA)
        , m_frameInB(frameInB),
		m_useLinearReferenceFrameA(useLinearReferenceFrameA)
{

}





void btGeneric6DofConstraint::calculateAngleInfo()
{
	btMatrix3x3 relative_frame = m_calculatedTransformA.getBasis().inverse()*m_calculatedTransformB.getBasis();

	matrixToEulerXYZ(relative_frame,m_calculatedAxisAngleDiff);



	// in euler angle mode we do not actually constrain the angular velocity
  // along the axes axis[0] and axis[2] (although we do use axis[1]) :
  //
  //    to get			constrain w2-w1 along		...not
  //    ------			---------------------		------
  //    d(angle[0])/dt = 0	ax[1] x ax[2]			ax[0]
  //    d(angle[1])/dt = 0	ax[1]
  //    d(angle[2])/dt = 0	ax[0] x ax[1]			ax[2]
  //
  // constraining w2-w1 along an axis 'a' means that a'*(w2-w1)=0.
  // to prove the result for angle[0], write the expression for angle[0] from
  // GetInfo1 then take the derivative. to prove this for angle[2] it is
  // easier to take the euler rate expression for d(angle[2])/dt with respect
  // to the components of w and set that to 0.

	btVector3 axis0 = m_calculatedTransformB.getBasis().getColumn(0);
	btVector3 axis2 = m_calculatedTransformA.getBasis().getColumn(2);

	m_calculatedAxis[1] = axis2.cross(axis0);
	m_calculatedAxis[0] = m_calculatedAxis[1].cross(axis2);
	m_calculatedAxis[2] = axis0.cross(m_calculatedAxis[1]);


//    if(m_debugDrawer)
//    {
//
//    	char buff[300];
//		sprintf(buff,"\n X: %.2f ; Y: %.2f ; Z: %.2f ",
//		m_calculatedAxisAngleDiff[0],
//		m_calculatedAxisAngleDiff[1],
//		m_calculatedAxisAngleDiff[2]);
//    	m_debugDrawer->reportErrorWarning(buff);
//    }

}

void btGeneric6DofConstraint::calculateTransforms()
{
    m_calculatedTransformA = m_rbA.getCenterOfMassTransform() * m_frameInA;
    m_calculatedTransformB = m_rbB.getCenterOfMassTransform() * m_frameInB;

    calculateAngleInfo();
}


void btGeneric6DofConstraint::buildLinearJacobian(
    btJacobianEntry & jacLinear,const btVector3 & normalWorld,
    const btVector3 & pivotAInW,const btVector3 & pivotBInW)
{
    new (&jacLinear) btJacobianEntry(
        m_rbA.getCenterOfMassTransform().getBasis().transpose(),
        m_rbB.getCenterOfMassTransform().getBasis().transpose(),
        pivotAInW - m_rbA.getCenterOfMassPosition(),
        pivotBInW - m_rbB.getCenterOfMassPosition(),
        normalWorld,
        m_rbA.getInvInertiaDiagLocal(),
        m_rbA.getInvMass(),
        m_rbB.getInvInertiaDiagLocal(),
        m_rbB.getInvMass());

}

void btGeneric6DofConstraint::buildAngularJacobian(
    btJacobianEntry & jacAngular,const btVector3 & jointAxisW)
{
    new (&jacAngular)	btJacobianEntry(jointAxisW,
                                      m_rbA.getCenterOfMassTransform().getBasis().transpose(),
                                      m_rbB.getCenterOfMassTransform().getBasis().transpose(),
                                      m_rbA.getInvInertiaDiagLocal(),
                                      m_rbB.getInvInertiaDiagLocal());

}

bool btGeneric6DofConstraint::testAngularLimitMotor(int axis_index)
{
    btScalar angle = m_calculatedAxisAngleDiff[axis_index];

    //test limits
    m_angularLimits[axis_index].testLimitValue(angle);
    return m_angularLimits[axis_index].needApplyTorques();
}

void btGeneric6DofConstraint::buildJacobian()
{
    //calculates transform
    calculateTransforms();

    const btVector3& pivotAInW = m_calculatedTransformA.getOrigin();
    const btVector3& pivotBInW = m_calculatedTransformB.getOrigin();


    btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition();
    btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();

    btVector3 normalWorld;
    int i;
    //linear part
    for (i=0;i<3;i++)
    {
        if (m_linearLimits.isLimited(i))
        {
			if (m_useLinearReferenceFrameA)
	            normalWorld = m_calculatedTransformA.getBasis().getColumn(i);
			else
	            normalWorld = m_calculatedTransformB.getBasis().getColumn(i);

            buildLinearJacobian(
                m_jacLinear[i],normalWorld ,
                pivotAInW,pivotBInW);

        }
    }

    // angular part
    for (i=0;i<3;i++)
    {
        //calculates error angle
        if (testAngularLimitMotor(i))
        {
            normalWorld = this->getAxis(i);
            // Create angular atom
            buildAngularJacobian(m_jacAng[i],normalWorld);
        }
    }


}


void btGeneric6DofConstraint::solveConstraint(btScalar	timeStep)
{
    m_timeStep = timeStep;

    //calculateTransforms();

    int i;

    // linear

    btVector3 pointInA = m_calculatedTransformA.getOrigin();
	btVector3 pointInB = m_calculatedTransformB.getOrigin();

	btScalar jacDiagABInv;
	btVector3 linear_axis;
    for (i=0;i<3;i++)
    {
        if (m_linearLimits.isLimited(i))
        {
            jacDiagABInv = btScalar(1.) / m_jacLinear[i].getDiagonal();

			if (m_useLinearReferenceFrameA)
	            linear_axis = m_calculatedTransformA.getBasis().getColumn(i);
			else
	            linear_axis = m_calculatedTransformB.getBasis().getColumn(i);

            m_linearLimits.solveLinearAxis(
            	m_timeStep,
            	jacDiagABInv,
            	m_rbA,pointInA,
                m_rbB,pointInB,
                i,linear_axis);

        }
    }

    // angular
    btVector3 angular_axis;
    btScalar angularJacDiagABInv;
    for (i=0;i<3;i++)
    {
        if (m_angularLimits[i].needApplyTorques())
        {

			// get axis
			angular_axis = getAxis(i);

			angularJacDiagABInv = btScalar(1.) / m_jacAng[i].getDiagonal();

			m_angularLimits[i].solveAngularLimits(m_timeStep,angular_axis,angularJacDiagABInv, &m_rbA,&m_rbB);
        }
    }
}

void	btGeneric6DofConstraint::updateRHS(btScalar	timeStep)
{
    (void)timeStep;

}

btVector3 btGeneric6DofConstraint::getAxis(int axis_index) const
{
    return m_calculatedAxis[axis_index];
}

btScalar btGeneric6DofConstraint::getAngle(int axis_index) const
{
    return m_calculatedAxisAngleDiff[axis_index];
}


