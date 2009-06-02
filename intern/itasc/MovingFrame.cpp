/* $Id$
 * MovingFrame.cpp
 *
 *  Created on: Feb 10, 2009
 *      Author: benoitbolsee
 */

#include "MovingFrame.hpp"
#include <malloc.h>
#include <string.h>
namespace iTaSC{

static const unsigned int frameCacheSize = sizeof(((Frame*)0)->p.data)+sizeof(((Frame*)0)->M.data);

MovingFrame::MovingFrame(const Frame& frame):UncontrolledObject(),
	m_function(NULL), m_param(NULL), m_velocity(), m_poseCCh(-1), m_poseCTs(0)
{
	m_internalPose = m_nextPose = frame;
	initialize(6, 1);
	e_matrix& Ju = m_JuArray[0];
	Ju = e_identity_matrix(6,6);
}

MovingFrame::~MovingFrame()
{
}

void MovingFrame::finalize()
{
	updateJacobian();
}

void MovingFrame::initCache(Cache *_cache)
{
	m_cache = _cache;
	m_poseCCh = -1;
	if (m_cache) {
		m_poseCCh = m_cache->addChannel(this,"pose",frameCacheSize);
		// don't store the initial pose, it's causing unnecessary large velocity on the first step
		//pushInternalFrame(0);
	}
}

void MovingFrame::pushInternalFrame(CacheTS timestamp)
{
	if (m_poseCCh >= 0) {
		char *item;
		item = (char *)m_cache->addCacheItem(this, m_poseCCh, timestamp, NULL, frameCacheSize);
		if (item) {
			memcpy(item, m_internalPose.p.data, sizeof(m_internalPose.p.data));
			item += sizeof(m_internalPose.p.data);
			memcpy(item, m_internalPose.M.data, sizeof(m_internalPose.M.data));
		}
		m_poseCTs = timestamp;
	}
}

// load pose just preceeding timestamp
// return false if no cache position was found
bool MovingFrame::popInternalFrame(CacheTS timestamp)
{
	if (m_poseCCh >= 0) {
		char *item;
		item = (char *)m_cache->getPreviousCacheItem(this, m_poseCCh, &timestamp);
		if (item && m_poseCTs != timestamp) {
			memcpy(m_internalPose.p.data, item, sizeof(m_internalPose.p.data));
			item += sizeof(m_internalPose.p.data);
			memcpy(m_internalPose.M.data, item, sizeof(m_internalPose.M.data));
			m_poseCTs = timestamp;
			// changing the starting pose, recompute the jacobian
			updateJacobian();
		}
		return (item) ? true : false;
	}
	// in case of no cache, there is always a previous position
	return true;
}

bool MovingFrame::setFrame(const Frame& frame)
{
	m_internalPose = m_nextPose = frame;
	return true;
}

bool MovingFrame::setCallback(MovingFrameCallback _function, void* _param)
{
	m_function = _function;
	m_param = _param;
	return true;
}

void MovingFrame::updateCoordinates(const Timestamp& timestamp)
{
	// don't compute the velocity during substepping, it is assumed constant.
	if (!timestamp.substep) {
		bool cacheAvail = true;
		if (!timestamp.reiterate) {
			cacheAvail = popInternalFrame(timestamp.cacheTimestamp);
			if (m_function)
				(*m_function)(timestamp, m_internalPose, m_nextPose, m_param);
		}
		// only compute velocity if we have a previous pose
		if (cacheAvail) {
			unsigned int iXu;
			m_velocity = diff(m_internalPose, m_nextPose, timestamp.realTimestep);
			for (iXu=0; iXu<6; iXu++)
				m_xudot(iXu) = m_velocity(iXu);
		} else {
			// first position in cache, no velocity as we cannot interpolate
			m_internalPose = m_nextPose;
			m_xudot = e_zero_vector(6);
			// recompute the jacobian
			updateJacobian();
		}
	}
}

void MovingFrame::updateKinematics(const Timestamp& timestamp)
{
	if (timestamp.substep) {
		// during substepping, update the internal pose from velocity information
		Twist localvel = m_internalPose.M.Inverse(m_velocity);
		m_internalPose.Integrate(localvel, 1.0/timestamp.realTimestep);
	} else {
		m_internalPose = m_nextPose;
		pushInternalFrame(timestamp.cacheTimestamp);
	}
	// m_internalPose is updated, recompute the jacobian
	updateJacobian();
}

void MovingFrame::updateJacobian()
{
	Twist m_jac;
	e_matrix& Ju = m_JuArray[0];

    //Jacobian is always identity at position on the object, 
	//we ust change the reference to the world.
	//instead of going through complicated jacobian operation, implemented direct formula
	Ju(1,3) = m_internalPose.p.z();
	Ju(2,3) = -m_internalPose.p.y();
	Ju(0,4) = -m_internalPose.p.z();
	Ju(2,4) = m_internalPose.p.x();
	Ju(0,5) = m_internalPose.p.y();
	Ju(1,5) = -m_internalPose.p.x();
}

}
