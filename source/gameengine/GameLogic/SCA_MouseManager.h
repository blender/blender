/**
 * Manager for mouse events
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


#ifndef __KX_MOUSEMANAGER
#define __KX_MOUSEMANAGER


#include "SCA_EventManager.h"

#include <vector>

using namespace std;

#include "SCA_IInputDevice.h"


class SCA_MouseManager : public SCA_EventManager
{

	class 	SCA_IInputDevice*				m_mousedevice;
	class 	SCA_LogicManager*				m_logicmanager;
	
	unsigned short m_xpos; // Cached location of the mouse pointer
	unsigned short m_ypos;
	
public:
	SCA_MouseManager(class SCA_LogicManager* logicmgr,class SCA_IInputDevice* mousedev);
	virtual ~SCA_MouseManager();

	/**
	 * Checks whether a mouse button is depressed. Ignores requests on non-
	 * mouse related evenst. Can also flag mouse movement.
	 */
	bool IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode);
	virtual void 	NextFrame();	
	SCA_IInputDevice* GetInputDevice();


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:SCA_MouseManager"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__KX_MOUSEMANAGER

