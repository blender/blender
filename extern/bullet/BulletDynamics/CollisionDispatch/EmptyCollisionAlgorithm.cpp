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
#include "EmptyCollisionAlgorithm.h"



EmptyAlgorithm::EmptyAlgorithm(const CollisionAlgorithmConstructionInfo& ci)
	: CollisionAlgorithm(ci)
{
}

void EmptyAlgorithm::ProcessCollision (BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep, int stepCount,bool useContinuous)
{

}

float EmptyAlgorithm::CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep,int stepCount)
{
	return 1.f;
}


