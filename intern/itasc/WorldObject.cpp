/* $Id: WorldObject.cpp 19907 2009-04-23 13:41:59Z ben2610 $
 * WorldObject.cpp
 *
 *  Created on: Feb 10, 2009
 *      Author: benoitbolsee
 */

#include "WorldObject.hpp"

namespace iTaSC{

/* special singleton to be used as base for uncontrolled object */
WorldObject Object::world;

WorldObject::WorldObject():UncontrolledObject()
{
	initialize(0,1);
	m_internalPose = Frame::Identity();
}

WorldObject::~WorldObject() 
{
}


}
