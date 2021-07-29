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

/** \file gameengine/BlenderRoutines/KX_BlenderKeyboardDevice.cpp
 *  \ingroup blroutines
 */


#ifdef _MSC_VER
   /* annoying warnings about truncated STL debug info */
#  pragma warning (disable:4786)
#endif 

#include "KX_BlenderKeyboardDevice.h"
#include "KX_KetsjiEngine.h"

KX_BlenderKeyboardDevice::KX_BlenderKeyboardDevice()
	: m_hookesc(false)
{

}
KX_BlenderKeyboardDevice::~KX_BlenderKeyboardDevice()
{

}

/**
 * IsPressed gives boolean information about keyboard status, true if pressed, false if not
 */

bool KX_BlenderKeyboardDevice::IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode)
{
	const SCA_InputEvent & inevent =  m_eventStatusTables[m_currentTable][inputcode];
	bool pressed = (inevent.m_status == SCA_InputEvent::KX_JUSTACTIVATED || 
		inevent.m_status == SCA_InputEvent::KX_ACTIVE);
	return pressed;
}
/*const SCA_InputEvent&	KX_BlenderKeyboardDevice::GetEventValue(SCA_IInputDevice::KX_EnumInputs inputcode)
{
	return m_eventStatusTables[m_currentTable][inputcode];
}
*/
/**
 * NextFrame toggles currentTable with previousTable,
 * and copy relevant event information from previous to current
 * (pressed keys need to be remembered)
 */
void	KX_BlenderKeyboardDevice::NextFrame()
{
	SCA_IInputDevice::NextFrame();
	
	// now convert justpressed keyevents into regular (active) keyevents
	int previousTable = 1-m_currentTable;
	for (int keyevent= KX_BEGINKEY; keyevent<= KX_ENDKEY;keyevent++)
	{
		SCA_InputEvent& oldevent = m_eventStatusTables[previousTable][keyevent];
		if (oldevent.m_status == SCA_InputEvent::KX_JUSTACTIVATED ||
			oldevent.m_status == SCA_InputEvent::KX_ACTIVE	)
		{
			m_eventStatusTables[m_currentTable][keyevent] = oldevent;
			m_eventStatusTables[m_currentTable][keyevent].m_status = SCA_InputEvent::KX_ACTIVE;
		}
	}
}

/**
 * ConvertBlenderEvent translates blender keyboard events into ketsji kbd events
 * extra event information is stored, like ramp-mode (just released/pressed)
*/
bool KX_BlenderKeyboardDevice::ConvertBlenderEvent(unsigned short incode, short val, unsigned int unicode)
{
	bool result = false;
	
	// convert event
	KX_EnumInputs kxevent = this->ToNative(incode);

	// only process it, if it's a key
	if (kxevent >= KX_BEGINKEY && kxevent <= KX_ENDKEY)
	{
		int previousTable = 1-m_currentTable;

		if (val == KM_PRESS || val == KM_DBL_CLICK)
		{
			if (kxevent == KX_KetsjiEngine::GetExitKey() && val != 0 && !m_hookesc)
				result = true;
			if (kxevent == KX_PAUSEKEY && val && (IsPressed(KX_LEFTCTRLKEY) || IsPressed(KX_RIGHTCTRLKEY)))
				result = true;

			// todo: convert val ??
			m_eventStatusTables[m_currentTable][kxevent].m_eventval = val ; //???
			m_eventStatusTables[m_currentTable][kxevent].m_unicode = unicode;

			switch (m_eventStatusTables[previousTable][kxevent].m_status)
			{
			case SCA_InputEvent::KX_JUSTACTIVATED:
				{
					m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_ACTIVE;
					break;
				}
			case SCA_InputEvent::KX_ACTIVE:
			
				{
					m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_ACTIVE;
					break;
				}
			case SCA_InputEvent::KX_NO_INPUTSTATUS:
				{
						m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_JUSTACTIVATED;
						break;
				}
			default:
				{
					m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_JUSTACTIVATED;
				}
			}
			
		} else if (val == KM_RELEASE)
		{
			// blender eventval == 0
			switch (m_eventStatusTables[previousTable][kxevent].m_status)
			{
			case SCA_InputEvent::KX_JUSTACTIVATED:
				{
					m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_JUSTRELEASED;
					break;
				}
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
	return result;
}

void KX_BlenderKeyboardDevice::HookEscape()
{
	m_hookesc = true;
}
