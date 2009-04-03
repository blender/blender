/**
 * Abstract class for sensor logic bricks
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

#include "SCA_ISensor.h"
#include "SCA_EventManager.h"
#include "SCA_LogicManager.h"
// needed for IsTriggered()
#include "SCA_PythonController.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Native functions */
void	SCA_ISensor::ReParent(SCA_IObject* parent)
{
	SCA_ILogicBrick::ReParent(parent);
	// will be done when the sensor is activated
	//m_eventmgr->RegisterSensor(this);
	this->SetActive(false);
}


SCA_ISensor::SCA_ISensor(SCA_IObject* gameobj,
						 class SCA_EventManager* eventmgr,
						 PyTypeObject* T ) :
	SCA_ILogicBrick(gameobj,T),
	m_triggered(false)
{
	m_links = 0;
	m_suspended = false;
	m_invert = false;
	m_level = false;
	m_reset = false;
	m_pos_ticks = 0;
	m_neg_ticks = 0;
	m_pos_pulsemode = false;
	m_neg_pulsemode = false;
	m_pulse_frequency = 0;
	
	m_eventmgr = eventmgr;
}


SCA_ISensor::~SCA_ISensor()  
{
	// intentionally empty
}

bool SCA_ISensor::IsPositiveTrigger() { 
	bool result = false;
	
	if (m_eventval) {
		result = (m_eventval->GetNumber() != 0.0);
	}
	if (m_invert) {
		result = !result;
	}
	
	return result;
}

void SCA_ISensor::SetPulseMode(bool posmode, 
							   bool negmode,
							   int freq) {
	m_pos_pulsemode = posmode;
	m_neg_pulsemode = negmode;
	m_pulse_frequency = freq;
}

void SCA_ISensor::SetInvert(bool inv) {
	m_invert = inv;
}

void SCA_ISensor::SetLevel(bool lvl) {
	m_level = lvl;
}


float SCA_ISensor::GetNumber() {
	return IsPositiveTrigger();
}

void SCA_ISensor::Suspend() {
	m_suspended = true;
}

bool SCA_ISensor::IsSuspended() {
	return m_suspended;
}

void SCA_ISensor::Resume() {
	m_suspended = false;
}

void SCA_ISensor::Init() {
	printf("Sensor %s has no init function, please report this bug to Blender.org\n", m_name.Ptr());
}

void SCA_ISensor::DecLink() {
	m_links--;
	if (m_links < 0) 
	{
		printf("Warning: sensor %s has negative m_links: %d\n", m_name.Ptr(), m_links);
		m_links = 0;
	}
	if (!m_links)
	{
		// sensor is detached from all controllers, remove it from manager
		UnregisterToManager();
	}
}

void SCA_ISensor::RegisterToManager()
{
	// sensor is just activated, initialize it
	Init();
	m_newControllers.erase(m_newControllers.begin(), m_newControllers.end());
	m_eventmgr->RegisterSensor(this);
}

void SCA_ISensor::UnregisterToManager()
{
	m_eventmgr->RemoveSensor(this);
}

void SCA_ISensor::Activate(class SCA_LogicManager* logicmgr,	  CValue* event)
{
	
	// calculate if a __triggering__ is wanted
	// don't evaluate a sensor that is not connected to any controller
	if (m_links && !m_suspended) {
		bool result = this->Evaluate(event);
		if (result) {
			logicmgr->AddActivatedSensor(this);	
			// reset these counters so that pulse are synchronized with transition
			m_pos_ticks = 0;
			m_neg_ticks = 0;
		} else
		{
			/* First, the pulsing behaviour, if pulse mode is
			 * active. It seems something goes wrong if pulse mode is
			 * not set :( */
			if (m_pos_pulsemode) {
				m_pos_ticks++;
				if (m_pos_ticks > m_pulse_frequency) {
					if ( this->IsPositiveTrigger() )
					{
						logicmgr->AddActivatedSensor(this);
					}
					m_pos_ticks = 0;
				} 
			}
			
			if (m_neg_pulsemode)
			{
				m_neg_ticks++;
				if (m_neg_ticks > m_pulse_frequency) {
					if (!this->IsPositiveTrigger() )
					{
						logicmgr->AddActivatedSensor(this);
					}
					m_neg_ticks = 0;
				}
			}
		}
		if (!m_newControllers.empty())
		{
			if (!IsActive() && m_level)
			{
				// This level sensor is connected to at least one controller that was just made 
				// active but it did not generate an event yet, do it now to those controllers only 
				for (std::vector<SCA_IController*>::iterator ci=m_newControllers.begin();
					 ci != m_newControllers.end(); ci++)
				{
					logicmgr->AddTriggeredController(*ci, this);
				}
			}
			// clear the list. Instead of using clear, which also release the memory,
			// use erase, which keeps the memory available for next time.
			m_newControllers.erase(m_newControllers.begin(), m_newControllers.end());
		}
	} 
}
/* ----------------------------------------------- */
/* Python Functions						           */
/* ----------------------------------------------- */

//Deprecated Functions ------>
const char SCA_ISensor::IsPositive_doc[] = 
"isPositive()\n"
"\tReturns whether the sensor is in an active state.\n";
PyObject* SCA_ISensor::PyIsPositive(PyObject* self)
{
	ShowDeprecationWarning("isPositive()", "the read-only positive property");
	int retval = IsPositiveTrigger();
	return PyInt_FromLong(retval);
}

const char SCA_ISensor::IsTriggered_doc[] = 
"isTriggered()\n"
"\tReturns whether the sensor has triggered the current controller.\n";
PyObject* SCA_ISensor::PyIsTriggered(PyObject* self)
{
	ShowDeprecationWarning("isTriggered()", "the read-only triggered property");
	// check with the current controller
	int retval = 0;
	if (SCA_PythonController::m_sCurrentController)
		retval = SCA_PythonController::m_sCurrentController->IsTriggered(this);
	return PyInt_FromLong(retval);
}

/**
 * getUsePulseMode: getter for the pulse mode (KX_TRUE = on)
 */
const char SCA_ISensor::GetUsePosPulseMode_doc[] = 
"getUsePosPulseMode()\n"
"\tReturns whether positive pulse mode is active.\n";
PyObject* SCA_ISensor::PyGetUsePosPulseMode(PyObject* self)
{
	ShowDeprecationWarning("getUsePosPulseMode()", "the usePosPulseMode property");
	return BoolToPyArg(m_pos_pulsemode);
}

/**
 * setUsePulseMode: setter for the pulse mode (KX_TRUE = on)
 */
const char SCA_ISensor::SetUsePosPulseMode_doc[] = 
"setUsePosPulseMode(pulse?)\n"
"\t - pulse? : Pulse when a positive event occurs?\n"
"\t            (KX_TRUE, KX_FALSE)\n"
"\tSet whether to do pulsing when positive pulses occur.\n";
PyObject* SCA_ISensor::PySetUsePosPulseMode(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("setUsePosPulseMode()", "the usePosPulseMode property");
	int pyarg = 0;
	if(!PyArg_ParseTuple(args, "i", &pyarg)) { return NULL; }
	m_pos_pulsemode = PyArgToBool(pyarg);
	Py_RETURN_NONE;
}

/**
 * getFrequency: getter for the pulse mode interval
 */
const char SCA_ISensor::GetFrequency_doc[] = 
"getFrequency()\n"
"\tReturns the frequency of the updates in pulse mode.\n" ;
PyObject* SCA_ISensor::PyGetFrequency(PyObject* self)
{
	ShowDeprecationWarning("getFrequency()", "the frequency property");
	return PyInt_FromLong(m_pulse_frequency);
}

/**
 * setFrequency: setter for the pulse mode (KX_TRUE = on)
 */
const char SCA_ISensor::SetFrequency_doc[] = 
"setFrequency(pulse_frequency)\n"
"\t- pulse_frequency: The frequency of the updates in pulse mode (integer)"
"\tSet the frequency of the updates in pulse mode.\n"
"\tIf the frequency is negative, it is set to 0.\n" ;
PyObject* SCA_ISensor::PySetFrequency(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("setFrequency()", "the frequency property");
	int pulse_frequencyArg = 0;

	if(!PyArg_ParseTuple(args, "i", &pulse_frequencyArg)) {
		return NULL;
	}
	
	/* We can do three things here: clip, ignore and raise an exception.  */
	/* Exceptions don't work yet, ignoring is not desirable now...        */
	if (pulse_frequencyArg < 0) {
		pulse_frequencyArg = 0;
	};	
	m_pulse_frequency = pulse_frequencyArg;

	Py_RETURN_NONE;
}


const char SCA_ISensor::GetInvert_doc[] = 
"getInvert()\n"
"\tReturns whether or not pulses from this sensor are inverted.\n" ;
PyObject* SCA_ISensor::PyGetInvert(PyObject* self)
{
	ShowDeprecationWarning("getInvert()", "the invert property");
	return BoolToPyArg(m_invert);
}

const char SCA_ISensor::SetInvert_doc[] = 
"setInvert(invert?)\n"
"\t- invert?: Invert the event-values? (KX_TRUE, KX_FALSE)\n"
"\tSet whether to invert pulses.\n";
PyObject* SCA_ISensor::PySetInvert(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("setInvert()", "the invert property");
	int pyarg = 0;
	if(!PyArg_ParseTuple(args, "i", &pyarg)) { return NULL; }
	m_invert = PyArgToBool(pyarg);
	Py_RETURN_NONE;
}

const char SCA_ISensor::GetLevel_doc[] = 
"getLevel()\n"
"\tReturns whether this sensor is a level detector or a edge detector.\n"
"\tIt makes a difference only in case of logic state transition (state actuator).\n"
"\tA level detector will immediately generate a pulse, negative or positive\n"
"\tdepending on the sensor condition, as soon as the state is activated.\n"
"\tA edge detector will wait for a state change before generating a pulse.\n";
PyObject* SCA_ISensor::PyGetLevel(PyObject* self)
{
	ShowDeprecationWarning("getLevel()", "the level property");
	return BoolToPyArg(m_level);
}

const char SCA_ISensor::SetLevel_doc[] = 
"setLevel(level?)\n"
"\t- level?: Detect level instead of edge? (KX_TRUE, KX_FALSE)\n"
"\tSet whether to detect level or edge transition when entering a state.\n";
PyObject* SCA_ISensor::PySetLevel(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("setLevel()", "the level property");
	int pyarg = 0;
	if(!PyArg_ParseTuple(args, "i", &pyarg)) { return NULL; }
	m_level = PyArgToBool(pyarg);
	Py_RETURN_NONE;
}

const char SCA_ISensor::GetUseNegPulseMode_doc[] = 
"getUseNegPulseMode()\n"
"\tReturns whether negative pulse mode is active.\n";
PyObject* SCA_ISensor::PyGetUseNegPulseMode(PyObject* self)
{
	ShowDeprecationWarning("getUseNegPulseMode()", "the useNegPulseMode property");
	return BoolToPyArg(m_neg_pulsemode);
}

const char SCA_ISensor::SetUseNegPulseMode_doc[] = 
"setUseNegPulseMode(pulse?)\n"
"\t - pulse? : Pulse when a negative event occurs?\n"
"\t            (KX_TRUE, KX_FALSE)\n"
"\tSet whether to do pulsing when negative pulses occur.\n";
PyObject* SCA_ISensor::PySetUseNegPulseMode(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("setUseNegPulseMode()", "the useNegPulseMode property");
	int pyarg = 0;
	if(!PyArg_ParseTuple(args, "i", &pyarg)) { return NULL; }
	m_neg_pulsemode = PyArgToBool(pyarg);
	Py_RETURN_NONE;
}
//<------Deprecated

KX_PYMETHODDEF_DOC_NOARGS(SCA_ISensor, reset,
"reset()\n"
"\tReset sensor internal state, effect depends on the type of sensor and settings.\n"
"\tThe sensor is put in its initial state as if it was just activated.\n")
{
	Init();
	Py_RETURN_NONE;
}

/* ----------------------------------------------- */
/* Python Integration Hooks					       */
/* ----------------------------------------------- */

PyTypeObject SCA_ISensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"SCA_ISensor",
	sizeof(SCA_ISensor),
	0,
	PyDestructor,
	0,
	__getattr,
	__setattr,
	0,
	__repr,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	Methods
};

PyParentObject SCA_ISensor::Parents[] = {
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};
PyMethodDef SCA_ISensor::Methods[] = {
	//Deprecated functions ----->
	{"isPositive", (PyCFunction) SCA_ISensor::sPyIsPositive, 
	 METH_NOARGS, (PY_METHODCHAR)IsPositive_doc},
	{"isTriggered", (PyCFunction) SCA_ISensor::sPyIsTriggered, 
	 METH_VARARGS, (PY_METHODCHAR)IsTriggered_doc},
	{"getUsePosPulseMode", (PyCFunction) SCA_ISensor::sPyGetUsePosPulseMode, 
	 METH_NOARGS, (PY_METHODCHAR)GetUsePosPulseMode_doc},
	{"setUsePosPulseMode", (PyCFunction) SCA_ISensor::sPySetUsePosPulseMode, 
	 METH_VARARGS, (PY_METHODCHAR)SetUsePosPulseMode_doc},
	{"getFrequency", (PyCFunction) SCA_ISensor::sPyGetFrequency, 
	 METH_NOARGS, (PY_METHODCHAR)GetFrequency_doc},
	{"setFrequency", (PyCFunction) SCA_ISensor::sPySetFrequency, 
	 METH_VARARGS, (PY_METHODCHAR)SetFrequency_doc},
	{"getUseNegPulseMode", (PyCFunction) SCA_ISensor::sPyGetUseNegPulseMode, 
	 METH_NOARGS, (PY_METHODCHAR)GetUseNegPulseMode_doc},
	{"setUseNegPulseMode", (PyCFunction) SCA_ISensor::sPySetUseNegPulseMode, 
	 METH_VARARGS, (PY_METHODCHAR)SetUseNegPulseMode_doc},
	{"getInvert", (PyCFunction) SCA_ISensor::sPyGetInvert, 
	 METH_NOARGS, (PY_METHODCHAR)GetInvert_doc},
	{"setInvert", (PyCFunction) SCA_ISensor::sPySetInvert, 
	 METH_VARARGS, (PY_METHODCHAR)SetInvert_doc},
	{"getLevel", (PyCFunction) SCA_ISensor::sPyGetLevel, 
	 METH_NOARGS, (PY_METHODCHAR)GetLevel_doc},
	{"setLevel", (PyCFunction) SCA_ISensor::sPySetLevel, 
	 METH_VARARGS, (PY_METHODCHAR)SetLevel_doc},
	 //<----- Deprecated
	KX_PYMETHODTABLE_NOARGS(SCA_ISensor, reset),
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_ISensor::Attributes[] = {
	KX_PYATTRIBUTE_BOOL_RW("usePosPulseMode",SCA_ISensor,m_pos_pulsemode),
	KX_PYATTRIBUTE_BOOL_RW("useNegPulseMode",SCA_ISensor,m_neg_pulsemode),
	KX_PYATTRIBUTE_INT_RW("frequency",0,100000,true,SCA_ISensor,m_pulse_frequency),
	KX_PYATTRIBUTE_BOOL_RW("invert",SCA_ISensor,m_invert),
	KX_PYATTRIBUTE_BOOL_RW("level",SCA_ISensor,m_level),
	// make these properties read-only in _setaddr, must still implement them in _getattr
	KX_PYATTRIBUTE_DUMMY("triggered"),
	KX_PYATTRIBUTE_DUMMY("positive"),
	{ NULL }	//Sentinel
};

PyObject*
SCA_ISensor::_getattr(const char *attr)
{
	PyObject* object = _getattr_self(Attributes, this, attr);
	if (object != NULL)
		return object;
	if (!strcmp(attr, "triggered"))
	{
		int retval = 0;
		if (SCA_PythonController::m_sCurrentController)
			retval = SCA_PythonController::m_sCurrentController->IsTriggered(this);
		return PyInt_FromLong(retval);
	}
	if (!strcmp(attr, "positive"))
	{	
		int retval = IsPositiveTrigger();
		return PyInt_FromLong(retval);
	}
	_getattr_up(SCA_ILogicBrick);
}

int SCA_ISensor::_setattr(const char *attr, PyObject *value)
{
	int ret = _setattr_self(Attributes, this, attr, value);
	if (ret >= 0)
		return ret;
	return SCA_ILogicBrick::_setattr(attr, value);
}
/* eof */
