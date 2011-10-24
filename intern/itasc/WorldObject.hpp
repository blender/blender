/*
 * WorldObject.h
 *
 *  Created on: Feb 10, 2009
 *      Author: benoitbolsee
 */

#ifndef WORLDOBJECT_HPP_
#define WORLDOBJECT_HPP_

#include "UncontrolledObject.hpp"
namespace iTaSC{

class WorldObject: public UncontrolledObject {
public:
    WorldObject();
    virtual ~WorldObject();

	virtual void updateCoordinates(const Timestamp& timestamp) {};
	virtual void updateKinematics(const Timestamp& timestamp) {};
	virtual void pushCache(const Timestamp& timestamp) {};
	virtual void initCache(Cache *_cache) {};
protected:
	virtual void updateJacobian() {}

};

}

#endif /* WORLDOBJECT_H_ */
