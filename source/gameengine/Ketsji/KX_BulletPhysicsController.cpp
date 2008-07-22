//under visual studio the #define in KX_ConvertPhysicsObject.h is quicker for recompilation
#include "KX_ConvertPhysicsObject.h"

#ifdef USE_BULLET

#include "KX_BulletPhysicsController.h"

#include "btBulletDynamicsCommon.h"
#include "SG_Spatial.h"

#include "KX_GameObject.h"
#include "KX_MotionState.h"
#include "KX_ClientObjectInfo.h"

#include "PHY_IPhysicsEnvironment.h"
#include "CcdPhysicsEnvironment.h"


KX_BulletPhysicsController::KX_BulletPhysicsController (const CcdConstructionInfo& ci, bool dyna)
: KX_IPhysicsController(dyna,(PHY_IPhysicsController*)this),
CcdPhysicsController(ci),
m_savedCollisionFlags(0)
{

}
	
KX_BulletPhysicsController::~KX_BulletPhysicsController ()
{
	// The game object has a direct link to 
	if (m_pObject)
	{
		// If we cheat in SetObject, we must also cheat here otherwise the 
		// object will still things it has a physical controller
		// Note that it requires that m_pObject is reset in case the object is deleted
		// before the controller (usual case, see KX_Scene::RemoveNodeDestructObjec)
		// The non usual case is when the object is not deleted because its reference is hanging
		// in a AddObject actuator but the node is deleted. This case is covered here.
		KX_GameObject* gameobj = (KX_GameObject*)	m_pObject->GetSGClientObject();
		gameobj->SetPhysicsController(NULL,false);
	}
}

void	KX_BulletPhysicsController::resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ)
{
	CcdPhysicsController::resolveCombinedVelocities(linvelX,linvelY,linvelZ,angVelX,angVelY,angVelZ);

}


	///////////////////////////////////
	//	KX_IPhysicsController interface
	////////////////////////////////////

void	KX_BulletPhysicsController::applyImpulse(const MT_Point3& attach, const MT_Vector3& impulse)
{
		CcdPhysicsController::applyImpulse(attach[0],attach[1],attach[2],impulse[0],impulse[1],impulse[2]);

}

void	KX_BulletPhysicsController::SetObject (SG_IObject* object)
{
	SG_Controller::SetObject(object);

	// cheating here...
	//should not be necessary, is it for duplicates ?

	KX_GameObject* gameobj = (KX_GameObject*)	object->GetSGClientObject();
	gameobj->SetPhysicsController(this,gameobj->IsDynamic());
	CcdPhysicsController::setNewClientInfo(gameobj->getClientInfo());


}


void	KX_BulletPhysicsController::setMargin (float collisionMargin)
{
	CcdPhysicsController::SetMargin(collisionMargin);
}
void	KX_BulletPhysicsController::RelativeTranslate(const MT_Vector3& dloc,bool local)
{
	CcdPhysicsController::RelativeTranslate(dloc[0],dloc[1],dloc[2],local);

}

void	KX_BulletPhysicsController::RelativeRotate(const MT_Matrix3x3& drot,bool local)
{
	float	rotval[12];
	drot.getValue(rotval);
	CcdPhysicsController::RelativeRotate(rotval,local);
}

void	KX_BulletPhysicsController::ApplyTorque(const MT_Vector3& torque,bool local)
{
		CcdPhysicsController::ApplyTorque(torque.x(),torque.y(),torque.z(),local);
}
void	KX_BulletPhysicsController::ApplyForce(const MT_Vector3& force,bool local)
{
	CcdPhysicsController::ApplyForce(force.x(),force.y(),force.z(),local);
}
MT_Vector3 KX_BulletPhysicsController::GetLinearVelocity()
{
	float angVel[3];
	//CcdPhysicsController::GetAngularVelocity(angVel[0],angVel[1],angVel[2]);
	CcdPhysicsController::GetLinearVelocity(angVel[0],angVel[1],angVel[2]);//rcruiz
	return MT_Vector3(angVel[0],angVel[1],angVel[2]);
}
MT_Vector3 KX_BulletPhysicsController::GetAngularVelocity()
{
	float angVel[3];
	//CcdPhysicsController::GetAngularVelocity(angVel[0],angVel[1],angVel[2]);
	CcdPhysicsController::GetAngularVelocity(angVel[0],angVel[1],angVel[2]);//rcruiz
	return MT_Vector3(angVel[0],angVel[1],angVel[2]);
}
MT_Vector3 KX_BulletPhysicsController::GetVelocity(const MT_Point3& pos)
{
	float linVel[3];
	CcdPhysicsController::GetLinearVelocity(linVel[0],linVel[1],linVel[2]);
	return MT_Vector3(linVel[0],linVel[1],linVel[2]);
}

void	KX_BulletPhysicsController::SetAngularVelocity(const MT_Vector3& ang_vel,bool local)
{
	CcdPhysicsController::SetAngularVelocity(ang_vel.x(),ang_vel.y(),ang_vel.z(),local);

}
void	KX_BulletPhysicsController::SetLinearVelocity(const MT_Vector3& lin_vel,bool local)
{
	CcdPhysicsController::SetLinearVelocity(lin_vel.x(),lin_vel.y(),lin_vel.z(),local);
}
void	KX_BulletPhysicsController::getOrientation(MT_Quaternion& orn)
{
	float myorn[4];
	CcdPhysicsController::getOrientation(myorn[0],myorn[1],myorn[2],myorn[3]);
	orn = MT_Quaternion(myorn[0],myorn[1],myorn[2],myorn[3]);
}
void KX_BulletPhysicsController::setOrientation(const MT_Matrix3x3& orn)
{
	btMatrix3x3 btmat(orn[0][0], orn[0][1], orn[0][2], orn[1][0], orn[1][1], orn[1][2], orn[2][0], orn[2][1], orn[2][2]);
	CcdPhysicsController::setWorldOrientation(btmat);
}
void KX_BulletPhysicsController::setPosition(const MT_Point3& pos)
{
	CcdPhysicsController::setPosition(pos.x(),pos.y(),pos.z());
}
void KX_BulletPhysicsController::setScaling(const MT_Vector3& scaling)
{
	CcdPhysicsController::setScaling(scaling.x(),scaling.y(),scaling.z());
}
MT_Scalar	KX_BulletPhysicsController::GetMass()
{

	MT_Scalar invmass = GetRigidBody()->getInvMass();
	if (invmass)
		return 1.f/invmass;
	return 0.f;

}
MT_Vector3	KX_BulletPhysicsController::getReactionForce()
{
	assert(0);
	return MT_Vector3(0.f,0.f,0.f);
}
void	KX_BulletPhysicsController::setRigidBody(bool rigid)
{
}

void	KX_BulletPhysicsController::SuspendDynamics(bool ghost)
{
	btRigidBody *body = GetRigidBody();
	if (body->getActivationState() != DISABLE_SIMULATION)
	{
		btBroadphaseProxy* handle = body->getBroadphaseHandle();
		m_savedCollisionFlags = body->getCollisionFlags();
		m_savedMass = GetMass();
		m_savedCollisionFilterGroup = handle->m_collisionFilterGroup;
		m_savedCollisionFilterMask = handle->m_collisionFilterMask;
		body->setActivationState(DISABLE_SIMULATION);
		GetPhysicsEnvironment()->updateCcdPhysicsController(this, 
			0.0,
			btCollisionObject::CF_STATIC_OBJECT|((ghost)?btCollisionObject::CF_NO_CONTACT_RESPONSE:(m_savedCollisionFlags&btCollisionObject::CF_NO_CONTACT_RESPONSE)),
			btBroadphaseProxy::StaticFilter, 
			btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter);
	}
}

void	KX_BulletPhysicsController::RestoreDynamics()
{
	btRigidBody *body = GetRigidBody();
	if (body->getActivationState() == DISABLE_SIMULATION)
	{
		GetPhysicsEnvironment()->updateCcdPhysicsController(this, 
			m_savedMass,
			m_savedCollisionFlags,
			m_savedCollisionFilterGroup,
			m_savedCollisionFilterMask);
		GetRigidBody()->forceActivationState(ACTIVE_TAG);
	}
}

SG_Controller*	KX_BulletPhysicsController::GetReplica(class SG_Node* destnode)
{
	PHY_IMotionState* motionstate = new KX_MotionState(destnode);

	KX_BulletPhysicsController* physicsreplica = new KX_BulletPhysicsController(*this);

	//parentcontroller is here be able to avoid collisions between parent/child

	PHY_IPhysicsController* parentctrl = NULL;
	
	if (destnode != destnode->GetRootSGParent())
	{
		KX_GameObject* clientgameobj = (KX_GameObject*) destnode->GetRootSGParent()->GetSGClientObject();
		if (clientgameobj)
		{
			parentctrl = (KX_BulletPhysicsController*)clientgameobj->GetPhysicsController();
		} else
		{
			// it could be a false node, try the children
			NodeList::const_iterator childit;
			for (
				childit = destnode->GetSGChildren().begin();
			childit!= destnode->GetSGChildren().end();
			++childit
				) {
				KX_GameObject *clientgameobj = static_cast<KX_GameObject*>( (*childit)->GetSGClientObject());
				if (clientgameobj)
				{
					parentctrl = (KX_BulletPhysicsController*)clientgameobj->GetPhysicsController();
				}
			}
		}
	}

	physicsreplica->PostProcessReplica(motionstate,parentctrl);
	physicsreplica->m_userdata = (PHY_IPhysicsController*)physicsreplica;
	return physicsreplica;
	
}



void	KX_BulletPhysicsController::SetSumoTransform(bool nondynaonly)
{
	GetRigidBody()->activate(true);

	if (!m_bDyna)
	{
		GetRigidBody()->setCollisionFlags(GetRigidBody()->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	} else
	{
		if (!nondynaonly)
		{
			btTransform worldTrans;
			GetRigidBody()->getMotionState()->getWorldTransform(worldTrans);
			GetRigidBody()->setCenterOfMassTransform(worldTrans);
			
			/*
			scaling?
			if (m_bDyna)
			{
				m_sumoObj->setScaling(MT_Vector3(1,1,1));
			} else
			{
				MT_Vector3 scale;
				GetWorldScaling(scale);
				m_sumoObj->setScaling(scale);
			}
			*/

		}
	}
}

// todo: remove next line !
void	KX_BulletPhysicsController::SetSimulatedTime(double time)
{
}
	
// call from scene graph to update
bool KX_BulletPhysicsController::Update(double time)
{
	return false;

	// todo: check this code
	//if (GetMass())
	//{
	//	return false;//true;
//	}
//	return false;
}

#endif //#ifdef USE_BULLET
