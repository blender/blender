/**
 * Sensor for mouse input
 *
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

#include "SCA_MouseSensor.h"
#include "SCA_EventManager.h"
#include "SCA_MouseManager.h"
#include "SCA_LogicManager.h"
#include "SCA_IInputDevice.h"
#include "ConstExpr.h"
#include <iostream>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_MouseSensor::SCA_MouseSensor(SCA_MouseManager* eventmgr, 
								 int startx,int starty,
								 short int mousemode,
								 SCA_IObject* gameobj, 
								 PyTypeObject* T)
    : SCA_ISensor(gameobj,eventmgr, T),
	m_pMouseMgr(eventmgr),
	m_x(startx),
	m_y(starty)
{
	m_mousemode   = mousemode;
	m_triggermode = true;

	switch (m_mousemode) {
	case KX_MOUSESENSORMODE_LEFTBUTTON:
		m_hotkey = SCA_IInputDevice::KX_LEFTMOUSE;
		break;
	case KX_MOUSESENSORMODE_MIDDLEBUTTON:
		m_hotkey = SCA_IInputDevice::KX_MIDDLEMOUSE;
		break;
	case KX_MOUSESENSORMODE_RIGHTBUTTON:
		m_hotkey = SCA_IInputDevice::KX_RIGHTMOUSE;
		break;
	case KX_MOUSESENSORMODE_WHEELUP:
		m_hotkey = SCA_IInputDevice::KX_WHEELUPMOUSE;
		break;
	case KX_MOUSESENSORMODE_WHEELDOWN:
		m_hotkey = SCA_IInputDevice::KX_WHEELDOWNMOUSE;
		break;
	default:
		; /* ignore, no hotkey */
	}
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



CValue* SCA_MouseSensor::GetReplica()
{
	SCA_MouseSensor* replica = new SCA_MouseSensor(*this);
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
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



bool SCA_MouseSensor::Evaluate(CValue* event)
{
	bool result = false;
	bool reset = m_reset && m_level;
	SCA_IInputDevice* mousedev = m_pMouseMgr->GetInputDevice();



//	SCA_ILogicBrick::RegisterEvent(event);
//  	if (m_mousemode == KX_MOUSESENSORMODE_MOVEMENT) cout << "\nChecking for movement...";
//CValue* val = event->GetProperty("val");

	/* both MOUSEX and MOUSEY. Treat all of these as key-presses.            */
	/* So, treat KX_MOUSESENSORMODE_POSITION as                              */
	/* KX_MOUSESENSORMODE_POSITIONX || KX_MOUSESENSORMODE_POSITIONY          */
	m_reset = false;
	switch (m_mousemode) {
	case KX_MOUSESENSORMODE_LEFTBUTTON:
	case KX_MOUSESENSORMODE_MIDDLEBUTTON:
	case KX_MOUSESENSORMODE_RIGHTBUTTON:
	case KX_MOUSESENSORMODE_WHEELUP:
	case KX_MOUSESENSORMODE_WHEELDOWN:
		{
			const SCA_InputEvent& event = mousedev->GetEventValue(m_hotkey);
			if (event.m_status == SCA_InputEvent::KX_JUSTACTIVATED)
			{
				m_val = 1;
				result = true;
			} else
			{
				if (event.m_status == SCA_InputEvent::KX_JUSTRELEASED)
				{
					m_val = 0;
					result = true;
				} else
				{
					if (event.m_status == SCA_InputEvent::KX_ACTIVE)
					{
						if (m_val == 0)
						{
							m_val = 1;
							if (m_level)
							{
								result = true;
							}
						}
					} else
					{
						if (m_val == 1)
						{
							m_val = 0;
							result = true;
						}
					}
				}
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
			} else
			{
				if (eventX.m_status == SCA_InputEvent::KX_JUSTRELEASED ||
					eventY.m_status == SCA_InputEvent::KX_JUSTRELEASED )
				{
					m_val = 0;
					result = true;
				} else
				{
					if (m_val == 1)
					{
						m_val = 0;
						result = true;
					}
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
	bool res = false;

	res = ((m > KX_MOUSESENSORMODE_NODEF) && (m < KX_MOUSESENSORMODE_MAX));
	
	return res;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_MouseSensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"SCA_MouseSensor",
	sizeof(SCA_MouseSensor),
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

PyParentObject SCA_MouseSensor::Parents[] = {
	&SCA_MouseSensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef SCA_MouseSensor::Methods[] = {
	{"getXPosition", (PyCFunction) SCA_MouseSensor::sPyGetXPosition, METH_VARARGS, GetXPosition_doc},
	{"getYPosition", (PyCFunction) SCA_MouseSensor::sPyGetYPosition, METH_VARARGS, GetYPosition_doc},
	{NULL,NULL} //Sentinel
};

PyObject* SCA_MouseSensor::_getattr(const STR_String& attr) {
	_getattr_up(SCA_ISensor);
}

/* get x position ---------------------------------------------------------- */
const char SCA_MouseSensor::GetXPosition_doc[] = 
"getXPosition\n"
"\tReturns the x-coordinate of the mouse sensor, in frame coordinates.\n"
"\tThe lower-left corner is the origin. The coordinate is given in\n"
"\tpixels\n";
PyObject* SCA_MouseSensor::PyGetXPosition(PyObject* self, 
										 PyObject* args, 
										 PyObject* kwds) {
	return PyInt_FromLong(m_x);
}

/* get y position ---------------------------------------------------------- */
const char SCA_MouseSensor::GetYPosition_doc[] = 
"getYPosition\n"
"\tReturns the y-coordinate of the mouse sensor, in frame coordinates.\n"
"\tThe lower-left corner is the origin. The coordinate is given in\n"
"\tpixels\n";
PyObject* SCA_MouseSensor::PyGetYPosition(PyObject* self, 
										 PyObject* args, 
										 PyObject* kwds) {
	return PyInt_FromLong(m_y);
}

/* eof */
