/*
 * Sensor for mouse input
 *
 *
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
 * Contributor(s): José I. Romero (cleanup and fixes)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/GameLogic/SCA_MouseSensor.cpp
 *  \ingroup gamelogic
 */


#include <stddef.h>

#include "SCA_MouseSensor.h"
#include "SCA_EventManager.h"
#include "SCA_MouseManager.h"
#include "SCA_LogicManager.h"
#include "SCA_IInputDevice.h"
#include "ConstExpr.h"
#include <iostream>

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_MouseSensor::SCA_MouseSensor(SCA_MouseManager* eventmgr, 
                                 int startx,int starty,
                                 short int mousemode,
                                 SCA_IObject* gameobj)
    : SCA_ISensor(gameobj,eventmgr),
      m_x(startx),
      m_y(starty)
{
	m_mousemode   = mousemode;
	m_triggermode = true;

	UpdateHotkey(this);
	Init();
}

void SCA_MouseSensor::Init()
{
	m_val = (m_invert)?1:0; /* stores the latest attribute */
	m_reset = true;
}

SCA_MouseSensor::~SCA_MouseSensor() 
{
	/* Nothing to be done here. */
}

void SCA_MouseSensor::UpdateHotkey(void *self)
{
	// gosh, this function is so damn stupid
	// its here because of a design mistake in the mouse sensor, it should only
	// have 3 trigger modes (button, wheel, move), and let the user set the 
	// hotkey separately, like the other sensors. but instead it has a mode for 
	// each friggin key and i have to update the hotkey based on it... genius!
	SCA_MouseSensor* sensor = reinterpret_cast<SCA_MouseSensor*>(self);

	switch (sensor->m_mousemode) {
	case KX_MOUSESENSORMODE_LEFTBUTTON:
		sensor->m_hotkey = SCA_IInputDevice::KX_LEFTMOUSE;
		break;
	case KX_MOUSESENSORMODE_MIDDLEBUTTON:
		sensor->m_hotkey = SCA_IInputDevice::KX_MIDDLEMOUSE;
		break;
	case KX_MOUSESENSORMODE_RIGHTBUTTON:
		sensor->m_hotkey = SCA_IInputDevice::KX_RIGHTMOUSE;
		break;
	case KX_MOUSESENSORMODE_WHEELUP:
		sensor->m_hotkey = SCA_IInputDevice::KX_WHEELUPMOUSE;
		break;
	case KX_MOUSESENSORMODE_WHEELDOWN:
		sensor->m_hotkey = SCA_IInputDevice::KX_WHEELDOWNMOUSE;
		break;
	default:
		; /* ignore, no hotkey */
	}
}

CValue* SCA_MouseSensor::GetReplica()
{
	SCA_MouseSensor* replica = new SCA_MouseSensor(*this);
	// this will copy properties and so on...
	replica->ProcessReplica();
	replica->Init();

	return replica;
}



bool SCA_MouseSensor::IsPositiveTrigger()
{
	bool result = (m_val != 0);
	if (m_invert)
		result = !result;
		
	return result;
}



short int SCA_MouseSensor::GetModeKey()
{ 
	return m_mousemode;
}



SCA_IInputDevice::KX_EnumInputs SCA_MouseSensor::GetHotKey()
{ 
	return m_hotkey;
}



bool SCA_MouseSensor::Evaluate()
{
	bool result = false;
	bool reset = m_reset && m_level;
	SCA_IInputDevice* mousedev = ((SCA_MouseManager *)m_eventmgr)->GetInputDevice();

	m_reset = false;
	switch (m_mousemode) {
	case KX_MOUSESENSORMODE_LEFTBUTTON:
	case KX_MOUSESENSORMODE_MIDDLEBUTTON:
	case KX_MOUSESENSORMODE_RIGHTBUTTON:
	case KX_MOUSESENSORMODE_WHEELUP:
	case KX_MOUSESENSORMODE_WHEELDOWN:
		{
			const SCA_InputEvent& mevent = mousedev->GetEventValue(m_hotkey);
			switch (mevent.m_status) {	
			case SCA_InputEvent::KX_JUSTACTIVATED:
				m_val = 1;
				result = true;
				break;
			case SCA_InputEvent::KX_JUSTRELEASED:
				m_val = 0;
				result = true;
				break;
			case SCA_InputEvent::KX_ACTIVE:
				if (m_val == 0)
				{
					m_val = 1;
					if (m_level)
						result = true;
				}
				break;
			default:
				if (m_val == 1)
				{
					m_val = 0;
					result = true;
				}
				break;
			}
			break;
		}
	case KX_MOUSESENSORMODE_MOVEMENT:
		{
			const SCA_InputEvent& eventX = mousedev->GetEventValue(SCA_IInputDevice::KX_MOUSEX);
			const SCA_InputEvent& eventY = mousedev->GetEventValue(SCA_IInputDevice::KX_MOUSEY);

			if (eventX.m_status == SCA_InputEvent::KX_JUSTACTIVATED ||
				eventY.m_status == SCA_InputEvent::KX_JUSTACTIVATED ||
				eventX.m_status == SCA_InputEvent::KX_ACTIVE ||
				eventY.m_status == SCA_InputEvent::KX_ACTIVE)	
			{
				m_val = 1;
				result = true;
			} 
			else if (eventX.m_status == SCA_InputEvent::KX_JUSTRELEASED ||
					eventY.m_status == SCA_InputEvent::KX_JUSTRELEASED )
			{
				m_val = 0;
				result = true;
			} 
			else //KX_NO_IMPUTSTATUS
			{ 
				if (m_val == 1)
				{
					m_val = 0;
					result = true;
				}
			}
			
			break;
		}
	default:
		; /* error */
	}

	if (reset)
		// force an event
		result = true;
	return result;
}

void SCA_MouseSensor::setX(short x)
{
	m_x = x;
}

void SCA_MouseSensor::setY(short y)
{
	m_y = y;
}

bool SCA_MouseSensor::isValid(SCA_MouseSensor::KX_MOUSESENSORMODE m)
{
	return ((m > KX_MOUSESENSORMODE_NODEF) && (m < KX_MOUSESENSORMODE_MAX));
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

KX_PYMETHODDEF_DOC_O(SCA_MouseSensor, getButtonStatus,
"getButtonStatus(button)\n"
"\tGet the given button's status (KX_INPUT_NONE, KX_INPUT_NONE, KX_INPUT_JUST_ACTIVATED, KX_INPUT_ACTIVE, KX_INPUT_JUST_RELEASED).\n")
{
	if (PyLong_Check(value))
	{
		int button = PyLong_AsSsize_t(value);
		
		if ((button < SCA_IInputDevice::KX_LEFTMOUSE)
			|| (button > SCA_IInputDevice::KX_RIGHTMOUSE)) {
			PyErr_SetString(PyExc_ValueError, "sensor.getButtonStatus(int): Mouse Sensor, invalid button specified!");
			return NULL;
		}
		
		SCA_IInputDevice* mousedev = ((SCA_MouseManager *)m_eventmgr)->GetInputDevice();
		const SCA_InputEvent& event = mousedev->GetEventValue((SCA_IInputDevice::KX_EnumInputs) button);
		return PyLong_FromSsize_t(event.m_status);
	}
	
	Py_RETURN_NONE;
}

/* ------------------------------------------------------------------------- */
/* Python Integration Hooks                                                  */
/* ------------------------------------------------------------------------- */

PyTypeObject SCA_MouseSensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_MouseSensor",
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
	&SCA_ISensor::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_MouseSensor::Methods[] = {
	KX_PYMETHODTABLE_O(SCA_MouseSensor, getButtonStatus),
	{NULL,NULL} //Sentinel
};

int SCA_MouseSensor::UpdateHotkeyPy(void *self, const PyAttributeDef*)
{
	UpdateHotkey(self);
	// return value is used in py_setattro(),
	// 0=attribute checked ok (see Attributes array definition)
	return 0;
}

PyAttributeDef SCA_MouseSensor::Attributes[] = {
	KX_PYATTRIBUTE_SHORT_RW_CHECK("mode",KX_MOUSESENSORMODE_NODEF,KX_MOUSESENSORMODE_MAX-1,true,SCA_MouseSensor,m_mousemode,UpdateHotkeyPy),
	KX_PYATTRIBUTE_SHORT_LIST_RO("position",SCA_MouseSensor,m_x,2),
	{ NULL }	//Sentinel
};

#endif // WITH_PYTHON

/* eof */
