/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Copyright 2009 Benoit Bolsee. */

/** \file
 * \ingroup intern_itasc
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
