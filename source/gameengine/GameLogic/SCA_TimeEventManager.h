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


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:SCA_TimeEventManager"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__KX_TIMEEVENTMANAGER

