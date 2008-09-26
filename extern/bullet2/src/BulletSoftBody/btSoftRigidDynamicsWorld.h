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

#ifndef BT_SOFT_RIGID_DYNAMICS_WORLD_H
#define BT_SOFT_RIGID_DYNAMICS_WORLD_H

#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h"
#include "btSoftBody.h"

typedef	btAlignedObjectArray<btSoftBody*> btSoftBodyArray;

class btSoftRigidDynamicsWorld : public btDiscreteDynamicsWorld
{
	
	btSoftBodyArray	m_softBodies;
	int				m_drawFlags;
	bool			m_drawNodeTree;
	bool			m_drawFaceTree;
	bool			m_drawClusterTree;
	btSoftBodyWorldInfo m_sbi;

protected:
	
	virtual void	predictUnconstraintMotion(btScalar timeStep);
	
	virtual void	internalSingleStepSimulation( btScalar timeStep);

	void	updateSoftBodies();

	void	solveSoftBodiesConstraints();


public:
	
	btSoftRigidDynamicsWorld(btDispatcher* dispatcher,btBroadphaseInterface* pairCache,btConstraintSolver* constraintSolver,btCollisionConfiguration* collisionConfiguration);

	virtual ~btSoftRigidDynamicsWorld();
	
	virtual void	debugDrawWorld();
			
	void	addSoftBody(btSoftBody* body);

	void	removeSoftBody(btSoftBody* body);
	
	int		getDrawFlags() const { return(m_drawFlags); }
	void	setDrawFlags(int f)	{ m_drawFlags=f; }

	btSoftBodyWorldInfo&	getWorldInfo()
	{
		return m_sbi;
	}
	const btSoftBodyWorldInfo&	getWorldInfo() const
	{
		return m_sbi;
	}

			
	btSoftBodyArray& getSoftBodyArray()
	{
		return m_softBodies;
	}

	const btSoftBodyArray& getSoftBodyArray() const
	{
		return m_softBodies;
	}
		
};

#endif //BT_SOFT_RIGID_DYNAMICS_WORLD_H
