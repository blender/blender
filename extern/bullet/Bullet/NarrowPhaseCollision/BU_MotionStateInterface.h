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

#ifndef BU_MOTIONSTATE
#define BU_MOTIONSTATE


#include <SimdTransform.h>
#include <SimdPoint3.h>
#include <SimdQuaternion.h>

class BU_MotionStateInterface
{
public:
	virtual ~BU_MotionStateInterface(){};

	virtual void	SetTransform(const SimdTransform& trans) = 0;
	virtual void	GetTransform(SimdTransform& trans) const = 0; 

	virtual void	SetPosition(const SimdPoint3& position) = 0;
	virtual void	GetPosition(SimdPoint3& position) const = 0; 

	virtual void	SetOrientation(const SimdQuaternion& orientation) = 0;
	virtual void	GetOrientation(SimdQuaternion& orientation) const = 0; 

	virtual void	SetBasis(const SimdMatrix3x3& basis) = 0;
	virtual void	GetBasis(SimdMatrix3x3& basis) const = 0; 

	virtual void	SetLinearVelocity(const SimdVector3& linvel) = 0;
	virtual void	GetLinearVelocity(SimdVector3& linvel) const = 0;
	
	virtual void	GetAngularVelocity(SimdVector3& angvel) const = 0;
	virtual void	SetAngularVelocity(const SimdVector3& angvel) = 0;

};

#endif //BU_MOTIONSTATE
