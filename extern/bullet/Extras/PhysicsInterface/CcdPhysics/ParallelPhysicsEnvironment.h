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

#ifndef PARALLELPHYSICSENVIRONMENT
#define PARALLELPHYSICSENVIRONMENT

#include "CcdPhysicsEnvironment.h"
class ParallelIslandDispatcher;


/// ParallelPhysicsEnvironment is experimental parallel mainloop for physics simulation
/// Physics Environment takes care of stepping the simulation and is a container for physics entities.
/// It stores rigidbodies,constraints, materials etc.
/// A derived class may be able to 'construct' entities by loading and/or converting
class ParallelPhysicsEnvironment : public CcdPhysicsEnvironment
{
	

	public:
		ParallelPhysicsEnvironment(ParallelIslandDispatcher* dispatcher=0, OverlappingPairCache* pairCache=0);

		virtual		~ParallelPhysicsEnvironment();

		
		/// Perform an integration step of duration 'timeStep'.
		virtual bool						proceedDeltaTimeOneStep(float timeStep);
		
		//void	BuildSimulationIslands();

};

#endif //PARALLELPHYSICSENVIRONMENT
