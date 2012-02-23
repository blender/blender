/*
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

/** \file KX_BlenderInputDevice.h
 *  \ingroup blroutines
 */

#ifndef __KX_BLENDERINPUTDEVICE_H__
#define __KX_BLENDERINPUTDEVICE_H__

#if defined(WIN32) && !defined(FREE_WINDOWS)
#pragma warning(disable : 4786)  // shut off 255 char limit debug template warning
#endif

#include <map>

#include "wm_event_types.h"
#include "WM_types.h"
#include "SCA_IInputDevice.h"
#include "BL_BlenderDataConversion.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

/**
 Base Class for Blender specific inputdevices. Blender specific inputdevices are used when the gameengine is running in embedded mode instead of standalone mode.
*/
class BL_BlenderInputDevice : public SCA_IInputDevice
{
public:
	BL_BlenderInputDevice()
	{
	}

	virtual ~BL_BlenderInputDevice()
	{

	}

	KX_EnumInputs ToNative(unsigned short incode) {
		 return ConvertKeyCode(incode);
	}

	virtual bool	IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode)=0;
	//	virtual const SCA_InputEvent&	GetEventValue(SCA_IInputDevice::KX_EnumInputs inputcode)=0;
	virtual bool	ConvertBlenderEvent(unsigned short incode,short val)=0;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:BL_BlenderInputDevice"); }
	void operator delete(void *mem) { MEM_freeN(mem); }
#endif
};
#endif //__KX_BLENDERINPUTDEVICE_H__

