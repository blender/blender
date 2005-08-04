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
#include "ConvexConvexAlgorithm.h"

#include <stdio.h>
#include "NarrowPhaseCollision/DiscreteCollisionDetectorInterface.h"
#include "BroadphaseCollision/BroadphaseInterface.h"
#include "Dynamics/RigidBody.h"
#include "CollisionShapes/ConvexShape.h"
#include "NarrowPhaseCollision/GjkPairDetector.h"
#include "BroadphaseCollision/BroadphaseProxy.h"
#include "BroadphaseCollision/CollisionDispatcher.h"
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

bool gUseEpa = false;


ConvexConvexAlgorithm::ConvexConvexAlgorithm(PersistentManifold* mf,const CollisionAlgorithmConstructionInfo& ci,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1)
: CollisionAlgorithm(ci),
m_gjkPairDetector(0,0,&m_simplexSolver,0),
m_box0(*proxy0),
m_box1(*proxy1),
m_collisionImpulse(0.f),
m_ownManifold (false),
m_manifoldPtr(mf),
m_lowLevelOfDetail(false),
m_useEpa(gUseEpa)
{
	CheckPenetrationDepthSolver();

	RigidBody* body0 = (RigidBody*)m_box0.m_clientObject;
	RigidBody* body1 = (RigidBody*)m_box1.m_clientObject;

	if ((body0->getInvMass() != 0.f) || 
		(body1->getInvMass() != 0.f))
	{
		if (!m_manifoldPtr)
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

void	ConvexConvexAlgorithm::CheckPenetrationDepthSolver()
{
//	if (m_useEpa != gUseEpa)
	{
		m_useEpa  = gUseEpa;
		if (m_useEpa)
		{
			//not distributed
			//m_gjkPairDetector.SetPenetrationDepthSolver(new Solid3EpaPenetrationDepth);
		} else
		{
			m_gjkPairDetector.SetPenetrationDepthSolver(new MinkowskiPenetrationDepthSolver);
		}
	}
	
}
bool extra = false;

float gFriction = 0.5f;
//
// box-box collision algorithm, for simplicity also applies resolution-impulse
//
void ConvexConvexAlgorithm ::ProcessCollision (BroadphaseProxy* ,BroadphaseProxy* ,float timeStep,int stepCount, bool useContinuous)
{
	CheckPenetrationDepthSolver();

//	printf("ConvexConvexAlgorithm::ProcessCollision\n");
m_collisionImpulse = 0.f;
	
	RigidBody* body0 = (RigidBody*)m_box0.m_clientObject;
	RigidBody* body1 = (RigidBody*)m_box1.m_clientObject;

	//todo: move this in the dispatcher
	if ((body0->GetActivationState() == 2) &&(body1->GetActivationState() == 2))
		return;


	if (!m_manifoldPtr)
		return;

	if ((body0->getInvMass() == 0.f) && 
		(body1->getInvMass() == 0.f))
	{
		return;
	}

	ManifoldResult output(body0,body1,m_manifoldPtr);
	
	ConvexShape* min0 = static_cast<ConvexShape*>(body0->GetCollisionShape());
	ConvexShape* min1 = static_cast<ConvexShape*>(body1->GetCollisionShape());	
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
	
	input.m_transformA = body0->getCenterOfMassTransform();
	input.m_transformB = body1->getCenterOfMassTransform();
    
	m_gjkPairDetector.GetClosestPoints(input,output);

}
bool disableCcd = false;
float	ConvexConvexAlgorithm::CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep,int stepCount)
{

	CheckPenetrationDepthSolver();

	m_collisionImpulse = 0.f;
	
	RigidBody* body0 = (RigidBody*)m_box0.m_clientObject;
	RigidBody* body1 = (RigidBody*)m_box1.m_clientObject;

	if (!m_manifoldPtr)
		return 1.f;

	if ((body0->getInvMass() == 0.f) && 
		(body1->getInvMass() == 0.f))
	{
		return 1.f;
	}


	ConvexShape* min0 = static_cast<ConvexShape*>(body0->GetCollisionShape());
	ConvexShape* min1 = static_cast<ConvexShape*>(body1->GetCollisionShape());	
	
	GjkPairDetector::ClosestPointInput input;
	input.m_transformA = body0->getCenterOfMassTransform();
	input.m_transformB = body1->getCenterOfMassTransform();
	SimdTransform predictA,predictB;

	body0->predictIntegratedTransform(timeStep,predictA);
	body1->predictIntegratedTransform(timeStep,predictB);


	ConvexCast::CastResult result;


	VoronoiSimplexSolver voronoiSimplex;
	//SubsimplexConvexCast ccd(&voronoiSimplex);
	//GjkConvexCast ccd(&voronoiSimplex);
	
	ContinuousConvexCollision ccd(min0,min1,&voronoiSimplex,m_penetrationDepthSolver);

	if (disableCcd)
		return 1.f;

	if (ccd.calcTimeOfImpact(input.m_transformA,predictA,input.m_transformB,predictB,result))
	{
	
		//store result.m_fraction in both bodies
		int i;
		i=0;
		
//		if (result.m_fraction< 0.1f)
//			result.m_fraction = 0.1f;

		if (body0->m_hitFraction > result.m_fraction)
			body0->m_hitFraction  = result.m_fraction;

		if (body1->m_hitFraction > result.m_fraction)
			body1->m_hitFraction  = result.m_fraction;

		return result.m_fraction;
	}

	return 1.f;


}
