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

#include "OdeConstraintSolver.h"

#include "NarrowPhaseCollision/PersistentManifold.h"
#include "Dynamics/RigidBody.h"
#include "ContactConstraint.h"
#include "Solve2LinearConstraint.h"
#include "ContactSolverInfo.h"
#include "Dynamics/BU_Joint.h"
#include "Dynamics/ContactJoint.h"

#define USE_SOR_SOLVER

#include "SorLcp.h"

#include <math.h>
#include <float.h>//FLT_MAX
#ifdef WIN32
#include <memory.h>
#endif
#include <string.h>
#include <stdio.h>

#ifdef WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

class BU_Joint;

//see below

int gCurBody = 0;
int gCurJoint= 0;

int ConvertBody(RigidBody* body,RigidBody** bodies,int& numBodies);
void ConvertConstraint(PersistentManifold* manifold,BU_Joint** joints,int& numJoints,
					   RigidBody** bodies,int bodyId0,int bodyId1);





//iterative lcp and penalty method
float OdeConstraintSolver::SolveGroup(PersistentManifold** manifoldPtr, int numManifolds,const ContactSolverInfo& infoGlobal)
{
	gCurBody = 0;
	gCurJoint = 0;

	float cfm = 1e-5f;
	float erp = 0.2f;

	RigidBody* bodies [128];

	int numBodies = 0;
	BU_Joint* joints [128*5];
	int numJoints = 0;

	for (int j=0;j<numManifolds;j++)
	{

		int body0=-1,body1=-1;

		PersistentManifold* manifold = manifoldPtr[j];
		if (manifold->GetNumContacts() > 0)
		{
			body0 = ConvertBody((RigidBody*)manifold->GetBody0(),bodies,numBodies);
			body1 = ConvertBody((RigidBody*)manifold->GetBody1(),bodies,numBodies);
			ConvertConstraint(manifold,joints,numJoints,bodies,body0,body1);
		}
	}

	SolveInternal1(cfm,erp,bodies,numBodies,joints,numJoints,infoGlobal);

	return 0.f;

}

/////////////////////////////////////////////////////////////////////////////////


typedef SimdScalar dQuaternion[4];
#define _R(i,j) R[(i)*4+(j)]

void dRfromQ1 (dMatrix3 R, const dQuaternion q)
{
  // q = (s,vx,vy,vz)
  SimdScalar qq1 = 2*q[1]*q[1];
  SimdScalar qq2 = 2*q[2]*q[2];
  SimdScalar qq3 = 2*q[3]*q[3];
  _R(0,0) = 1 - qq2 - qq3;
  _R(0,1) = 2*(q[1]*q[2] - q[0]*q[3]);
  _R(0,2) = 2*(q[1]*q[3] + q[0]*q[2]);
  _R(1,0) = 2*(q[1]*q[2] + q[0]*q[3]);
  _R(1,1) = 1 - qq1 - qq3;
  _R(1,2) = 2*(q[2]*q[3] - q[0]*q[1]);
  _R(2,0) = 2*(q[1]*q[3] - q[0]*q[2]);
  _R(2,1) = 2*(q[2]*q[3] + q[0]*q[1]);
  _R(2,2) = 1 - qq1 - qq2;
}



int ConvertBody(RigidBody* body,RigidBody** bodies,int& numBodies)
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


	body->m_facc.setValue(0,0,0);
	body->m_tacc.setValue(0,0,0);
	
	//are the indices the same ?
	for (i=0;i<4;i++)
	{
		for ( j=0;j<3;j++)
		{
			body->m_invI[i+4*j] = 0.f;
		}
	}
	body->m_invI[0+4*0] = 	body->getInvInertiaDiagLocal()[0];
	body->m_invI[1+4*1] = 	body->getInvInertiaDiagLocal()[1];
	body->m_invI[2+4*2] = 	body->getInvInertiaDiagLocal()[2];
	
	
	SimdMatrix3x3 invI;
	invI.setIdentity();
	invI[0][0] = body->getInvInertiaDiagLocal()[0];
	invI[1][1] = body->getInvInertiaDiagLocal()[1];
	invI[2][2] = body->getInvInertiaDiagLocal()[2];
	SimdMatrix3x3 inertia = invI.inverse();

	for (i=0;i<3;i++)
	{
		for (j=0;j<3;j++)
		{
			body->m_I[i+4*j] = inertia[i][j];
		}
	}
	body->m_I[3+0*4] = 0.f;
	body->m_I[3+1*4] = 0.f;
	body->m_I[3+2*4] = 0.f;
	body->m_I[3+3*4] = 0.f;
	
	
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


void ConvertConstraint(PersistentManifold* manifold,BU_Joint** joints,int& numJoints,
					   RigidBody** bodies,int _bodyId0,int _bodyId1)
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

	for (i=0;i<numContacts;i++)
	{
		
		assert (gCurJoint < MAX_JOINTS_1);

		ContactJoint* cont = new (&gJointArray[gCurJoint++]) ContactJoint( manifold ,i, swapBodies,body0,body1);

		cont->node[0].joint = cont;
		cont->node[0].body = bodyId0 >= 0 ? bodies[bodyId0] : 0;
		
		cont->node[1].joint = cont;
		cont->node[1].body = bodyId1 >= 0 ? bodies[bodyId1] : 0;
		
		joints[numJoints++] = cont;
		for (int i=0;i<6;i++)
			cont->lambda[i] = 0.f;

		cont->flags = 0;
	
	}

	//create a new contact constraint
};

