/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * The contents of this file may be used under the terms of either the GNU
 * General Public License Version 2 or later (the "GPL", see
 * http://www.gnu.org/licenses/gpl.html ), or the Blender License 1.0 or
 * later (the "BL", see http://www.blender.org/BL/ ) which has to be
 * bought from the Blender Foundation to become active, in which case the
 * above mentioned GPL option does not apply.
 *
 * The Original Code is Copyright (C) 2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include "KX_ConvertPhysicsObject.h"

#ifdef USE_ODE

#include "KX_OdePhysicsController.h"
#include "KX_GameObject.h"
#include "KX_MotionState.h"

#include "MT_assert.h"

#include "PHY_IPhysicsEnvironment.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_OdePhysicsController::KX_OdePhysicsController(
												 bool dyna,
												 bool fullRigidBody,
												 bool phantom,
												 class PHY_IMotionState* motionstate,
												 struct dxSpace* space,
												 struct dxWorld*	world,
												 float	mass,
												 float	friction,
												 float	restitution,
												 bool	implicitsphere,
												 float	center[3],
												 float	extends[3],
												 float	radius
												 ) 
: KX_IPhysicsController(dyna,(PHY_IPhysicsController*)this),
ODEPhysicsController(
dyna,fullRigidBody,phantom,motionstate,
space,world,mass,friction,restitution,
implicitsphere,center,extends,radius)
{
};


bool	KX_OdePhysicsController::Update(double time)
{
	return SynchronizeMotionStates(time);
}

void			KX_OdePhysicsController::SetObject (SG_IObject* object)
{
	SG_Controller::SetObject(object);

	// cheating here...
	KX_GameObject* gameobj = (KX_GameObject*)	object->GetSGClientObject();
	gameobj->SetPhysicsController(this);
	
}



void	KX_OdePhysicsController::applyImpulse(const MT_Point3& attach, const MT_Vector3& impulse)
{
	ODEPhysicsController::applyImpulse(attach[0],attach[1],attach[2],impulse[0],impulse[1],impulse[2]);
}
	


void	KX_OdePhysicsController::RelativeTranslate(const MT_Vector3& dloc,bool local)
{
	ODEPhysicsController::RelativeTranslate(dloc[0],dloc[1],dloc[2],local);

}
void	KX_OdePhysicsController::RelativeRotate(const MT_Matrix3x3& drot,bool local)
{
	double oldmat[12];
	drot.getValue(oldmat);
	float newmat[9];
	float *m = &newmat[0];
	double *orgm = &oldmat[0];

	 *m++ = *orgm++;*m++ = *orgm++;*m++ = *orgm++;orgm++;
	 *m++ = *orgm++;*m++ = *orgm++;*m++ = *orgm++;orgm++;
	 *m++ = *orgm++;*m++ = *orgm++;*m++ = *orgm++;orgm++;

	 ODEPhysicsController::RelativeRotate(newmat,local);

}

void	KX_OdePhysicsController::ApplyTorque(const MT_Vector3& torque,bool local)
{
		ODEPhysicsController::ApplyTorque(torque[0],torque[1],torque[2],local);

}
void	KX_OdePhysicsController::ApplyForce(const MT_Vector3& force,bool local)
{
		ODEPhysicsController::ApplyForce(force[0],force[1],force[2],local);

}
MT_Vector3 KX_OdePhysicsController::GetLinearVelocity()
{
	return MT_Vector3(0,0,0);
}

MT_Vector3 KX_OdePhysicsController::GetVelocity(const MT_Point3& pos)
{
	return MT_Vector3(0,0,0);
}

void	KX_OdePhysicsController::SetAngularVelocity(const MT_Vector3& ang_vel,bool local)
{

}
void	KX_OdePhysicsController::SetLinearVelocity(const MT_Vector3& lin_vel,bool local)
{
	ODEPhysicsController::SetLinearVelocity(lin_vel[0],lin_vel[1],lin_vel[2],local);
}

void KX_OdePhysicsController::setOrientation(const MT_Matrix3x3& rot)
{
	MT_Quaternion orn = rot.getRotation();
	ODEPhysicsController::setOrientation(orn[0],orn[1],orn[2],orn[3]);
}

void KX_OdePhysicsController::getOrientation(MT_Quaternion& orn)
{
	float florn[4];
	florn[0]=orn[0];
	florn[1]=orn[1];
	florn[2]=orn[2];
	florn[3]=orn[3];
	ODEPhysicsController::getOrientation(florn[0],florn[1],florn[2],florn[3]);
	orn[0] = florn[0];
	orn[1] = florn[1];
	orn[2] = florn[2];
	orn[3] = florn[3];


}

void KX_OdePhysicsController::setPosition(const MT_Point3& pos)
{
	ODEPhysicsController::setPosition(pos[0],pos[1],pos[2]);
}

void KX_OdePhysicsController::setScaling(const MT_Vector3& scaling)
{
}

MT_Scalar	KX_OdePhysicsController::GetMass()
{
	return ODEPhysicsController::getMass();
}

MT_Vector3	KX_OdePhysicsController::getReactionForce()
{
	return MT_Vector3(0,0,0);
}
void	KX_OdePhysicsController::setRigidBody(bool rigid)
{

}

void	KX_OdePhysicsController::SuspendDynamics(bool)
{
	ODEPhysicsController::SuspendDynamics();
}
void	KX_OdePhysicsController::RestoreDynamics()
{
	ODEPhysicsController::RestoreDynamics();
}
	

SG_Controller*	KX_OdePhysicsController::GetReplica(class SG_Node* destnode)
{
	PHY_IMotionState* motionstate = new KX_MotionState(destnode);
	KX_OdePhysicsController* copyctrl = new KX_OdePhysicsController(*this);

	// nlin: copied from KX_SumoPhysicsController.cpp. Not 100% sure what this does....
	// furthermore, the parentctrl is not used in ODEPhysicsController::PostProcessReplica, but
	// maybe it can/should be used in the future...

	// begin copy block ------------------------------------------------------------------

	//parentcontroller is here be able to avoid collisions between parent/child

	PHY_IPhysicsController* parentctrl = NULL;

	if (destnode != destnode->GetRootSGParent())
	{
		KX_GameObject* clientgameobj = (KX_GameObject*) destnode->GetRootSGParent()->GetSGClientObject();
		if (clientgameobj)
		{
			parentctrl = (KX_OdePhysicsController*)clientgameobj->GetPhysicsController();
		} else
		{
			// it could be a false node, try the children
			NodeList::const_iterator childit;
			for (
				childit = destnode->GetSGChildren().begin();
			childit!= destnode->GetSGChildren().end();
			++childit
				) {
				KX_GameObject* clientgameobj = static_cast<KX_GameObject*>( (*childit)->GetSGClientObject());
				if (clientgameobj)
				{
					parentctrl = (KX_OdePhysicsController*)clientgameobj->GetPhysicsController();
				}
			}
		}
	}
	// end copy block ------------------------------------------------------------------

	copyctrl->PostProcessReplica(motionstate, this);

	return copyctrl;
	
}

void		KX_OdePhysicsController::resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ)
{
}

	
void	KX_OdePhysicsController::SetSumoTransform(bool nondynaonly)
{

}
	// todo: remove next line !
void	KX_OdePhysicsController::SetSimulatedTime(double time)
{

}
	
#endif //USE_ODE
