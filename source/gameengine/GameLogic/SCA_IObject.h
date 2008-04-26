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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * An abstract object that has some logic, python scripting and
 * reference counting Note: transformation stuff has been moved to
 * SceneGraph
 */

#ifndef SCA_IOBJECT_H
#define SCA_IOBJECT_H

#include "Value.h"
#include <vector>

class SCA_ISensor;
class SCA_IController;
class SCA_IActuator;

template<class T> T PyVecTo(PyObject*);

typedef std::vector<SCA_ISensor *>       SCA_SensorList;
typedef std::vector<SCA_IController *>   SCA_ControllerList;
typedef std::vector<SCA_IActuator *>     SCA_ActuatorList;

class SCA_IObject :	public CValue
{
	
	Py_Header;
	
protected:
	SCA_SensorList         m_sensors;
	SCA_ControllerList     m_controllers;
	SCA_ActuatorList       m_actuators;
	SCA_ActuatorList       m_registeredActuators;	// actuators that use a pointer to this object
	static class MT_Point3 m_sDummy;

	/**
	 * Ignore activity culling requests?
	 */
	bool m_ignore_activity_culling;

	/**
	 * Ignore updates?
	 */
	bool m_suspended;
	
public:
	
	SCA_IObject(PyTypeObject* T=&Type);
	virtual ~SCA_IObject();

	SCA_ControllerList& GetControllers();
	SCA_SensorList& GetSensors();
	SCA_ActuatorList& GetActuators();

	void AddSensor(SCA_ISensor* act);
	void AddController(SCA_IController* act);
	void AddActuator(SCA_IActuator* act);
	void RegisterActuator(SCA_IActuator* act);
	void UnregisterActuator(SCA_IActuator* act);
	
	SCA_ISensor* FindSensor(const STR_String& sensorname);
	SCA_IActuator* FindActuator(const STR_String& actuatorname);
	SCA_IController* FindController(const STR_String& controllername);

	void SetCurrentTime(float currentTime);

	void ReParentLogic();
	
	/**
	 * Set whether or not to ignore activity culling requests
	 */
	void SetIgnoreActivityCulling(bool b);

	/**
	 * Set whether or not this object wants to ignore activity culling
	 * requests
	 */
	bool GetIgnoreActivityCulling();

	/**
	 * Suspend all progress.
	 */
	void Suspend(void);
	
	/**
	 * Resume progress
	 */
	void Resume(void);
	
//	const class MT_Point3&	ConvertPythonPylist(PyObject* pylist);
	
	// here come the python forwarded methods
	virtual PyObject* _getattr(const STR_String& attr);

	virtual int GetGameObjectType() {return -1;}
	
	typedef enum ObjectTypes {
		OBJ_ARMATURE=0
	}ObjectTypes;

};

#endif //SCA_IOBJECT_H

