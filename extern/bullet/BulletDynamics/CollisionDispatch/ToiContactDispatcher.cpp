#include "ToiContactDispatcher.h"
#include "ConstraintSolver/ConstraintSolver.h"
#include "Dynamics/RigidBody.h"
#include "BroadphaseCollision/CollisionAlgorithm.h"
#include "ConstraintSolver/ContactSolverInfo.h"
#include "CollisionDispatch/ConvexConvexAlgorithm.h"
#include "CollisionDispatch/EmptyCollisionAlgorithm.h"
#include "CollisionDispatch/ConvexConcaveCollisionAlgorithm.h"



void ToiContactDispatcher::FindUnions()
{
	if (m_useIslands)
	{
		for (int i=0;i<GetNumManifolds();i++)
		{
			const PersistentManifold* manifold = this->GetManifoldByIndexInternal(i);
			//static objects (invmass 0.f) don't merge !
			if ((((RigidBody*)manifold->GetBody0()) && (((RigidBody*)manifold->GetBody0())->mergesSimulationIslands())) &&
				(((RigidBody*)manifold->GetBody1()) && (((RigidBody*)manifold->GetBody1())->mergesSimulationIslands())))
			{

				m_unionFind.unite(((RigidBody*)manifold->GetBody0())->m_islandTag1,
					((RigidBody*)manifold->GetBody1())->m_islandTag1);
			}
			
			
		}
	}
	
}
	

	
ToiContactDispatcher::ToiContactDispatcher (ConstraintSolver* solver): 
	m_useIslands(true),
		m_unionFind(MAX_RIGIDBODIES),
		m_solver(solver),
		m_count(0),
		m_sor(1.3f),
		m_tau(0.4f),
		m_damping(0.9f)
		
{
	int i;
	
	for (i=0;i<MAX_BROADPHASE_COLLISION_TYPES;i++)
	{
		for (int j=0;j<MAX_BROADPHASE_COLLISION_TYPES;j++)
		{
			m_doubleDispatch[i][j] = 0;
		}
	}
	
	
};
	
PersistentManifold*	ToiContactDispatcher::GetNewManifold(void* b0,void* b1) 
{ 

	RigidBody* body0 = (RigidBody*)b0;
	RigidBody* body1 = (RigidBody*)b1;
	
	PersistentManifold* manifold = new PersistentManifold (body0,body1);
	m_manifoldsPtr.push_back(manifold);

	return manifold;
}
#include <algorithm>
	
void ToiContactDispatcher::ReleaseManifold(PersistentManifold* manifold)
{
	manifold->ClearManifold();

	std::vector<PersistentManifold*>::iterator i =
		std::find(m_manifoldsPtr.begin(), m_manifoldsPtr.end(), manifold);
	if (!(i == m_manifoldsPtr.end()))
	{
		std::swap(*i, m_manifoldsPtr.back());
		m_manifoldsPtr.pop_back();
	}
	
	
}
	
	
//
// todo: this is random access, it can be walked 'cache friendly'!
//
void ToiContactDispatcher::SolveConstraints(float timeStep, int numIterations,int numRigidBodies,IDebugDraw* debugDrawer) 
{
	int i;


	for (int islandId=0;islandId<numRigidBodies;islandId++)
	{

		std::vector<PersistentManifold*>  islandmanifold;
		
		//int numSleeping = 0;

		bool allSleeping = true;

		for (i=0;i<GetNumManifolds();i++)
		{
			 PersistentManifold* manifold = this->GetManifoldByIndexInternal(i);
			if ((((RigidBody*)manifold->GetBody0()) && ((RigidBody*)manifold->GetBody0())->m_islandTag1 == (islandId)) ||
				(((RigidBody*)manifold->GetBody1()) && ((RigidBody*)manifold->GetBody1())->m_islandTag1 == (islandId)))
			{

				if ((((RigidBody*)manifold->GetBody0()) && ((RigidBody*)manifold->GetBody0())->GetActivationState()== ACTIVE_TAG) ||
					(((RigidBody*)manifold->GetBody1()) && ((RigidBody*)manifold->GetBody1())->GetActivationState() == ACTIVE_TAG))
				{
					allSleeping = false;
				}

				islandmanifold.push_back(manifold);
			}
		}
		if (allSleeping)
		{
			//tag all as 'ISLAND_SLEEPING'
			for (i=0;i<islandmanifold.size();i++)
			{
				 PersistentManifold* manifold = islandmanifold[i];
				if (((RigidBody*)manifold->GetBody0()))	
				{
					((RigidBody*)manifold->GetBody0())->SetActivationState( ISLAND_SLEEPING );
				}
				if (((RigidBody*)manifold->GetBody1()))	
				{
					((RigidBody*)manifold->GetBody1())->SetActivationState( ISLAND_SLEEPING);
				}

			}
		} else
		{

			//tag all as 'ISLAND_SLEEPING'
			for (i=0;i<islandmanifold.size();i++)
			{
				 PersistentManifold* manifold = islandmanifold[i];
				 RigidBody* body0 = (RigidBody*)manifold->GetBody0();
				 RigidBody* body1 = (RigidBody*)manifold->GetBody1();

				if (body0)	
				{
					if ( body0->GetActivationState() == ISLAND_SLEEPING)
					{
						body0->SetActivationState( WANTS_DEACTIVATION);
					}
				}
				if (body1)	
				{
					if ( body1->GetActivationState() == ISLAND_SLEEPING)
					{
						body1->SetActivationState(WANTS_DEACTIVATION);
					}
				}

			}

			///This island solving can all be scheduled in parallel
			ContactSolverInfo info;
			info.m_friction = 0.9f;
			info.m_numIterations = numIterations;
			info.m_timeStep = timeStep;
			
			info.m_restitution = 0.0f;//m_restitution;
			
			info.m_sor = m_sor;
			info.m_tau = m_tau;
			info.m_damping = m_damping;

			m_solver->SolveGroup( &islandmanifold[0], islandmanifold.size(),info,debugDrawer );

		}
	}




}



CollisionAlgorithm* ToiContactDispatcher::InternalFindAlgorithm(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1)
{
	m_count++;
	
	CollisionAlgorithmConstructionInfo ci;
	ci.m_dispatcher = this;
	
	if (proxy0.IsConvexShape() && proxy1.IsConvexShape() )
	{
		return new ConvexConvexAlgorithm(0,ci,&proxy0,&proxy1);			
	}

	if (proxy0.IsConvexShape() && proxy1.IsConcaveShape())
	{
		return new ConvexConcaveCollisionAlgorithm(ci,&proxy0,&proxy1);
	}

	if (proxy1.IsConvexShape() && proxy0.IsConcaveShape())
	{
		return new ConvexConcaveCollisionAlgorithm(ci,&proxy1,&proxy0);
	}

	//failed to find an algorithm
	return new EmptyAlgorithm(ci);
	
}
