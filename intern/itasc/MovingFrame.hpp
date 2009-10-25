/* $Id: MovingFrame.hpp 19907 2009-04-23 13:41:59Z ben2610 $
 * MovingFrame.h
 *
 *  Created on: Feb 10, 2009
 *      Author: benoitbolsee
 */

#ifndef MOVINGFRAME_HPP_
#define MOVINGFRAME_HPP_

#include "UncontrolledObject.hpp"
#include <vector>


namespace iTaSC{

typedef bool (*MovingFrameCallback)(const Timestamp& timestamp, const Frame& _current, Frame& _next, void *param);

class MovingFrame: public UncontrolledObject {
public:
    MovingFrame(const Frame& frame=F_identity);
    virtual ~MovingFrame();

	bool setFrame(const Frame& frame);
	bool setCallback(MovingFrameCallback _function, void* _param);

	virtual void updateCoordinates(const Timestamp& timestamp);
	virtual void updateKinematics(const Timestamp& timestamp);
    virtual void pushCache(const Timestamp& timestamp);
	virtual void initCache(Cache *_cache);
	virtual void finalize();
protected:
	virtual void updateJacobian();

private:
	void pushInternalFrame(CacheTS timestamp);
	bool popInternalFrame(CacheTS timestamp);
	MovingFrameCallback m_function;
	void* m_param;
	Frame m_nextPose;
	Twist m_velocity;
	int m_poseCCh;		// cache channel for pose
	unsigned int m_poseCTs;
};

}

#endif /* MOVINGFRAME_H_ */
