#include "KX_ConvertPhysicsObject.h"

#ifdef USE_SUMO_SOLID

#ifdef WIN32
#pragma warning (disable : 4786)
#endif

#include "KX_SumoPhysicsController.h"
#include "SG_Spatial.h"
#include "SM_Scene.h"
#include "KX_GameObject.h"
#include "KX_MotionState.h"
#include "KX_ClientObjectInfo.h"

#include "PHY_IPhysicsEnvironment.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void	KX_SumoPhysicsController::applyImpulse(const MT_Point3& attach, const MT_Vector3& impulse)
{
	SumoPhysicsController::applyImpulse(attach[0],attach[1],attach[2],impulse[0],impulse[1],impulse[2]);
}
void	KX_SumoPhysicsController::RelativeTranslate(const MT_Vector3& dloc,bool local)
{
	SumoPhysicsController::RelativeTranslate(dloc[0],dloc[1],dloc[2],local);

}
void	KX_SumoPhysicsController::RelativeRotate(const MT_Matrix3x3& drot,bool local)
{
	float oldmat[12];
	drot.getValue(oldmat);
/*	float newmat[9];
	float *m = &newmat[0];
	double *orgm = &oldmat[0];

	 *m++ = *orgm++;*m++ = *orgm++;*m++ = *orgm++;orgm++;
	 *m++ = *orgm++;*m++ = *orgm++;*m++ = *orgm++;orgm++;
	 *m++ = *orgm++;*m++ = *orgm++;*m++ = *orgm++;orgm++; */

	SumoPhysicsController::RelativeRotate(oldmat,local);
}

void	KX_SumoPhysicsController::SetLinearVelocity(const MT_Vector3& lin_vel,bool local)
{
	SumoPhysicsController::SetLinearVelocity(lin_vel[0],lin_vel[1],lin_vel[2],local);

}

void	KX_SumoPhysicsController::SetAngularVelocity(const MT_Vector3& ang_vel,bool local)
{
	SumoPhysicsController::SetAngularVelocity(ang_vel[0],ang_vel[1],ang_vel[2],local);
}

MT_Vector3 KX_SumoPhysicsController::GetVelocity(const MT_Point3& pos)
{

	float linvel[3];
	SumoPhysicsController::GetVelocity(pos[0],pos[1],pos[2],linvel[0],linvel[1],linvel[2]);

	return MT_Vector3 (linvel);
}

MT_Vector3 KX_SumoPhysicsController::GetLinearVelocity()
{
	return GetVelocity(MT_Point3(0,0,0));
	
}

void		KX_SumoPhysicsController::resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ)
{
	SumoPhysicsController::resolveCombinedVelocities(linvelX,linvelY,linvelZ,angVelX,angVelY,angVelZ);
}

void	KX_SumoPhysicsController::ApplyTorque(const MT_Vector3& torque,bool local)
{
	SumoPhysicsController::ApplyTorque(torque[0],torque[1],torque[2],local);

}

void	KX_SumoPhysicsController::ApplyForce(const MT_Vector3& force,bool local)
{
	SumoPhysicsController::ApplyForce(force[0],force[1],force[2],local);
}

bool KX_SumoPhysicsController::Update(double time)
{
	return SynchronizeMotionStates(time); 
}

void	KX_SumoPhysicsController::SetSimulatedTime(double time)
{
	
}

void	KX_SumoPhysicsController::SetSumoTransform(bool nondynaonly)
{
	SumoPhysicsController::setSumoTransform(nondynaonly);

}

void	KX_SumoPhysicsController::SuspendDynamics(bool)
{
	SumoPhysicsController::SuspendDynamics();
}

void	KX_SumoPhysicsController::RestoreDynamics()
{
	SumoPhysicsController::RestoreDynamics();
}

SG_Controller*	KX_SumoPhysicsController::GetReplica(SG_Node* destnode)
{

	PHY_IMotionState* motionstate = new KX_MotionState(destnode);

	KX_SumoPhysicsController* physicsreplica = new KX_SumoPhysicsController(*this);

	//parentcontroller is here be able to avoid collisions between parent/child

	PHY_IPhysicsController* parentctrl = NULL;
	
	if (destnode != destnode->GetRootSGParent())
	{
		KX_GameObject* clientgameobj = (KX_GameObject*) destnode->GetRootSGParent()->GetSGClientObject();
		if (clientgameobj)
		{
			parentctrl = (KX_SumoPhysicsController*)clientgameobj->GetPhysicsController();
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
					parentctrl = (KX_SumoPhysicsController*)clientgameobj->GetPhysicsController();
				}
			}
		}
	}

	physicsreplica->PostProcessReplica(motionstate,parentctrl);

	return physicsreplica;
}


void	KX_SumoPhysicsController::SetObject (SG_IObject* object)
{
	SG_Controller::SetObject(object);

	// cheating here...
//should not be necessary, is it for duplicates ?

KX_GameObject* gameobj = (KX_GameObject*)	object->GetSGClientObject();
gameobj->SetPhysicsController(this,gameobj->IsDynamic());
GetSumoObject()->setClientObject(gameobj->getClientInfo());
}

void	KX_SumoPhysicsController::setMargin(float collisionMargin)
{
	SumoPhysicsController::SetMargin(collisionMargin);
}


void KX_SumoPhysicsController::setOrientation(const MT_Quaternion& orn)
{
	SumoPhysicsController::setOrientation(
		orn[0],orn[1],orn[2],orn[3]);

}
void KX_SumoPhysicsController::getOrientation(MT_Quaternion& orn)
{

	float quat[4];

	SumoPhysicsController::getOrientation(quat[0],quat[1],quat[2],quat[3]);

	orn = MT_Quaternion(quat);

}

void KX_SumoPhysicsController::setPosition(const MT_Point3& pos)
{
	SumoPhysicsController::setPosition(pos[0],pos[1],pos[2]);
	
}
	
void KX_SumoPhysicsController::setScaling(const MT_Vector3& scaling)
{
	SumoPhysicsController::setScaling(scaling[0],scaling[1],scaling[2]);
	
}

MT_Scalar	KX_SumoPhysicsController::GetMass()
{
	return SumoPhysicsController::getMass();
}

MT_Vector3	KX_SumoPhysicsController::getReactionForce()
{
	float force[3];
	SumoPhysicsController::getReactionForce(force[0],force[1],force[2]);
	return MT_Vector3(force);

}

void	KX_SumoPhysicsController::setRigidBody(bool rigid)
{
	SumoPhysicsController::setRigidBody(rigid);
	
}


KX_SumoPhysicsController::~KX_SumoPhysicsController()
{

	
}


#endif//USE_SUMO_SOLID
