
#include "ManifoldContactAddResult.h"
#include "NarrowPhaseCollision/PersistentManifold.h"

ManifoldContactAddResult::ManifoldContactAddResult(SimdTransform transA,SimdTransform transB,PersistentManifold* manifoldPtr)
		:m_manifoldPtr(manifoldPtr)
{
	m_transAInv = transA.inverse();
	m_transBInv = transB.inverse();

}


void ManifoldContactAddResult::AddContactPoint(const SimdVector3& normalOnBInWorld,const SimdVector3& pointInWorld,float depth)
{
	if (depth > m_manifoldPtr->GetManifoldMargin())
		return;


	SimdVector3 pointA = pointInWorld + normalOnBInWorld * depth;
	SimdVector3 localA = m_transAInv(pointA );
	SimdVector3 localB = m_transBInv(pointInWorld);
	ManifoldPoint newPt(localA,localB,normalOnBInWorld,depth);

	int insertIndex = m_manifoldPtr->GetCacheEntry(newPt);
	if (insertIndex >= 0)
	{
		m_manifoldPtr->ReplaceContactPoint(newPt,insertIndex);
	} else
	{
		m_manifoldPtr->AddManifoldPoint(newPt);
	}
}

