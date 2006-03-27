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

#include "ConvexConvexAlgorithm.h"

#include <stdio.h>
#include "NarrowPhaseCollision/DiscreteCollisionDetectorInterface.h"
#include "BroadphaseCollision/BroadphaseInterface.h"
#include "CollisionDispatch/CollisionObject.h"
#include "CollisionShapes/ConvexShape.h"
#include "NarrowPhaseCollision/GjkPairDetector.h"
#include "BroadphaseCollision/BroadphaseProxy.h"
#include "CollisionDispatch/CollisionDispatcher.h"
#include "CollisionShapes/BoxShape.h"
#include "CollisionDispatch/ManifoldResult.h"

#include "NarrowPhaseCollision/ConvexPenetrationDepthSolver.h"
#include "NarrowPhaseCollision/ContinuousConvexCollision.h"
#include "NarrowPhaseCollision/SubSimplexConvexCast.h"
#include "NarrowPhaseCollision/GjkConvexCast.h"



#include "CollisionShapes/MinkowskiSumShape.h"
#include "NarrowPhaseCollision/VoronoiSimplexSolver.h"
#include "CollisionShapes/SphereShape.h"

#include "NarrowPhaseCollision/MinkowskiPenetrationDepthSolver.h"

///Solid3EpaPenetrationDepth is not shipped by default, the license doesn't allow commercial, closed source. contact if you want the file
///It improves the penetration depth handling dramatically
//#define USE_EPA
#ifdef USE_EPA
#include "../Extras/ExtraSolid35/Solid3EpaPenetrationDepth.h"
bool gUseEpa = true;
#else
bool gUseEpa = false;
#endif// USE_EPA

#ifdef WIN32
void DrawRasterizerLine(const float* from,const float* to,int color);
#endif




//#define PROCESS_SINGLE_CONTACT
#ifdef WIN32
bool gForceBoxBox = false;//false;//true;

#else
bool gForceBoxBox = false;//false;//true;
#endif
bool gBoxBoxUseGjk = true;//true;//false;
bool gDisableConvexCollision = false;



ConvexConvexAlgorithm::ConvexConvexAlgorithm(PersistentManifold* mf,const CollisionAlgorithmConstructionInfo& ci,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1)
: CollisionAlgorithm(ci),
m_gjkPairDetector(0,0,&m_simplexSolver,0),
m_useEpa(!gUseEpa),
m_box0(*proxy0),
m_box1(*proxy1),
m_ownManifold (false),
m_manifoldPtr(mf),
m_lowLevelOfDetail(false)
{
	CheckPenetrationDepthSolver();

	{
		if (!m_manifoldPtr && m_dispatcher->NeedsCollision(m_box0,m_box1))
		{
			m_manifoldPtr = m_dispatcher->GetNewManifold(proxy0->m_clientObject,proxy1->m_clientObject);
			m_ownManifold = true;
		}
	}

}



ConvexConvexAlgorithm::~ConvexConvexAlgorithm()
{
	if (m_ownManifold)
	{
		if (m_manifoldPtr)
			m_dispatcher->ReleaseManifold(m_manifoldPtr);
	}
}

void	ConvexConvexAlgorithm ::SetLowLevelOfDetail(bool useLowLevel)
{
	m_lowLevelOfDetail = useLowLevel;
}

float	ConvexConvexAlgorithm::GetCollisionImpulse() const
{
	if (m_manifoldPtr)
		return m_manifoldPtr->GetCollisionImpulse();
	
	return 0.f;
}


class FlippedContactResult : public DiscreteCollisionDetectorInterface::Result
{
	DiscreteCollisionDetectorInterface::Result* m_org;

public:

	FlippedContactResult(DiscreteCollisionDetectorInterface::Result* org)
		: m_org(org)
	{

	}

	virtual void AddContactPoint(const SimdVector3& normalOnBInWorld,const SimdVector3& pointInWorld,float depth)
	{
		SimdVector3 flippedNormal = -normalOnBInWorld;

		m_org->AddContactPoint(flippedNormal,pointInWorld,depth);
	}

};

static MinkowskiPenetrationDepthSolver	gPenetrationDepthSolver;

#ifdef USE_EPA
Solid3EpaPenetrationDepth	gSolidEpaPenetrationSolver;
#endif //USE_EPA

void	ConvexConvexAlgorithm::CheckPenetrationDepthSolver()
{
	if (m_useEpa != gUseEpa)
	{
		m_useEpa  = gUseEpa;
		if (m_useEpa)
		{
			//not distributed, see top of this file
			#ifdef USE_EPA
			m_gjkPairDetector.SetPenetrationDepthSolver(&gSolidEpaPenetrationSolver);
			#else
			m_gjkPairDetector.SetPenetrationDepthSolver(&gPenetrationDepthSolver);
			#endif
			
		} else
		{
			m_gjkPairDetector.SetPenetrationDepthSolver(&gPenetrationDepthSolver);
		}
	}
	
}

//
// box-box collision algorithm, for simplicity also applies resolution-impulse
//
void ConvexConvexAlgorithm ::ProcessCollision (BroadphaseProxy* ,BroadphaseProxy* ,float timeStep,int stepCount, bool useContinuous)
{
	if (!m_manifoldPtr)
		return;

	CheckPenetrationDepthSolver();

//	printf("ConvexConvexAlgorithm::ProcessCollision\n");

	bool needsCollision = m_dispatcher->NeedsCollision(m_box0,m_box1);
	if (!needsCollision)
		return;
	
	CollisionObject*	col0 = static_cast<CollisionObject*>(m_box0.m_clientObject);
	CollisionObject*	col1 = static_cast<CollisionObject*>(m_box1.m_clientObject);
	
	ManifoldResult* resultOut = m_dispatcher->GetNewManifoldResult(col0,col1,m_manifoldPtr);
	
	ConvexShape* min0 = static_cast<ConvexShape*>(col0->m_collisionShape);
	ConvexShape* min1 = static_cast<ConvexShape*>(col1->m_collisionShape);
	
	GjkPairDetector::ClosestPointInput input;

	SphereShape	sphere(0.2f);
	MinkowskiSumShape	expanded0(min0,&sphere);
	MinkowskiSumShape	expanded1(min1,&sphere);

	if (useContinuous)
	{
		m_gjkPairDetector.SetMinkowskiA(&expanded0);
		m_gjkPairDetector.SetMinkowskiB(&expanded1);
		input.m_maximumDistanceSquared = expanded0.GetMargin()+expanded1.GetMargin();
		input.m_maximumDistanceSquared *= input.m_maximumDistanceSquared;
	}
	else
	{
		m_gjkPairDetector.SetMinkowskiA(min0);
		m_gjkPairDetector.SetMinkowskiB(min1);
		input.m_maximumDistanceSquared = min0->GetMargin() + min1->GetMargin() + m_manifoldPtr->GetManifoldMargin();
		input.m_maximumDistanceSquared*= input.m_maximumDistanceSquared;
	}

	input.m_maximumDistanceSquared = 1e30;//
	
	input.m_transformA = col0->m_worldTransform;
	input.m_transformB = col1->m_worldTransform;
    
	m_gjkPairDetector.GetClosestPoints(input,*resultOut);

	m_dispatcher->ReleaseManifoldResult(resultOut);
}
bool disableCcd = false;
float	ConvexConvexAlgorithm::CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep,int stepCount)
{

	CheckPenetrationDepthSolver();


	bool needsCollision = m_dispatcher->NeedsCollision(m_box0,m_box1);

	if (!needsCollision)
		return 1.f;

	
	CollisionObject* col0 = static_cast<CollisionObject*>(m_box0.m_clientObject);
	CollisionObject* col1 = static_cast<CollisionObject*>(m_box1.m_clientObject);
	
	ConvexShape* min0 = static_cast<ConvexShape*>(col0->m_collisionShape);
	ConvexShape* min1 = static_cast<ConvexShape*>(col1->m_collisionShape);
	
	ConvexCast::CastResult result;

	VoronoiSimplexSolver voronoiSimplex;
	//SubsimplexConvexCast ccd(&voronoiSimplex);
	//GjkConvexCast ccd(&voronoiSimplex);
	
	ContinuousConvexCollision ccd(min0,min1,&voronoiSimplex,m_penetrationDepthSolver);

	if (disableCcd)
		return 1.f;

	if (ccd.calcTimeOfImpact(col0->m_worldTransform,col0->m_nextPredictedWorldTransform,
		col1->m_worldTransform,col1->m_nextPredictedWorldTransform,result))
	{
	
		//store result.m_fraction in both bodies
	
		if (col0->m_hitFraction	> result.m_fraction)
			col0->m_hitFraction  = result.m_fraction;

		if (col1->m_hitFraction > result.m_fraction)
			col1->m_hitFraction  = result.m_fraction;

		return result.m_fraction;
	}


	return 1.f;


}
