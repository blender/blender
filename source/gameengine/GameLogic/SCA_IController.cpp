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

#include "SCA_IController.h"
#include "SCA_LogicManager.h"
#include "SCA_IActuator.h"
#include "SCA_ISensor.h"
#include "PyObjectPlus.h"
#include "../Ketsji/KX_PythonSeq.h" /* not nice, only need for KX_PythonSeq_CreatePyObject */

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SCA_IController::SCA_IController(SCA_IObject* gameobj)
	:
	SCA_ILogicBrick(gameobj),
	m_statemask(0),
	m_justActivated(false)
{
}
	

	
SCA_IController::~SCA_IController()
{
	//UnlinkAllActuators();
}



std::vector<class SCA_ISensor*>& SCA_IController::GetLinkedSensors()
{
	return m_linkedsensors;
}



std::vector<class SCA_IActuator*>& SCA_IController::GetLinkedActuators()
{
	return m_linkedactuators;
}



void SCA_IController::UnlinkAllSensors()
{
	std::vector<class SCA_ISensor*>::iterator sensit;
	for (sensit = m_linkedsensors.begin();!(sensit==m_linkedsensors.end());++sensit)
	{
		if (IsActive()) 
		{
			(*sensit)->DecLink();
		}
		(*sensit)->UnlinkController(this);
	}
	m_linkedsensors.clear();
}



void SCA_IController::UnlinkAllActuators()
{
	std::vector<class SCA_IActuator*>::iterator actit;
	for (actit = m_linkedactuators.begin();!(actit==m_linkedactuators.end());++actit)
	{
		if (IsActive()) 
		{
			(*actit)->DecLink();
		}
		(*actit)->UnlinkController(this);
	}
	m_linkedactuators.clear();
}

void SCA_IController::LinkToActuator(SCA_IActuator* actua)
{
	m_linkedactuators.push_back(actua);
	if (IsActive())
	{
		actua->IncLink();
	}
}

void	SCA_IController::UnlinkActuator(class SCA_IActuator* actua)
{
	std::vector<class SCA_IActuator*>::iterator actit;
	for (actit = m_linkedactuators.begin();!(actit==m_linkedactuators.end());++actit)
	{
		if ((*actit) == actua)
		{
			if (IsActive())
			{
				(*actit)->DecLink();
			}
			*actit = m_linkedactuators.back();
			m_linkedactuators.pop_back();
			return;
		}
	}
	printf("Missing link from controller %s:%s to actuator %s:%s\n", 
		m_gameobj->GetName().ReadPtr(), GetName().ReadPtr(), 
		actua->GetParent()->GetName().ReadPtr(), actua->GetName().ReadPtr());
}

void SCA_IController::LinkToSensor(SCA_ISensor* sensor)
{
	m_linkedsensors.push_back(sensor);
	if (IsActive())
	{
		sensor->IncLink();
	}
}

void SCA_IController::UnlinkSensor(class SCA_ISensor* sensor)
{
	std::vector<class SCA_ISensor*>::iterator sensit;
	for (sensit = m_linkedsensors.begin();!(sensit==m_linkedsensors.end());++sensit)
	{
		if ((*sensit) == sensor)
		{
			if (IsActive())
			{
				sensor->DecLink();
			}
			*sensit = m_linkedsensors.back();
			m_linkedsensors.pop_back();
			return;
		}
	}
	printf("Missing link from controller %s:%s to sensor %s:%s\n", 
		m_gameobj->GetName().ReadPtr(), GetName().ReadPtr(), 
		sensor->GetParent()->GetName().ReadPtr(), sensor->GetName().ReadPtr());
}


void SCA_IController::ApplyState(unsigned int state)
{
	std::vector<class SCA_IActuator*>::iterator actit;
	std::vector<class SCA_ISensor*>::iterator sensit;

	if (m_statemask & state) 
	{
		if (!IsActive()) 
		{
			// reactive the controller, all the links to actuator are valid again
			for (actit = m_linkedactuators.begin();!(actit==m_linkedactuators.end());++actit)
			{
				(*actit)->IncLink();
			}

			for (sensit = m_linkedsensors.begin();!(sensit==m_linkedsensors.end());++sensit)
			{
				(*sensit)->IncLink();
			}
			SetActive(true);
			m_justActivated = true;
		}
	} else if (IsActive())
	{
		for (actit = m_linkedactuators.begin();!(actit==m_linkedactuators.end());++actit)
		{
			(*actit)->DecLink();
		}
		for (sensit = m_linkedsensors.begin();!(sensit==m_linkedsensors.end());++sensit)
		{
			(*sensit)->DecLink();
		}
		SetActive(false);
		m_justActivated = false;
	}
}

#ifndef DISABLE_PYTHON

/* Python api */

PyTypeObject SCA_IController::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_IController",
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
	&SCA_ILogicBrick::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_IController::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_IController::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("state", SCA_IController, pyattr_get_state),
	KX_PYATTRIBUTE_RO_FUNCTION("sensors", SCA_IController, pyattr_get_sensors),
	KX_PYATTRIBUTE_RO_FUNCTION("actuators", SCA_IController, pyattr_get_actuators),
	KX_PYATTRIBUTE_BOOL_RW("useHighPriority",SCA_IController,m_bookmark),
	{ NULL }	//Sentinel
};

PyObject* SCA_IController::pyattr_get_state(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_IController* self= static_cast<SCA_IController*>(self_v);
	return PyLong_FromSsize_t(self->m_statemask);
}

PyObject* SCA_IController::pyattr_get_sensors(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return KX_PythonSeq_CreatePyObject((static_cast<SCA_IController*>(self_v))->m_proxy, KX_PYGENSEQ_CONT_TYPE_SENSORS);	
}

PyObject* SCA_IController::pyattr_get_actuators(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return KX_PythonSeq_CreatePyObject((static_cast<SCA_IController*>(self_v))->m_proxy, KX_PYGENSEQ_CONT_TYPE_ACTUATORS);	
}
#endif // DISABLE_PYTHON
