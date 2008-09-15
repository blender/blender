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
#include "OdePhysicsEnvironment.h"
#include "PHY_IMotionState.h"
#include "OdePhysicsController.h"

#include <ode/ode.h>
#include <../ode/src/joint.h>
#include <ode/odemath.h>

ODEPhysicsEnvironment::ODEPhysicsEnvironment()
{
	m_OdeWorld = dWorldCreate();
	m_OdeSpace = dHashSpaceCreate();
	m_OdeContactGroup = dJointGroupCreate (0);
	dWorldSetCFM (m_OdeWorld,1e-5f);

	m_JointGroup = dJointGroupCreate(0);

	setFixedTimeStep(true,1.f/60.f);
}



ODEPhysicsEnvironment::~ODEPhysicsEnvironment()
{
	dJointGroupDestroy (m_OdeContactGroup);
	dJointGroupDestroy (m_JointGroup);

	dSpaceDestroy (m_OdeSpace);
	dWorldDestroy (m_OdeWorld);
}



void		ODEPhysicsEnvironment::setFixedTimeStep(bool useFixedTimeStep,float fixedTimeStep)
{
	m_useFixedTimeStep = useFixedTimeStep;

	if (useFixedTimeStep)
	{
		m_fixedTimeStep = fixedTimeStep;
	} else
	{
		m_fixedTimeStep = 0.f;
	}
	m_currentTime = 0.f;

	//todo:implement fixed timestepping

}
float		ODEPhysicsEnvironment::getFixedTimeStep()
{
	return m_fixedTimeStep;
}



bool		ODEPhysicsEnvironment::proceedDeltaTime(double  curTime,float timeStep1)
{

	float deltaTime = timeStep1;
	int	numSteps = 1;

	if (m_useFixedTimeStep)
	{
		m_currentTime += timeStep1;		
		// equal to subSampling (might be a little smaller).
		numSteps = (int)(m_currentTime / m_fixedTimeStep);
		m_currentTime -= m_fixedTimeStep * (float)numSteps;
		deltaTime = m_fixedTimeStep;
		//todo: experiment by smoothing the remaining time over the substeps
	}

	for (int i=0;i<numSteps;i++)
	{
		// ode collision update
		dSpaceCollide (m_OdeSpace,this,&ODEPhysicsEnvironment::OdeNearCallback);

		int m_odeContacts = GetNumOdeContacts();
		
		//physics integrator + resolver update
		//dWorldStep (m_OdeWorld,deltaTime);
		//dWorldQuickStep (m_OdeWorld,deltaTime);
		//dWorldID w, dReal stepsize)

		//clear collision points
		this->ClearOdeContactGroup();
	}
	return true;
}

void ODEPhysicsEnvironment::setGravity(float x,float y,float z)
{
	dWorldSetGravity (m_OdeWorld,x,y,z);
}



int			ODEPhysicsEnvironment::createConstraint(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsController* ctrl2,PHY_ConstraintType type,
		float pivotX,float pivotY,float pivotZ,float axisX,float axisY,float axisZ)
{

	int constraintid = 0;
	ODEPhysicsController* dynactrl = (ODEPhysicsController*)ctrl;
	ODEPhysicsController* dynactrl2 = (ODEPhysicsController*)ctrl2;

	switch (type)
	{
	case PHY_POINT2POINT_CONSTRAINT:
		{
			if (dynactrl)
			{
				dJointID jointid = dJointCreateBall (m_OdeWorld,m_JointGroup);
				struct	dxBody*	bodyid1 = dynactrl->GetOdeBodyId();
				struct	dxBody*	bodyid2=0;
				const dReal* pos = dBodyGetPosition(bodyid1);
				const dReal* R = dBodyGetRotation(bodyid1);
				dReal offset[3] = {pivotX,pivotY,pivotZ};
				dReal newoffset[3];
				dMULTIPLY0_331 (newoffset,R,offset);
				newoffset[0] += pos[0];
				newoffset[1] += pos[1];
				newoffset[2] += pos[2];
				

				if (dynactrl2)
					bodyid2 = dynactrl2->GetOdeBodyId();
				
				dJointAttach (jointid, bodyid1, bodyid2);
				
				dJointSetBallAnchor (jointid, newoffset[0], newoffset[1], newoffset[2]);
				
				constraintid = (int) jointid;
			}
			break;
		}
	case PHY_LINEHINGE_CONSTRAINT:
	{
			if (dynactrl)
			{
				dJointID jointid = dJointCreateHinge (m_OdeWorld,m_JointGroup);
				struct	dxBody*	bodyid1 = dynactrl->GetOdeBodyId();
				struct	dxBody*	bodyid2=0;
				const dReal* pos = dBodyGetPosition(bodyid1);
				const dReal* R = dBodyGetRotation(bodyid1);
				dReal offset[3] = {pivotX,pivotY,pivotZ};
				dReal axisset[3] = {axisX,axisY,axisZ};
				
				dReal newoffset[3];
				dReal newaxis[3];
				dMULTIPLY0_331 (newaxis,R,axisset);
				
				dMULTIPLY0_331 (newoffset,R,offset);
				newoffset[0] += pos[0];
				newoffset[1] += pos[1];
				newoffset[2] += pos[2];
				

				if (dynactrl2)
					bodyid2 = dynactrl2->GetOdeBodyId();
				
				dJointAttach (jointid, bodyid1, bodyid2);
				
				dJointSetHingeAnchor (jointid, newoffset[0], newoffset[1], newoffset[2]);
				dJointSetHingeAxis(jointid,newaxis[0],newaxis[1],newaxis[2]);

				constraintid = (int) jointid;
			}
			break;
		}
	default:
		{
			//not yet
		}
	}
	
	return constraintid;

}

void		ODEPhysicsEnvironment::removeConstraint(int constraintid)
{
	if (constraintid)
	{
		dJointDestroy((dJointID) constraintid);
	}
}

PHY_IPhysicsController* ODEPhysicsEnvironment::rayTest(PHY_IRayCastFilterCallback &filterCallback,float fromX,float fromY,float fromZ, float toX,float toY,float toZ)
{

	//m_OdeWorld
	//collision detection / raytesting
	return NULL;
}


void ODEPhysicsEnvironment::OdeNearCallback (void *data, dGeomID o1, dGeomID o2)
{
	// \todo if this is a registered collision sensor
	// fire the callback

	int i;
	// if (o1->body && o2->body) return;
	ODEPhysicsEnvironment* env = (ODEPhysicsEnvironment*) data;
	dBodyID b1,b2;
	
	b1 = dGeomGetBody(o1);
	b2 = dGeomGetBody(o2);
	// exit without doing anything if the two bodies are connected by a joint
	if (b1 && b2 && dAreConnected (b1,b2)) return;

	ODEPhysicsController * ctrl1 =(ODEPhysicsController *)dGeomGetData(o1);
	ODEPhysicsController * ctrl2 =(ODEPhysicsController *)dGeomGetData(o2);
	float friction=ctrl1->getFriction();
	float restitution = ctrl1->getRestitution();
	//for friction, take minimum

	friction=(friction < ctrl2->getFriction() ?  
	friction :ctrl2->getFriction());

	//restitution:take minimum
	restitution = restitution < ctrl2->getRestitution()?
	restitution : ctrl2->getRestitution();

	dContact contact[3];			// up to 3 contacts per box
	for (i=0; i<3; i++) {
		contact[i].surface.mode = dContactBounce; //dContactMu2;
		contact[i].surface.mu = friction;//dInfinity;
		contact[i].surface.mu2 = 0;
		contact[i].surface.bounce = restitution;//0.5;
		contact[i].surface.bounce_vel = 0.1f;
		contact[i].surface.slip1=0.0;
	}
	
	if (int numc = dCollide (o1,o2,3,&contact[0].geom,sizeof(dContact))) {
		// dMatrix3 RI;
		// dRSetIdentity (RI);
		// const dReal ss[3] = {0.02,0.02,0.02};
		for (i=0; i<numc; i++) {
			dJointID c = dJointCreateContact (env->m_OdeWorld,env->m_OdeContactGroup,contact+i);
			dJointAttach (c,b1,b2);
		}
	}
}


void	ODEPhysicsEnvironment::ClearOdeContactGroup()
{
	dJointGroupEmpty (m_OdeContactGroup);
}

int	ODEPhysicsEnvironment::GetNumOdeContacts()
{
	return m_OdeContactGroup->num;
}

