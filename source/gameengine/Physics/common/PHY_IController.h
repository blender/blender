/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef PHY_ICONTROLLER_H
#define PHY_ICONTROLLER_H

#include "PHY_DynamicTypes.h"

class PHY_IPhysicsEnvironment;

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

/**
	PHY_IController is the abstract simplified Interface to objects 
	controlled by the physics engine. This includes the physics objects
	and the graphics object for view frustrum and occlusion culling.
*/
class PHY_IController	
{
	public:
		virtual ~PHY_IController();
		// clientinfo for raycasts for example
		virtual	void*	getNewClientInfo()=0;
		virtual	void	setNewClientInfo(void* clientinfo)=0;
		virtual void	SetPhysicsEnvironment(class PHY_IPhysicsEnvironment *env)=0;

	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:PHY_IController"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //PHY_ICONTROLLER_H

