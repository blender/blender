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

#include "SCA_IActuator.h"
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

using namespace std;

SCA_IActuator::SCA_IActuator(SCA_IObject* gameobj, KX_ACTUATOR_TYPE type) :
	SCA_ILogicBrick(gameobj),
	m_type(type),
	m_links(0),
	m_posevent(false),
	m_negevent(false)
{
	// nothing to do
}

bool SCA_IActuator::Update(double curtime, bool frame)
{
	if (frame)
		return Update();
	
	return true;
}

bool SCA_IActuator::Update()
{
	assert(false && "Actuators should override an Update method.");
	return false;
}

void SCA_IActuator::Activate(SG_DList& head)
{
	if (QEmpty())
	{
		InsertActiveQList(m_gameobj->m_activeActuators);
		head.AddBack(&m_gameobj->m_activeActuators);
	}
}

// this function is only used to deactivate actuators outside the logic loop
// e.g. when an object is deleted.
void SCA_IActuator::Deactivate()
{
	if (QDelink())
	{
		// the actuator was in the active list
		if (m_gameobj->m_activeActuators.QEmpty())
			// the owner object has no more active actuators, remove it from the global list
			m_gameobj->m_activeActuators.Delink();
	}
}


void SCA_IActuator::ProcessReplica()
{
	SCA_ILogicBrick::ProcessReplica();
	RemoveAllEvents();
	m_linkedcontrollers.clear();
}



SCA_IActuator::~SCA_IActuator()
{
	RemoveAllEvents();
}

void SCA_IActuator::DecLink()
{
	m_links--;
	if (m_links < 0) 
	{
		printf("Warning: actuator %s has negative m_links: %d\n", m_name.Ptr(), m_links);
		m_links = 0;
	}
}

void SCA_IActuator::LinkToController(SCA_IController* controller)
{
	m_linkedcontrollers.push_back(controller);
}

void SCA_IActuator::UnlinkController(SCA_IController* controller)
{
	std::vector<class SCA_IController*>::iterator contit;
	for (contit = m_linkedcontrollers.begin();!(contit==m_linkedcontrollers.end());++contit)
	{
		if ((*contit) == controller)
		{
			*contit = m_linkedcontrollers.back();
			m_linkedcontrollers.pop_back();
			return;
		}
	}
	printf("Missing link from actuator %s:%s to controller %s:%s\n", 
		m_gameobj->GetName().ReadPtr(), GetName().ReadPtr(), 
		controller->GetParent()->GetName().ReadPtr(), controller->GetName().ReadPtr());
}

void SCA_IActuator::UnlinkAllControllers()
{
	std::vector<class SCA_IController*>::iterator contit;
	for (contit = m_linkedcontrollers.begin();!(contit==m_linkedcontrollers.end());++contit)
	{
		(*contit)->UnlinkActuator(this);
	}
	m_linkedcontrollers.clear();
}

