/**
 * Manager for mouse events
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "BoolValue.h"
#include "SCA_MouseManager.h"
#include "SCA_MouseSensor.h"
#include "IntValue.h"


SCA_MouseManager::SCA_MouseManager(SCA_LogicManager* logicmgr,
								   SCA_IInputDevice* mousedev)
	:	SCA_EventManager(MOUSE_EVENTMGR),
		m_mousedevice (mousedev),
		m_logicmanager(logicmgr)
{
	m_xpos = 0;
	m_ypos = 0;
}



SCA_MouseManager::~SCA_MouseManager()
{
}



SCA_IInputDevice* SCA_MouseManager::GetInputDevice()
{
	return m_mousedevice;
}



void SCA_MouseManager::NextFrame()
{
	if (m_mousedevice)
	{
		set<SCA_ISensor*>::iterator it;
		for (it=m_sensors.begin(); it!=m_sensors.end(); it++)
		{
			SCA_MouseSensor* mousesensor = (SCA_MouseSensor*)(*it);
			// (0,0) is the Upper Left corner in our local window
			// coordinates
			if (!mousesensor->IsSuspended())
			{
				const SCA_InputEvent& event = 
					m_mousedevice->GetEventValue(SCA_IInputDevice::KX_MOUSEX);
				int mx = event.m_eventval;
				const SCA_InputEvent& event2 = 
					m_mousedevice->GetEventValue(SCA_IInputDevice::KX_MOUSEY);
				int my = event2.m_eventval;
				
				mousesensor->setX(mx);
				mousesensor->setY(my);
				
				mousesensor->Activate(m_logicmanager,NULL);
			}
		}
	}
}

bool SCA_MouseManager::IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode)
{
	/* We should guard for non-mouse events maybe? A rather silly side       */
	/* effect here is that position-change events are considered presses as  */
	/* well.                                                                 */
	
	return m_mousedevice->IsPressed(inputcode);
}
