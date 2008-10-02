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


#include "btSoftRigidDynamicsWorld.h"
#include "LinearMath/btQuickprof.h"

//softbody & helpers
#include "btSoftBody.h"
#include "btSoftBodyHelpers.h"





btSoftRigidDynamicsWorld::btSoftRigidDynamicsWorld(btDispatcher* dispatcher,btBroadphaseInterface* pairCache,btConstraintSolver* constraintSolver,btCollisionConfiguration* collisionConfiguration)
:btDiscreteDynamicsWorld(dispatcher,pairCache,constraintSolver,collisionConfiguration)
{
m_drawFlags			=	fDrawFlags::Std;
m_drawNodeTree		=	true;
m_drawFaceTree		=	false;
m_drawClusterTree	=	false;
m_sbi.m_broadphase = pairCache;
m_sbi.m_dispatcher = dispatcher;
m_sbi.m_sparsesdf.Initialize();
m_sbi.m_sparsesdf.Reset();

}
		
btSoftRigidDynamicsWorld::~btSoftRigidDynamicsWorld()
{

}

void	btSoftRigidDynamicsWorld::predictUnconstraintMotion(btScalar timeStep)
{
	btDiscreteDynamicsWorld::predictUnconstraintMotion( timeStep);

	for ( int i=0;i<m_softBodies.size();++i)
	{
		btSoftBody*	psb= m_softBodies[i];

		psb->predictMotion(timeStep);		
	}
}
		
void	btSoftRigidDynamicsWorld::internalSingleStepSimulation( btScalar timeStep)
{
	btDiscreteDynamicsWorld::internalSingleStepSimulation( timeStep );

	///solve soft bodies constraints
	solveSoftBodiesConstraints();

	///update soft bodies
	updateSoftBodies();

}

void	btSoftRigidDynamicsWorld::updateSoftBodies()
{
	BT_PROFILE("updateSoftBodies");
	
	for ( int i=0;i<m_softBodies.size();i++)
	{
		btSoftBody*	psb=(btSoftBody*)m_softBodies[i];
		psb->integrateMotion();	
	}
}

void	btSoftRigidDynamicsWorld::solveSoftBodiesConstraints()
{
	BT_PROFILE("solveSoftConstraints");
	
	if(m_softBodies.size())
		{
		btSoftBody::solveClusters(m_softBodies);
		}
	
	for(int i=0;i<m_softBodies.size();++i)
	{
		btSoftBody*	psb=(btSoftBody*)m_softBodies[i];
		psb->solveConstraints();
	}	
}

void	btSoftRigidDynamicsWorld::addSoftBody(btSoftBody* body)
{
	m_softBodies.push_back(body);

	btCollisionWorld::addCollisionObject(body,
					btBroadphaseProxy::DefaultFilter,
					btBroadphaseProxy::AllFilter);

}

void	btSoftRigidDynamicsWorld::removeSoftBody(btSoftBody* body)
{
	m_softBodies.remove(body);

	btCollisionWorld::removeCollisionObject(body);
}

void	btSoftRigidDynamicsWorld::debugDrawWorld()
{
	btDiscreteDynamicsWorld::debugDrawWorld();

	if (getDebugDrawer())
	{
		int i;
		for (  i=0;i<this->m_softBodies.size();i++)
		{
			btSoftBody*	psb=(btSoftBody*)this->m_softBodies[i];
			btSoftBodyHelpers::DrawFrame(psb,m_debugDrawer);
			btSoftBodyHelpers::Draw(psb,m_debugDrawer,m_drawFlags);
			if (m_debugDrawer && (m_debugDrawer->getDebugMode() & btIDebugDraw::DBG_DrawAabb))
			{
				if(m_drawNodeTree)		btSoftBodyHelpers::DrawNodeTree(psb,m_debugDrawer);
				if(m_drawFaceTree)		btSoftBodyHelpers::DrawFaceTree(psb,m_debugDrawer);
				if(m_drawClusterTree)	btSoftBodyHelpers::DrawClusterTree(psb,m_debugDrawer);
			}
		}		
	}	
}
