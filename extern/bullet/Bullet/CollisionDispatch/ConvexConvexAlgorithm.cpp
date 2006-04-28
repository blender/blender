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

//#include "NarrowPhaseCollision/EpaPenetrationDepthSolver.h"

#ifdef WIN32
#if _MSC_VER >= 1310
//only use SIMD Hull code under Win32
#ifdef TEST_HULL
#define USE_HULL 1
#endif //TEST_HULL
#endif //_MSC_VER 
#endif //WIN32


#ifdef USE_HULL

#include "NarrowPhaseCollision/Hull.h"
#include "NarrowPhaseCollision/HullContactCollector.h"


#endif //USE_HULL

bool gUseEpa = false;


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

//static EpaPenetrationDepthSolver	gEpaPenetrationDepthSolver;

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
			
		//	m_gjkPairDetector.SetPenetrationDepthSolver(&gEpaPenetrationDepthSolver);
						
			
		} else
		{
			m_gjkPairDetector.SetPenetrationDepthSolver(&gPenetrationDepthSolver);
		}
	}
	
}

#ifdef USE_HULL

Transform	GetTransformFromSimdTransform(const SimdTransform& trans)
{
			//const SimdVector3& rowA0 = trans.getBasis().getRow(0);
			////const SimdVector3& rowA1 = trans.getBasis().getRow(1);
			//const SimdVector3& rowA2 = trans.getBasis().getRow(2);

			SimdVector3 rowA0 = trans.getBasis().getColumn(0);
			SimdVector3 rowA1 = trans.getBasis().getColumn(1);
			SimdVector3 rowA2 = trans.getBasis().getColumn(2);


			Vector3	x(rowA0.getX(),rowA0.getY(),rowA0.getZ());
			Vector3	y(rowA1.getX(),rowA1.getY(),rowA1.getZ());
			Vector3	z(rowA2.getX(),rowA2.getY(),rowA2.getZ());
			
			Matrix33 ornA(x,y,z);
	
			Point3 transA(
				trans.getOrigin().getX(),
				trans.getOrigin().getY(),
				trans.getOrigin().getZ());

			return Transform(ornA,transA);
}

class ManifoldResultCollector : public HullContactCollector
{
public:
	ManifoldResult& m_manifoldResult;

	ManifoldResultCollector(ManifoldResult& manifoldResult)
		:m_manifoldResult(manifoldResult)
	{

	}
	

	virtual ~ManifoldResultCollector() {};

	virtual int	BatchAddContactGroup(const Separation& sep,int numContacts,const Vector3& normalWorld,const Vector3& tangent,const Point3* positionsWorld,const float* depths)
	{
		for (int i=0;i<numContacts;i++)
		{
			//printf("numContacts = %i\n",numContacts);
			SimdVector3 normalOnBInWorld(sep.m_axis.GetX(),sep.m_axis.GetY(),sep.m_axis.GetZ());
			//normalOnBInWorld.normalize();
			SimdVector3 pointInWorld(positionsWorld[i].GetX(),positionsWorld[i].GetY(),positionsWorld[i].GetZ());
			float depth = -depths[i];
			m_manifoldResult.AddContactPoint(normalOnBInWorld,pointInWorld,depth);

		}
		return 0;
	}

	virtual int		GetMaxNumContacts() const
	{
		return 4;
	}

};
#endif //USE_HULL

//
// box-box collision algorithm, for simplicity also applies resolution-impulse
//
void ConvexConvexAlgorithm ::ProcessCollision (BroadphaseProxy* ,BroadphaseProxy* ,const DispatcherInfo& dispatchInfo)
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

#ifdef USE_HULL


	if (dispatchInfo.m_enableSatConvex)
	{
		if ((col0->m_collisionShape->IsPolyhedral()) &&
			(col1->m_collisionShape->IsPolyhedral()))
		{
		
			
			PolyhedralConvexShape* polyhedron0 = static_cast<PolyhedralConvexShape*>(col0->m_collisionShape);
			PolyhedralConvexShape* polyhedron1 = static_cast<PolyhedralConvexShape*>(col1->m_collisionShape);
			if (polyhedron0->m_optionalHull && polyhedron1->m_optionalHull)
			{
				//printf("Hull-Hull");

				//todo: cache this information, rather then initialize
				Separation sep;
				sep.m_featureA = 0;
				sep.m_featureB = 0;
				sep.m_contact = -1;
				sep.m_separator = 0;

				//convert from SimdTransform to Transform
				
				Transform trA = GetTransformFromSimdTransform(col0->m_worldTransform);
				Transform trB = GetTransformFromSimdTransform(col1->m_worldTransform);

				//either use persistent manifold or clear it every time
				m_manifoldPtr->ClearManifold();
				ManifoldResult* resultOut = m_dispatcher->GetNewManifoldResult(col0,col1,m_manifoldPtr);

				ManifoldResultCollector hullContactCollector(*resultOut);
				
				Hull::ProcessHullHull(sep,*polyhedron0->m_optionalHull,*polyhedron1->m_optionalHull,
					trA,trB,&hullContactCollector);

				
				//user provided hull's, so we use SAT Hull collision detection
				return;
			}
		}
	}

#endif //USE_HULL

	
	ManifoldResult* resultOut = m_dispatcher->GetNewManifoldResult(col0,col1,m_manifoldPtr);
	
	ConvexShape* min0 = static_cast<ConvexShape*>(col0->m_collisionShape);
	ConvexShape* min1 = static_cast<ConvexShape*>(col1->m_collisionShape);
	
	GjkPairDetector::ClosestPointInput input;

	SphereShape	sphere(0.2f);
	MinkowskiSumShape	expanded0(min0,&sphere);
	MinkowskiSumShape	expanded1(min1,&sphere);

	if (dispatchInfo.m_useContinuous)
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
		input.m_maximumDistanceSquared = min0->GetMargin() + min1->GetMargin() + m_manifoldPtr->GetContactBreakingTreshold();
		input.m_maximumDistanceSquared*= input.m_maximumDistanceSquared;
	}

	input.m_maximumDistanceSquared = 1e30f;//
	
	input.m_transformA = col0->m_worldTransform;
	input.m_transformB = col1->m_worldTransform;
    
	m_gjkPairDetector.GetClosestPoints(input,*resultOut,dispatchInfo.m_debugDraw);

	m_dispatcher->ReleaseManifoldResult(resultOut);
}
bool disableCcd = false;
float	ConvexConvexAlgorithm::CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,const DispatcherInfo& dispatchInfo)
{

	CheckPenetrationDepthSolver();

	//An adhoc way of testing the Continuous Collision Detection algorithms
	//One object is approximated as a point, to simplify things
	//Starting in penetration should report no time of impact
	//For proper CCD, better accuracy and handling of 'allowed' penetration should be added
	//also the mainloop of the physics should have a kind of toi queue (something like Brian Mirtich's application of Timewarp for Rigidbodies)

	bool needsCollision = m_dispatcher->NeedsCollision(m_box0,m_box1);

	if (!needsCollision)
		return 1.f;

	
	CollisionObject* col0 = static_cast<CollisionObject*>(m_box0.m_clientObject);
	CollisionObject* col1 = static_cast<CollisionObject*>(m_box1.m_clientObject);
	
	SphereShape	sphere(0.f);

	ConvexShape* min0 = static_cast<ConvexShape*>(col0->m_collisionShape);
	ConvexShape* min1 = static_cast<ConvexShape*>(col1->m_collisionShape);
	
	ConvexCast::CastResult result;


	VoronoiSimplexSolver voronoiSimplex;
	SubsimplexConvexCast ccd0(&sphere,min1,&voronoiSimplex);

	///Simplification, one object is simplified as a sphere
	GjkConvexCast ccd1(&sphere,min0,&voronoiSimplex);
	//ContinuousConvexCollision ccd(min0,min1,&voronoiSimplex,0);

	if (disableCcd)
		return 1.f;

	if (ccd1.calcTimeOfImpact(col0->m_worldTransform,col0->m_nextPredictedWorldTransform,
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
