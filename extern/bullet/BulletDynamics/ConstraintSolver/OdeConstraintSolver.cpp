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



#include "OdeConstraintSolver.h"

#include "NarrowPhaseCollision/PersistentManifold.h"
#include "Dynamics/RigidBody.h"
#include "ContactConstraint.h"
#include "Solve2LinearConstraint.h"
#include "ContactSolverInfo.h"
#include "Dynamics/BU_Joint.h"
#include "Dynamics/ContactJoint.h"

#include "IDebugDraw.h"

#define USE_SOR_SOLVER

#include "SorLcp.h"

#include <math.h>
#include <float.h>//FLT_MAX
#ifdef WIN32
#include <memory.h>
#endif
#include <string.h>
#include <stdio.h>

#if defined (WIN32)
#include <malloc.h>
#else
#if defined (__FreeBSD__)
#include <stdlib.h>
#else
#include <alloca.h>
#endif
#endif

class BU_Joint;

//see below

OdeConstraintSolver::OdeConstraintSolver():
m_cfm(0.f),//1e-5f),
m_erp(0.4f)
{
}







//iterative lcp and penalty method
float OdeConstraintSolver::SolveGroup(PersistentManifold** manifoldPtr, int numManifolds,const ContactSolverInfo& infoGlobal,IDebugDraw* debugDrawer)
{
	m_CurBody = 0;
	m_CurJoint = 0;


	RigidBody* bodies [MAX_RIGIDBODIES];

	int numBodies = 0;
	BU_Joint* joints [MAX_RIGIDBODIES*4];
	int numJoints = 0;

	for (int j=0;j<numManifolds;j++)
	{

		int body0=-1,body1=-1;

		PersistentManifold* manifold = manifoldPtr[j];
		if (manifold->GetNumContacts() > 0)
		{
			body0 = ConvertBody((RigidBody*)manifold->GetBody0(),bodies,numBodies);
			body1 = ConvertBody((RigidBody*)manifold->GetBody1(),bodies,numBodies);
			ConvertConstraint(manifold,joints,numJoints,bodies,body0,body1,debugDrawer);
		}
	}

	SolveInternal1(m_cfm,m_erp,bodies,numBodies,joints,numJoints,infoGlobal);

	return 0.f;

}

/////////////////////////////////////////////////////////////////////////////////


typedef SimdScalar dQuaternion[4];
#define _R(i,j) R[(i)*4+(j)]

void dRfromQ1 (dMatrix3 R, const dQuaternion q)
{
  // q = (s,vx,vy,vz)
  SimdScalar qq1 = 2.f*q[1]*q[1];
  SimdScalar qq2 = 2.f*q[2]*q[2];
  SimdScalar qq3 = 2.f*q[3]*q[3];
  _R(0,0) = 1.f - qq2 - qq3;
  _R(0,1) = 2*(q[1]*q[2] - q[0]*q[3]);
  _R(0,2) = 2*(q[1]*q[3] + q[0]*q[2]);
  _R(0,3) = 0.f;
	
  _R(1,0) = 2*(q[1]*q[2] + q[0]*q[3]);
  _R(1,1) = 1.f - qq1 - qq3;
  _R(1,2) = 2*(q[2]*q[3] - q[0]*q[1]);
  _R(1,3) = 0.f;

  _R(2,0) = 2*(q[1]*q[3] - q[0]*q[2]);
  _R(2,1) = 2*(q[2]*q[3] + q[0]*q[1]);
  _R(2,2) = 1.f - qq1 - qq2;
  _R(2,3) = 0.f;

}



int OdeConstraintSolver::ConvertBody(RigidBody* body,RigidBody** bodies,int& numBodies)
{
	if (!body || (body->getInvMass() == 0.f) )
	{
		return -1;
	}
	//first try to find
	int i,j;
	for (i=0;i<numBodies;i++)
	{
		if (bodies[i] == body)
			return i;
	}
	//if not found, create a new body
	bodies[numBodies++] = body;
	//convert data


	body->m_facc.setValue(0,0,0,0);
	body->m_tacc.setValue(0,0,0,0);
	
	//are the indices the same ?
	for (i=0;i<4;i++)
	{
		for ( j=0;j<3;j++)
		{
			body->m_invI[i+4*j] = 0.f;
			body->m_I[i+4*j] = 0.f;
		}
	}
	body->m_invI[0+4*0] = 	body->getInvInertiaDiagLocal()[0];
	body->m_invI[1+4*1] = 	body->getInvInertiaDiagLocal()[1];
	body->m_invI[2+4*2] = 	body->getInvInertiaDiagLocal()[2];

	body->m_I[0+0*4] = 1.f/body->getInvInertiaDiagLocal()[0];
	body->m_I[1+1*4] = 1.f/body->getInvInertiaDiagLocal()[1];
	body->m_I[2+2*4] = 1.f/body->getInvInertiaDiagLocal()[2];
	

	
	
	dQuaternion q;

	q[1] = body->getOrientation()[0];
	q[2] = body->getOrientation()[1];
	q[3] = body->getOrientation()[2];
	q[0] = body->getOrientation()[3];
	
	dRfromQ1(body->m_R,q);
	
	return numBodies-1;
}




	
#define MAX_JOINTS_1 8192

static ContactJoint gJointArray[MAX_JOINTS_1];


void OdeConstraintSolver::ConvertConstraint(PersistentManifold* manifold,BU_Joint** joints,int& numJoints,
					   RigidBody** bodies,int _bodyId0,int _bodyId1,IDebugDraw* debugDrawer)
{


	manifold->RefreshContactPoints(((RigidBody*)manifold->GetBody0())->getCenterOfMassTransform(),
		((RigidBody*)manifold->GetBody1())->getCenterOfMassTransform());

	int bodyId0 = _bodyId0,bodyId1 = _bodyId1;

	int i,numContacts = manifold->GetNumContacts();
	
	bool swapBodies = (bodyId0 < 0);

	
	RigidBody* body0,*body1;

	if (swapBodies)
	{
		bodyId0 = _bodyId1;
		bodyId1 = _bodyId0;

		body0 = (RigidBody*)manifold->GetBody1();
		body1 = (RigidBody*)manifold->GetBody0();

	} else
	{
		body0 = (RigidBody*)manifold->GetBody0();
		body1 = (RigidBody*)manifold->GetBody1();
	}

	assert(bodyId0 >= 0);

	SimdVector3 color(0,1,0);
	for (i=0;i<numContacts;i++)
	{

		if (debugDrawer)
		{
			const ManifoldPoint& cp = manifold->GetContactPoint(i);

			debugDrawer->DrawContactPoint(
				cp.m_positionWorldOnB,
				cp.m_normalWorldOnB,
				cp.GetDistance(),
				cp.GetLifeTime(),
				color);

		}
		assert (m_CurJoint < MAX_JOINTS_1);

//		if (manifold->GetContactPoint(i).GetDistance() < 0.0f)
		{
			ContactJoint* cont = new (&gJointArray[m_CurJoint++]) ContactJoint( manifold ,i, swapBodies,body0,body1);

			cont->node[0].joint = cont;
			cont->node[0].body = bodyId0 >= 0 ? bodies[bodyId0] : 0;
			
			cont->node[1].joint = cont;
			cont->node[1].body = bodyId1 >= 0 ? bodies[bodyId1] : 0;
			
			joints[numJoints++] = cont;
			for (int i=0;i<6;i++)
				cont->lambda[i] = 0.f;

			cont->flags = 0;
		}
	}

	//create a new contact constraint
};

