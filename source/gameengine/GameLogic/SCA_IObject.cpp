/**
 * $Id$
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
#include <iostream>
#include <algorithm>

#include "SCA_IObject.h"
#include "SCA_ISensor.h"
#include "SCA_IController.h"
#include "SCA_IActuator.h"
#include "MT_Point3.h"
#include "ListValue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

MT_Point3 SCA_IObject::m_sDummy=MT_Point3(0,0,0);
SG_QList SCA_IObject::m_activeBookmarkedControllers;

SCA_IObject::SCA_IObject():
	CValue(),
	m_initState(0),
	m_state(0),
	m_firstState(NULL)
{
	m_suspended = false;
}

SCA_IObject::~SCA_IObject()
{
	SCA_SensorList::iterator its;
	for (its = m_sensors.begin(); !(its == m_sensors.end()); ++its)
	{
		//Use Delete for sensor to ensure proper cleaning
		(*its)->Delete();
		//((CValue*)(*its))->Release();
	}
	SCA_ControllerList::iterator itc; 
	for (itc = m_controllers.begin(); !(itc == m_controllers.end()); ++itc)
	{
		//Use Delete for controller to ensure proper cleaning (expression controller)
		(*itc)->Delete();
		//((CValue*)(*itc))->Release();
	}
	SCA_ActuatorList::iterator ita;
	for (ita = m_registeredActuators.begin(); !(ita==m_registeredActuators.end()); ++ita)
	{
		(*ita)->UnlinkObject(this);
	}
	for (ita = m_actuators.begin(); !(ita==m_actuators.end()); ++ita)
	{
		(*ita)->Delete();
	}

	SCA_ObjectList::iterator ito;
	for (ito = m_registeredObjects.begin(); !(ito==m_registeredObjects.end()); ++ito)
	{
		(*ito)->UnlinkObject(this);
	}

	//T_InterpolatorList::iterator i;
	//for (i = m_interpolators.begin(); !(i == m_interpolators.end()); ++i) {
	//	delete *i;
	//}
}

void SCA_IObject::AddSensor(SCA_ISensor* act)
{
	act->AddRef();
	m_sensors.push_back(act);
}



void SCA_IObject::AddController(SCA_IController* act)
{
	act->AddRef();
	m_controllers.push_back(act);
}



void SCA_IObject::AddActuator(SCA_IActuator* act)
{
	act->AddRef();
	m_actuators.push_back(act);
}

void SCA_IObject::RegisterActuator(SCA_IActuator* act)
{
	// don't increase ref count, it would create dead lock
	m_registeredActuators.push_back(act);
}

void SCA_IObject::UnregisterActuator(SCA_IActuator* act)
{
	SCA_ActuatorList::iterator ita;
	for (ita = m_registeredActuators.begin(); ita != m_registeredActuators.end(); ++ita)
	{
		if ((*ita) == act) {
			(*ita) = m_registeredActuators.back();
			m_registeredActuators.pop_back();
			break;
		}
	}
}

void SCA_IObject::RegisterObject(SCA_IObject* obj)
{
	// one object may be registered multiple times via constraint target
	// store multiple reference, this will serve as registration counter
	m_registeredObjects.push_back(obj);
}

void SCA_IObject::UnregisterObject(SCA_IObject* obj)
{
	SCA_ObjectList::iterator ito;
	for (ito = m_registeredObjects.begin(); ito != m_registeredObjects.end(); ++ito)
	{
		if ((*ito) == obj) {
			(*ito) = m_registeredObjects.back();
			m_registeredObjects.pop_back();
			break;
		}
	}
}

void SCA_IObject::ReParentLogic()
{
	SCA_ActuatorList& oldactuators  = GetActuators();
	int act = 0;
	SCA_ActuatorList::iterator ita;
	for (ita = oldactuators.begin(); !(ita==oldactuators.end()); ++ita)
	{
		SCA_IActuator* newactuator = (SCA_IActuator*) (*ita)->GetReplica();
		newactuator->ReParent(this);
		// actuators are initially not connected to any controller
		newactuator->SetActive(false);
		newactuator->ClrLink();
		oldactuators[act++] = newactuator;
	}

	SCA_ControllerList& oldcontrollers = GetControllers();
	int con = 0;
	SCA_ControllerList::iterator itc;
	for (itc = oldcontrollers.begin(); !(itc==oldcontrollers.end()); ++itc)
	{
		SCA_IController* newcontroller = (SCA_IController*)(*itc)->GetReplica();
		newcontroller->ReParent(this);
		newcontroller->SetActive(false);
		oldcontrollers[con++]=newcontroller;

	}
	// convert sensors last so that actuators are already available for Actuator sensor
	SCA_SensorList& oldsensors = GetSensors();
	int sen = 0;
	SCA_SensorList::iterator its;
	for (its = oldsensors.begin(); !(its==oldsensors.end()); ++its)
	{
		SCA_ISensor* newsensor = (SCA_ISensor*)(*its)->GetReplica();
		newsensor->ReParent(this);
		newsensor->SetActive(false);
		// sensors are initially not connected to any controller
		newsensor->ClrLink();
		oldsensors[sen++] = newsensor;
	}

	// a new object cannot be client of any actuator
	m_registeredActuators.clear();
	m_registeredObjects.clear();
}



SCA_ISensor* SCA_IObject::FindSensor(const STR_String& sensorname)
{
	SCA_ISensor* foundsensor = NULL;

	for (SCA_SensorList::iterator its = m_sensors.begin();!(its==m_sensors.end());++its)
	{
		if ((*its)->GetName() == sensorname)
		{
			foundsensor = (*its);
			break;
		}
	}
	return foundsensor;
}



SCA_IController* SCA_IObject::FindController(const STR_String& controllername)
{
	SCA_IController* foundcontroller = NULL;

	for (SCA_ControllerList::iterator itc = m_controllers.begin();!(itc==m_controllers.end());++itc)
	{
		if ((*itc)->GetName() == controllername)
		{
			foundcontroller = (*itc);
			break;
		}	
	}
	return foundcontroller;
}



SCA_IActuator* SCA_IObject::FindActuator(const STR_String& actuatorname)
{
	SCA_IActuator* foundactuator = NULL;

	for (SCA_ActuatorList::iterator ita = m_actuators.begin();!(ita==m_actuators.end());++ita)
	{
		if ((*ita)->GetName() == actuatorname)
		{
			foundactuator = (*ita);
			break;
		}
	}

	return foundactuator;
}


void SCA_IObject::Suspend()
{
	if ((!m_ignore_activity_culling) 
		&& (!m_suspended))  {
		m_suspended = true;
		/* flag suspend for all sensors */
		SCA_SensorList::iterator i = m_sensors.begin();
		while (i != m_sensors.end()) {
			(*i)->Suspend();
			++i;
		}
	}
}



void SCA_IObject::Resume(void)
{
	if (m_suspended) {
		m_suspended = false;
		/* unflag suspend for all sensors */
		SCA_SensorList::iterator i = m_sensors.begin();
		while (i != m_sensors.end()) {
			(*i)->Resume();
			++i;
		}
	}
}

void SCA_IObject::SetState(unsigned int state)
{
	unsigned int tmpstate;
	SCA_ControllerList::iterator contit;

	// we will update the state in two steps:
	// 1) set the new state bits that are 1
	// 2) clr the new state bits that are 0
	// This to ensure continuity if a sensor is attached to two states
	// that are switching state: no need to deactive and reactive the sensor 
	
	tmpstate = m_state | state;
	if (tmpstate != m_state)
	{
		// update the status of the controllers
		for (contit = m_controllers.begin(); contit != m_controllers.end(); ++contit)
		{
			(*contit)->ApplyState(tmpstate);
		}
	}
	m_state = state;
	if (m_state != tmpstate)
	{
		for (contit = m_controllers.begin(); contit != m_controllers.end(); ++contit)
		{
			(*contit)->ApplyState(m_state);
		}
	}
}

#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_IObject::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_IObject",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_IObject::Methods[] = {
	//{"setOrientation", (PyCFunction) SCA_IObject::sPySetOrientation, METH_VARARGS},
	//{"getOrientation", (PyCFunction) SCA_IObject::sPyGetOrientation, METH_VARARGS},
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_IObject::Attributes[] = {
	{ NULL }	//Sentinel
};

#endif // DISABLE_PYTHON
