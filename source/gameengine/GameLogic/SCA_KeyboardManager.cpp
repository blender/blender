/**
 * Manager for keyboard events
 *
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <iostream.h>
#include "BoolValue.h"
#include "SCA_KeyboardManager.h"
#include "SCA_KeyboardSensor.h"
#include "IntValue.h"
#include <vector>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SCA_KeyboardManager::SCA_KeyboardManager(SCA_LogicManager* logicmgr,
										 SCA_IInputDevice* inputdev)
	:	SCA_EventManager(KEYBOARD_EVENTMGR),
		m_logicmanager(logicmgr),
		m_inputDevice(inputdev)
{
}



SCA_KeyboardManager::~SCA_KeyboardManager()
{
}



SCA_IInputDevice* SCA_KeyboardManager::GetInputDevice()
{
	return m_inputDevice;
}



void SCA_KeyboardManager::NextFrame(double curtime,double deltatime)
{
	//const SCA_InputEvent& event =	GetEventValue(SCA_IInputDevice::KX_EnumInputs inputcode)=0;
  //	cerr << "SCA_KeyboardManager::NextFrame"<< endl;
	for (int i=0;i<m_sensors.size();i++)
	{
		SCA_KeyboardSensor* keysensor = (SCA_KeyboardSensor*)m_sensors[i];
		keysensor->Activate(m_logicmanager,NULL);
	}

}



void  SCA_KeyboardManager::RegisterSensor(SCA_ISensor* keysensor)
{
	m_sensors.push_back(keysensor);
}



bool SCA_KeyboardManager::IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode)
{
	return false;
	//return m_kxsystem->IsPressed(inputcode);
}

