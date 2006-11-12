#ifndef DEFAULT_MOTION_STATE_H
#define DEFAULT_MOTION_STATE_H

///btDefaultMotionState provides a common implementation to synchronize world transforms with offsets
struct	btDefaultMotionState : public btMotionState
{
	btTransform m_graphicsWorldTrans;
	btTransform	m_centerOfMassOffset;
	btTransform m_startWorldTrans;
	void*		m_userPointer;

	btDefaultMotionState(const btTransform& startTrans = btTransform::getIdentity(),const btTransform& centerOfMassOffset = btTransform::getIdentity())
		: m_graphicsWorldTrans(startTrans),
		m_centerOfMassOffset(centerOfMassOffset),
		m_startWorldTrans(startTrans),
		m_userPointer(0)

	{
	}

	///synchronizes world transform from user to physics
	virtual void	getWorldTransform(btTransform& centerOfMassWorldTrans ) const 
	{
			centerOfMassWorldTrans = 	m_centerOfMassOffset.inverse() * m_graphicsWorldTrans ;
	}

	///synchronizes world transform from physics to user
	///Bullet only calls the update of worldtransform for active objects
	virtual void	setWorldTransform(const btTransform& centerOfMassWorldTrans)
	{
			m_graphicsWorldTrans = centerOfMassWorldTrans * m_centerOfMassOffset ;
	}

	///Bullet gives a callback for objects that are about to be deactivated (put asleep)
	/// You can intercept this callback for your own bookkeeping. 
	///Also you can return false to disable deactivation for this object this frame.
	virtual bool deactivationCallback(void*	userPointer) {
		return true;
	}

};

#endif //DEFAULT_MOTION_STATE_H
