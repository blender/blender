/*
 * Manager for keyboard events
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/GameLogic/SCA_KeyboardManager.cpp
 *  \ingroup gamelogic
 */


#include "EXP_BoolValue.h"
#include "SCA_KeyboardManager.h"
#include "SCA_KeyboardSensor.h"
#include "EXP_IntValue.h"
#include <vector>

SCA_KeyboardManager::SCA_KeyboardManager(SCA_LogicManager* logicmgr,
										 SCA_IInputDevice* inputdev)
	:	SCA_EventManager(logicmgr, KEYBOARD_EVENTMGR),
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



void SCA_KeyboardManager::NextFrame()
{
	//const SCA_InputEvent& event =	GetEventValue(SCA_IInputDevice::KX_EnumInputs inputcode)=0;
//	cerr << "SCA_KeyboardManager::NextFrame"<< endl;
	SG_DList::iterator<SCA_ISensor> it(m_sensors);
	for (it.begin();!it.end();++it)
	{
		(*it)->Activate(m_logicmgr);
	}
}

bool SCA_KeyboardManager::IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode)
{
	return false;
	//return m_kxsystem->IsPressed(inputcode);
}

