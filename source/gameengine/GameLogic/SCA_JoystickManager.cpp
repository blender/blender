/**
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
#include "SCA_JoystickSensor.h"
#include "SCA_JoystickManager.h"
#include "SCA_LogicManager.h"
//#include <vector>
#include "SCA_ISensor.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
//using namespace std;


SCA_JoystickManager::SCA_JoystickManager(class SCA_LogicManager* logicmgr)
	: SCA_EventManager(JOY_EVENTMGR),
	m_logicmgr(logicmgr)
{
	int i;
	for (i=0; i<JOYINDEX_MAX; i++) {
		m_joystick[i] = SCA_Joystick::GetInstance( i );
	}
}


SCA_JoystickManager::~SCA_JoystickManager()
{
	int i;
	for (i=0; i<JOYINDEX_MAX; i++) {
		if(m_joystick[i])
			m_joystick[i]->ReleaseInstance();
	}
}


void SCA_JoystickManager::NextFrame(double curtime,double deltatime)
{
	if (m_sensors.Empty()) {
		return;
	}
	else {
		;
#ifndef	DISABLE_SDL
		SCA_Joystick::HandleEvents(); /* Handle all SDL Joystick events */
#endif
		SG_DList::iterator<SCA_JoystickSensor> it(m_sensors);
		for (it.begin();!it.end();++it)
		{
			SCA_JoystickSensor* joysensor = *it;
			if(!joysensor->IsSuspended())
			{
				joysensor->Activate(m_logicmgr);
			}
		}
	}
}


SCA_Joystick *SCA_JoystickManager::GetJoystickDevice( short int joyindex)
{
	/* 
	 *Return the instance of SCA_Joystick for use 
 	 */
	return m_joystick[joyindex];
}
