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
Added by Roman Ponomarev (rponom@gmail.com)
April 04, 2008

TODO:
 - add clamping od accumulated impulse to improve stability
 - add conversion for ODE constraint solver
*/

#ifndef SLIDER_CONSTRAINT_H
#define SLIDER_CONSTRAINT_H

//-----------------------------------------------------------------------------

#include "LinearMath/btVector3.h"
#include "btJacobianEntry.h"
#include "btTypedConstraint.h"

//-----------------------------------------------------------------------------

class btRigidBody;

//-----------------------------------------------------------------------------

#define SLIDER_CONSTRAINT_DEF_SOFTNESS		(btScalar(1.0))
#define SLIDER_CONSTRAINT_DEF_DAMPING		(btScalar(1.0))
#define SLIDER_CONSTRAINT_DEF_RESTITUTION	(btScalar(0.7))

//-----------------------------------------------------------------------------

class btSliderConstraint : public btTypedConstraint
{
protected:
	///for backwards compatibility during the transition to 'getInfo/getInfo2'
	bool		m_useSolveConstraintObsolete;
	btTransform	m_frameInA;
    btTransform	m_frameInB;
	// use frameA fo define limits, if true
	bool m_useLinearReferenceFrameA;
	// linear limits
	btScalar m_lowerLinLimit;
	btScalar m_upperLinLimit;
	// angular limits
	btScalar m_lowerAngLimit;
	btScalar m_upperAngLimit;
	// softness, restitution and damping for different cases
	// DirLin - moving inside linear limits
	// LimLin - hitting linear limit
	// DirAng - moving inside angular limits
	// LimAng - hitting angular limit
	// OrthoLin, OrthoAng - against constraint axis
	btScalar m_softnessDirLin;
	btScalar m_restitutionDirLin;
	btScalar m_dampingDirLin;
	btScalar m_softnessDirAng;
	btScalar m_restitutionDirAng;
	btScalar m_dampingDirAng;
	btScalar m_softnessLimLin;
	btScalar m_restitutionLimLin;
	btScalar m_dampingLimLin;
	btScalar m_softnessLimAng;
	btScalar m_restitutionLimAng;
	btScalar m_dampingLimAng;
	btScalar m_softnessOrthoLin;
	btScalar m_restitutionOrthoLin;
	btScalar m_dampingOrthoLin;
	btScalar m_softnessOrthoAng;
	btScalar m_restitutionOrthoAng;
	btScalar m_dampingOrthoAng;
	
	// for interlal use
	bool m_solveLinLim;
	bool m_solveAngLim;

	btJacobianEntry	m_jacLin[3];
	btScalar		m_jacLinDiagABInv[3];

    btJacobianEntry	m_jacAng[3];

	btScalar m_timeStep;
    btTransform m_calculatedTransformA;
    btTransform m_calculatedTransformB;

	btVector3 m_sliderAxis;
	btVector3 m_realPivotAInW;
	btVector3 m_realPivotBInW;
	btVector3 m_projPivotInW;
	btVector3 m_delta;
	btVector3 m_depth;
	btVector3 m_relPosA;
	btVector3 m_relPosB;

	btScalar m_linPos;
	btScalar m_angPos;

	btScalar m_angDepth;
	btScalar m_kAngle;

	bool	 m_poweredLinMotor;
    btScalar m_targetLinMotorVelocity;
    btScalar m_maxLinMotorForce;
    btScalar m_accumulatedLinMotorImpulse;
	
	bool	 m_poweredAngMotor;
    btScalar m_targetAngMotorVelocity;
    btScalar m_maxAngMotorForce;
    btScalar m_accumulatedAngMotorImpulse;

	//------------------------    
	void initParams();
public:
	// constructors
    btSliderConstraint(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB ,bool useLinearReferenceFrameA);
    btSliderConstraint();
	// overrides
    virtual void	buildJacobian();
    virtual void getInfo1 (btConstraintInfo1* info);
	
	virtual void getInfo2 (btConstraintInfo2* info);

    virtual	void	solveConstraintObsolete(btSolverBody& bodyA,btSolverBody& bodyB,btScalar	timeStep);
	

	// access
    const btRigidBody& getRigidBodyA() const { return m_rbA; }
    const btRigidBody& getRigidBodyB() const { return m_rbB; }
    const btTransform & getCalculatedTransformA() const { return m_calculatedTransformA; }
    const btTransform & getCalculatedTransformB() const { return m_calculatedTransformB; }
    const btTransform & getFrameOffsetA() const { return m_frameInA; }
    const btTransform & getFrameOffsetB() const { return m_frameInB; }
    btTransform & getFrameOffsetA() { return m_frameInA; }
    btTransform & getFrameOffsetB() { return m_frameInB; }
    btScalar getLowerLinLimit() { return m_lowerLinLimit; }
    void setLowerLinLimit(btScalar lowerLimit) { m_lowerLinLimit = lowerLimit; }
    btScalar getUpperLinLimit() { return m_upperLinLimit; }
    void setUpperLinLimit(btScalar upperLimit) { m_upperLinLimit = upperLimit; }
    btScalar getLowerAngLimit() { return m_lowerAngLimit; }
    void setLowerAngLimit(btScalar lowerLimit) { m_lowerAngLimit = lowerLimit; }
    btScalar getUpperAngLimit() { return m_upperAngLimit; }
    void setUpperAngLimit(btScalar upperLimit) { m_upperAngLimit = upperLimit; }
	bool getUseLinearReferenceFrameA() { return m_useLinearReferenceFrameA; }
	btScalar getSoftnessDirLin() { return m_softnessDirLin; }
	btScalar getRestitutionDirLin() { return m_restitutionDirLin; }
	btScalar getDampingDirLin() { return m_dampingDirLin ; }
	btScalar getSoftnessDirAng() { return m_softnessDirAng; }
	btScalar getRestitutionDirAng() { return m_restitutionDirAng; }
	btScalar getDampingDirAng() { return m_dampingDirAng; }
	btScalar getSoftnessLimLin() { return m_softnessLimLin; }
	btScalar getRestitutionLimLin() { return m_restitutionLimLin; }
	btScalar getDampingLimLin() { return m_dampingLimLin; }
	btScalar getSoftnessLimAng() { return m_softnessLimAng; }
	btScalar getRestitutionLimAng() { return m_restitutionLimAng; }
	btScalar getDampingLimAng() { return m_dampingLimAng; }
	btScalar getSoftnessOrthoLin() { return m_softnessOrthoLin; }
	btScalar getRestitutionOrthoLin() { return m_restitutionOrthoLin; }
	btScalar getDampingOrthoLin() { return m_dampingOrthoLin; }
	btScalar getSoftnessOrthoAng() { return m_softnessOrthoAng; }
	btScalar getRestitutionOrthoAng() { return m_restitutionOrthoAng; }
	btScalar getDampingOrthoAng() { return m_dampingOrthoAng; }
	void setSoftnessDirLin(btScalar softnessDirLin) { m_softnessDirLin = softnessDirLin; }
	void setRestitutionDirLin(btScalar restitutionDirLin) { m_restitutionDirLin = restitutionDirLin; }
	void setDampingDirLin(btScalar dampingDirLin) { m_dampingDirLin = dampingDirLin; }
	void setSoftnessDirAng(btScalar softnessDirAng) { m_softnessDirAng = softnessDirAng; }
	void setRestitutionDirAng(btScalar restitutionDirAng) { m_restitutionDirAng = restitutionDirAng; }
	void setDampingDirAng(btScalar dampingDirAng) { m_dampingDirAng = dampingDirAng; }
	void setSoftnessLimLin(btScalar softnessLimLin) { m_softnessLimLin = softnessLimLin; }
	void setRestitutionLimLin(btScalar restitutionLimLin) { m_restitutionLimLin = restitutionLimLin; }
	void setDampingLimLin(btScalar dampingLimLin) { m_dampingLimLin = dampingLimLin; }
	void setSoftnessLimAng(btScalar softnessLimAng) { m_softnessLimAng = softnessLimAng; }
	void setRestitutionLimAng(btScalar restitutionLimAng) { m_restitutionLimAng = restitutionLimAng; }
	void setDampingLimAng(btScalar dampingLimAng) { m_dampingLimAng = dampingLimAng; }
	void setSoftnessOrthoLin(btScalar softnessOrthoLin) { m_softnessOrthoLin = softnessOrthoLin; }
	void setRestitutionOrthoLin(btScalar restitutionOrthoLin) { m_restitutionOrthoLin = restitutionOrthoLin; }
	void setDampingOrthoLin(btScalar dampingOrthoLin) { m_dampingOrthoLin = dampingOrthoLin; }
	void setSoftnessOrthoAng(btScalar softnessOrthoAng) { m_softnessOrthoAng = softnessOrthoAng; }
	void setRestitutionOrthoAng(btScalar restitutionOrthoAng) { m_restitutionOrthoAng = restitutionOrthoAng; }
	void setDampingOrthoAng(btScalar dampingOrthoAng) { m_dampingOrthoAng = dampingOrthoAng; }
	void setPoweredLinMotor(bool onOff) { m_poweredLinMotor = onOff; }
	bool getPoweredLinMotor() { return m_poweredLinMotor; }
	void setTargetLinMotorVelocity(btScalar targetLinMotorVelocity) { m_targetLinMotorVelocity = targetLinMotorVelocity; }
	btScalar getTargetLinMotorVelocity() { return m_targetLinMotorVelocity; }
	void setMaxLinMotorForce(btScalar maxLinMotorForce) { m_maxLinMotorForce = maxLinMotorForce; }
	btScalar getMaxLinMotorForce() { return m_maxLinMotorForce; }
	void setPoweredAngMotor(bool onOff) { m_poweredAngMotor = onOff; }
	bool getPoweredAngMotor() { return m_poweredAngMotor; }
	void setTargetAngMotorVelocity(btScalar targetAngMotorVelocity) { m_targetAngMotorVelocity = targetAngMotorVelocity; }
	btScalar getTargetAngMotorVelocity() { return m_targetAngMotorVelocity; }
	void setMaxAngMotorForce(btScalar maxAngMotorForce) { m_maxAngMotorForce = maxAngMotorForce; }
	btScalar getMaxAngMotorForce() { return m_maxAngMotorForce; }
	btScalar getLinearPos() { return m_linPos; }
	

	// access for ODE solver
	bool getSolveLinLimit() { return m_solveLinLim; }
	btScalar getLinDepth() { return m_depth[0]; }
	bool getSolveAngLimit() { return m_solveAngLim; }
	btScalar getAngDepth() { return m_angDepth; }
	// internal
    void	buildJacobianInt(btRigidBody& rbA, btRigidBody& rbB, const btTransform& frameInA, const btTransform& frameInB);
    void	solveConstraintInt(btRigidBody& rbA, btSolverBody& bodyA,btRigidBody& rbB, btSolverBody& bodyB);
	// shared code used by ODE solver
	void	calculateTransforms(void);
	void	testLinLimits(void);
	void	testLinLimits2(btConstraintInfo2* info);
	void	testAngLimits(void);
	// access for PE Solver
	btVector3 getAncorInA(void);
	btVector3 getAncorInB(void);
};

//-----------------------------------------------------------------------------

#endif //SLIDER_CONSTRAINT_H

