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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

using namespace std;

SCA_IActuator::SCA_IActuator(SCA_IObject* gameobj,
							 PyTypeObject* T) :
	m_links(0),
	SCA_ILogicBrick(gameobj,T) 
{
	// nothing to do
}



void SCA_IActuator::AddEvent(CValue* event)
{
	m_events.push_back(event);
}



void SCA_IActuator::RemoveAllEvents()
{	// remove event queue!
	for (vector<CValue*>::iterator i=m_events.begin(); !(i==m_events.end());i++)
	{
		(*i)->Release();
	}
	m_events.clear();
}





bool SCA_IActuator::IsNegativeEvent() const
{
	bool bPositiveEvent(false);
	bool bNegativeEvent(false);

	for (vector<CValue*>::const_iterator i=m_events.begin(); i!=m_events.end();++i)
	{
		if ((*i)->GetNumber() == 0.0f)
		{
			bNegativeEvent = true;
		} else {
			bPositiveEvent = true;
		}
	}

	// if at least 1 positive event, return false
	
	return !bPositiveEvent && bNegativeEvent;
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

void SCA_IActuator::ProcessReplica()
{
	m_events.clear();
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
