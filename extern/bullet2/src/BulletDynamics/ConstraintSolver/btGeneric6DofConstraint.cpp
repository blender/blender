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


#include "btGeneric6DofConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btTransformUtil.h"
#include <new>

static const btScalar kSign[] = { btScalar(1.0), btScalar(-1.0), btScalar(1.0) };
static const int kAxisA[] = { 1, 0, 0 };
static const int kAxisB[] = { 2, 2, 1 };
#define GENERIC_D6_DISABLE_WARMSTARTING 1

btGeneric6DofConstraint::btGeneric6DofConstraint()
{
}

btGeneric6DofConstraint::btGeneric6DofConstraint(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB)
: btTypedConstraint(rbA, rbB)
, m_frameInA(frameInA)
, m_frameInB(frameInB)
{
	//free means upper < lower, 
	//locked means upper == lower
	//limited means upper > lower
	//so start all locked
	for (int i=0; i<6;++i)
	{
		m_lowerLimit[i] = btScalar(0.0);
		m_upperLimit[i] = btScalar(0.0);
		m_accumulatedImpulse[i] = btScalar(0.0);
	}

}


void btGeneric6DofConstraint::buildJacobian()
{
	btVector3	localNormalInA(0,0,0);

	const btVector3& pivotInA = m_frameInA.getOrigin();
	const btVector3& pivotInB = m_frameInB.getOrigin();

	btVector3 pivotAInW = m_rbA.getCenterOfMassTransform() * m_frameInA.getOrigin();
	btVector3 pivotBInW = m_rbB.getCenterOfMassTransform() * m_frameInB.getOrigin();

	btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
	btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();

	int i;
	//linear part
	for (i=0;i<3;i++)
	{
		if (isLimited(i))
		{
			localNormalInA[i] = 1;
			btVector3 normalWorld = m_rbA.getCenterOfMassTransform().getBasis() * localNormalInA;

			
			// Create linear atom
			new (&m_jacLinear[i]) btJacobianEntry(
				m_rbA.getCenterOfMassTransform().getBasis().transpose(),
				m_rbB.getCenterOfMassTransform().getBasis().transpose(),
				m_rbA.getCenterOfMassTransform()*pivotInA - m_rbA.getCenterOfMassPosition(),
				m_rbB.getCenterOfMassTransform()*pivotInB - m_rbB.getCenterOfMassPosition(),
				normalWorld,
				m_rbA.getInvInertiaDiagLocal(),
				m_rbA.getInvMass(),
				m_rbB.getInvInertiaDiagLocal(),
				m_rbB.getInvMass());

			//optionally disable warmstarting
#ifdef GENERIC_D6_DISABLE_WARMSTARTING
			m_accumulatedImpulse[i] = btScalar(0.);
#endif //GENERIC_D6_DISABLE_WARMSTARTING

			// Apply accumulated impulse
			btVector3 impulse_vector = m_accumulatedImpulse[i] * normalWorld;

			m_rbA.applyImpulse( impulse_vector, rel_pos1);
			m_rbB.applyImpulse(-impulse_vector, rel_pos2);

			localNormalInA[i] = 0;
		}
	}

	// angular part
	for (i=0;i<3;i++)
	{
		if (isLimited(i+3))
		{
			btVector3 axisA = m_rbA.getCenterOfMassTransform().getBasis() * m_frameInA.getBasis().getColumn( kAxisA[i] );
			btVector3 axisB = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn( kAxisB[i] );

			// Dirk: This is IMO mathematically the correct way, but we should consider axisA and axisB being near parallel maybe
			btVector3 axis = kSign[i] * axisA.cross(axisB);

			// Create angular atom
			new (&m_jacAng[i])	btJacobianEntry(axis,
				m_rbA.getCenterOfMassTransform().getBasis().transpose(),
				m_rbB.getCenterOfMassTransform().getBasis().transpose(),
				m_rbA.getInvInertiaDiagLocal(),
				m_rbB.getInvInertiaDiagLocal());

#ifdef GENERIC_D6_DISABLE_WARMSTARTING
			m_accumulatedImpulse[i + 3] = btScalar(0.);
#endif //GENERIC_D6_DISABLE_WARMSTARTING

			// Apply accumulated impulse
			btVector3 impulse_vector = m_accumulatedImpulse[i + 3] * axis;

			m_rbA.applyTorqueImpulse( impulse_vector);
			m_rbB.applyTorqueImpulse(-impulse_vector);
		}
	}
}

btScalar getMatrixElem(const btMatrix3x3& mat,int index)
{
	int row = index%3;
	int col = index / 3;
	return mat[row][col];
}

///MatrixToEulerXYZ from http://www.geometrictools.com/LibFoundation/Mathematics/Wm4Matrix3.inl.html
bool	MatrixToEulerXYZ(const btMatrix3x3& mat,btVector3& xyz)
{
    // rot =  cy*cz          -cy*sz           sy
    //        cz*sx*sy+cx*sz  cx*cz-sx*sy*sz -cy*sx
    //       -cx*cz*sy+sx*sz  cz*sx+cx*sy*sz  cx*cy

///	0..8

	 if (getMatrixElem(mat,2) < btScalar(1.0))
    {
        if (getMatrixElem(mat,2) > btScalar(-1.0))
        {
            xyz[0] = btAtan2(-getMatrixElem(mat,5),getMatrixElem(mat,8));
            xyz[1] = btAsin(getMatrixElem(mat,2));
            xyz[2] = btAtan2(-getMatrixElem(mat,1),getMatrixElem(mat,0));
            return true;
        }
        else
        {
            // WARNING.  Not unique.  XA - ZA = -atan2(r10,r11)
            xyz[0] = -btAtan2(getMatrixElem(mat,3),getMatrixElem(mat,4));
            xyz[1] = -SIMD_HALF_PI;
            xyz[2] = btScalar(0.0);
            return false;
        }
    }
    else
    {
        // WARNING.  Not unique.  XAngle + ZAngle = atan2(r10,r11)
        xyz[0] = btAtan2(getMatrixElem(mat,3),getMatrixElem(mat,4));
        xyz[1] = SIMD_HALF_PI;
        xyz[2] = 0.0;
     
    }
	 
	return false;
}


void	btGeneric6DofConstraint::solveConstraint(btScalar	timeStep)
{
	btScalar tau = btScalar(0.1);
	btScalar damping = btScalar(1.0);

	btVector3 pivotAInW = m_rbA.getCenterOfMassTransform() * m_frameInA.getOrigin();
	btVector3 pivotBInW = m_rbB.getCenterOfMassTransform() * m_frameInB.getOrigin();

	btVector3 rel_pos1 = pivotAInW - m_rbA.getCenterOfMassPosition(); 
	btVector3 rel_pos2 = pivotBInW - m_rbB.getCenterOfMassPosition();
	
	btVector3 localNormalInA(0,0,0);
	int i;

	// linear
	for (i=0;i<3;i++)
	{		
		if (isLimited(i))
		{
			btVector3 angvelA = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
			btVector3 angvelB = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();
		
			localNormalInA.setValue(0,0,0);
			localNormalInA[i] = 1;
			btVector3 normalWorld = m_rbA.getCenterOfMassTransform().getBasis() * localNormalInA;

			btScalar jacDiagABInv = btScalar(1.) / m_jacLinear[i].getDiagonal();

			//velocity error (first order error)
			btScalar rel_vel = m_jacLinear[i].getRelativeVelocity(m_rbA.getLinearVelocity(),angvelA, 
																	m_rbB.getLinearVelocity(),angvelB);
		
			//positional error (zeroth order error)
			btScalar depth = -(pivotAInW - pivotBInW).dot(normalWorld); 
			btScalar	lo = btScalar(-1e30);
			btScalar	hi = btScalar(1e30);
		
			//handle the limits
			if (m_lowerLimit[i] < m_upperLimit[i])
			{
				{
					if (depth > m_upperLimit[i])
					{
						depth -= m_upperLimit[i];
						lo = btScalar(0.);
					
					} else
					{
						if (depth < m_lowerLimit[i])
						{
							depth -= m_lowerLimit[i];
							hi = btScalar(0.);
						} else
						{
							continue;
						}
					}
				}
			}

			btScalar normalImpulse= (tau*depth/timeStep - damping*rel_vel) * jacDiagABInv;
			btScalar oldNormalImpulse = m_accumulatedImpulse[i];
			btScalar sum = oldNormalImpulse + normalImpulse;
			m_accumulatedImpulse[i] = sum > hi ? btScalar(0.) : sum < lo ? btScalar(0.) : sum;
			normalImpulse = m_accumulatedImpulse[i] - oldNormalImpulse;

			btVector3 impulse_vector = normalWorld * normalImpulse;
			m_rbA.applyImpulse( impulse_vector, rel_pos1);
			m_rbB.applyImpulse(-impulse_vector, rel_pos2);
			
			localNormalInA[i] = 0;
		}
	}

	btVector3	axis;
	btScalar	angle;
	btTransform	frameAWorld = m_rbA.getCenterOfMassTransform() * m_frameInA;
	btTransform	frameBWorld = m_rbB.getCenterOfMassTransform() * m_frameInB;

	btTransformUtil::calculateDiffAxisAngle(frameAWorld,frameBWorld,axis,angle);
	btQuaternion diff(axis,angle);
	btMatrix3x3 diffMat (diff);
	btVector3 xyz;
	///this is not perfect, we can first check which axis are limited, and choose a more appropriate order
	MatrixToEulerXYZ(diffMat,xyz);

	// angular
	for (i=0;i<3;i++)
	{
		if (isLimited(i+3))
		{
			btVector3 angvelA = m_rbA.getCenterOfMassTransform().getBasis().transpose() * m_rbA.getAngularVelocity();
			btVector3 angvelB = m_rbB.getCenterOfMassTransform().getBasis().transpose() * m_rbB.getAngularVelocity();
		
			btScalar jacDiagABInv = btScalar(1.) / m_jacAng[i].getDiagonal();
			
			//velocity error (first order error)
			btScalar rel_vel = m_jacAng[i].getRelativeVelocity(m_rbA.getLinearVelocity(),angvelA, 
																			m_rbB.getLinearVelocity(),angvelB);

			//positional error (zeroth order error)
			btVector3 axisA = m_rbA.getCenterOfMassTransform().getBasis() * m_frameInA.getBasis().getColumn( kAxisA[i] );
			btVector3 axisB = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn( kAxisB[i] );

			btScalar rel_pos = kSign[i] * axisA.dot(axisB);

			btScalar	lo = btScalar(-1e30);
			btScalar	hi = btScalar(1e30);
		
			//handle the twist limit
			if (m_lowerLimit[i+3] < m_upperLimit[i+3])
			{
				//clamp the values
				btScalar loLimit =  m_lowerLimit[i+3] > -3.1415 ? m_lowerLimit[i+3] : btScalar(-1e30);
				btScalar hiLimit = m_upperLimit[i+3] < 3.1415 ? m_upperLimit[i+3] : btScalar(1e30);

				btScalar projAngle  = btScalar(-1.)*xyz[i];
				
				if (projAngle < loLimit)
				{
					hi = btScalar(0.);
					rel_pos = (loLimit - projAngle);
				} else
				{
					if (projAngle > hiLimit)
					{
						lo = btScalar(0.);
						rel_pos = (hiLimit - projAngle);
					} else
					{
						continue;
					}
				}
			}
		
			//impulse
			
			btScalar normalImpulse= -(tau*rel_pos/timeStep + damping*rel_vel) * jacDiagABInv;
			btScalar oldNormalImpulse = m_accumulatedImpulse[i+3];
			btScalar sum = oldNormalImpulse + normalImpulse;
			m_accumulatedImpulse[i+3] = sum > hi ? btScalar(0.) : sum < lo ? btScalar(0.) : sum;
			normalImpulse = m_accumulatedImpulse[i+3] - oldNormalImpulse;
			
			// Dirk: Not needed - we could actually project onto Jacobian entry here (same as above)
			btVector3 axis = kSign[i] * axisA.cross(axisB);
			btVector3 impulse_vector = axis * normalImpulse;

			m_rbA.applyTorqueImpulse( impulse_vector);
			m_rbB.applyTorqueImpulse(-impulse_vector);
		}
	}
}

void	btGeneric6DofConstraint::updateRHS(btScalar	timeStep)
{
	(void)timeStep;

}

btScalar btGeneric6DofConstraint::computeAngle(int axis) const
	{
	btScalar angle = btScalar(0.f);

	switch (axis)
		{
		case 0:
			{
			btVector3 v1 = m_rbA.getCenterOfMassTransform().getBasis() * m_frameInA.getBasis().getColumn(1);
			btVector3 v2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(1);
			btVector3 w2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(2);

			btScalar s = v1.dot(w2);
			btScalar c = v1.dot(v2);

			angle = btAtan2( s, c );
			}
			break;

		case 1:
			{
			btVector3 w1 = m_rbA.getCenterOfMassTransform().getBasis() * m_frameInA.getBasis().getColumn(2);
			btVector3 w2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(2);
			btVector3 u2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(0);

			btScalar s = w1.dot(u2);
			btScalar c = w1.dot(w2);

			angle = btAtan2( s, c );
			}
			break;

		case 2:
			{
			btVector3 u1 = m_rbA.getCenterOfMassTransform().getBasis() * m_frameInA.getBasis().getColumn(0);
			btVector3 u2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(0);
			btVector3 v2 = m_rbB.getCenterOfMassTransform().getBasis() * m_frameInB.getBasis().getColumn(1);

			btScalar s = u1.dot(v2);
			btScalar c = u1.dot(u2);

			angle = btAtan2( s, c );
			}
			break;
                  default: 
					  btAssert ( 0 ) ; 
					  
					  break ;
		}

		return angle;
	}

