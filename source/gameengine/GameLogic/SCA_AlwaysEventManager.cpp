/**
 * Manager for 'always' events. Since always sensors can operate in pulse
 * mode, they need to be activated.
 *
 * $Id$
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

#include "SCA_AlwaysEventManager.h"
#include "SCA_LogicManager.h"
#include <vector>
#include "SCA_ISensor.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

using namespace std;

SCA_AlwaysEventManager::SCA_AlwaysEventManager(class SCA_LogicManager* logicmgr)
	: SCA_EventManager(ALWAYS_EVENTMGR),
	m_logicmgr(logicmgr)
{
}



void SCA_AlwaysEventManager::NextFrame()
{
	for (set<class SCA_ISensor*>::const_iterator i= m_sensors.begin();!(i==m_sensors.end());i++)
	{
		(*i)->Activate(m_logicmgr, NULL);
	}
}

