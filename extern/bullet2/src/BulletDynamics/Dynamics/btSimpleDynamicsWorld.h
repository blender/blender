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

#ifndef BT_SIMPLE_DYNAMICS_WORLD_H
#define BT_SIMPLE_DYNAMICS_WORLD_H

#include "btDynamicsWorld.h"

class btDispatcher;
class btOverlappingPairCache;
class btConstraintSolver;

///btSimpleDynamicsWorld demonstrates very basic usage of Bullet rigid body dynamics
///It can be used for basic simulations, and as a starting point for porting Bullet
///btSimpleDynamicsWorld lacks object deactivation, island management and other concepts.
///For more complicated simulations, btDiscreteDynamicsWorld and btContinuousDynamicsWorld are recommended
///those classes replace the obsolete CcdPhysicsEnvironment/CcdPhysicsController
class btSimpleDynamicsWorld : public btDynamicsWorld
{
protected:

	btConstraintSolver*	m_constraintSolver;

	bool	m_ownsConstraintSolver;

	btIDebugDraw*	m_debugDrawer;

	void	predictUnconstraintMotion(btScalar timeStep);
	
	void	integrateTransforms(btScalar timeStep);
		
	btVector3	m_gravity;
	
public:



	///this btSimpleDynamicsWorld constructor creates dispatcher, broadphase pairCache and constraintSolver
	btSimpleDynamicsWorld(btDispatcher* dispatcher,btOverlappingPairCache* pairCache,btConstraintSolver* constraintSolver);

	virtual ~btSimpleDynamicsWorld();
		
	///maxSubSteps/fixedTimeStep for interpolation is currently ignored for btSimpleDynamicsWorld, use btDiscreteDynamicsWorld instead
	virtual int	stepSimulation( btScalar timeStep,int maxSubSteps=1, btScalar fixedTimeStep=btScalar(1.)/btScalar(60.));

	virtual void	setDebugDrawer(btIDebugDraw*	debugDrawer) 
	{
		m_debugDrawer = debugDrawer;
	};

	virtual btIDebugDraw*	getDebugDrawer()
	{
		return m_debugDrawer;
	}

	virtual void	setGravity(const btVector3& gravity);

	virtual void	addRigidBody(btRigidBody* body);

	virtual void	removeRigidBody(btRigidBody* body);
	
	virtual void	updateAabbs();

	void	synchronizeMotionStates();

	virtual void	setConstraintSolver(btConstraintSolver* solver);

};

#endif //BT_SIMPLE_DYNAMICS_WORLD_H
