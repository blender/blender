/**
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
#ifndef __KX_TIMEEVENTMANAGER
#define __KX_TIMEEVENTMANAGER

#include "SCA_EventManager.h"
#include "Value.h"
#include <vector>

using namespace std;

class SCA_TimeEventManager : public SCA_EventManager
{
	vector<CValue*>		m_timevalues; // values that need their time updated regularly
	
public:
	SCA_TimeEventManager(class SCA_LogicManager* logicmgr);
	virtual ~SCA_TimeEventManager();

	virtual void	NextFrame(double curtime, double fixedtime);
	virtual void	RegisterSensor(class SCA_ISensor* sensor);
	virtual void	RemoveSensor(class SCA_ISensor* sensor);
	void			AddTimeProperty(CValue* timeval);
	void			RemoveTimeProperty(CValue* timeval);
};

#endif //__KX_TIMEEVENTMANAGER

