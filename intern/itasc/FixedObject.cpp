/* $Id: FixedObject.cpp 19905 2009-04-23 13:29:54Z ben2610 $
 * FixedObject.cpp
 *
 *  Created on: Feb 10, 2009
 *      Author: benoitbolsee
 */

#include "FixedObject.hpp"

namespace iTaSC{


FixedObject::FixedObject():UncontrolledObject(),
	m_finalized(false), m_nframe(0)
{
}

FixedObject::~FixedObject() 
{
	m_frameArray.clear();
}

int FixedObject::addFrame(const std::string& name, const Frame& frame)
{
	if (m_finalized)
		return -1;
	FrameList::iterator it;
	unsigned int i;
	for (i=0, it=m_frameArray.begin(); i<m_nframe; i++, it++) {
		if (it->first == name) {
			// this frame will replace the old frame
			it->second = frame;
			return i;
		}
	}
	m_frameArray.push_back(FrameList::value_type(name,frame));
	return m_nframe++;
}

int FixedObject::addEndEffector(const std::string& name)
{
	// verify that this frame name exist
	FrameList::iterator it;
	unsigned int i;
	for (i=0, it=m_frameArray.begin(); i<m_nframe; i++, it++) {
		if (it->first == name) {
			return i;
		}
	}
	return -1;
}

void FixedObject::finalize()
{
	if (m_finalized)
		return;
	initialize(0, m_nframe);
	m_finalized = true;
}

const Frame& FixedObject::getPose(const unsigned int frameIndex)
{
	if (frameIndex < m_nframe) {
		return m_frameArray[frameIndex].second;
	} else {
		return F_identity;
	}
}

}
