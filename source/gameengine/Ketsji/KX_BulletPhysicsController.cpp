/** \file gameengine/Ketsji/KX_BulletPhysicsController.cpp
 *  \ingroup ketsji
 */
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
#include "BulletSoftBody/btSoftBody.h"


KX_BulletPhysicsController::KX_BulletPhysicsController (const CcdConstructionInfo& ci, bool dyna, bool sensor, bool compound)
: KX_IPhysicsController(dyna,sensor,compound,(PHY_IPhysicsController*)this),
CcdPhysicsController(ci),
m_savedCollisionFlags(0),
m_savedCollisionFilterGroup(0),
m_savedCollisionFilterMask(0),
m_savedMass(0.0),
m_savedDyna(false),
m_suspended(false),
m_bulletChildShape(NULL)
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

float KX_BulletPhysicsController::GetLinVelocityMin()
{
	return (float)CcdPhysicsController::GetLinVelocityMin();
}
void  KX_BulletPhysicsController::SetLinVelocityMin(float val)
{
	CcdPhysicsController::SetLinVelocityMin(val);
}

float KX_BulletPhysicsController::GetLinVelocityMax()
{
	return (float)CcdPhysicsController::GetLinVelocityMax();
}
void  KX_BulletPhysicsController::SetLinVelocityMax(float val)
{
	CcdPhysicsController::SetLinVelocityMax(val);
}

void	KX_BulletPhysicsController::SetObject (SG_IObject* object)
{
	SG_Controller::SetObject(object);

	// cheating here...
	//should not be necessary, is it for duplicates ?

	KX_GameObject* gameobj = (KX_GameObject*)	object->GetSGClientObject();
	gameobj->SetPhysicsController(this,gameobj->IsDynamic());
	CcdPhysicsController::setNewClientInfo(gameobj->getClientInfo());

	if (m_bSensor)
	{
		// use a different callback function for sensor object, 
		// bullet will not synchronize, we must do it explicitly
		SG_Callbacks& callbacks = gameobj->GetSGNode()->GetCallBackFunctions();
		callbacks.m_updatefunc = KX_GameObject::SynchronizeTransformFunc;
	} 
}

MT_Scalar KX_BulletPhysicsController::GetRadius()
{
	return MT_Scalar(CcdPhysicsController::GetRadius());
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
	CcdPhysicsController::GetVelocity(pos[0], pos[1], pos[2], linVel[0],linVel[1],linVel[2]);
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
void KX_BulletPhysicsController::SetTransform()
{
	btVector3 pos;
	btVector3 scale;
	float ori[12];
	m_MotionState->getWorldPosition(pos.m_floats[0],pos.m_floats[1],pos.m_floats[2]);
	m_MotionState->getWorldScaling(scale.m_floats[0],scale.m_floats[1],scale.m_floats[2]);
	m_MotionState->getWorldOrientation(ori);
	btMatrix3x3 rot(ori[0], ori[4], ori[8],
					ori[1], ori[5], ori[9],
					ori[2], ori[6], ori[10]);
	CcdPhysicsController::forceWorldTransform(rot, pos);
}

MT_Scalar	KX_BulletPhysicsController::GetMass()
{
	if (GetSoftBody())
		return GetSoftBody()->getTotalMass();
	
	MT_Scalar invmass = 0.f;
	if (GetRigidBody())
		invmass = GetRigidBody()->getInvMass();
	if (invmass)
		return 1.f/invmass;
	return 0.f;

}

MT_Vector3 KX_BulletPhysicsController::GetLocalInertia()
{
	MT_Vector3 inertia(0.f, 0.f, 0.f);
	btVector3 inv_inertia;
	if (GetRigidBody()) {
		inv_inertia = GetRigidBody()->getInvInertiaDiagLocal();
		if (!btFuzzyZero(inv_inertia.getX()) &&
		        !btFuzzyZero(inv_inertia.getY()) &&
		        !btFuzzyZero(inv_inertia.getZ()))
			inertia = MT_Vector3(1.f/inv_inertia.getX(), 1.f/inv_inertia.getY(), 1.f/inv_inertia.getZ());
	}
	return inertia;
}

MT_Vector3	KX_BulletPhysicsController::getReactionForce()
{
	assert(0);
	return MT_Vector3(0.f,0.f,0.f);
}
void	KX_BulletPhysicsController::setRigidBody(bool rigid)
{
}

/* This function dynamically adds the collision shape of another controller to
 * the current controller shape provided it is a compound shape.
 * The idea is that dynamic parenting on a compound object will dynamically extend the shape
 */
void    KX_BulletPhysicsController::AddCompoundChild(KX_IPhysicsController* child)
{ 
	if (child == NULL || !IsCompound())
		return;
	// other controller must be a bullet controller too
	// verify that body and shape exist and match
	KX_BulletPhysicsController* childCtrl = dynamic_cast<KX_BulletPhysicsController*>(child);
	btRigidBody* rootBody = GetRigidBody();
	btRigidBody* childBody = childCtrl->GetRigidBody();
	if (!rootBody || !childBody)
		return;
	const btCollisionShape* rootShape = rootBody->getCollisionShape();
	const btCollisionShape* childShape = childBody->getCollisionShape();
	if (!rootShape || 
		!childShape || 
		rootShape->getShapeType() != COMPOUND_SHAPE_PROXYTYPE ||
		childShape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE)
		return;
	btCompoundShape* compoundShape = (btCompoundShape*)rootShape;
	// compute relative transformation between parent and child
	btTransform rootTrans;
	btTransform childTrans;
	rootBody->getMotionState()->getWorldTransform(rootTrans);
	childBody->getMotionState()->getWorldTransform(childTrans);
	btVector3 rootScale = rootShape->getLocalScaling();
	rootScale[0] = 1.0/rootScale[0];
	rootScale[1] = 1.0/rootScale[1];
	rootScale[2] = 1.0/rootScale[2];
	// relative scale = child_scale/parent_scale
	btVector3 relativeScale = childShape->getLocalScaling()*rootScale;
	btMatrix3x3 rootRotInverse = rootTrans.getBasis().transpose();	
	// relative pos = parent_rot^-1 * ((parent_pos-child_pos)/parent_scale)
	btVector3 relativePos = rootRotInverse*((childTrans.getOrigin()-rootTrans.getOrigin())*rootScale);
	// relative rot = parent_rot^-1 * child_rot
	btMatrix3x3 relativeRot = rootRotInverse*childTrans.getBasis();
	// create a proxy shape info to store the transformation
	CcdShapeConstructionInfo* proxyShapeInfo = new CcdShapeConstructionInfo();
	// store the transformation to this object shapeinfo
	proxyShapeInfo->m_childTrans.setOrigin(relativePos);
	proxyShapeInfo->m_childTrans.setBasis(relativeRot);
	proxyShapeInfo->m_childScale.setValue(relativeScale[0], relativeScale[1], relativeScale[2]);
	// we will need this to make sure that we remove the right proxy later when unparenting
	proxyShapeInfo->m_userData = childCtrl;
	proxyShapeInfo->SetProxy(childCtrl->GetShapeInfo()->AddRef());
	// add to parent compound shapeinfo (increments ref count)
	GetShapeInfo()->AddShape(proxyShapeInfo);
	// create new bullet collision shape from the object shapeinfo and set scaling
	btCollisionShape* newChildShape = proxyShapeInfo->CreateBulletShape(childCtrl->GetMargin(), childCtrl->getConstructionInfo().m_bGimpact, true);
	newChildShape->setLocalScaling(relativeScale);
	// add bullet collision shape to parent compound collision shape
	compoundShape->addChildShape(proxyShapeInfo->m_childTrans,newChildShape);
	// proxyShapeInfo is not needed anymore, release it
	proxyShapeInfo->Release();
	// remember we created this shape
	childCtrl->m_bulletChildShape = newChildShape;
	// recompute inertia of parent
	if (!rootBody->isStaticOrKinematicObject())
	{
		btVector3 localInertia;
		float mass = 1.f/rootBody->getInvMass();
		compoundShape->calculateLocalInertia(mass,localInertia);
		rootBody->setMassProps(mass,localInertia);
	}
	// must update the broadphase cache,
	GetPhysicsEnvironment()->refreshCcdPhysicsController(this);
	// remove the children
	GetPhysicsEnvironment()->disableCcdPhysicsController(childCtrl);
}

/* Reverse function of the above, it will remove a shape from a compound shape
 * provided that the former was added to the later using  AddCompoundChild()
 */
void    KX_BulletPhysicsController::RemoveCompoundChild(KX_IPhysicsController* child)
{ 
	if (child == NULL || !IsCompound())
		return;
	// other controller must be a bullet controller too
	// verify that body and shape exist and match
	KX_BulletPhysicsController* childCtrl = dynamic_cast<KX_BulletPhysicsController*>(child);
	btRigidBody* rootBody = GetRigidBody();
	btRigidBody* childBody = childCtrl->GetRigidBody();
	if (!rootBody || !childBody)
		return;
	const btCollisionShape* rootShape = rootBody->getCollisionShape();
	if (!rootShape || 
		rootShape->getShapeType() != COMPOUND_SHAPE_PROXYTYPE)
		return;
	btCompoundShape* compoundShape = (btCompoundShape*)rootShape;
	// retrieve the shapeInfo
	CcdShapeConstructionInfo* childShapeInfo = childCtrl->GetShapeInfo();
	CcdShapeConstructionInfo* rootShapeInfo = GetShapeInfo();
	// and verify that the child is part of the parent
	int i = rootShapeInfo->FindChildShape(childShapeInfo, childCtrl);
	if (i < 0)
		return;
	rootShapeInfo->RemoveChildShape(i);
	if (childCtrl->m_bulletChildShape)
	{
		int numChildren = compoundShape->getNumChildShapes();
		for (i=0; i<numChildren; i++)
		{
			if (compoundShape->getChildShape(i) == childCtrl->m_bulletChildShape)
			{
				compoundShape->removeChildShapeByIndex(i);
				compoundShape->recalculateLocalAabb();
				break;
			}
		}
		delete childCtrl->m_bulletChildShape;
		childCtrl->m_bulletChildShape = NULL;
	}
	// recompute inertia of parent
	if (!rootBody->isStaticOrKinematicObject())
	{
		btVector3 localInertia;
		float mass = 1.f/rootBody->getInvMass();
		compoundShape->calculateLocalInertia(mass,localInertia);
		rootBody->setMassProps(mass,localInertia);
	}
	// must update the broadphase cache,
	GetPhysicsEnvironment()->refreshCcdPhysicsController(this);
	// reactivate the children
	GetPhysicsEnvironment()->enableCcdPhysicsController(childCtrl);
}

void KX_BulletPhysicsController::SetMass(MT_Scalar newmass)
{
	btRigidBody *body = GetRigidBody();
	if (body && !m_suspended && newmass>MT_EPSILON && GetMass()>MT_EPSILON)
	{
		btVector3 grav = body->getGravity();
		btVector3 accel = grav / GetMass();
		
		btBroadphaseProxy* handle = body->getBroadphaseHandle();
		GetPhysicsEnvironment()->updateCcdPhysicsController(this, 
			newmass,
			body->getCollisionFlags(),
			handle->m_collisionFilterGroup, 
			handle->m_collisionFilterMask);
		body->setGravity(accel);
	}
}

void	KX_BulletPhysicsController::SuspendDynamics(bool ghost)
{
	btRigidBody *body = GetRigidBody();
	if (body && !m_suspended && !IsSensor())
	{
		btBroadphaseProxy* handle = body->getBroadphaseHandle();
		m_savedCollisionFlags = body->getCollisionFlags();
		m_savedMass = GetMass();
		m_savedDyna = m_bDyna;
		m_savedCollisionFilterGroup = handle->m_collisionFilterGroup;
		m_savedCollisionFilterMask = handle->m_collisionFilterMask;
		m_suspended = true;
		GetPhysicsEnvironment()->updateCcdPhysicsController(this, 
			0.0,
			btCollisionObject::CF_STATIC_OBJECT|((ghost)?btCollisionObject::CF_NO_CONTACT_RESPONSE:(m_savedCollisionFlags&btCollisionObject::CF_NO_CONTACT_RESPONSE)),
			btBroadphaseProxy::StaticFilter, 
			btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter);
		m_bDyna = false;
	}
}

void	KX_BulletPhysicsController::RestoreDynamics()
{
	btRigidBody *body = GetRigidBody();
	if (body && m_suspended)
	{
		// before make sure any position change that was done in this logic frame are accounted for
		SetTransform();
		GetPhysicsEnvironment()->updateCcdPhysicsController(this, 
			m_savedMass,
			m_savedCollisionFlags,
			m_savedCollisionFilterGroup,
			m_savedCollisionFilterMask);
		body->activate();
		m_bDyna = m_savedDyna;
		m_suspended = false;
	}
}

SG_Controller*	KX_BulletPhysicsController::GetReplica(class SG_Node* destnode)
{
	PHY_IMotionState* motionstate = new KX_MotionState(destnode);

	KX_BulletPhysicsController* physicsreplica = new KX_BulletPhysicsController(*this);

	//parentcontroller is here be able to avoid collisions between parent/child

	PHY_IPhysicsController* parentctrl = NULL;
	KX_BulletPhysicsController* parentKxCtrl = NULL;
	CcdPhysicsController* ccdParent = NULL;

	
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
				KX_GameObject *clientgameobj_child = static_cast<KX_GameObject*>( (*childit)->GetSGClientObject());
				if (clientgameobj_child)
				{
					parentKxCtrl = (KX_BulletPhysicsController*)clientgameobj_child->GetPhysicsController();
					parentctrl = parentKxCtrl;
					ccdParent = parentKxCtrl;
				}
			}
		}
	}

	physicsreplica->setParentCtrl(ccdParent);
	physicsreplica->PostProcessReplica(motionstate,parentctrl);
	physicsreplica->m_userdata = (PHY_IPhysicsController*)physicsreplica;
	physicsreplica->m_bulletChildShape = NULL;
	return physicsreplica;
	
}



void	KX_BulletPhysicsController::SetSumoTransform(bool nondynaonly)
{

	if (!m_bDyna && !m_bSensor)
	{
		btCollisionObject* object = GetRigidBody();
		object->setActivationState(ACTIVE_TAG);
		object->setCollisionFlags(object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	} else
	{
		if (!nondynaonly)
		{
			/*
			btTransform worldTrans;
			if (GetRigidBody())
			{
				GetRigidBody()->getMotionState()->getWorldTransform(worldTrans);
				GetRigidBody()->setCenterOfMassTransform(worldTrans);
			}
			*/
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


const char* KX_BulletPhysicsController::getName()
{
	if (m_pObject)
	{
		KX_GameObject* gameobj = (KX_GameObject*)	m_pObject->GetSGClientObject();
		return gameobj->GetName();
	}
	return 0;
}

#endif // USE_BULLET
