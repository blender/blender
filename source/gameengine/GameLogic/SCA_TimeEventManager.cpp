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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32

// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "SCA_TimeEventManager.h"

#include "SCA_LogicManager.h"
#include "FloatValue.h"

SCA_TimeEventManager::SCA_TimeEventManager(SCA_LogicManager* logicmgr)
: SCA_EventManager(NULL, TIME_EVENTMGR)
{
}



SCA_TimeEventManager::~SCA_TimeEventManager()
{
	for (vector<CValue*>::iterator it = m_timevalues.begin();
			!(it == m_timevalues.end()); ++it)
	{
		(*it)->Release();
	}	
}



void SCA_TimeEventManager::RegisterSensor(SCA_ISensor* sensor)
{
	// not yet
}

void SCA_TimeEventManager::RemoveSensor(SCA_ISensor* sensor)
{
	// empty
}



void SCA_TimeEventManager::NextFrame(double curtime, double fixedtime)
{
	if (m_timevalues.size() > 0 && fixedtime > 0.0)
	{
		CFloatValue* floatval = new CFloatValue(curtime);
		
		// update sensors, but ... need deltatime !
		for (vector<CValue*>::iterator it = m_timevalues.begin();
		!(it == m_timevalues.end()); ++it)
		{
			float newtime = (*it)->GetNumber() + fixedtime;
			floatval->SetFloat(newtime);
			(*it)->SetValue(floatval);
		}
		
		floatval->Release();
	}
}



void SCA_TimeEventManager::AddTimeProperty(CValue* timeval)
{
	timeval->AddRef();
	m_timevalues.push_back(timeval);
}



void SCA_TimeEventManager::RemoveTimeProperty(CValue* timeval)
{
	for (vector<CValue*>::iterator it = m_timevalues.begin();
			!(it == m_timevalues.end()); ++it)
	{
		if ((*it) == timeval)
		{
			this->m_timevalues.erase(it);
			timeval->Release();
			break;
		}
	}
}
