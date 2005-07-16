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
#ifndef COLLISION_ALGORITHM_H
#define COLLISION_ALGORITHM_H

struct BroadphaseProxy;
class Dispatcher;

struct CollisionAlgorithmConstructionInfo
{
	CollisionAlgorithmConstructionInfo()
		:m_dispatcher(0)
	{
	}
	CollisionAlgorithmConstructionInfo(Dispatcher* dispatcher,int temp)
		:m_dispatcher(dispatcher)
	{
	}

	Dispatcher*	m_dispatcher;

	int	GetDispatcherId();

};


///CollisionAlgorithm is an collision interface that is compatible with the Broadphase and Dispatcher.
///It is persistent over frames
class CollisionAlgorithm
{

protected:

	Dispatcher*	m_dispatcher;

protected:
	int	GetDispatcherId();
	
public:

	CollisionAlgorithm() {};

	CollisionAlgorithm(const CollisionAlgorithmConstructionInfo& ci);

	virtual ~CollisionAlgorithm() {};

	virtual void ProcessCollision (BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep,int stepCount, bool useContinuous) = 0;

	virtual float CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,float timeStep,int stepCount) = 0;

};


#endif //COLLISION_ALGORITHM_H
