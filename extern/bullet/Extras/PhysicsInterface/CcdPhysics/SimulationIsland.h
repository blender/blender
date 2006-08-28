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

#ifndef SIMULATION_ISLAND_H
#define SIMULATION_ISLAND_H

#include <vector>
class BroadphaseInterface;
class Dispatcher;
class IDebugDraw;

///SimulationIsland groups all computations and data (for collision detection and dynamics) that can execute in parallel with other SimulationIsland's
///The ParallelPhysicsEnvironment and ParallelIslandDispatcher will dispatch SimulationIsland's
///At the start of the simulation timestep the simulation islands are re-calculated
///During one timestep there is no merging or splitting of Simulation Islands
class SimulationIsland
{
	
	public:
	std::vector<class CcdPhysicsController*> m_controllers;
	std::vector<class PersistentManifold*> m_manifolds;
	
	std::vector<int> m_overlappingPairIndices;
	std::vector<int> m_constraintIndices;

	bool	Simulate(IDebugDraw* debugDrawer,int numSolverIterations,class TypedConstraint** constraintsBaseAddress,struct BroadphasePair*	overlappingPairBaseAddress, Dispatcher* dispatcher,BroadphaseInterface* broadphase,	class ConstraintSolver*	solver, float timeStep);
	
	
	int	GetNumControllers()
	{
		return m_controllers.size();
	}

	
	

	void	SyncMotionStates(float timeStep);
	void	UpdateAabbs(IDebugDraw* debugDrawer,BroadphaseInterface* broadphase,float timeStep);
};

#endif //SIMULATION_ISLAND_H