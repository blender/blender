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
#ifndef __KX_IACTUATOR
#define __KX_IACTUATOR

#include "SCA_IController.h"
#include <vector>

/*
 * Use of SG_DList : None
 * Use of SG_QList : element of activated actuator list of their owner
 *                   Head: SCA_IObject::m_activeActuators
 */
class SCA_IActuator : public SCA_ILogicBrick
{
	friend class SCA_LogicManager;
protected:
	int					 m_type;
	int					 m_links;	// number of active links to controllers
									// when 0, the actuator is automatically stopped
	//std::vector<CValue*> m_events;
	bool			     m_posevent;
	bool			     m_negevent;

	std::vector<class SCA_IController*>		m_linkedcontrollers;

	void RemoveAllEvents()
	{
		m_posevent = false;
		m_negevent = false;
	}


public:
	/**
	 * This class also inherits the default copy constructors
	 */
	enum KX_ACTUATOR_TYPE {
		KX_ACT_OBJECT,
		KX_ACT_IPO,
		KX_ACT_CAMERA,
		KX_ACT_SOUND,
		KX_ACT_PROPERTY,
		KX_ACT_ADD_OBJECT,
		KX_ACT_END_OBJECT,
		KX_ACT_DYNAMIC,
		KX_ACT_REPLACE_MESH,
		KX_ACT_TRACKTO,
		KX_ACT_CONSTRAINT,
		KX_ACT_SCENE,
		KX_ACT_RANDOM,
		KX_ACT_MESSAGE,
		KX_ACT_ACTION,
		KX_ACT_CD,
		KX_ACT_GAME,
		KX_ACT_VISIBILITY,
		KX_ACT_2DFILTER,
		KX_ACT_PARENT,
		KX_ACT_SHAPEACTION,
		KX_ACT_STATE,
		KX_ACT_ARMATURE,
	};

	SCA_IActuator(SCA_IObject* gameobj, KX_ACTUATOR_TYPE type); 

	/**
	 * UnlinkObject(...)
	 * Certain actuator use gameobject pointers (like TractTo actuator)
	 * This function can be called when an object is removed to make
	 * sure that the actuator will not use it anymore.
	 */

	virtual bool UnlinkObject(SCA_IObject* clientobj) { return false; }

	/**
	 * Update(...)
	 * Update the actuator based upon the events received since 
	 * the last call to Update, the current time and deltatime the
	 * time elapsed in this frame ?
	 * It is the responsibility of concrete Actuators to clear
	 * their event's. This is usually done in the Update() method via 
	 * a call to RemoveAllEvents()
	 */


	virtual bool Update(double curtime, bool frame);
	virtual bool Update();

	/** 
	 * Add an event to an actuator.
	 */ 
	//void AddEvent(CValue* event)
	void AddEvent(bool event)
	{
		if (event)
			m_posevent = true;
		else
			m_negevent = true;
	}

	virtual void ProcessReplica();

	/** 
	 * Return true iff all the current events 
	 * are negative. The definition of negative event is
	 * not immediately clear. But usually refers to key-up events
	 * or events where no action is required.
	 */
	bool IsNegativeEvent() const
	{
		return !m_posevent && m_negevent;
	}

	virtual ~SCA_IActuator();

	/**
	 * remove this actuator from the list of active actuators
	 */
	virtual void Deactivate();
	virtual void Activate(SG_DList& head);

	void	LinkToController(SCA_IController* controller);
	void	UnlinkController(class SCA_IController* cont);
	void	UnlinkAllControllers();

	void ClrLink() { m_links=0; }
	void IncLink() { m_links++; }
	void DecLink();
	bool IsNoLink() const { return !m_links; }
	bool IsType(KX_ACTUATOR_TYPE type) { return m_type == type; }
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:SCA_IActuator"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__KX_IACTUATOR

