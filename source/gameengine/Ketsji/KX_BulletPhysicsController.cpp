//under visual studio the #define in KX_ConvertPhysicsObject.h is quicker for recompilation
#include "KX_ConvertPhysicsObject.h"

#ifdef USE_BULLET

#include "KX_BulletPhysicsController.h"

#include "Dynamics/RigidBody.h"

KX_BulletPhysicsController::KX_BulletPhysicsController (const CcdConstructionInfo& ci, bool dyna)
: KX_IPhysicsController(dyna,(PHY_IPhysicsController*)this),
CcdPhysicsController(ci)
{

}
	
KX_BulletPhysicsController::~KX_BulletPhysicsController ()
{

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
}

void	KX_BulletPhysicsController::RelativeTranslate(const MT_Vector3& dloc,bool local)
{
	CcdPhysicsController::RelativeTranslate(dloc[0],dloc[1],dloc[2],local);

}

void	KX_BulletPhysicsController::RelativeRotate(const MT_Matrix3x3& drot,bool local)
{
	printf("he1\n");
	float	rotval[12];
	drot.getValue(rotval);


	
	printf("hi\n");
	CcdPhysicsController::RelativeRotate(rotval,local);
}

void	KX_BulletPhysicsController::ApplyTorque(const MT_Vector3& torque,bool local)
{
}
void	KX_BulletPhysicsController::ApplyForce(const MT_Vector3& force,bool local)
{
}
MT_Vector3 KX_BulletPhysicsController::GetLinearVelocity()
{
	assert(0);
	return MT_Vector3(0.f,0.f,0.f);

}
MT_Vector3 KX_BulletPhysicsController::GetVelocity(const MT_Point3& pos)
{
		assert(0);
	return MT_Vector3(0.f,0.f,0.f);

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
}
void KX_BulletPhysicsController::setOrientation(const MT_Quaternion& orn)
{
}
void KX_BulletPhysicsController::setPosition(const MT_Point3& pos)
{

}
void KX_BulletPhysicsController::setScaling(const MT_Vector3& scaling)
{
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

void	KX_BulletPhysicsController::SuspendDynamics()
{
}
void	KX_BulletPhysicsController::RestoreDynamics()
{
}

SG_Controller*	KX_BulletPhysicsController::GetReplica(class SG_Node* destnode)
{
	assert(0);
	return 0;
}



void	KX_BulletPhysicsController::SetSumoTransform(bool nondynaonly)
{
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
