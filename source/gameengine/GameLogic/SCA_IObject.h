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
/** \file SCA_IController.h
 *  \ingroup gamelogic
 *  \brief An abstract object that has some logic, python scripting and
 *   reference counting Note: transformation stuff has been moved to
 *   SceneGraph
 */

#ifndef __SCA_IOBJECT_H__
#define __SCA_IOBJECT_H__

#include "Value.h"
#include <vector>

class SCA_IObject;
class SCA_ISensor;
class SCA_IController;
class SCA_IActuator;

#ifdef WITH_PYTHON
template<class T> T PyVecTo(PyObject*);
#endif

typedef std::vector<SCA_ISensor *>       SCA_SensorList;
typedef std::vector<SCA_IController *>   SCA_ControllerList;
typedef std::vector<SCA_IActuator *>     SCA_ActuatorList;
typedef std::vector<SCA_IObject *>		 SCA_ObjectList;

class SCA_IObject :	public CValue
{
	
	Py_Header
	
protected:
	friend class KX_StateActuator;
	friend class SCA_IActuator;
	friend class SCA_IController;
	SCA_SensorList         m_sensors;
	SCA_ControllerList     m_controllers;
	SCA_ActuatorList       m_actuators;
	SCA_ActuatorList       m_registeredActuators;	// actuators that use a pointer to this object
	SCA_ObjectList		   m_registeredObjects;		// objects that hold reference to this object

	// SG_Dlist: element of objects with active actuators
	//           Head: SCA_LogicManager::m_activeActuators
	// SG_QList: Head of active actuators list on this object
	//           Elements: SCA_IActuator
	SG_QList			   m_activeActuators;
	// SG_Dlist: element of list os lists with active controllers
	//           Head: SCA_LogicManager::m_activeControllers
	// SG_QList: Head of active controller list on this object
	//           Elements: SCA_IController
	SG_QList			   m_activeControllers;
	// SG_Dlist: element of list of lists of active controllers
	//           Head: SCA_LogicManager::m_activeControllers
	// SG_QList: Head of active bookmarked controller list globally
	//           Elements: SCA_IController with bookmark option
	static SG_QList		   m_activeBookmarkedControllers;

	static class MT_Point3 m_sDummy;

	/**
	 * Ignore activity culling requests?
	 */
	bool m_ignore_activity_culling;

	/**
	 * Ignore updates?
	 */
	bool m_suspended;

	/**
	 * init state of object (used when object is created)
	 */
	unsigned int			m_initState;

	/**
	 * current state = bit mask of state that are active
	 */
	unsigned int			m_state;

	/**
	 * pointer inside state actuator list for sorting
	 */
	SG_QList*				m_firstState;

public:
	
	SCA_IObject();
	virtual ~SCA_IObject();

	SCA_ControllerList& GetControllers()
	{
		return m_controllers;
	}
	SCA_SensorList& GetSensors()
	{
		return m_sensors;
	}
	SCA_ActuatorList& GetActuators()
	{
		return m_actuators;
	}
	SG_QList& GetActiveActuators()
	{
		return m_activeActuators;
	}

	void AddSensor(SCA_ISensor* act);
	void ReserveSensor(int num)
	{
		m_sensors.reserve(num);
	}
	void AddController(SCA_IController* act);
	void ReserveController(int num)
	{
		m_controllers.reserve(num);
	}
	void AddActuator(SCA_IActuator* act);
	void ReserveActuator(int num)
	{
		m_actuators.reserve(num);
	}
	void RegisterActuator(SCA_IActuator* act);
	void UnregisterActuator(SCA_IActuator* act);
	
	void RegisterObject(SCA_IObject* objs);
	void UnregisterObject(SCA_IObject* objs);
	/**
	 * UnlinkObject(...)
	 * this object is informed that one of the object to which it holds a reference is deleted
	 * returns true if there was indeed a reference.
	 */
	virtual bool UnlinkObject(SCA_IObject* clientobj) { return false; }

	SCA_ISensor* FindSensor(const STR_String& sensorname);
	SCA_IActuator* FindActuator(const STR_String& actuatorname);
	SCA_IController* FindController(const STR_String& controllername);

	void SetCurrentTime(float currentTime) {}

	virtual void ReParentLogic();
	
	/**
	 * Set whether or not to ignore activity culling requests
	 */
	void SetIgnoreActivityCulling(bool b)
	{
		m_ignore_activity_culling = b;
	}

	/**
	 * Set whether or not this object wants to ignore activity culling
	 * requests
	 */
	bool GetIgnoreActivityCulling()
	{
		return m_ignore_activity_culling;
	}

	/**
	 * Suspend all progress.
	 */
	void Suspend(void);
	
	/**
	 * Resume progress
	 */
	void Resume(void);

	/**
	 * Set init state
	 */
	void SetInitState(unsigned int initState) { m_initState = initState; }

	/**
	 * initialize the state when object is created
	 */
	void ResetState(void) { SetState(m_initState); }

	/**
	 * Set the object state
	 */
	void SetState(unsigned int state);

	/**
	 * Get the object state
	 */
	unsigned int GetState(void)	{ return m_state; }

//	const class MT_Point3&	ConvertPythonPylist(PyObject* pylist);

	virtual int GetGameObjectType() {return -1;}
	
	typedef enum ObjectTypes {
		OBJ_ARMATURE=0,
		OBJ_CAMERA=1,
		OBJ_LIGHT=2,
	}ObjectTypes;

};

#endif //__SCA_IOBJECT_H__

