/**
 * Delay trigger
 *
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "SCA_DelaySensor.h"
#include "SCA_LogicManager.h"
#include "SCA_EventManager.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_DelaySensor::SCA_DelaySensor(class SCA_EventManager* eventmgr,
								 SCA_IObject* gameobj,
								 int delay,
								 int duration,
								 bool repeat,
								 PyTypeObject* T)
	: SCA_ISensor(gameobj,eventmgr, T), 
	m_delay(delay),
	m_duration(duration),
	m_repeat(repeat)
{
	Init();
}

void SCA_DelaySensor::Init()
{
	m_lastResult = false;
	m_frameCount = -1;
	m_reset = true;
}

SCA_DelaySensor::~SCA_DelaySensor()
{
	/* intentionally empty */
}

CValue* SCA_DelaySensor::GetReplica()
{
	CValue* replica = new SCA_DelaySensor(*this);
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
}



bool SCA_DelaySensor::IsPositiveTrigger()
{ 
	return (m_invert ? !m_lastResult : m_lastResult);
}

bool SCA_DelaySensor::Evaluate(CValue* event)
{
	bool trigger = false;
	bool result;

	if (m_frameCount==-1) {
		// this is needed to ensure ON trigger in case delay==0
		// and avoid spurious OFF trigger when duration==0
		m_lastResult = false;
		m_frameCount = 0;
	}

	if (m_frameCount<m_delay) {
		m_frameCount++;
		result = false;
	} else if (m_duration > 0) {
		if (m_frameCount < m_delay+m_duration) {
			m_frameCount++;
			result = true;
		} else {
			result = false;
			if (m_repeat)
				m_frameCount = -1;
		}
	} else {
		result = true;
		if (m_repeat)
			m_frameCount = -1;
	}
	if ((m_reset && m_level) || result != m_lastResult)
		trigger = true;
	m_reset = false;
	m_lastResult = result;
	return trigger;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_DelaySensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"SCA_DelaySensor",
	sizeof(SCA_DelaySensor),
	0,
	PyDestructor,
	0,
	__getattr,
	__setattr,
	0, //&MyPyCompare,
	__repr,
	0, //&cvalue_as_number,
	0,
	0,
	0,
	0
};

PyParentObject SCA_DelaySensor::Parents[] = {
	&SCA_DelaySensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef SCA_DelaySensor::Methods[] = {
	/* setProperty */
	{"setDelay", (PyCFunction) SCA_DelaySensor::sPySetDelay, METH_VARARGS, SetDelay_doc},
	{"setDuration", (PyCFunction) SCA_DelaySensor::sPySetDuration, METH_VARARGS, SetDuration_doc},
	{"setRepeat", (PyCFunction) SCA_DelaySensor::sPySetRepeat, METH_VARARGS, SetRepeat_doc},
	/* getProperty */
	{"getDelay", (PyCFunction) SCA_DelaySensor::sPyGetDelay, METH_NOARGS, GetDelay_doc},
	{"getDuration", (PyCFunction) SCA_DelaySensor::sPyGetDuration, METH_NOARGS, GetDuration_doc},
	{"getRepeat", (PyCFunction) SCA_DelaySensor::sPyGetRepeat, METH_NOARGS, GetRepeat_doc},
	{NULL,NULL} //Sentinel
};

PyObject* SCA_DelaySensor::_getattr(const STR_String& attr) {
	_getattr_up(SCA_ISensor);
}

const char SCA_DelaySensor::SetDelay_doc[] = 
"setDelay(delay)\n"
"\t- delay: length of the initial OFF period as number of frame\n"
"\t         0 for immediate trigger\n"
"\tSet the initial delay before the positive trigger\n";
PyObject* SCA_DelaySensor::PySetDelay(PyObject* self, PyObject* args, PyObject* kwds)
{
	int delay;
	
	if(!PyArg_ParseTuple(args, "i", &delay)) {
		return NULL;
	}
	if (delay < 0) {
		PyErr_SetString(PyExc_ValueError, "Delay cannot be negative");
		return NULL;
	}
	m_delay = delay;
	Py_Return;
}

const char SCA_DelaySensor::SetDuration_doc[] = 
"setDuration(duration)\n"
"\t- duration: length of the ON period in number of frame after the initial off period\n"
"\t            0 for no ON period\n"
"\tSet the duration of the ON pulse after initial delay.\n"
"\tIf > 0, a negative trigger is fired at the end of the ON pulse.\n";
PyObject* SCA_DelaySensor::PySetDuration(PyObject* self, PyObject* args, PyObject* kwds)
{
	int duration;
	
	if(!PyArg_ParseTuple(args, "i", &duration)) {
		return NULL;
	}
	if (duration < 0) {
		PyErr_SetString(PyExc_ValueError, "Duration cannot be negative");
		return NULL;
	}
	m_duration = duration;
	Py_Return;
}

const char SCA_DelaySensor::SetRepeat_doc[] = 
"setRepeat(repeat)\n"
"\t- repeat: 1 if the initial OFF-ON cycle should be repeated indefinately\n"
"\t          0 if the initial OFF-ON cycle should run only once\n"
"\tSet the sensor repeat mode\n";
PyObject* SCA_DelaySensor::PySetRepeat(PyObject* self, PyObject* args, PyObject* kwds)
{
	int repeat;
	
	if(!PyArg_ParseTuple(args, "i", &repeat)) {
		return NULL;
	}
	m_repeat = (repeat != 0);
	Py_Return;
}

const char SCA_DelaySensor::GetDelay_doc[] = 
"getDelay()\n"
"\tReturn the delay parameter value\n";
PyObject* SCA_DelaySensor::PyGetDelay(PyObject* self)
{
	return PyInt_FromLong(m_delay);
}

const char SCA_DelaySensor::GetDuration_doc[] = 
"getDuration()\n"
"\tReturn the duration parameter value\n";
PyObject* SCA_DelaySensor::PyGetDuration(PyObject* self)
{
	return PyInt_FromLong(m_duration);
}

const char SCA_DelaySensor::GetRepeat_doc[] = 
"getRepeat()\n"
"\tReturn the repeat parameter value\n";
PyObject* SCA_DelaySensor::PyGetRepeat(PyObject* self)
{
	return BoolToPyArg(m_repeat);
}

/* eof */
