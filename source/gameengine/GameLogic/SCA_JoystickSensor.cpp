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
#include "SCA_JoystickManager.h"
#include "SCA_JoystickSensor.h"

#include "SCA_EventManager.h"
#include "SCA_LogicManager.h"

#include "PyObjectPlus.h"

#include <iostream>


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


SCA_JoystickSensor::SCA_JoystickSensor(class SCA_JoystickManager* eventmgr,
									   SCA_IObject* gameobj,
									   short int joyindex,
									   short int joymode,
									   int axis, int axisf,int prec,
									   int button, int buttonf,
									   int hat, int hatf,
									   PyTypeObject* T )
									   :SCA_ISensor(gameobj,eventmgr,T),
									   m_pJoystickMgr(eventmgr),
									   m_axis(axis),
									   m_axisf(axisf),
									   m_button(button),
									   m_buttonf(buttonf),
									   m_hat(hat),
									   m_hatf(hatf),
									   m_precision(prec),
									   m_joymode(joymode),
									   m_joyindex(joyindex)
{	
/*
std::cout << " axis "		<< m_axis		<< std::endl;
std::cout << " axis flag "	<< m_axisf		<< std::endl;
std::cout << " precision "	<< m_precision	<< std::endl;
std::cout << " button " 	<< m_button 	<< std::endl;
std::cout << " button flag "<< m_buttonf	<< std::endl;
std::cout << " hat "		<< m_hat		<< std::endl;
std::cout << " hat flag "	<< m_hatf		<< std::endl;
*/
	Init();
}

void SCA_JoystickSensor::Init()
{
	m_istrig=(m_invert)?1:0;
	m_reset = true;
}

SCA_JoystickSensor::~SCA_JoystickSensor()
{
}


CValue* SCA_JoystickSensor::GetReplica()
{
	SCA_JoystickSensor* replica = new SCA_JoystickSensor(*this);
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	replica->Init();
	return replica;
}


bool SCA_JoystickSensor::IsPositiveTrigger()
{ 
	bool result =	m_istrig;
	if (m_invert)
		result = !result;
	return result;
}


bool SCA_JoystickSensor::Evaluate(CValue* event)
{
	SCA_Joystick *js = m_pJoystickMgr->GetJoystickDevice(m_joyindex);
	bool result = false;
	bool reset = m_reset && m_level;
	
	if(js==NULL)
		return false;
	
	m_reset = false;
	switch(m_joymode)
	{
	case KX_JOYSENSORMODE_AXIS:
		{
		/* what is what!
			m_axisf == 0 == right
			m_axisf == 1 == up
			m_axisf == 2 == left
			m_axisf == 3 == down
			numberof== m_axis  -- max 2
			*/
			js->cSetPrecision(m_precision);
			if(m_axisf == 1){
				if(js->aUpAxisIsPositive(m_axis)){
					m_istrig = 1;
					result = true;
				}else{
					if(m_istrig){
						m_istrig = 0;
						result = true;
					}
				}
			}
			if(m_axisf == 3){
				if(js->aDownAxisIsPositive(m_axis)){
					m_istrig = 1;
					result = true;
				}else{
					if(m_istrig){
						m_istrig = 0;
						result = true;
					}
				}
			}
			if(m_axisf == 2){
				if(js->aLeftAxisIsPositive(m_axis)){
					m_istrig = 1;
					result = true;
				}else{
					if(m_istrig){
						m_istrig = 0;
						result = true;
					}
				}
			}
			if(m_axisf == 0){
				if(js->aRightAxisIsPositive(m_axis)){
					m_istrig = 1;
					result = true;
				}else{
					if(m_istrig){
						m_istrig = 0;
						result = true;
					}
				}
			}
			break;
		}
	case KX_JOYSENSORMODE_BUTTON:
		{
		/* what is what!
			pressed  = m_buttonf == 0
			released = m_buttonf == 1
			m_button = the actual button in question
			*/
			if(m_buttonf == 0){
				if(js->aButtonPressIsPositive(m_button)){
					m_istrig = 1;
					result = true;
				}else {
					if(m_istrig){
						m_istrig = 0;
						result = true;
					}
				}
			}
			if(m_buttonf == 1){
				if(js->aButtonReleaseIsPositive(m_button)){
					m_istrig = 1;
					result = true;
				}else {
					if(m_istrig){
						m_istrig = 0;
						result = true;
					}
				}			
			}
			break;
		}
	case KX_JOYSENSORMODE_HAT:
		{
		/* what is what!
			numberof = m_hat  -- max 2
			direction= m_hatf -- max 12
			*/
			if(m_hat == 1){
				if(js->aHatIsPositive(m_hatf)){
					m_istrig = 1;
					result = true;
				}else{
					if(m_istrig){
						m_istrig = 0;
						result = true;
					}
				}
			}
			if(m_hat == 2){
				if(js->aHatIsPositive(m_hatf)){
					m_istrig = 1;
					result = true;
				}else{
					if(m_istrig){
						m_istrig = 0;
						result = true;
					}
				}
			}
			/*
			if(m_hat == 3){
				if(js->aHatIsPositive(m_hatf)){
					m_istrig = 1;
					result = true;
				}else{
					if(m_istrig){
						m_istrig = 0;
						result = true;
					}
				}
			}
			*/
			break;
		}
		/* test for ball anyone ?*/
	default:
		printf("Error invalid switch statement\n");
		break;
	}
	
	if (js->IsTrig()) {
		/* The if below detects changes with the joystick trigger state.
		 * js->IsTrig() will stay true as long as the key is held.
		 * even though the event from SDL will only be sent once.
		 * (js->IsTrig() && m_istrig_lastjs) - when true it means this sensor
		 * had the same joystick trigger state last time,
		 * Setting the result false this time means it wont run the sensors
		 * controller every time (like a pulse sensor)
		 *
		 * This is not done with the joystick its self incase other sensors use
		 * it or become active.
		 */
		if (m_istrig_lastjs) {
			result = false;
		}
		m_istrig_lastjs = true;
	} else {
		m_istrig = 0;
		m_istrig_lastjs = false;
	}
	
	if (reset)
		result = true;
	
	return result;
}


bool SCA_JoystickSensor::isValid(SCA_JoystickSensor::KX_JOYSENSORMODE m)
{
	bool res = false;
	res = ((m > KX_JOYSENSORMODE_NODEF) && (m < KX_JOYSENSORMODE_MAX));
	return res;
}


/* ------------------------------------------------------------------------- */
/* Python functions 														 */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_JoystickSensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"SCA_JoystickSensor",
		sizeof(SCA_JoystickSensor),
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


PyParentObject SCA_JoystickSensor::Parents[] = {
		&SCA_JoystickSensor::Type,
		&SCA_ISensor::Type,
		&SCA_ILogicBrick::Type,
		&CValue::Type,
		NULL
};


PyMethodDef SCA_JoystickSensor::Methods[] = {
	{"getIndex", 	 (PyCFunction) SCA_JoystickSensor::sPyGetIndex,		METH_NOARGS,	GetIndex_doc},
	{"setIndex",	 (PyCFunction) SCA_JoystickSensor::sPySetIndex,		METH_O,			SetIndex_doc},
	{"getAxis", 	 (PyCFunction) SCA_JoystickSensor::sPyGetAxis,		METH_NOARGS,	GetAxis_doc},
	{"setAxis", 	 (PyCFunction) SCA_JoystickSensor::sPySetAxis,		METH_VARARGS,	SetAxis_doc},
	{"getAxisValue", (PyCFunction) SCA_JoystickSensor::sPyGetRealAxis,	METH_NOARGS,	GetRealAxis_doc},
	{"getThreshold", (PyCFunction) SCA_JoystickSensor::sPyGetThreshold, METH_NOARGS,	GetThreshold_doc},
	{"setThreshold", (PyCFunction) SCA_JoystickSensor::sPySetThreshold, METH_VARARGS,	SetThreshold_doc},
	{"getButton",	 (PyCFunction) SCA_JoystickSensor::sPyGetButton,	METH_NOARGS,	GetButton_doc},
	{"setButton",	 (PyCFunction) SCA_JoystickSensor::sPySetButton,	METH_VARARGS,	SetButton_doc},
	{"getHat",		 (PyCFunction) SCA_JoystickSensor::sPyGetHat,		METH_NOARGS,	GetHat_doc},
	{"setHat",		 (PyCFunction) SCA_JoystickSensor::sPySetHat,		METH_VARARGS,	SetHat_doc},
	{"getNumAxes",	 (PyCFunction) SCA_JoystickSensor::sPyNumberOfAxes,	METH_NOARGS,	NumberOfAxes_doc},
	{"getNumButtons",(PyCFunction) SCA_JoystickSensor::sPyNumberOfButtons,METH_NOARGS,	NumberOfButtons_doc},
	{"getNumHats",	 (PyCFunction) SCA_JoystickSensor::sPyNumberOfHats,	METH_NOARGS,	NumberOfHats_doc},
	{"isConnected", (PyCFunction) SCA_JoystickSensor::sPyConnected,		METH_NOARGS,	Connected_doc},
	{NULL,NULL} //Sentinel
};


PyObject* SCA_JoystickSensor::_getattr(const STR_String& attr) {
	_getattr_up(SCA_ISensor);
}


/* get index ---------------------------------------------------------- */
const char SCA_JoystickSensor::GetIndex_doc[] = 
"getIndex\n"
"\tReturns the joystick index to use.\n";
PyObject* SCA_JoystickSensor::PyGetIndex( PyObject* self ) {
	return PyInt_FromLong(m_joyindex);
}


/* set index ---------------------------------------------------------- */
const char SCA_JoystickSensor::SetIndex_doc[] = 
"setIndex\n"
"\tSets the joystick index to use.\n";
PyObject* SCA_JoystickSensor::PySetIndex( PyObject* self, PyObject* value ) {
	int index = PyInt_AsLong( value ); /* -1 on error, will raise an error in this case */
	if (index < 0 || index >= JOYINDEX_MAX) {
		PyErr_SetString(PyExc_ValueError, "joystick index out of range or not an int");
		return NULL;
	}
	
	m_joyindex = index;
	Py_RETURN_NONE;
}

/* get axis  ---------------------------------------------------------- */
const char SCA_JoystickSensor::GetAxis_doc[] = 
"getAxis\n"
"\tReturns the current state of the axis.\n";
PyObject* SCA_JoystickSensor::PyGetAxis( PyObject* self) {
	return PyInt_FromLong(m_joyindex);
}


/* set axis  ---------------------------------------------------------- */
const char SCA_JoystickSensor::SetAxis_doc[] = 
"setAxis\n"
"\tSets the current state of the axis.\n";
PyObject* SCA_JoystickSensor::PySetAxis( PyObject* self, PyObject* args ) {
	
	int axis,axisflag;
	if(!PyArg_ParseTuple(args, "ii", &axis, &axisflag)){
		return NULL;
	}
	m_axis = axis;
	m_axisf = axisflag;
	Py_RETURN_NONE;
}


/* get axis value ----------------------------------------------------- */
const char SCA_JoystickSensor::GetRealAxis_doc[] = 
"getAxisValue\n"
"\tReturns a list of the values for each axis .\n";
PyObject* SCA_JoystickSensor::PyGetRealAxis( PyObject* self) {
	SCA_Joystick *joy = m_pJoystickMgr->GetJoystickDevice(m_joyindex);
	if(joy)
		return Py_BuildValue("[iiii]", joy->GetAxis10(), joy->GetAxis11(), joy->GetAxis20(), joy->GetAxis21());
	else
		return Py_BuildValue("[iiii]", 0, 0, 0, 0);
}


/* get threshold  ----------------------------------------------------- */
const char SCA_JoystickSensor::GetThreshold_doc[] = 
"getThreshold\n"
"\tReturns the threshold of the axis.\n";
PyObject* SCA_JoystickSensor::PyGetThreshold( PyObject* self) {
	return PyInt_FromLong(m_precision);
}


/* set threshold  ----------------------------------------------------- */
const char SCA_JoystickSensor::SetThreshold_doc[] = 
"setThreshold\n"
"\tSets the threshold of the axis.\n";
PyObject* SCA_JoystickSensor::PySetThreshold( PyObject* self, PyObject* args ) {
	int thresh;
	if(!PyArg_ParseTuple(args, "i", &thresh)){
		return NULL;
	}
	m_precision = thresh;
	Py_RETURN_NONE;
}


/* get button  -------------------------------------------------------- */
const char SCA_JoystickSensor::GetButton_doc[] = 
"getButton\n"
"\tReturns the currently pressed button.\n";
PyObject* SCA_JoystickSensor::PyGetButton( PyObject* self) {
	return Py_BuildValue("[ii]",m_button, m_buttonf);
}


/* set button  -------------------------------------------------------- */
const char SCA_JoystickSensor::SetButton_doc[] = 
"setButton\n"
"\tSets the button the sensor reacts to.\n";
PyObject* SCA_JoystickSensor::PySetButton( PyObject* self, PyObject* args ) {
	int button,buttonflag;
	if(!PyArg_ParseTuple(args, "ii", &button, &buttonflag)){
		return NULL;
	}
	m_button = button;
	m_buttonf = buttonflag;
	Py_RETURN_NONE;	
}


/* get hat	----------------------------------------------------------- */
const char SCA_JoystickSensor::GetHat_doc[] = 
"getHat\n"
"\tReturns the current direction of the hat.\n";
PyObject* SCA_JoystickSensor::PyGetHat( PyObject* self ) {
	return Py_BuildValue("[ii]",m_hat, m_hatf);
}


/* set hat	----------------------------------------------------------- */
const char SCA_JoystickSensor::SetHat_doc[] = 
"setHat\n"
"\tSets the hat the sensor reacts to.\n";
PyObject* SCA_JoystickSensor::PySetHat( PyObject* self, PyObject* args ) {
	int hat,hatflag;
	if(!PyArg_ParseTuple(args, "ii", &hat, &hatflag)){
		return NULL;
	}
	m_hat = hat;
	m_hatf = hatflag;
	Py_RETURN_NONE;
}


/* get # of ----------------------------------------------------- */
const char SCA_JoystickSensor::NumberOfAxes_doc[] = 
"getNumAxes\n"
"\tReturns the number of axes .\n";
PyObject* SCA_JoystickSensor::PyNumberOfAxes( PyObject* self ) {
	SCA_Joystick *joy = m_pJoystickMgr->GetJoystickDevice(m_joyindex);
	// when the joystick is null their is 0 exis still. dumb but scripters should use isConnected()
	return PyInt_FromLong( joy ? joy->GetNumberOfAxes() : 0 );
}


const char SCA_JoystickSensor::NumberOfButtons_doc[] = 
"getNumButtons\n"
"\tReturns the number of buttons .\n";
PyObject* SCA_JoystickSensor::PyNumberOfButtons( PyObject* self ) {
	SCA_Joystick *joy = m_pJoystickMgr->GetJoystickDevice(m_joyindex);
	return PyInt_FromLong( joy ? joy->GetNumberOfButtons() : 0 );
}


const char SCA_JoystickSensor::NumberOfHats_doc[] = 
"getNumHats\n"
"\tReturns the number of hats .\n";
PyObject* SCA_JoystickSensor::PyNumberOfHats( PyObject* self ) {
	SCA_Joystick *joy = m_pJoystickMgr->GetJoystickDevice(m_joyindex);
	return PyInt_FromLong( joy ? joy->GetNumberOfHats() : 0 );
}

const char SCA_JoystickSensor::Connected_doc[] = 
"getConnected\n"
"\tReturns True if a joystick is connected at this joysticks index.\n";
PyObject* SCA_JoystickSensor::PyConnected( PyObject* self ) {
	SCA_Joystick *joy = m_pJoystickMgr->GetJoystickDevice(m_joyindex);
	return PyBool_FromLong( joy ? joy->Connected() : 0 );
}
