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
btGeneric6DofConstraint Refactored by Francisco Le?n
email: projectileman@yahoo.com
http://gimpact.sf.net
*/


#ifndef GENERIC_6DOF_CONSTRAINT_H
#define GENERIC_6DOF_CONSTRAINT_H

#include "LinearMath/btVector3.h"
#include "btJacobianEntry.h"
#include "btTypedConstraint.h"

class btRigidBody;




//! Rotation Limit structure for generic joints
class btRotationalLimitMotor
{
public:
    //! limit_parameters
    //!@{
    btScalar m_loLimit;//!< joint limit
    btScalar m_hiLimit;//!< joint limit
    btScalar m_targetVelocity;//!< target motor velocity
    btScalar m_maxMotorForce;//!< max force on motor
    btScalar m_maxLimitForce;//!< max force on limit
    btScalar m_damping;//!< Damping.
    btScalar m_limitSoftness;//! Relaxation factor
    btScalar m_ERP;//!< Error tolerance factor when joint is at limit
    btScalar m_bounce;//!< restitution factor
    bool m_enableMotor;

    //!@}

    //! temp_variables
    //!@{
    btScalar m_currentLimitError;//!  How much is violated this limit
    int m_currentLimit;//!< 0=free, 1=at lo limit, 2=at hi limit
    btScalar m_accumulatedImpulse;
    //!@}

    btRotationalLimitMotor()
    {
    	m_accumulatedImpulse = 0.f;
        m_targetVelocity = 0;
        m_maxMotorForce = 0.1f;
        m_maxLimitForce = 300.0f;
        m_loLimit = -SIMD_INFINITY;
        m_hiLimit = SIMD_INFINITY;
        m_ERP = 0.5f;
        m_bounce = 0.0f;
        m_damping = 1.0f;
        m_limitSoftness = 0.5f;
        m_currentLimit = 0;
        m_currentLimitError = 0;
        m_enableMotor = false;
    }

    btRotationalLimitMotor(const btRotationalLimitMotor & limot)
    {
        m_targetVelocity = limot.m_targetVelocity;
        m_maxMotorForce = limot.m_maxMotorForce;
        m_limitSoftness = limot.m_limitSoftness;
        m_loLimit = limot.m_loLimit;
        m_hiLimit = limot.m_hiLimit;
        m_ERP = limot.m_ERP;
        m_bounce = limot.m_bounce;
        m_currentLimit = limot.m_currentLimit;
        m_currentLimitError = limot.m_currentLimitError;
        m_enableMotor = limot.m_enableMotor;
    }



	//! Is limited
    bool isLimited()
    {
    	if(m_loLimit > m_hiLimit) return false;
    	return true;
    }

	//! Need apply correction
    bool needApplyTorques()
    {
    	if(m_currentLimit == 0 && m_enableMotor == false) return false;
    	return true;
    }

	//! calculates  error
	/*!
	calculates m_currentLimit and m_currentLimitError.
	*/
	int testLimitValue(btScalar test_value);

	//! apply the correction impulses for two bodies
    btScalar solveAngularLimits(btScalar timeStep,btVector3& axis, btScalar jacDiagABInv,btRigidBody * body0, btSolverBody& bodyA,btRigidBody * body1,btSolverBody& bodyB);

};



class btTranslationalLimitMotor
{
public:
	btVector3 m_lowerLimit;//!< the constraint lower limits
    btVector3 m_upperLimit;//!< the constraint upper limits
    btVector3 m_accumulatedImpulse;
    //! Linear_Limit_parameters
    //!@{
    btScalar	m_limitSoftness;//!< Softness for linear limit
    btScalar	m_damping;//!< Damping for linear limit
    btScalar	m_restitution;//! Bounce parameter for linear limit
    //!@}
	bool		m_enableMotor[3];
    btVector3	m_targetVelocity;//!< target motor velocity
    btVector3	m_maxMotorForce;//!< max force on motor
    btVector3	m_currentLimitError;//!  How much is violated this limit
    int			m_currentLimit[3];//!< 0=free, 1=at lower limit, 2=at upper limit

    btTranslationalLimitMotor()
    {
    	m_lowerLimit.setValue(0.f,0.f,0.f);
    	m_upperLimit.setValue(0.f,0.f,0.f);
    	m_accumulatedImpulse.setValue(0.f,0.f,0.f);

    	m_limitSoftness = 0.7f;
    	m_damping = btScalar(1.0f);
    	m_restitution = btScalar(0.5f);
		for(int i=0; i < 3; i++) 
		{
			m_enableMotor[i] = false;
			m_targetVelocity[i] = btScalar(0.f);
			m_maxMotorForce[i] = btScalar(0.f);
		}
    }

    btTranslationalLimitMotor(const btTranslationalLimitMotor & other )
    {
    	m_lowerLimit = other.m_lowerLimit;
    	m_upperLimit = other.m_upperLimit;
    	m_accumulatedImpulse = other.m_accumulatedImpulse;

    	m_limitSoftness = other.m_limitSoftness ;
    	m_damping = other.m_damping;
    	m_restitution = other.m_restitution;
		for(int i=0; i < 3; i++) 
		{
			m_enableMotor[i] = other.m_enableMotor[i];
			m_targetVelocity[i] = other.m_targetVelocity[i];
			m_maxMotorForce[i] = other.m_maxMotorForce[i];
		}
    }

    //! Test limit
	/*!
    - free means upper < lower,
    - locked means upper == lower
    - limited means upper > lower
    - limitIndex: first 3 are linear, next 3 are angular
    */
    inline bool	isLimited(int limitIndex)
    {
       return (m_upperLimit[limitIndex] >= m_lowerLimit[limitIndex]);
    }
    inline bool needApplyForce(int limitIndex)
    {
    	if(m_currentLimit[limitIndex] == 0 && m_enableMotor[limitIndex] == false) return false;
    	return true;
    }
	int testLimitValue(int limitIndex, btScalar test_value);


    btScalar solveLinearAxis(
    	btScalar timeStep,
        btScalar jacDiagABInv,
        btRigidBody& body1,btSolverBody& bodyA,const btVector3 &pointInA,
        btRigidBody& body2,btSolverBody& bodyB,const btVector3 &pointInB,
        int limit_index,
        const btVector3 & axis_normal_on_a,
		const btVector3 & anchorPos);


};

/// btGeneric6DofConstraint between two rigidbodies each with a pivotpoint that descibes the axis location in local space
/*!
btGeneric6DofConstraint can leave any of the 6 degree of freedom 'free' or 'locked'.
currently this limit supports rotational motors<br>
<ul>
<li> For Linear limits, use btGeneric6DofConstraint.setLinearUpperLimit, btGeneric6DofConstraint.setLinearLowerLimit. You can set the parameters with the btTranslationalLimitMotor structure accsesible through the btGeneric6DofConstraint.getTranslationalLimitMotor method.
At this moment translational motors are not supported. May be in the future. </li>

<li> For Angular limits, use the btRotationalLimitMotor structure for configuring the limit.
This is accessible through btGeneric6DofConstraint.getLimitMotor method,
This brings support for limit parameters and motors. </li>

<li> Angulars limits have these possible ranges:
<table border=1 >
<tr

	<td><b>AXIS</b></td>
	<td><b>MIN ANGLE</b></td>
	<td><b>MAX ANGLE</b></td>
	<td>X</td>
		<td>-PI</td>
		<td>PI</td>
	<td>Y</td>
		<td>-PI/2</td>
		<td>PI/2</td>
	<td>Z</td>
		<td>-PI/2</td>
		<td>PI/2</td>
</tr>
</table>
</li>
</ul>

*/
class btGeneric6DofConstraint : public btTypedConstraint
{
protected:

	//! relative_frames
    //!@{
	btTransform	m_frameInA;//!< the constraint space w.r.t body A
    btTransform	m_frameInB;//!< the constraint space w.r.t body B
    //!@}

    //! Jacobians
    //!@{
    btJacobianEntry	m_jacLinear[3];//!< 3 orthogonal linear constraints
    btJacobianEntry	m_jacAng[3];//!< 3 orthogonal angular constraints
    //!@}

	//! Linear_Limit_parameters
    //!@{
    btTranslationalLimitMotor m_linearLimits;
    //!@}


    //! hinge_parameters
    //!@{
    btRotationalLimitMotor m_angularLimits[3];
	//!@}


protected:
    //! temporal variables
    //!@{
    btScalar m_timeStep;
    btTransform m_calculatedTransformA;
    btTransform m_calculatedTransformB;
    btVector3 m_calculatedAxisAngleDiff;
    btVector3 m_calculatedAxis[3];
    btVector3 m_calculatedLinearDiff;
    
	btVector3 m_AnchorPos; // point betwen pivots of bodies A and B to solve linear axes

    bool	m_useLinearReferenceFrameA;
    
    //!@}

    btGeneric6DofConstraint&	operator=(btGeneric6DofConstraint&	other)
    {
        btAssert(0);
        (void) other;
        return *this;
    }


	int setAngularLimits(btConstraintInfo2 *info, int row_offset);

	int setLinearLimits(btConstraintInfo2 *info);

    void buildLinearJacobian(
        btJacobianEntry & jacLinear,const btVector3 & normalWorld,
        const btVector3 & pivotAInW,const btVector3 & pivotBInW);

    void buildAngularJacobian(btJacobianEntry & jacAngular,const btVector3 & jointAxisW);

	// tests linear limits
	void calculateLinearInfo();

	//! calcs the euler angles between the two bodies.
    void calculateAngleInfo();



public:

	///for backwards compatibility during the transition to 'getInfo/getInfo2'
	bool		m_useSolveConstraintObsolete;

    btGeneric6DofConstraint(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB ,bool useLinearReferenceFrameA);

    btGeneric6DofConstraint();

	//! Calcs global transform of the offsets
	/*!
	Calcs the global transform for the joint offset for body A an B, and also calcs the agle differences between the bodies.
	\sa btGeneric6DofConstraint.getCalculatedTransformA , btGeneric6DofConstraint.getCalculatedTransformB, btGeneric6DofConstraint.calculateAngleInfo
	*/
    void calculateTransforms();

	//! Gets the global transform of the offset for body A
    /*!
    \sa btGeneric6DofConstraint.getFrameOffsetA, btGeneric6DofConstraint.getFrameOffsetB, btGeneric6DofConstraint.calculateAngleInfo.
    */
    const btTransform & getCalculatedTransformA() const
    {
    	return m_calculatedTransformA;
    }

    //! Gets the global transform of the offset for body B
    /*!
    \sa btGeneric6DofConstraint.getFrameOffsetA, btGeneric6DofConstraint.getFrameOffsetB, btGeneric6DofConstraint.calculateAngleInfo.
    */
    const btTransform & getCalculatedTransformB() const
    {
    	return m_calculatedTransformB;
    }

    const btTransform & getFrameOffsetA() const
    {
    	return m_frameInA;
    }

    const btTransform & getFrameOffsetB() const
    {
    	return m_frameInB;
    }


    btTransform & getFrameOffsetA()
    {
    	return m_frameInA;
    }

    btTransform & getFrameOffsetB()
    {
    	return m_frameInB;
    }


	//! performs Jacobian calculation, and also calculates angle differences and axis
    virtual void	buildJacobian();

	virtual void getInfo1 (btConstraintInfo1* info);

	virtual void getInfo2 (btConstraintInfo2* info);

    virtual	void	solveConstraintObsolete(btSolverBody& bodyA,btSolverBody& bodyB,btScalar	timeStep);

    void	updateRHS(btScalar	timeStep);

	//! Get the rotation axis in global coordinates
	/*!
	\pre btGeneric6DofConstraint.buildJacobian must be called previously.
	*/
    btVector3 getAxis(int axis_index) const;

    //! Get the relative Euler angle
    /*!
	\pre btGeneric6DofConstraint.buildJacobian must be called previously.
	*/
    btScalar getAngle(int axis_index) const;

	//! Test angular limit.
	/*!
	Calculates angular correction and returns true if limit needs to be corrected.
	\pre btGeneric6DofConstraint.buildJacobian must be called previously.
	*/
    bool testAngularLimitMotor(int axis_index);

    void	setLinearLowerLimit(const btVector3& linearLower)
    {
    	m_linearLimits.m_lowerLimit = linearLower;
    }

    void	setLinearUpperLimit(const btVector3& linearUpper)
    {
    	m_linearLimits.m_upperLimit = linearUpper;
    }

    void	setAngularLowerLimit(const btVector3& angularLower)
    {
        m_angularLimits[0].m_loLimit = angularLower.getX();
        m_angularLimits[1].m_loLimit = angularLower.getY();
        m_angularLimits[2].m_loLimit = angularLower.getZ();
    }

    void	setAngularUpperLimit(const btVector3& angularUpper)
    {
        m_angularLimits[0].m_hiLimit = angularUpper.getX();
        m_angularLimits[1].m_hiLimit = angularUpper.getY();
        m_angularLimits[2].m_hiLimit = angularUpper.getZ();
    }

	//! Retrieves the angular limit informacion
    btRotationalLimitMotor * getRotationalLimitMotor(int index)
    {
    	return &m_angularLimits[index];
    }

    //! Retrieves the  limit informacion
    btTranslationalLimitMotor * getTranslationalLimitMotor()
    {
    	return &m_linearLimits;
    }

    //first 3 are linear, next 3 are angular
    void setLimit(int axis, btScalar lo, btScalar hi)
    {
    	if(axis<3)
    	{
    		m_linearLimits.m_lowerLimit[axis] = lo;
    		m_linearLimits.m_upperLimit[axis] = hi;
    	}
    	else
    	{
    		m_angularLimits[axis-3].m_loLimit = lo;
    		m_angularLimits[axis-3].m_hiLimit = hi;
    	}
    }

	//! Test limit
	/*!
    - free means upper < lower,
    - locked means upper == lower
    - limited means upper > lower
    - limitIndex: first 3 are linear, next 3 are angular
    */
    bool	isLimited(int limitIndex)
    {
    	if(limitIndex<3)
    	{
			return m_linearLimits.isLimited(limitIndex);

    	}
        return m_angularLimits[limitIndex-3].isLimited();
    }

    const btRigidBody& getRigidBodyA() const
    {
        return m_rbA;
    }
    const btRigidBody& getRigidBodyB() const
    {
        return m_rbB;
    }

	virtual void calcAnchorPos(void); // overridable

	int get_limit_motor_info2(	btRotationalLimitMotor * limot,
								btRigidBody * body0, btRigidBody * body1,
								btConstraintInfo2 *info, int row, btVector3& ax1, int rotational);


};

#endif //GENERIC_6DOF_CONSTRAINT_H
