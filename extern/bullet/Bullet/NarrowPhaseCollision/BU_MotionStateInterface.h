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
