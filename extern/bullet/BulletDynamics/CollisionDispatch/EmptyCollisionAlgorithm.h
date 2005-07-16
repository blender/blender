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
#ifndef EMPTY_ALGORITH
#define EMPTY_ALGORITH
#include "BroadphaseCollision/CollisionAlgorithm.h"

#define ATTRIBUTE_ALIGNED(a)

///EmptyAlgorithm is a stub for unsupported collision pairs.
///The dispatcher can dispatch a persistent EmptyAlgorithm to avoid a search every frame.
class EmptyAlgorithm : public CollisionAlgorithm
{

public:
	
	EmptyAlgorithm(const CollisionAlgorithmConstructionInfo& ci);

	virtual void ProcessCollision (BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep, int stepCount, bool useContinuous);

	virtual float CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep,int stepCount);




} ATTRIBUTE_ALIGNED(16);

#endif //EMPTY_ALGORITH
