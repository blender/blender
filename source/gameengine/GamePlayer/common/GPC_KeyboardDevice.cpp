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

/** \file gameengine/GamePlayer/common/GPC_KeyboardDevice.cpp
 *  \ingroup player
 */


#include "GPC_KeyboardDevice.h"

#include <cstdlib>

/** 
 * NextFrame toggles currentTable with previousTable,
 * and copies relevant event information from previous to current table
 * (pressed keys need to be remembered).
 */
void GPC_KeyboardDevice::NextFrame()
{
	SCA_IInputDevice::NextFrame();

	// Now convert justpressed key events into regular (active) keyevents
	int previousTable = 1-m_currentTable;
	for (int keyevent= KX_BEGINKEY; keyevent<= KX_ENDKEY;keyevent++)
	{
		SCA_InputEvent& oldevent = m_eventStatusTables[previousTable][keyevent];
		if (oldevent.m_status == SCA_InputEvent::KX_JUSTACTIVATED ||
			oldevent.m_status == SCA_InputEvent::KX_ACTIVE	)
		{
			m_eventStatusTables[m_currentTable][keyevent] = oldevent;
			m_eventStatusTables[m_currentTable][keyevent].m_status = SCA_InputEvent::KX_ACTIVE;
			//m_eventStatusTables[m_currentTable][keyevent].m_status = SCA_InputEvent::KX_JUSTACTIVATED;
		}
	}
}



/** 
 * ConvertBPEvent translates Windows keyboard events into ketsji kbd events.
 * Extra event information is stored, like ramp-mode (just released/pressed)
 */
bool GPC_KeyboardDevice::ConvertEvent(int incode, int val)
{
	bool result = false;

	// convert event
	KX_EnumInputs kxevent = this->ToNative(incode);

	// only process it, if it's a key
	if (kxevent >= KX_BEGINKEY && kxevent <= KX_ENDKEY)
	{
		int previousTable = 1-m_currentTable;

		if (val > 0)
		{
			if (kxevent == SCA_IInputDevice::KX_ESCKEY && val != 0 && !m_hookesc)
				result = true;

			// todo: convert val ??
			m_eventStatusTables[m_currentTable][kxevent].m_eventval = val ; //???

			switch (m_eventStatusTables[previousTable][kxevent].m_status)
			{
			case SCA_InputEvent::KX_JUSTACTIVATED:
			case SCA_InputEvent::KX_ACTIVE:
				{
					m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_ACTIVE;
					break;
				}

			case SCA_InputEvent::KX_NO_INPUTSTATUS:
			default:
				{
					m_eventStatusTables[m_currentTable][kxevent].m_status = SCA_InputEvent::KX_JUSTACTIVATED;
				}
			}
			
		} else
		{

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
	return result;
}

void GPC_KeyboardDevice::HookEscape()
{
	m_hookesc = true;
}
