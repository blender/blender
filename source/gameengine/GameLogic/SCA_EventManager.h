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
#ifndef __KX_EVENTMANAGER
#define __KX_EVENTMANAGER

#include <vector>
#include <set>
#include <algorithm>

#include "SG_DList.h"

class SCA_EventManager
{
protected:
	// use a set to speed-up insertion/removal
	//std::set <class SCA_ISensor*>				m_sensors;
	SG_DList		m_sensors;

public:
	enum EVENT_MANAGER_TYPE {
		KEYBOARD_EVENTMGR = 0,
		MOUSE_EVENTMGR,
		ALWAYS_EVENTMGR, 
		TOUCH_EVENTMGR, 
		PROPERTY_EVENTMGR,
		TIME_EVENTMGR,
		RANDOM_EVENTMGR,
		RAY_EVENTMGR,
		RADAR_EVENTMGR,
		NETWORK_EVENTMGR,
		JOY_EVENTMGR,
		ACTUATOR_EVENTMGR
	};

	SCA_EventManager(EVENT_MANAGER_TYPE mgrtype);
	virtual ~SCA_EventManager();
	
	virtual void	RemoveSensor(class SCA_ISensor* sensor);
	virtual void	NextFrame(double curtime, double fixedtime);
	virtual void	NextFrame();
	virtual void    UpdateFrame();
	virtual void	EndFrame();
	virtual void	RegisterSensor(class SCA_ISensor* sensor);
	int		GetType();

protected:
	EVENT_MANAGER_TYPE		m_mgrtype;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:SCA_EventManager"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif

