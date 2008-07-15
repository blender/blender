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

#include "SCA_ISensor.h"
#include "SCA_ActuatorEventManager.h"
#include "SCA_ActuatorSensor.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


SCA_ActuatorEventManager::SCA_ActuatorEventManager(class SCA_LogicManager* logicmgr)
	: SCA_EventManager(ACTUATOR_EVENTMGR),
	m_logicmgr(logicmgr)
{
}



SCA_ActuatorEventManager::~SCA_ActuatorEventManager()
{

}



void SCA_ActuatorEventManager::RegisterSensor(SCA_ISensor* sensor)
{
	m_sensors.push_back(sensor);
}



void SCA_ActuatorEventManager::NextFrame()
{
	// check for changed actuator
	for (vector<SCA_ISensor*>::const_iterator it = m_sensors.begin();!(it==m_sensors.end());it++)
	{
		(*it)->Activate(m_logicmgr,NULL);
	}
}

void SCA_ActuatorEventManager::UpdateFrame()
{
	// update the state of actuator before executing them
	for (vector<SCA_ISensor*>::const_iterator it = m_sensors.begin();!(it==m_sensors.end());it++)
	{
		((SCA_ActuatorSensor*)(*it))->Update();
	}
}