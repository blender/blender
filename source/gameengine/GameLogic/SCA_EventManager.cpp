/**
 * $Id$
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

#include <assert.h>
#include "SCA_EventManager.h"
#include "SCA_ISensor.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


SCA_EventManager::SCA_EventManager(SCA_LogicManager* logicmgr, EVENT_MANAGER_TYPE mgrtype)
	:m_logicmgr(logicmgr),
	m_mgrtype(mgrtype)
{
}



SCA_EventManager::~SCA_EventManager()
{
	// all sensors should be removed
	assert(m_sensors.Empty());
}

void SCA_EventManager::RegisterSensor(class SCA_ISensor* sensor)
{
	m_sensors.AddBack(sensor);
}

void SCA_EventManager::RemoveSensor(class SCA_ISensor* sensor)
{
	sensor->Delink();
}

void SCA_EventManager::NextFrame(double curtime, double fixedtime)
{
	NextFrame();
}

void SCA_EventManager::NextFrame()
{
	assert(false); // && "Event managers should override a NextFrame method");
}

void SCA_EventManager::EndFrame()
{
}

void SCA_EventManager::UpdateFrame()
{
}

int SCA_EventManager::GetType()
{
	return (int) m_mgrtype;
}
