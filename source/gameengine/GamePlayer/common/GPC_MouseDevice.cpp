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

/** \file gameengine/GamePlayer/common/GPC_MouseDevice.cpp
 *  \ingroup player
 */


#include "GPC_MouseDevice.h"

GPC_MouseDevice::GPC_MouseDevice()
{

}
GPC_MouseDevice::~GPC_MouseDevice()
{

}

/**
 * IsPressed gives boolean information about mouse status, true if pressed, false if not.
 */
bool GPC_MouseDevice::IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode)
{
	const SCA_InputEvent & inevent =  m_eventStatusTables[m_currentTable][inputcode];
	bool pressed = (inevent.m_status == SCA_InputEvent::KX_JUSTACTIVATED || 
		inevent.m_status == SCA_InputEvent::KX_ACTIVE);
	return pressed;
}


/** 
 * NextFrame toggles currentTable with previousTable,
 * and copies relevant event information from previous to current table
 * (pressed keys need to be remembered).
 */
void GPC_MouseDevice::NextFrame()
{
	SCA_IInputDevice::NextFrame();
	
	// Convert just pressed events into regular (active) events
	int previousTable = 1-m_currentTable;
	for (int mouseevent= KX_BEGINMOUSE; mouseevent< KX_ENDMOUSEBUTTONS; mouseevent++) {
		SCA_InputEvent& oldevent = m_eventStatusTables[previousTable][mouseevent];
		if (oldevent.m_status == SCA_InputEvent::KX_JUSTACTIVATED ||
		    oldevent.m_status == SCA_InputEvent::KX_ACTIVE)
		{
			m_eventStatusTables[m_currentTable][mouseevent] = oldevent;
			m_eventStatusTables[m_currentTable][mouseevent].m_status = SCA_InputEvent::KX_ACTIVE;
		}
	}
	for (int mousemove= KX_ENDMOUSEBUTTONS; mousemove< KX_ENDMOUSE; mousemove++) {
		SCA_InputEvent& oldevent = m_eventStatusTables[previousTable][mousemove];
		m_eventStatusTables[m_currentTable][mousemove] = oldevent;
		if (oldevent.m_status == SCA_InputEvent::KX_JUSTACTIVATED ||
		    oldevent.m_status == SCA_InputEvent::KX_ACTIVE)
		{
			m_eventStatusTables[m_currentTable][mousemove].m_status = SCA_InputEvent::KX_JUSTRELEASED;
		}
		else {
			if (oldevent.m_status == SCA_InputEvent::KX_JUSTRELEASED) {
				m_eventStatusTables[m_currentTable][mousemove].m_status = SCA_InputEvent::KX_NO_INPUTSTATUS;
			}
		}
	}
}


bool GPC_MouseDevice::ConvertButtonEvent(TButtonId button, bool isDown)
{
	bool result = false;

	switch (button)
	{
	case buttonLeft:
		result = ConvertEvent(KX_LEFTMOUSE, isDown, 0);
		break;
	case buttonMiddle:
		result = ConvertEvent(KX_MIDDLEMOUSE, isDown, 0);
		break;
	case buttonRight:
		result = ConvertEvent(KX_RIGHTMOUSE, isDown, 0);
		break;
	case buttonWheelUp:
		result = ConvertEvent(KX_WHEELUPMOUSE, isDown, 0);
		break;
	case buttonWheelDown:
		result = ConvertEvent(KX_WHEELDOWNMOUSE, isDown, 0);
		break;
	default:
		// Should not happen!
		break;
	}

	return result;
}

/**
 * Splits combined button and x,y cursor move events into separate Ketsji
 * x and y move and button events.
 */
bool GPC_MouseDevice::ConvertButtonEvent(TButtonId button, bool isDown, int x, int y)
{
	// First update state tables for cursor move.
	bool result = ConvertMoveEvent(x, y);

	// Now update for button state.
	if (result) {
		result = ConvertButtonEvent(button, isDown);
	}

	return result;
}

/**
 * Splits combined x,y move into separate Ketsji x and y move events.
 */
bool GPC_MouseDevice::ConvertMoveEvent(int x, int y)
{
	bool result;

	// Convert to local coordinates?
	result = ConvertEvent(KX_MOUSEX, x, 0);
	if (result) {
		result = ConvertEvent(KX_MOUSEY, y, 0);
	}

	return result;
}


bool GPC_MouseDevice::ConvertEvent(KX_EnumInputs kxevent, int eventval, unsigned int unicode)
{
	bool result = true;
	
	// Only process it, if it's a mouse event
	if (kxevent > KX_BEGINMOUSE && kxevent < KX_ENDMOUSE) {
		int previousTable = 1-m_currentTable;

		if (eventval > 0) {
			m_eventStatusTables[m_currentTable][kxevent].m_eventval = eventval;

			switch (m_eventStatusTables[previousTable][kxevent].m_status)
			{
			case SCA_InputEvent::KX_ACTIVE:
			case SCA_InputEvent::KX_JUSTACTIVATED:
				{
					m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_ACTIVE;
					break;
				}
			case SCA_InputEvent::KX_JUSTRELEASED:
				{
					if ( kxevent > KX_BEGINMOUSEBUTTONS && kxevent < KX_ENDMOUSEBUTTONS)
					{
						m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_JUSTACTIVATED;
					} else
					{
						m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_ACTIVE;
						
					}
					break;
				}
			default:
				{
					m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_JUSTACTIVATED;
				}
			}
			
		} 
		else {
			switch (m_eventStatusTables[previousTable][kxevent].m_status)
			{
			case SCA_InputEvent::KX_JUSTACTIVATED:
			case SCA_InputEvent::KX_ACTIVE:
				{
					m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_JUSTRELEASED;
					break;
				}
			default:
				{
					m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_NO_INPUTSTATUS;
				}
			}
		}
	}
	else {
		result = false;
	}
	return result;
}
