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

#include "SG_IObject.h"
#include "SG_Controller.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SG_Stage gSG_Stage = SG_STAGE_UNKNOWN;

SG_IObject::
SG_IObject(
	void* clientobj,
	void* clientinfo,
	SG_Callbacks& callbacks
): 
	SG_QList(),
	m_SGclientObject(clientobj),
	m_SGclientInfo(clientinfo)
{
	m_callbacks = callbacks;
}

SG_IObject::
SG_IObject(
	const SG_IObject &other
) :
	SG_QList(),
	m_SGclientObject(other.m_SGclientObject),
	m_SGclientInfo(other.m_SGclientInfo),
	m_callbacks(other.m_callbacks) 
{
	//nothing to do
}

	void 
SG_IObject::
AddSGController(
	SG_Controller* cont
){
	m_SGcontrollers.push_back(cont);
}

	void				
SG_IObject::
RemoveAllControllers(
) { 
	m_SGcontrollers.clear(); 
}

void SG_IObject::SetControllerTime(double time)
{
	SGControllerList::iterator contit;
	for (contit = m_SGcontrollers.begin();contit!=m_SGcontrollers.end();++contit)
	{
		(*contit)->SetSimulatedTime(time);
	}
}

/// Needed for replication


SG_IObject::
~SG_IObject()
{
	SGControllerList::iterator contit;

	for (contit = m_SGcontrollers.begin();contit!=m_SGcontrollers.end();++contit)
	{
		delete (*contit);
	}
}
