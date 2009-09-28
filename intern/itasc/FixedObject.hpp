/* $Id: FixedObject.hpp 19905 2009-04-23 13:29:54Z ben2610 $
 * FixedObject.h
 *
 *  Created on: Feb 10, 2009
 *      Author: benoitbolsee
 */

#ifndef FIXEDOBJECT_HPP_
#define FIXEDOBJECT_HPP_

#include "UncontrolledObject.hpp"
#include <vector>


namespace iTaSC{

class FixedObject: public UncontrolledObject {
public:
    FixedObject();
    virtual ~FixedObject();

	int addFrame(const std::string& name, const Frame& frame);

	virtual void updateCoordinates(const Timestamp& timestamp) {};
	virtual int addEndEffector(const std::string& name);
	virtual void finalize();
	virtual const Frame& getPose(const unsigned int frameIndex);
	virtual void updateKinematics(const Timestamp& timestamp) {};
	virtual void pushCache(const Timestamp& timestamp) {};
	virtual void initCache(Cache *_cache) {};

protected:
	virtual void updateJacobian() {}
private:
    typedef std::vector<std::pair<std::string, Frame> > FrameList;

	bool m_finalized;
	unsigned int m_nframe;
	FrameList m_frameArray;

};

}

#endif /* FIXEDOBJECT_H_ */
