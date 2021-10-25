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

/** \file CcdGraphicController.h
 *  \ingroup physbullet
 */


#ifndef __CCDGRAPHICCONTROLLER_H__
#define __CCDGRAPHICCONTROLLER_H__

#include "PHY_IGraphicController.h"

#include "btBulletDynamicsCommon.h"
#include "LinearMath/btTransform.h"

#include "PHY_IMotionState.h"
#include "MT_Point3.h"

class CcdPhysicsEnvironment;
class btCollisionObject;

///CcdGraphicController is a graphic object that supports view frustrum culling and occlusion
class CcdGraphicController : public PHY_IGraphicController
{
public:
	CcdGraphicController(CcdPhysicsEnvironment* phyEnv, PHY_IMotionState* motionState);

	virtual ~CcdGraphicController();

	void SetLocalAabb(const btVector3& aabbMin,const btVector3& aabbMax);
	void SetLocalAabb(const MT_Point3& aabbMin,const MT_Point3& aabbMax);
	virtual void SetLocalAabb(const MT_Vector3& aabbMin,const MT_Vector3& aabbMax);
	virtual void SetLocalAabb(const float aabbMin[3],const float aabbMax[3]);

	PHY_IMotionState* GetMotionState() { return m_motionState; }
	void GetAabb(btVector3& aabbMin, btVector3& aabbMax);

	virtual void SetBroadphaseHandle(btBroadphaseProxy* handle) { m_handle = handle; }
	virtual btBroadphaseProxy* GetBroadphaseHandle() { return m_handle; }

	virtual void SetPhysicsEnvironment(class PHY_IPhysicsEnvironment* env);

	////////////////////////////////////
	// PHY_IGraphicController interface
	////////////////////////////////////

	/**
	 * Updates the Aabb based on the motion state
	 */
	virtual bool SetGraphicTransform();
	/**
	 * Add/remove to environment
	 */
	virtual void Activate(bool active);

	// client info for culling
	virtual	void* GetNewClientInfo() { return m_newClientInfo; }
	virtual	void SetNewClientInfo(void* clientinfo) { m_newClientInfo = clientinfo; }
	virtual PHY_IGraphicController*	GetReplica(class PHY_IMotionState* motionstate);
		
private:
	// unscaled aabb corner
	btVector3 m_localAabbMin;
	btVector3 m_localAabbMax;

	PHY_IMotionState* m_motionState;
	CcdPhysicsEnvironment* m_phyEnv;
	btBroadphaseProxy* m_handle;
	void* m_newClientInfo;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:CcdGraphicController")
#endif
};

#endif  /* BULLET2_PHYSICSCONTROLLER_H */
