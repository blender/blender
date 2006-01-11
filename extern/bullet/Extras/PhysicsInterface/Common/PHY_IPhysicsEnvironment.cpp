/*
 * Copyright (c) 2001-2005 Erwin Coumans <phy@erwincoumans.com>
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Erwin Coumans makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#include "PHY_IPhysicsEnvironment.h"

/**
*	Physics Environment takes care of stepping the simulation and is a container for physics entities (rigidbodies,constraints, materials etc.)
*	A derived class may be able to 'construct' entities by loading and/or converting
*/



PHY_IPhysicsEnvironment::~PHY_IPhysicsEnvironment()
{

}
