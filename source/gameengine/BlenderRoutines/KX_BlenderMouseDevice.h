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

/** \file KX_BlenderMouseDevice.h
 *  \ingroup blroutines
 */

#ifndef __KX_BLENDERMOUSEDEVICE_H__
#define __KX_BLENDERMOUSEDEVICE_H__

#include "KX_BlenderInputDevice.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class KX_BlenderMouseDevice : public BL_BlenderInputDevice
{
public:
	KX_BlenderMouseDevice();
	virtual ~KX_BlenderMouseDevice();

	virtual bool	IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode);
//	virtual const SCA_InputEvent&	GetEventValue(SCA_IInputDevice::KX_EnumInputs inputcode);
	virtual bool	ConvertBlenderEvent(unsigned short incode,short val);
	virtual void	NextFrame();


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:KX_BlenderMouseDevice")
#endif
};

#endif //__KX_BLENDERMOUSEDEVICE_H__

