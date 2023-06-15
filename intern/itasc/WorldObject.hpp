/* SPDX-FileCopyrightText: 2009 Ruben Smits
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */

/** \file
 * \ingroup intern_itasc
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
