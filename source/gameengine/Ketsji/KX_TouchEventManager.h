/*
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

/** \file KX_TouchEventManager.h
 *  \ingroup ketsji
 */

#ifndef __KX_TOUCHEVENTMANAGER_H__
#define __KX_TOUCHEVENTMANAGER_H__


#include "SCA_EventManager.h"
#include "KX_TouchSensor.h"
#include "KX_GameObject.h"

#include <vector>
#include <set>

class SCA_ISensor;
class PHY_IPhysicsEnvironment;

class KX_TouchEventManager : public SCA_EventManager
{
	/**
	 * Contains two colliding objects and the first contact point.
	 */
	class NewCollision {
	public:
		PHY_IPhysicsController *first;
		PHY_IPhysicsController *second;
		PHY_CollData *colldata;

		/**
	     * Creates a copy of the given PHY_CollData; freeing that copy should be done by the owner of
	     * the NewCollision object.
	     *
	     * This allows us to efficiently store NewCollision objects in a std::set without creating more
	     * copies of colldata, as the NewCollision copy constructor reuses the pointer and doesn't clone
	     * it again. */
		NewCollision(PHY_IPhysicsController *first,
		             PHY_IPhysicsController *second,
		             const PHY_CollData *colldata);
		NewCollision(const NewCollision &to_copy);
		bool operator<(const NewCollision &other) const;
	};

	PHY_IPhysicsEnvironment*	m_physEnv;
	
	std::set<NewCollision> m_newCollisions;
	
	
	static bool newCollisionResponse(void *client_data, 
						void *object1,
						void *object2,
						const PHY_CollData *coll_data);

	static bool newBroadphaseResponse(void *client_data, 
						void *object1,
						void *object2,
						const PHY_CollData *coll_data);

	virtual bool	NewHandleCollision(void* obj1,void* obj2,
						const PHY_CollData * coll_data); 





public:
	KX_TouchEventManager(class SCA_LogicManager* logicmgr,  
		PHY_IPhysicsEnvironment* physEnv);
	virtual void NextFrame();
	virtual void	EndFrame();
	virtual void RegisterSensor(SCA_ISensor* sensor);
	virtual void RemoveSensor(SCA_ISensor* sensor);
	SCA_LogicManager* GetLogicManager() { return m_logicmgr;}
	PHY_IPhysicsEnvironment *GetPhysicsEnvironment() { return m_physEnv; }

	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:KX_TouchEventManager")
#endif
};

#endif  /* __KX_TOUCHEVENTMANAGER_H__ */
