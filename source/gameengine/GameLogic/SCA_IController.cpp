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

#include "SCA_IController.h"
#include "SCA_LogicManager.h"
#include "SCA_IActuator.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SCA_IController::SCA_IController(SCA_IObject* gameobj,
								 PyTypeObject* T)
	:
	SCA_ILogicBrick(gameobj,T)
{
}
	

	
SCA_IController::~SCA_IController()
{
}



const std::vector<class SCA_ISensor*>& SCA_IController::GetLinkedSensors()
{
	return m_linkedsensors;
}



const std::vector<class SCA_IActuator*>& SCA_IController::GetLinkedActuators()
{
	return m_linkedactuators;
}



void SCA_IController::UnlinkAllSensors()
{
	m_linkedsensors.clear();
}



void SCA_IController::UnlinkAllActuators()
{
	m_linkedactuators.clear();
}



/*
void SCA_IController::Trigger(SCA_LogicManager* logicmgr)
{
	//for (int i=0;i<m_linkedactuators.size();i++)
	for (vector<SCA_IActuator*>::const_iterator i=m_linkedactuators.begin();
	!(i==m_linkedactuators.end());i++)
	{
		SCA_IActuator* actua = *i;//m_linkedactuators.at(i);
		
		logicmgr->AddActiveActuator(actua);
	}

}
*/

void SCA_IController::LinkToActuator(SCA_IActuator* actua)
{
	m_linkedactuators.push_back(actua);
}

void	SCA_IController::UnlinkActuator(class SCA_IActuator* actua)
{
	std::vector<class SCA_IActuator*>::iterator actit;
	std::vector<class SCA_IActuator*>::iterator actfound = m_linkedactuators.end();
	for (actit = m_linkedactuators.begin();!(actit==m_linkedactuators.end());++actit)
	{
		if ((*actit) == actua)
			actfound = actit;
		
	}
	if (!(actfound==m_linkedactuators.end()))
	{
		m_linkedactuators.erase(actfound);
	}
	
}

void SCA_IController::LinkToSensor(SCA_ISensor* sensor)
{
	m_linkedsensors.push_back(sensor);
}
